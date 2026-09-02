#include "HjChaseDriver.h"

#include "CarBase.h"

namespace CH = ChaseConst;

namespace
{
	// 角度の差を -180〜+180 に収める。
	// これをやらないと、行き過ぎた瞬間に「反対側へ350度ずれている」と
	// 判断して、逆へ全力で切る
	float WrapDeg(float d)
	{
		while (d >  180.0f) { d -= 360.0f; }
		while (d < -180.0f) { d += 360.0f; }
		return d;
	}

	// いまの値を目標へ、決めた速さまでで近づける。
	// 制限しないと計算どおりに一瞬で切れて、人の操作に見えない
	float MoveToward(float now, float target, float rate, float dt)
	{
		const float step = rate * dt;
		const float d    = target - now;
		if (fabsf(d) <= step) { return target; }
		return now + ((d > 0.0f) ? step : -step);
	}

	// 0〜1 へ均す。near で 1、far で 0、間は直線
	float Falloff(float v, float nearV, float farV)
	{
		if (farV <= nearV) { return (v <= nearV) ? 1.0f : 0.0f; }
		return std::clamp((farV - v) / (farV - nearV), 0.0f, 1.0f);
	}
}

//----------------------------------------------------------
void HjChaseDriver::Reset()
{
	m_throttle     = 0.0f;
	m_steer        = 0.0f;
	m_prevAngleErr = 0.0f;
	m_hasPrev      = false;
	m_kickTimer    = 0.0f;
	m_kickWait     = 0.0f;
	m_recovering   = false;
	m_shiftWait    = 0.0f;
	m_dbg          = Debug();
}

//----------------------------------------------------------
// 手本をどれだけ信じるか。
//
// 手本の値が正しいのは、自分が手本と似た状態にいるときだけ。
// 押し出されて離れるほど、借りた値は毒になる。
// 角度と位置の両方を見て、悪いほうに合わせる
//----------------------------------------------------------
float HjChaseDriver::Trust(float angleErrDeg, float distErr)
{
	const float a = Falloff(fabsf(angleErrDeg), CH::TrustNearDeg, CH::TrustFarDeg);
	const float d = Falloff(distErr,            CH::TrustNearM,   CH::TrustFarM);
	return std::min(a, d);
}

//----------------------------------------------------------
void HjChaseDriver::Update(float dt, const CarBase& self, const HjCarTrail& trail,
                           Output& out)
{
	out = Output();

	if (dt <= 0.0f || trail.Empty()) { return; }

	// 基準は跡の一番新しい点。
	//
	// 自分で数えた時計を使うと、走り出しを待っている間にずれる。
	// 手本の側は最初から数え始めているので、こちらが遅れたぶんだけ
	// ずっと昔の位置を目標にすることになり、後ろへ戻ろうとする。
	//
	// 「相手のいまの位置から何秒ぶん後ろ」で見れば、ずれようがない
	const float now = trail.NewestTime() + CH::FollowDelay;

	if (m_kickWait  > 0.0f) { m_kickWait  -= dt; }
	if (m_kickTimer > 0.0f) { m_kickTimer -= dt; }
	if (m_shiftWait > 0.0f) { m_shiftWait -= dt; }

	const Math::Vector3 pos   = self.GetPos();
	const Math::Vector3 vel   = self.GetVel();
	const float speed         = vel.Length();
	const float yaw           = self.GetYaw();
	const float yawRate       = self.GetYawRate();
	const float driftDeg      = self.GetDriftAngleDegSigned();
	const float maxSteer      = self.GetMaxSteerAngle();

	//===== 目標を決める =====
	//
	// 時刻で追うだけにすると、押し出された瞬間に目標が先へ逃げていき、
	// 追いつこうとして全開になる。
	// 一番近い点も見て、そこからどれだけ離れているかを別に持つ
	HjCarTrail::Point aim;
	if (!trail.SampleAt(now - CH::FollowDelay + CH::LookAhead, aim)) { return; }

	HjCarTrail::Point nearP;
	float lineDist = 0.0f;
	trail.NearestTo(pos, nearP, lineDist);

	const Math::Vector3 toAim = aim.pos - pos;
	const float gap = toAim.Length();

	//===== 復帰へ入るか =====
	//
	// 前輪の切れ角には上限があるので、それを超えて回ったら姿勢は戻せない。
	// 入るときと出るときで閾値を変える(同じだと境目で細かく切り替わる)
	if (!m_recovering)
	{
		if (fabsf(driftDeg) > CH::SpinDeg
		 || fabsf(yawRate)  > CH::SpinYawRate
		 || lineDist        > CH::LostDist)
		{
			m_recovering = true;
		}
	}
	else
	{
		if (fabsf(driftDeg) < CH::BackDeg
		 && fabsf(yawRate)  < CH::BackYawRate
		 && lineDist        < CH::BackDist)
		{
			m_recovering = false;
		}
	}

	//===== 進みたい向き =====
	// 復帰中は目標点ではなく、線の上の一番近い点の向きへ揃える。
	// 遠くの目標を狙うと、横を向いたまま斜めに突っ込む
	Math::Vector3 want = m_recovering ? nearP.vel : toAim;
	if (want.LengthSquared() < 1e-4f) { want = nearP.vel; }
	if (want.LengthSquared() < 1e-4f) { return; }

	want.y = 0.0f;
	want.Normalize();
	const float wantYaw = atan2f(want.x, want.z);

	//===== 目標の滑り角 =====
	//
	// 審査が見ているのは角度が揃っているか。
	// 手本がその地点で向いていた角度を、そのまま目標にする。
	// 復帰中は狙わない(まず車体を揃えるのが先)
	const float targetDrift = m_recovering ? 0.0f : aim.driftDeg;

	// 手本をどれだけ信じるか
	const float angleErr = WrapDeg(targetDrift - driftDeg);
	const float trust    = m_recovering ? 0.0f : Trust(angleErr, lineDist);

	// 角度のズレの変化。
	// ズレだけを見て直すと、直した頃には行き過ぎていて左右に振れる。
	// 「いま深くなりつつある」段階から効かせるために、変化の速さを見る
	float angleErrRate = 0.0f;
	if (m_hasPrev) { angleErrRate = (angleErr - m_prevAngleErr) / dt; }
	m_prevAngleErr = angleErr;
	m_hasPrev      = true;

	//===== ステア =====
	//
	// 土台：カウンター。
	// ドリフト中、前輪は進んでいる向きを向いていないとグリップを失う。
	// つまり必要なカウンターは滑り角そのもの。
	// これを先に当てておかないと、ズレを見てから直す形になって振れる
	const float counterRad = -driftDeg * (3.14159265f / 180.0f) * CH::CounterGain;

	// 補正：車体の向きと、向きたい向きのズレ。
	// 目標の滑り角ぶんだけ、わざとずらした所を向きたい
	const float aimYaw  = wantYaw + targetDrift * (3.14159265f / 180.0f);
	float yawErr = aimYaw - yaw;
	while (yawErr >  3.14159265f) { yawErr -= 6.28318530f; }
	while (yawErr < -3.14159265f) { yawErr += 6.28318530f; }

	// D は「向きが変わっている速さ」。行き過ぎる前に止める
	const float steerRad = counterRad
	                     + yawErr  * CH::SteerP
	                     - yawRate * CH::SteerD;

	// 切れ角の割合へ直す
	const float steerTarget =
		std::clamp(steerRad / std::max(maxSteer, 0.01f), -1.0f, 1.0f);

	m_steer = MoveToward(m_steer, steerTarget, CH::SteerRate, dt);

	//===== アクセル =====
	//
	// 土台：手本の踏み量。信じる度合いで薄める
	float throttleTarget = aim.throttle * CH::ThrottleFF * trust;

	// 補正：角度のズレ。
	// ドリフト中に角度を支配しているのはアクセル。
	// ステアだけで支えようとすると切れ角の上限に当たって足りなくなる
	throttleTarget += angleErr * CH::AngleP + angleErrRate * CH::AngleD;

	// 補正：距離のズレ。角度より弱くする。
	// 両方を強くすると「追いつきたいから踏む」と
	// 「深いから戻す」が毎フレーム喧嘩する
	if (!m_recovering && gap > CH::GapDeadZone)
	{
		throttleTarget += (gap - CH::GapDeadZone) * CH::GapP;
	}

	throttleTarget = std::clamp(throttleTarget, -1.0f, 1.0f);

	// 復帰中は上限を掛ける。
	// 横を向いたまま踏むと、戻るどころか回り続ける
	if (m_recovering)
	{
		throttleTarget = std::clamp(throttleTarget,
		                            -CH::RecoverThrottle, CH::RecoverThrottle);
	}

	m_throttle = MoveToward(m_throttle, throttleTarget, CH::ThrottleRate, dt);

	//===== 滑り出し =====
	//
	// 角度が欲しいのに滑っていないとき、連続の制御では作れない。
	// きっかけとしてサイドを短く引く
	if (!m_recovering && m_kickTimer <= 0.0f && m_kickWait <= 0.0f
	 && speed > CH::KickMinSpeed
	 && fabsf(targetDrift) > CH::KickNeedDeg
	 && fabsf(driftDeg)    < CH::KickHaveDeg)
	{
		m_kickTimer = CH::KickHold;
		m_kickWait  = CH::KickCooldown;
	}

	//===== 変速 =====
	//
	// 人と同じで、自分でギアを変える必要がある。
	// 1速のままだと吹け切って、そこから先へ進めない。
	//
	// 待ちを入れないと、上げた直後に「回転が落ちた」と判断して下げ、
	// また上げる、を繰り返す
	const float rpm = self.GetRpmRatio();
	if (m_shiftWait <= 0.0f && !m_recovering)
	{
		if (rpm > CH::ShiftUpRpm && m_throttle > 0.0f)
		{
			out.shiftUp = true;
			m_shiftWait = CH::ShiftWait;
		}
		else if (rpm < CH::ShiftDownRpm && self.GetGear() > 1)
		{
			out.shiftDown = true;
			m_shiftWait   = CH::ShiftWait;
		}
	}

	out.throttle  = m_throttle;
	out.steer     = m_steer;
	out.handbrake = (m_kickTimer > 0.0f);

	//===== 調整のための記録 =====
	m_dbg.targetDeg  = targetDrift;
	m_dbg.actualDeg  = driftDeg;
	m_dbg.gap        = gap;
	m_dbg.trust      = trust;
	m_dbg.recovering = m_recovering;
	m_dbg.kicking    = (m_kickTimer > 0.0f);
}
