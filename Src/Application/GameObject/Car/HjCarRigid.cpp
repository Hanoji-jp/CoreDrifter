#include "HjCarRigid.h"

#include "../Stage/HjHeightField.h"
#include "../Stage/HjRoad.h"

#include "../../Const/RigidCarConst.h"
#include "../../Const/CarConst.h"
#include "../../Const/TerrainConst.h"
#include "../../Const/SmokeConst.h"
#include "../../Const/SkidMarkConst.h"

namespace RC = RigidCarConst;

//----------------------------------------------------------
void HjCarRigid::Init()
{
	m_body.SetBox(RC::Mass,
	              Math::Vector3(RC::HalfW, RC::HalfH, RC::HalfL));

	// 車輪の取り付け位置。重心から見た場所。
	// 車ごとに違うものを固定値にすると、どの車も同じ挙動になる
	const float x = m_setup.track;
	const float z = m_setup.base;

	// サス上端の高さ。
	//
	// 自然長のとき、上端は接地点から「タイヤ半径 + 自然長」だけ上に無いといけない。
	// 重心はそこから CgHeight の高さにあるので、差し引いた値が
	// 重心から見た取り付け位置になる。
	//
	// ここを間違えると、置いた瞬間にサスが底付きする
	const float y = RC::WheelRadius + RC::SuspRest - RC::CgHeight;

	const float cz = RC::CgOffsetZ;

	m_wheel[0].mount = Math::Vector3(-x, y,  z - cz);   // 前左
	m_wheel[1].mount = Math::Vector3( x, y,  z - cz);   // 前右
	m_wheel[2].mount = Math::Vector3(-x, y, -z - cz);   // 後左
	m_wheel[3].mount = Math::Vector3( x, y, -z - cz);   // 後右

	m_wheel[0].front = true;
	m_wheel[1].front = true;

	m_wheel[0].left = true;
	m_wheel[2].left = true;
}

//----------------------------------------------------------
void HjCarRigid::Place(const Math::Vector3& pos, float yaw)
{
	//===== 置く高さ =====
	// 受け取るのは接地面の高さ。重心はそこから CgHeight 上。
	//
	// 地形より下を渡されたら持ち上げる。
	// 埋まったまま置くと、サスが縮みきった反力で打ち上げられる。
	// 上に置くぶんは妨げない(空から落としたいこともある)
	Math::Vector3 p = pos;

	{
		float gy = 0.0f;
		Math::Vector3 gn;

		if (SampleGround(p.x, p.z, gy, gn) && p.y < gy) { p.y = gy; }
	}

	m_body.SetPos(p + Math::Vector3::Up * RC::CgHeight);
	m_body.SetRot(Math::Quaternion::CreateFromYawPitchRoll(yaw, 0.0f, 0.0f));
	m_body.Halt();
	m_body.ClearAccum();

	m_flipTime   = 0.0f;
	m_needReset  = false;
	m_driveSpeed = 0.0f;
	m_driveDiff  = 0.0f;

	for (auto& w : m_wheel)
	{
		w.compress = 0.0f;
		w.prevCompress = 0.0f;
		w.bump = 0.0f;
		w.prevBump = 0.0f;
		w.load = 0.0f;
		w.slip = 0.0f;
		w.fy   = 0.0f;
		w.hit  = false;
	}
}

//----------------------------------------------------------
float HjCarRigid::Yaw() const
{
	// 前向きを水平へ落として角度を取る。
	// クォータニオンから直に取り出すと、傾いたときに暴れる。
	//
	// この枠組みは左手系で +Z が前。SimpleMath の Vector3::Forward は
	// 右手系の (0,0,-1) なので、ここで使うと前後が逆になる
	Math::Vector3 f = m_body.ToWorldDir(Math::Vector3(0.0f, 0.0f, 1.0f));
	f.y = 0.0f;
	if (f.LengthSquared() < 1e-8f) { return 0.0f; }

	return atan2f(f.x, f.z);
}

float HjCarRigid::ForwardSpeed() const
{
	Math::Vector3 f = m_body.ToWorldDir(Math::Vector3(0.0f, 0.0f, 1.0f));
	f.y = 0.0f;
	if (f.LengthSquared() > 1e-8f) { f.Normalize(); }

	return m_body.Vel().Dot(f);
}

float HjCarRigid::SlipAngle() const
{
	Math::Vector3 v = m_body.Vel();
	v.y = 0.0f;

	if (v.LengthSquared() < 0.25f) { return 0.0f; }

	const Math::Vector3 local = m_body.ToLocalDir(v);

	return atan2f(local.x, fabsf(local.z));
}

//----------------------------------------------------------
float HjCarRigid::Compression(int wheel) const
{
	if (wheel < 0 || wheel >= 4) { return 0.0f; }
	return m_wheel[wheel].compress;
}

bool HjCarRigid::IsFlipped() const
{
	return m_body.ToWorldDir(Math::Vector3::Up).y < RC::FlipUpDot;
}

bool HjCarRigid::ConsumeNeedReset()
{
	const bool v = m_needReset;
	m_needReset = false;
	return v;
}

//----------------------------------------------------------
// ヨーへの口
//
// アシストは CarBase が持っている。平面モデルではヨーを直に
// 触っていたので、剛体でも同じことができるようにしておく。
//
// ワールドの上向きまわりで見るのは、車体のY軸で見ると
// 傾いたときに「ヨーのつもりでロールを触る」ことになるため
//----------------------------------------------------------
void HjCarRigid::AddYawRate(float dw)
{
	Math::Vector3 w = m_body.AngVel();
	w.y += dw;
	m_body.SetAngVel(w);
}

void HjCarRigid::DampYaw(float k)
{
	Math::Vector3 w = m_body.AngVel();
	w.y -= w.y * std::clamp(k, 0.0f, 1.0f);
	m_body.SetAngVel(w);
}

void HjCarRigid::RotateYaw(float d)
{
	// ワールドの上向きまわりに回す。
	//
	// SimpleMath の a * b は数学の b ⊗ a を返すので、
	// 「いまの姿勢へワールド回転を掛ける」は m_rot * q と書く
	const Math::Quaternion q =
		Math::Quaternion::CreateFromAxisAngle(Math::Vector3::Up, d);

	m_body.SetRot(m_body.Rot() * q);
}

//----------------------------------------------------------
// 地面の高さと向き
//
// 道の上なら道の断面から、外なら高さマップから。
// 道を先に見るのは、高さマップが真上から見た格子なので
// カントを表現できないため
//----------------------------------------------------------
bool HjCarRigid::SampleGround(float x, float z, float& outY, Math::Vector3& outN) const
{
	if (m_pRoad)
	{
		float rh = 0.0f;
		Math::Vector3 rn = Math::Vector3::Up;

		if (m_pRoad->SampleAt(x, z, rh, rn))
		{
			outY = rh;
			outN = rn;
			return true;
		}
	}

	if (m_pField && m_pField->IsValid() && m_pField->Contains(x, z))
	{
		float h = 0.0f;
		Math::Vector3 n = Math::Vector3::Up;
		m_pField->SampleAt(x, z, h, n);

		if (h > TerrainConst::OutsideHeight)
		{
			outY = h;
			outN = n;
			return true;
		}
	}

	return false;
}

//----------------------------------------------------------
// 接地点を探す
//
// ■ サスの向きへ飛ばす
// 真下へ引くだけでは、車体が傾いたときに沈み込みの測り方がずれる。
// サスは車体の下方向に沿って動くので、そちらへ飛ばす。
//
// ■ 刻んで進めて、潜った所で詰める
// 高さマップは位置から高さが直に出るので、面を1枚ずつ調べる必要がない。
// 進めながら「地面より下へ入ったか」を見て、またいだ区間を二分する。
// 面を調べるより速く、しかも抜けない
//----------------------------------------------------------
bool HjCarRigid::Probe(const Math::Vector3& from, const Math::Vector3& dir,
                       float len, Math::Vector3& outPos,
                       Math::Vector3& outNormal) const
{
	// その点の真下の地面の高さ。届かない所は false
	auto above = [&](const Math::Vector3& p, float& outY) -> bool
	{
		Math::Vector3 n;
		return SampleGround(p.x, p.z, outY, n);
	};

	float y0 = 0.0f;
	if (!above(from, y0)) { return false; }

	float prevT = 0.0f;
	float prevD = from.y - y0;

	// 上端が既に地面より下。めり込んでいる
	if (prevD <= 0.0f)
	{
		Math::Vector3 n = Math::Vector3::Up;
		float y = 0.0f;
		SampleGround(from.x, from.z, y, n);

		outPos    = Math::Vector3(from.x, y, from.z);
		outNormal = n;
		return true;
	}

	const float step = len / RC::ProbeSteps;

	for (int i = 1; i <= RC::ProbeSteps; ++i)
	{
		const float t = step * i;
		const Math::Vector3 p = from + dir * t;

		float y = 0.0f;
		if (!above(p, y)) { return false; }

		const float d = p.y - y;

		if (d > 0.0f) { prevT = t; prevD = d; continue; }

		//===== またいだ。前後を詰める =====
		float lo = prevT, hi = t;

		for (int k = 0; k < RC::ProbeRefine; ++k)
		{
			const float mid = (lo + hi) * 0.5f;
			const Math::Vector3 q = from + dir * mid;

			float my = 0.0f;
			if (!above(q, my)) { break; }

			if (q.y - my > 0.0f) { lo = mid; } else { hi = mid; }
		}

		const Math::Vector3 hit = from + dir * hi;

		Math::Vector3 n = Math::Vector3::Up;
		float hy = 0.0f;
		SampleGround(hit.x, hit.z, hy, n);

		outPos    = Math::Vector3(hit.x, hy, hit.z);
		outNormal = n;
		return true;
	}

	return false;
}

//----------------------------------------------------------
void HjCarRigid::Step(const Input& in, float dt)
{
	if (dt <= 0.0f) { return; }

	// バネは刻みが粗いと発散する。
	// 1フレームが長いときは中で分割して解く
	const int steps = std::clamp(
		static_cast<int>(ceilf(dt / RC::MaxStep)), 1, RC::MaxSubSteps);

	const float sub = dt / static_cast<float>(steps);

	for (int i = 0; i < steps; ++i) { SubStep(in, sub); }

	//===== 世界の外へ落ちた =====
	//
	// 地図の外には地面が無いので、接地しないまま落ち続ける。
	// 拾わないと、永久に落ちて戻ってこない
	{
		float gy = 0.0f;
		Math::Vector3 gn;

		const Math::Vector3 p = m_body.Pos();

		const bool hasGround = SampleGround(p.x, p.z, gy, gn);

		if (!hasGround || p.y < gy - RC::FallLimit)
		{
			m_needReset = true;
		}
	}

	//===== 転倒の監視 =====
	if (IsFlipped())
	{
		m_flipTime += dt;
		if (m_flipTime >= RC::FlipResetTime)
		{
			m_flipTime  = 0.0f;
			m_needReset = true;
		}
	}
	else
	{
		m_flipTime = 0.0f;
	}
}

//----------------------------------------------------------
void HjCarRigid::SubStep(const Input& in, float dt)
{
	StepSuspension(dt);
	StepBodyForces(in, dt);
	StepTires(in, dt);

	//===== 空中 =====
	// 接地していないときの回転を落とす。
	// 落とさないと、跳ねた瞬間の回転がそのまま残って回り続ける
	if (m_grounded == 0)
	{
		const Math::Vector3 w = m_body.AngVel();
		m_body.AddTorque(-w * (RC::AirAngDamp * RC::Mass));
	}

	m_body.Integrate(dt);

	PushOut();

	//===== 最高速 =====
	// 水平だけ見る。落下の速さまで頭打ちにすると、
	// 高い所から落ちたときに浮いて見える
	{
		Math::Vector3 v = m_body.Vel();

		const float vy = v.y;
		v.y = 0.0f;

		const float sp = v.Length();

		if (sp > m_setup.maxSpeed && sp > 1e-4f)
		{
			v *= m_setup.maxSpeed / sp;
			v.y = vy;
			m_body.SetVel(v);
		}
	}
}

//----------------------------------------------------------
// サスペンション
//
// 接地点を探し、縮みからばねと減衰の力を出して接地点へかける。
// ここで出した荷重が、そのままタイヤのグリップになる
//----------------------------------------------------------
void HjCarRigid::StepSuspension(float dt)
{
	const Math::Vector3 up = m_body.ToWorldDir(Math::Vector3::Up);

	m_grounded = 0;

	//===== 接地を取る =====
	for (int i = 0; i < 4; ++i)
	{
		Wheel& w = m_wheel[i];

		const Math::Vector3 mount = m_body.ToWorldPos(w.mount);

		w.prevCompress = w.compress;
		w.prevBump     = w.bump;

		// サスの向きへ飛ばす。車体が傾けば、飛ばす向きも傾く
		w.hit = Probe(mount, -up, RC::ProbeLen, w.hitPos, w.hitNormal);

		if (!w.hit)
		{
			w.compress = 0.0f;
			w.load     = 0.0f;
			w.slip     = 0.0f;
			w.bump     = 0.0f;
			continue;
		}

		++m_grounded;

		// 接地点から取り付け位置までの距離。
		//
		// サスの軸へ落として測る。真上からの高さの差で測ると、
		// 車体が傾いたときに実際より短く出る
		const float along = (mount - w.hitPos).Dot(up);

		const float gap = along - RC::WheelRadius;

		const float raw = RC::SuspRest - gap;

		w.compress = std::clamp(raw, 0.0f, RC::SuspTravel);

		// 縮み代を使い切った先へどれだけ入ったか
		w.bump = std::max(raw - RC::SuspTravel, 0.0f);
	}

	//===== 力をかける =====
	for (int i = 0; i < 4; ++i)
	{
		Wheel& w = m_wheel[i];
		if (!w.hit) { continue; }

		// ばねと減衰は調整値の倍率を掛ける。
		// 減衰もばねに合わせて動かす。片方だけ変えると跳ねる
		const float k = w.front ? m_setup.springF : m_setup.springR;

		const float spring = (w.front ? RC::SpringF : RC::SpringR) * k;
		const float damper = (w.front ? RC::DamperF : RC::DamperR) * sqrtf(std::max(k, 0.01f));

		float f = spring * w.compress;

		// 減衰。縮む速さに比例して逆らう
		f += damper * (w.compress - w.prevCompress) / dt;

		//===== スタビライザ =====
		// 左右の沈み込みの差に効く。
		// 前を強くするとアンダー、後ろを強くするとオーバーに寄る
		{
			const int other = (i % 2 == 0) ? (i + 1) : (i - 1);
			const float diff = w.compress - m_wheel[other].compress;

			f += (w.front ? RC::ArbF : RC::ArbR)
			   * (w.front ? m_setup.arbF : m_setup.arbR) * diff;
		}

		//===== 底付き =====
		// 縮み代を使い切ったら、バンプラバーが当たって急に硬くなる。
		//
		// ここで力として受け止めないと、あとは位置の押し出しで
		// 直すしかなくなる。押し出しはエネルギーを奪わないので、
		// 高速で急斜面へ突っ込んでも減速せずに登り切ってしまう
		if (w.bump > 0.0f)
		{
			f += RC::BumpStop * w.bump;

			// 潰れる速さに逆らう。ここが衝撃を熱に変える所
			f += RC::BumpDamp * std::max((w.bump - w.prevBump) / dt, 0.0f);
		}

		// 引っ張る方向には効かない。タイヤは地面を押すだけ
		f = std::max(f, 0.0f);

		w.load = f;

		// 力は接地点へ。重心へまとめると荷重移動が出ない
		m_body.AddForceAt(w.hitNormal * f, w.hitPos);
	}
}

//----------------------------------------------------------
// 車体にかかる力
//
// 重力・空気抵抗・横滑りの抵抗・ダウンフォース・ヨーの減衰。
// 抵抗の式は旧モデルと同じものを使う
//----------------------------------------------------------
void HjCarRigid::StepBodyForces(const Input& in, float dt)
{
	(void)in;

	const float mass = RC::Mass;

	//===== 重力 =====
	m_body.AddForce(Math::Vector3(0.0f, -CarConst::Gravity * mass, 0.0f));

	//===== 進む向きと横 =====
	// 水平へ落とさない。
	//
	// 落とすと、坂を登っている間は速度の一部しか抵抗の対象に
	// ならない。急斜面ほど抵抗が抜けて、減速しなくなる
	const Math::Vector3 up = m_body.ToWorldDir(Math::Vector3::Up);

	Math::Vector3 fwd = m_body.ToWorldDir(Math::Vector3(0.0f, 0.0f, 1.0f));
	if (fwd.LengthSquared() < 1e-8f) { return; }
	fwd.Normalize();

	// 左手系では 上 × 前 が右
	Math::Vector3 right = up.Cross(fwd);
	if (right.LengthSquared() < 1e-8f) { return; }
	right.Normalize();

	const Math::Vector3 v = m_body.Vel();

	const float vLong = v.Dot(fwd);
	const float vLat  = v.Dot(right);

	//===== 空気/転がり抵抗 =====
	Math::Vector3 f = fwd * (-m_setup.drag * vLong * mass);

	//===== タイヤスクラブ抵抗 =====
	// 横滑りはタイヤが摩擦で削り取ってエネルギーを失う。
	// これが無いと横滑り速度が総速度に乗って
	// 「ドリフトの方が直線グリップより速い」になる
	if (m_setup.scrubDragEnabled)
	{
		f += right * (-m_setup.scrubDrag * vLat * mass);
	}

	m_body.AddForce(f);

	//===== ダウンフォース =====
	// 速度の2乗で押し付ける。
	//
	// 前軸と後軸へ分けてかける。重心へまとめると、
	// 前後の配り方を変えても姿勢もグリップ配分も変わらない
	if (m_setup.downforceCoef > 0.0f)
	{
		const float df = m_setup.downforceCoef * (vLong * vLong + vLat * vLat)
		               * mass * CarConst::Gravity;

		const Math::Vector3 down = -Math::Vector3::Up * df;

		const Math::Vector3 axleF =
			m_body.ToWorldPos(Math::Vector3(0.0f, 0.0f,  m_setup.base));
		const Math::Vector3 axleR =
			m_body.ToWorldPos(Math::Vector3(0.0f, 0.0f, -m_setup.base));

		m_body.AddForceAt(down * (1.0f - m_setup.downforceRearBias), axleF);
		m_body.AddForceAt(down * m_setup.downforceRearBias, axleR);
	}

	//===== ヨーの減衰 =====
	// 旧モデルの yawDamp。回り続けるのを抑える
	DampYaw(std::min(m_setup.yawDamp * dt, 1.0f));
}

//----------------------------------------------------------
// タイヤの力
//
// 旧モデル(CarBase::StepTireForces)と同じ式を使う。
//
// ■ 違うのは荷重の出どころだけ
// 旧モデルは前後Gと横Gから荷重を式で振り分けていた。
// こちらはサスのばねが出した実際の荷重(N)を使う。
// 縁石を踏んだ・片輪が浮いた、がそのままグリップに出る。
//
// ■ 単位
// 旧モデルは全部を加速度(m/s^2)で解いていた。
// 剛体は力(N)なので、質量を掛けて渡す。
// 荷重は「4輪均等を 1.0 とする比」に直してから同じ式へ入れる
//----------------------------------------------------------
void HjCarRigid::StepTires(const Input& in, float dt)
{
	const float mass = RC::Mass;

	// 4輪均等ならこれだけ載る
	const float loadRef = mass * CarConst::Gravity * 0.25f;

	//===== 駆動とブレーキ =====
	// 旧モデルと同じ単位(加速度)で受け取る
	float engineTotal = 0.0f;   // 後輪合計の駆動
	float brakeEach   = 0.0f;   // 各輪のブレーキ(負)

	if (in.reverse)
	{
		// 後退ギア：Sで後ろへ駆動(後退速度に上限)。ブレーキは掛けない
		const float v = ForwardSpeed();

		engineTotal = (v > -CarConst::MaxReverseSpeed)
		            ? (CarConst::ReversePower * in.throttle) : 0.0f;
	}
	else
	{
		engineTotal = (in.throttle > 0.0f) ? (in.driveAccel * in.throttle) : 0.0f;
		brakeEach   = (in.throttle < 0.0f)
		            ? (m_setup.brakePower * in.throttle * 0.25f) : 0.0f;
	}

	float slipF = 0.0f;
	float slipR = 0.0f;

	float rearReaction = 0.0f;              // 後輪の縦力合計(加速度)
	float rearFxL = 0.0f, rearFxR = 0.0f;   // 左右の後輪の縦力(デフに使う)

	// 後輪が路面に対して進んでいる速さ(m/s)。
	//
	// 坂では、水平に投影した速度(ForwardSpeed)と路面に沿った速度が
	// 食い違う。投影した値で駆動輪を合わせると差が残り続け、
	// 坂にいるだけで駆動輪が力を出す。
	// ずり落ちが止まったり、登れない坂を登ったりする
	float rearSurf = 0.0f;
	int   rearHit  = 0;

	for (int i = 0; i < 4; ++i)
	{
		Wheel& w = m_wheel[i];

		if (!w.hit || w.load <= 1.0f)
		{
			// 浮いている車輪。路面からの反力が無い。
			// 溜めた横力も捨てる(着地した瞬間に古い力が出ないように)
			w.slip = 0.0f;
			w.fy   = 0.0f;
			continue;
		}

		//===== その輪の実舵角 =====
		// トーは正=トーイン(左右が内側を向く)。
		// アッカーマンはイン側を多く/アウト側を少なく切る
		const float toeSign = w.left ? +1.0f : -1.0f;

		float wsteer;
		if (w.front)
		{
			const bool inner = (in.steer * (w.left ? -1.0f : 1.0f) > 0.0f);

			wsteer = in.steer * (inner ? (1.0f + m_setup.ackermann)
			                           : (1.0f - m_setup.ackermann))
			       + m_setup.toeFront * toeSign;
		}
		else
		{
			wsteer = m_setup.toeRear * toeSign;
		}

		const float wcs = cosf(wsteer);
		const float wsn = sinf(wsteer);

		//===== 接地面の座標 =====
		// 車体の前向きを路面へ落とす。落とさないと、坂で力が浮く
		Math::Vector3 fwd = m_body.ToWorldDir(Math::Vector3(0.0f, 0.0f, 1.0f));

		fwd -= w.hitNormal * fwd.Dot(w.hitNormal);
		if (fwd.LengthSquared() < 1e-8f) { continue; }
		fwd.Normalize();

		Math::Vector3 side = w.hitNormal.Cross(fwd);
		if (side.LengthSquared() < 1e-8f) { continue; }
		side.Normalize();

		//===== 接地点の速度 =====
		// 車体が回っていれば、前輪と後輪では地面に対する速度が違う
		const Math::Vector3 pv = m_body.PointVel(w.hitPos);

		const float vlx = pv.Dot(fwd);
		const float vly = pv.Dot(side);

		// ホイール座標へ回す(後輪もトーの分だけ回る)
		const float wLong =  vlx * wcs + vly * wsn;
		const float wLat  = -vlx * wsn + vly * wcs;

		if (!w.front) { rearSurf += wLong; ++rearHit; }

		//===== 摩擦の上限 =====
		// 荷重感度：実タイヤは荷重が増えるほど摩擦係数が下がる。
		// 下がる側の損失が上がる側の得より大きくなるので、
		// 「荷重が大きく動いた軸は総合的にグリップを失う」＝振ると抜ける
		const float loadRatio = w.load / loadRef;   // 1.0 が基準

		const float mu0 = w.front ? m_setup.muFront : m_setup.muRear;
		const float mu  = std::max(
			mu0 * (1.0f - m_setup.tireLoadSens * (loadRatio - 1.0f)), 0.05f);

		float Dmax = mu * w.load;   // この輪の摩擦上限(N。縦横で共有＝摩擦円)

		// サイドブレーキ：後輪をロックすると横グリップが激減してリアが外へ流れる。
		// これがハンドブレーキドリフトの核心
		if (in.handbrake && !w.front) { Dmax *= m_setup.handbrakeGripMul; }

		//===== 駆動輪の空転 =====
		// 摩擦円は縦横の「配分」しか決めないので、それだけだと
		// 空転してもその輪が出せる力の総量は変わらない。
		// 実タイヤは滑り比がピークを超えると摩擦係数そのものが落ちる。
		// 踏むほど後輪が失われ、旋回に使える力が減って外へ膨らむ
		float wheelDrive = 0.0f;

		if (!w.front)
		{
			// デフ：左右の駆動輪はそれぞれ違う速さで回る。
			// m_driveSpeed が左右の平均、m_driveDiff がその差の半分
			wheelDrive = m_driveSpeed + (w.left ? -m_driveDiff : +m_driveDiff);

			const float slipRatio =
				fabsf(wheelDrive - wLong) / std::max(fabsf(wLong), 2.0f);

			if (slipRatio > CarConst::SpinPeakSlip)
			{
				const float t = std::clamp(
					(slipRatio - CarConst::SpinPeakSlip) /
					std::max(CarConst::SpinFallSlip - CarConst::SpinPeakSlip, 1e-3f),
					0.0f, 1.0f);

				Dmax *= (1.0f - CarConst::SpinGripFall * t);
			}
		}

		//===== 横力(簡易Pacejka) =====
		//   Fy = -D * sin(C * atan(B * slipAngle))
		const float alpha = atan2f(wLat, fabsf(wLong) + m_setup.slipEps);

		float Fy = -Dmax * sinf(m_setup.tireC * atanf(m_setup.tireB * alpha));

		// キャンバー：ネガキャンに応じて横グリップを増す
		Fy *= (1.0f + m_setup.camberGrip * fabsf(m_setup.camber));

		// リラクゼーション長：横力は舵を切った瞬間には立ち上がらず、
		// タイヤがこの距離ぶん転がって初めて定常値に達する。
		// 追従の速さを「時間」ではなく「進んだ距離」で決めるのが要点で、
		// この遅れが振ってから食うまでの"間"を作り、その隙に車が回る
		if (m_setup.tireRelaxLen > 1e-3f)
		{
			const float travel = (fabsf(wLong) + 0.5f) * dt;
			const float k = std::clamp(travel / m_setup.tireRelaxLen, 0.0f, 1.0f);

			w.fy += (Fy - w.fy) * k;
			Fy = w.fy;
		}

		//===== 縦力 =====
		float Fx = 0.0f;

		// ブレーキはタイヤの回転を止める力なので、進行方向の逆へ効く。
		// 向きを見ずに一定の力をかけ続けると、止まったあとも押し続けて
		// 車が後ろへ這い出す。止まりかけたら弱めて、そこで止める
		if (brakeEach != 0.0f)
		{
			const float fade = std::clamp(fabsf(wLong) / CarConst::BrakeFadeSpeed, 0.0f, 1.0f);

			Fx = -std::copysign(fabsf(brakeEach), wLong) * fade * mass;
		}

		if (!w.front)
		{
			// 駆動/空転。車輪の接地面速度と路面速度の差から出す
			Fx += m_setup.longStiff * (wheelDrive - wLong) * mass;
		}

		//===== 摩擦円 =====
		// 縦横合力を Dmax で頭打ち(空転で横が食われて流れる)
		{
			const float mag = sqrtf(Fx * Fx + Fy * Fy);

			if (mag > Dmax && mag > 1e-4f)
			{
				const float s = Dmax / mag;
				Fx *= s;
				Fy *= s;
			}
		}

		//===== 滑り具合 =====
		// 煙と音に使う。旧モデルと同じ量・同じ閾値で正規化する
		{
			if (w.front)
			{
				w.slip = std::clamp(
					(fabsf(wLat) - SkidMarkConst::FrontSlipThreshold) /
					std::max(SkidMarkConst::FrontSlipFull
					       - SkidMarkConst::FrontSlipThreshold, 1e-4f),
					0.0f, 1.0f) * SkidMarkConst::FrontAlphaMul;

				slipF = std::max(slipF, w.slip);
			}
			else
			{
				// 横滑りぶん＋空転ぶん
				const float spin = std::max(wheelDrive - wLong, 0.0f);

				float raw = fabsf(wLat) + spin * 0.5f;

				// サイド中は常時煙。ただし動いているときだけ
				if (in.handbrake && m_body.Vel().Length() > SmokeConst::MinSpeed)
				{
					raw += SmokeConst::HandbrakeBoost;
				}

				w.slip = std::clamp(
					(raw - SmokeConst::SlipThreshold) /
					std::max(SmokeConst::SlipFull - SmokeConst::SlipThreshold, 1e-4f),
					0.0f, 1.0f);

				slipR = std::max(slipR, w.slip);
			}
		}

		//===== 力をかける =====
		// ホイール座標→路面の座標(実舵角ぶん戻す)
		const float fLong = Fx * wcs - Fy * wsn;
		const float fLat  = Fx * wsn + Fy * wcs;

		m_body.AddForceAt(fwd * fLong + side * fLat, w.hitPos);

		if (!w.front)
		{
			const float a = fLong / mass;   // 加速度へ戻して駆動輪の反力にする

			rearReaction += a;
			if (w.left) { rearFxL = a; } else { rearFxR = a; }
		}
	}

	m_slipFront = slipF;
	m_slipRear  = slipR;

	//===== 駆動輪の回転 =====
	// エンジン - 後輪の縦反力。
	// 路面が受け止めきれない力を入れると、車輪だけが先に回る
	const float wheelI = std::max(m_setup.wheelInertia, 0.05f);

	m_driveSpeed += (engineTotal - rearReaction) * (dt / wheelI);

	// デフの差回転：左右の後輪に掛かる縦力の差が回転差を広げ、
	// デフのロックがそれを戻す。ロックが強いほど左右直結に近づき、
	// リアが一体で流れる＝ドリフト向き
	m_driveDiff += (rearFxL - rearFxR) * (dt / (2.0f * wheelI));
	m_driveDiff -= m_driveDiff * std::min(m_setup.lsdLock * dt, 1.0f);

	// 駆動輪が見ている路面速度。浮いていれば水平の値で代用する
	const float vLong = (rearHit > 0)
	                  ? (rearSurf / static_cast<float>(rearHit))
	                  : ForwardSpeed();

	if (in.handbrake)
	{
		// ロック
		m_driveSpeed = 0.0f;
		m_driveDiff  = 0.0f;
	}
	else if (!in.accelPressed || in.clutchOut)
	{
		// 惰行 or クラッチ切断中はフリーホイール＝駆動輪が路面速へ緩和する。
		// これが無いと、切ったはずの高い駆動輪回転が凍結して残り、
		// 幽霊の駆動力が出続けて「クラッチ踏みながら加速」する
		m_driveSpeed += (vLong - m_driveSpeed) * std::min(m_setup.driveRelax * dt, 1.0f);
	}

	//===== 空転の上限 =====
	// 完全に滑り切ったタイヤは、それ以上速く回しても駆動力が増えない。
	// 上限が無いと駆動輪がはずみ車のように回転を溜め込み、
	// グリップが戻った瞬間に一気に放出して不自然な加速になる
	{
		const float cap = fabsf(vLong) * (1.0f + CarConst::MaxSlipRatio)
		                + CarConst::MaxSlipBase;

		m_driveSpeed = std::clamp(m_driveSpeed, -cap, cap);
	}
}

//----------------------------------------------------------
// めり込みを戻す
//
// バネだけでは、強く沈んだときに1フレームで戻りきらない。
// 縮みきっている輪があれば、その場で持ち上げる
//----------------------------------------------------------
void HjCarRigid::PushOut()
{
	const Math::Vector3 up = m_body.ToWorldDir(Math::Vector3::Up);

	// ゴムが潰れきった先の位置。
	//
	// 縮み代のすぐ先から押し出すと、バンプラバーが力で受け止める
	// 前にめり込みが消える。押し出しはエネルギーを奪わないので、
	// 衝撃を吸う役が居なくなって坂でも段差でも減速しない。
	// ここはゴムでも支えきれない深さの保険
	const float floorGap = RC::SuspRest - RC::SuspTravel - RC::BumpMax;

	//===== 一番深く入っている輪を探す =====
	// 輪ごとに押し上げると、同じフレームで4回ぶん重ねて持ち上がる。
	// 坂ではその持ち上げがそのまま「登った距離」になり、
	// 登れないはずの斜面をアクセルで登り切れてしまう
	float over = 0.0f;
	Math::Vector3 normal = Math::Vector3::Up;

	for (int i = 0; i < 4; ++i)
	{
		const Wheel& w = m_wheel[i];
		if (!w.hit) { continue; }

		const Math::Vector3 mount = m_body.ToWorldPos(w.mount);

		const float gap = (mount - w.hitPos).Dot(up) - RC::WheelRadius;

		const float d = floorGap - gap;

		if (d > over) { over = d; normal = w.hitNormal; }
	}

	if (over <= 0.0f) { return; }

	//===== 押し出す =====
	// 向きは車体の上。坂では路面の法線とほぼ同じになる。
	//
	// ワールドの真上へ押すと、斜面を登る向きの成分が混ざる。
	// めり込みを直しているつもりで、坂を登らせていた
	Math::Vector3 p = m_body.Pos();
	p += up * over;
	m_body.SetPos(p);

	//===== 路面へ入っていく速度だけ消す =====
	// ワールドの縦(v.y)で切ると、坂をずり落ちる速度まで消える。
	// 斜面に負けて下がるはずの場面で、ほとんど下がらなくなる
	Math::Vector3 v = m_body.Vel();

	const float into = v.Dot(normal);

	if (into < 0.0f)
	{
		v -= normal * into;
		m_body.SetVel(v);
	}
}
