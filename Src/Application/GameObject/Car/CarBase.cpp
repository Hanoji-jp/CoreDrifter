#include "CarBase.h"

void CarBase::Init()
{
	m_drawType = eDrawTypeLit;

	// 派生クラスが設定したパスでモデルを読み込む
	m_body.SetModelData(m_bodyPath);
	m_wheel.SetModelData(m_wheelPath);

	m_pos = Math::Vector3::Zero;
	m_vel = Math::Vector3::Zero;
	m_yaw = 0.0f;
	m_steer = 0.0f;

	// 保存済みの調整値があれば読み込む（コンストラクタの既定値を上書き）
	LoadTuning();

	// ドリフトスモーク初期化
	m_smoke.Init();

	// 当たり判定の可視化用ワイヤフレーム(F1でトグル)
	m_pDebugWire = std::make_unique<KdDebugWireFrame>();

	// 調整パネル(DrawImGui)はシーン側でステージ用パネルと合成して登録する
}

void CarBase::Update()
{
	const float dt = KdFPSController::GetDt();
	if (dt <= 0.0f) { return; }

	//===== 入力 =====
	// キーボード(デジタル)
	float throttle = 0.0f;
	if (GetAsyncKeyState('W') & 0x8000) { throttle += 1.0f; }
	if (GetAsyncKeyState('S') & 0x8000) { throttle -= 1.0f; }
	float steerInput = 0.0f;
	if (GetAsyncKeyState('A') & 0x8000) { steerInput -= 1.0f; }
	if (GetAsyncKeyState('D') & 0x8000) { steerInput += 1.0f; }
	bool handbrake = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

	// F1：当たり判定の可視化トグル(押した瞬間だけ切り替え)
	const bool debugKey = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
	if (debugKey && !m_prevDebugKey) { m_debugDraw = !m_debugDraw; }
	m_prevDebugKey = debugKey;

	// マニュアル操作(キーボード)：左Shift=クラッチ / E=シフトアップ / Q=シフトダウン
	bool clutchPressed = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0;
	const bool keyShiftUp   = (GetAsyncKeyState('E') & 0x8000) != 0;
	const bool keyShiftDown = (GetAsyncKeyState('Q') & 0x8000) != 0;
	bool shiftUp   = keyShiftUp   && !m_prevKeyShiftUp;    // 押した瞬間だけ
	bool shiftDown = keyShiftDown && !m_prevKeyShiftDown;
	m_prevKeyShiftUp   = keyShiftUp;
	m_prevKeyShiftDown = keyShiftDown;

	// コントローラー(接続時のみアナログ操作を加算)
	//   左スティックX=ステア / RT=アクセル / LT=ブレーキ・後退
	//   A=サイド / LB=クラッチ / RB=シフトアップ / X=シフトダウン
	m_pad.Update();
	if (m_pad.IsConnected())
	{
		const float padThrottle = m_pad.RightTrigger() - m_pad.LeftTrigger();
		const float padSteer     = m_pad.LeftStickX();
		if (fabsf(padThrottle) > 0.0f) { throttle += padThrottle; }
		steerInput += padSteer;
		if (m_pad.IsButtonDown(PadConst::HandbrakeButtons)) { handbrake = true; }
		if (m_pad.IsButtonDown(PadConst::ClutchButton))     { clutchPressed = true; }
		if (m_pad.IsButtonPressed(PadConst::ShiftUpButton))   { shiftUp   = true; }
		if (m_pad.IsButtonPressed(PadConst::ShiftDownButton)) { shiftDown = true; }

		throttle   = std::clamp(throttle,   -1.0f, 1.0f);
		steerInput = std::clamp(steerInput, -1.0f, 1.0f);
	}

	//===== オートカウンター(CarX風)：横滑り方向へ前輪を自動で当て続ける =====
	// 車体の横滑り角(sideslip)＝進行方向と車体前方の角度。ドリフト中は前輪が
	// 進行方向を向く＝カウンターになる。プレイヤー入力はこれに足し引きする。
	const Math::Vector3 fwd0(sinf(m_yaw), 0.0f, cosf(m_yaw));
	const Math::Vector3 right0(cosf(m_yaw), 0.0f, -sinf(m_yaw));
	const float vLong0 = m_vel.Dot(fwd0);
	const float vLat0  = m_vel.Dot(right0);
	const float speedNow = m_vel.Length();

	float autoCounter = 0.0f;
	if (m_counterSteerEnabled && speedNow > m_counterMinSpeed && vLong0 > 0.0f)
	{
		const float sideslip = atan2f(vLat0, fabsf(vLong0) + 1.0f); // 右滑り+ / 左滑り-
		autoCounter = sideslip * m_counterAssist;
		// カウンターは最大切れ角の1.3倍まで(スライド捕捉に十分。maxSteer自体を控えめにして
		// 過剰舵角でのcos減衰＝前輪が食わなくなる問題を避ける)
		autoCounter = std::clamp(autoCounter, -m_maxSteerAngle * 1.3f, m_maxSteerAngle * 1.3f);
	}
	// サイド中のカウンター倍率は"段差だけ"を平滑化(1↔m_handbrakeCounterMul)。
	// カウンター本体は横滑り角に即追従させる＝出口で舵が残らずワイドに膨らまない。
	const float hbTarget = handbrake ? m_handbrakeCounterMul : 1.0f;
	m_hbCounterFactor += (hbTarget - m_hbCounterFactor) * std::min(8.0f * dt, 1.0f);
	autoCounter *= m_hbCounterFactor;
	// アクセルに応じてアシストをなめらかに切り替える(1=アクセル中/ドリフト保持, 0=オフ/復帰)。
	// 二値でパチパチ切り替わらず、Wの踏み加減で角度を連続的にコントロールできる。
	const float liftTarget = (throttle > 0.0f) ? 1.0f : 0.0f;
	m_liftCounterFactor += (liftTarget - m_liftCounterFactor) * std::min(4.0f * dt, 1.0f);
	autoCounter *= m_liftCounterFactor;   // オフに近いほどカウンターが抜けて前輪が食う

	//===== ステア(CarX参考のドリフトアシスト) =====
	// プレイヤー入力は平滑化。オートカウンターは"即時"反映してスライドを素早く捕まえる。
	// これがCarXの「勝手に当て舵してドリフトを維持できる」手触りの核心。
	const float steerFade    = 1.0f / (1.0f + speedNow * 0.03f);   // 高速で舵角を絞る
	const float playerTarget = steerInput * m_maxSteerAngle * steerFade;
	m_playerSteer += (playerTarget - m_playerSteer) * std::min(m_steerSpeed * dt, 1.0f);
	// カウンターは横滑り角へ即追従(遅れなし)＝出口で当て舵が残らずワイドに膨らまない
	m_steer = m_playerSteer + autoCounter;
	// 舵角の上限。過剰に切ると前輪の横力がcos(舵角)で消えて食わなくなるので、
	// maxSteerを控えめにした上で1.3倍までに収める(捕捉に十分＋前輪が効く範囲)。
	const float steerLimit = m_maxSteerAngle * 1.3f;
	m_steer = std::clamp(m_steer, -steerLimit, steerLimit);

	//===== トランジション補助(振り返し) =====
	// ドリフト中(横滑りあり)に舵を切った方向へヨーを後押し＝反対側へパッと振り替えやすい。
	// 通常グリップ走行(横滑り小)では効かない。切った方向へ回頭を足すだけ。
	{
		const float ss = atan2f(vLat0, fabsf(vLong0) + 1.0f);   // 横滑り角
		if (m_transitionAssist > 0.0f && fabsf(ss) > 0.15f && vLong0 > 0.5f)
		{
			m_yawRate += steerInput * m_transitionAssist * dt;   // 切った方向へヨーを後押し
		}
	}

	//===== エンジン / ギア / クラッチ（マニュアルトランスミッション）=====
	// プレイヤーがギアとクラッチを手動操作する。サイドブレーキでもクラッチは切れる(CarX挙動)。
	// クラッチが切れている間はエンジンが駆動系から切り離され、アクセルで空ぶかしできる。
	{
		const float radius = std::max(m_wheelH, 0.01f);

		// 手動シフト(押した瞬間のみ1段。クラッチの有無に関係なく入る＝簡易化)
		if (shiftUp   && m_gear < CarConst::GearCount) { m_gear++; }
		if (shiftDown && m_gear > 1)                   { m_gear--; }

		// クラッチ切断＝サイドブレーキ or クラッチボタン
		const bool clutchOut = handbrake || clutchPressed;
		const float clutchTarget = clutchOut ? 0.0f : 1.0f;
		m_clutch += (clutchTarget - m_clutch) * std::min(CarConst::ClutchSpeed * dt, 1.0f);

		if (!clutchOut)
		{
			// クラッチ接続：駆動輪は地面と一緒に転がる＝車速より遅くならない
			if (vLong0 > 0.0f && m_driveSpeed < vLong0) { m_driveSpeed = vLong0; }

			// 表示RPMは駆動輪速×今のギア比由来。接続へ滑らかに追従。
			// マニュアルなので、高いギアで低速だとRPMが落ち、低いギアで高速だと吹け上がる。
			const float tr = CarConst::GearRatios[m_gear] * CarConst::FinalDrive;
			const float wheelRPM = std::clamp(fabsf(m_driveSpeed / radius) * tr * CarConst::RpmPerRadSec,
			                                  CarConst::IdleRPM, CarConst::MaxRPM);
			m_engineRPM += (wheelRPM - m_engineRPM) * std::min(CarConst::RpmLinkSpeed * dt, 1.0f);
			// レブリミッター：駆動輪速はレッド回転相当を超えられない(無限空転防止)
			const float maxDrive = CarConst::MaxRPM / (CarConst::RpmPerRadSec * tr) * radius;
			m_driveSpeed = std::clamp(m_driveSpeed, -maxDrive, maxDrive);
		}
		else
		{
			// クラッチ切断：ギア固定。アクセルで回転上昇。
			// サイド中は回転維持(空ぶかしキープ)、クラッチだけ切ったときはアイドルへ緩やかに戻る。
			if (throttle > 0.0f)   { m_engineRPM += CarConst::RevUp * throttle * dt; }
			else if (!handbrake)   { m_engineRPM -= CarConst::RevDown * dt; }
			m_engineRPM = std::clamp(m_engineRPM, CarConst::IdleRPM, CarConst::MaxRPM);
		}

		// トルクカーブ(中回転ピーク) × クラッチ × 出力 × ギア比 = 駆動加速
		// ギア比で駆動力が変わる＝1速は加速が強く低速向き、5速は弱く最高速向き。
		const float rpmN   = m_engineRPM / CarConst::MaxRPM;
		const float dd     = rpmN - CarConst::TorquePeakN;
		float torque = std::clamp(1.0f - CarConst::TorqueFall * dd * dd, CarConst::TorqueMin, 1.0f);
		// レブ手前でトルクを絞る(頭打ち)。RevCutStart→RevCutEndで1→0へ。
		// これでギアが上限に張り付き、伸ばすにはシフトアップが必要＝ギア差が体感できる。
		if (rpmN > CarConst::RevCutStart)
		{
			const float t = std::clamp((rpmN - CarConst::RevCutStart) /
			                           std::max(CarConst::RevCutEnd - CarConst::RevCutStart, 1e-4f), 0.0f, 1.0f);
			torque *= (1.0f - t);   // レッドでトルク0
		}
		const float gearFactor = CarConst::GearRatios[m_gear] / CarConst::DriveRefRatio;
		m_driveAccel = m_enginePower * torque * m_clutch * gearFactor;
	}

	//===== 4輪シミュレーション(各輪の荷重・スリップ・摩擦円)をサブステップ積分 =====
	const int   sub = std::max(CarConst::PhysicsSubsteps, 1);
	const float h   = dt / static_cast<float>(sub);
	const float g   = CarConst::Gravity;
	const float halfTrack = std::max(m_track, 0.05f);   // 左右半幅
	const float halfBase  = std::max(m_base,  0.05f);   // 前後半長

	// 4輪の車体ローカル位置(px=右, pz=前)。前=pz>0。
	struct WheelDef { float px; float pz; bool front; };
	const WheelDef wheelDef[4] =
	{
		{ -halfTrack,  halfBase, true  }, // 前左
		{  halfTrack,  halfBase, true  }, // 前右
		{ -halfTrack, -halfBase, false }, // 後左
		{  halfTrack, -halfBase, false }, // 後右
	};

	// 横力(簡易Pacejka)：Fy = -D * sin(C * atan(B * slipAngle))
	auto latForce = [&](float alpha, float D) -> float
	{
		return -D * sinf(m_tireC * atanf(m_tireB * alpha));
	};

	float aLongPrev = 0.0f, aLatPrev = 0.0f;   // 荷重移動に使う前サブステップの加速度

	for (int s = 0; s < sub; ++s)
	{
		const Math::Vector3 forward(sinf(m_yaw), 0.0f, cosf(m_yaw));
		const Math::Vector3 right(cosf(m_yaw), 0.0f, -sinf(m_yaw));
		const float vLong = m_vel.Dot(forward);
		const float vLat  = m_vel.Dot(right);
		const float r     = m_yawRate;
		const float cs    = cosf(m_steer);
		const float sn    = sinf(m_steer);

		// 荷重移動(前後G→前後、横G→左右)。基準は各輪0.25。
		// サスの反応時間ぶん"なめらかに"移す＝アクセルオフでリアが一気に抜けない(急スリップ防止)。
		const float dLongRaw = std::clamp(aLongPrev * m_cgHeight / (2.0f * halfBase  * g), -0.35f, 0.35f);
		const float dLatRaw  = std::clamp(aLatPrev  * m_cgHeight / (2.0f * halfTrack * g), -0.35f, 0.35f);
		m_dLongF += (dLongRaw - m_dLongF) * std::min(10.0f * h, 1.0f);
		m_dLatF  += (dLatRaw  - m_dLatF ) * std::min(10.0f * h, 1.0f);
		const float dLong = m_dLongF;
		const float dLat  = m_dLatF;

		// 駆動(後輪合計)とブレーキ(全輪)
		const float engineTotal = (throttle > 0.0f) ? (m_driveAccel * throttle) : 0.0f;
		const float brakeEach   = (throttle < 0.0f) ? (m_brakePower * throttle * 0.25f) : 0.0f;

		float sumLong = 0.0f, sumLat = 0.0f, sumMz = 0.0f;
		float rearReaction = 0.0f;   // 後輪の縦力合計(駆動輪回転の反力)

		for (int i = 0; i < 4; ++i)
		{
			const WheelDef& w = wheelDef[i];
			// 接地点速度(車体座標)：本体速度＋ヨー回転成分
			const float vlx = vLong - r * w.px;   // 縦
			const float vly = vLat  + r * w.pz;   // 横

			// 操舵輪はホイール座標へ回転
			float wLong = vlx, wLat = vly;
			if (w.front)
			{
				wLong =  vlx * cs + vly * sn;
				wLat  = -vlx * sn + vly * cs;
			}

			// 荷重：加速で後・ブレーキで前、旋回で外側(左右)へ移動
			const bool left = (w.px < 0.0f);
			float load = 0.25f
			           + (w.front ? -dLong : dLong) * 0.5f
			           + (left    ?  dLat  : -dLat) * 0.5f;
			load = std::clamp(load, 0.02f, 0.6f);

			const float mu   = w.front ? m_muFront : m_muRear;
			float Dmax = mu * load * g;   // この輪の摩擦上限(縦横で共有=摩擦円)
			// サイドブレーキ：後輪をロックすると横グリップが激減してリアが外へ流れる。
			// これがハンドブレーキドリフトの核心。倍率0.5=グリップ半減(ImGuiで調整可)。
			if (handbrake && !w.front) { Dmax *= m_handbrakeGripMul; }

			// 横力：スリップ角から
			const float denom = fabsf(wLong) + m_slipEps;
			const float alpha = atan2f(wLat, denom);
			float Fy = latForce(alpha, Dmax);

			// 縦力：後輪=駆動スリップ、全輪=ブレーキ
			float Fx = brakeEach;
			float rearFxTraction = 0.0f;
			if (!w.front)
			{
				rearFxTraction = m_longStiff * (m_driveSpeed - wLong);   // 駆動/空転
				Fx += rearFxTraction;
			}

			// 摩擦円：縦横合力を Dmax で頭打ち(空転で横が食われて流れる)
			const float mag = sqrtf(Fx * Fx + Fy * Fy);
			if (mag > Dmax && mag > 1e-4f) { const float scl = Dmax / mag; Fx *= scl; Fy *= scl; }

			// ホイール座標→車体座標
			float FcarLong = Fx, FcarLat = Fy;
			if (w.front)
			{
				FcarLong = Fx * cs - Fy * sn;
				FcarLat  = Fx * sn + Fy * cs;
			}

			sumLong += FcarLong;
			sumLat  += FcarLat;
			sumMz   += w.pz * FcarLat - w.px * FcarLong;   // ヨーモーメント
			if (!w.front) { rearReaction += FcarLong; }
		}

		// 空気/転がり抵抗(前後)
		sumLong += -m_drag * vLong;
		// タイヤスクラブ抵抗(横滑り)：ドリフト中の横方向の滑りはタイヤが摩擦で
		// 削り取ってエネルギーを失う。これが無いと横滑り速度が総速度|v|に乗って
		// 「ドリフトの方が直線グリップより速い」不具合になる。横滑りぶんを減衰させる。
		sumLat += -m_scrubDrag * vLat;

		const float aLong = sumLong;
		const float aLat  = sumLat;
		aLongPrev = aLong; aLatPrev = aLat;
		m_accelLong = aLong;   // サスペンション用に保持
		m_accelLat  = aLat;

		// 駆動輪の回転ダイナミクス：エンジン - 後輪縦反力
		m_driveSpeed += (engineTotal - rearReaction) * (h / std::max(m_wheelInertia, 0.05f));
		if (handbrake)            { m_driveSpeed = 0.0f; }                                            // ロック
		else if (throttle <= 0.0f){ m_driveSpeed += (vLong - m_driveSpeed) * std::min(m_driveRelax * h, 1.0f); } // 惰行で路面速へ

		// 積分(ヨーはタイヤ力に即応＝グリップは一瞬でかかる。切り返しがキレる)
		m_vel     += (forward * aLong + right * aLat) * h;
		m_yawRate += (sumMz / std::max(m_izz, 0.05f)) * h;
		m_yawRate -= m_yawRate * std::min(m_yawDamp * h, 1.0f);

		// スピン防止アシスト：横滑り角が大きい時、"スライドを深める向き(スピンアウト)"の
		// ヨーだけを抑える。戻す向き(アクセルオフ＋逆ハンでのリカバリー)は邪魔しない。
		if (m_spinAssistEnabled)
		{
			const float ss    = atan2f(vLat, fabsf(vLong) + 1.0f);   // 横滑り角(符号付)
			const float ssAbs = fabsf(ss);
			// ヨーと横滑りが同符号＝スピンアウト方向。逆符号＝リカバリー方向(抑えない)。
			const bool spinningOut = (m_yawRate * vLat > 0.0f);
			if (ssAbs > CarConst::SpinAssistThreshold && spinningOut)
			{
				// サイド中はアシストを弱めてリアを自由に回り込ませる(広がり感)
				const float assist = m_spinAssist * (handbrake ? CarConst::HandbrakeSpinAssistMul : 1.0f);
				const float over = ssAbs - CarConst::SpinAssistThreshold;
				m_yawRate -= m_yawRate * std::min(over * assist * h, 1.0f);
			}
		}
		m_yaw     += m_yawRate * h;

		// 最高速クランプ
		const float sp = m_vel.Length();
		if (sp > m_maxSpeed) { m_vel *= (m_maxSpeed / sp); }
	}

	//===== 車体アライン(アクセルオフ) =====
	// アクセルを離したら車体の向きを進行方向へ寄せる＝角度を抜いてグリップ状態へ。
	// カウンター当てて速度が入力方向へ向いた後、車体もそっちを向いて走り出す(カニ歩き解消)。
	// アクセルON中は何もしない＝物理どおりドリフト維持(スロットルコントロール可)。
	// アクセルを抜くほど強く効く(liftFactor=1で全開)。二値でなく踏み加減に連続。
	const float alignEngage = 1.0f - m_liftCounterFactor;   // オフに近いほど大きい
	if (!handbrake && m_bodyAlign > 0.0f && alignEngage > 0.01f)
	{
		const float sp = m_vel.Length();
		if (sp > 2.0f)
		{
			const float velYaw = atan2f(m_vel.x, m_vel.z);   // 進行方向のワールドヨー
			float d = velYaw - m_yaw;
			while (d >  3.14159265f) { d -= 6.2831853f; }
			while (d < -3.14159265f) { d += 6.2831853f; }
			const float k = std::min(m_bodyAlign * alignEngage * dt, 1.0f);
			m_yaw     += d * k;               // 車体を進行方向へ回頭
			m_yawRate -= m_yawRate * k;       // 余分な回転を抑えて収束
		}
	}

	//===== サスペンション(バネ・ダンパー)：車体をロール/ピッチさせる(見た目) =====
	// 横G→ロール(コーナーで外傾)、前後G→ピッチ(加速で後沈み/ブレーキで前沈み)。
	// 加速度入力を平滑化(空転やアクセルのガタつきで跳ねないように)
	const float aSmooth = std::min(m_accelSmooth * dt, 1.0f);
	m_accelLatF  += (m_accelLat  - m_accelLatF ) * aSmooth;
	m_accelLongF += (m_accelLong - m_accelLongF) * aSmooth;

	// ロール：遠心力で外側が沈む＝車体は旋回の外側へ傾く。
	// 加速(+)で前が上がる(後沈み)＝ノーズアップ、ブレーキ(-)で前ダイブ。
	const float rollTarget  = std::clamp( m_accelLatF  * m_rollGain,  -m_suspMax, m_suspMax);
	const float pitchTarget = std::clamp(-m_accelLongF * m_pitchGain, -m_suspMax, m_suspMax);
	{
		const float acc = m_suspStiff * (rollTarget - m_rollAngle) - m_suspDamp * m_rollVel;
		m_rollVel += acc * dt;  m_rollAngle += m_rollVel * dt;
	}
	{
		const float acc = m_suspStiff * (pitchTarget - m_pitchAngle) - m_suspDamp * m_pitchVel;
		m_pitchVel += acc * dt; m_pitchAngle += m_pitchVel * dt;
	}

	//===== 位置更新(XZ平面を移動 → 地形へ接地) =====
	// 水平方向は物理どおり進める。上下(Y)は地形コリジョンへ下方レイを飛ばして決める。
	m_pos += m_vel * dt;

	//----- 壁(TypeBump)：車体を複数の球で近似して押し戻す＋壁に沿って滑る(物理応答) -----
	// 実車ゲームはボディを凸包/カプセルで壁に当てる。ここは箱vsメッシュが未対応なので、
	// 車体を6個の球(四隅＋前後端)で近似＝カプセル/凸包相当にして車の形・向き・角を拾う。
	// 「面法線がほぼ垂直＝壁」のヒットだけ採用し(水平な床は無視)、壁向きの速度成分だけ殺す
	// →垂直な崖・ガードレールで止まり、接線方向へは滑る(擦りドリフト可)。
	{
		const Math::Vector3 fwdW(sinf(m_yaw), 0.0f, cosf(m_yaw));
		const Math::Vector3 rightW(cosf(m_yaw), 0.0f, -sinf(m_yaw));

		// 車体近似プローブ(車体ローカル：px=右, pz=前)。四隅＋前後端の6点。
		struct Probe { float px; float pz; };
		const float tip = m_base * CarConst::WallProbeTip;
		const Probe probes[6] =
		{
			{ -m_track,  m_base }, {  m_track,  m_base },  // 前左・前右
			{ -m_track, -m_base }, {  m_track, -m_base },  // 後左・後右
			{  0.0f,     tip    }, {  0.0f,    -tip    },  // 前端・後端(中央)
		};

		// 1個の球で壁を押し戻すヘルパ(法線フィルタ＋接線滑り)
		auto resolveSphere = [&](const Math::Vector3& center)
		{
			KdCollider::SphereInfo sph;
			sph.m_sphere.Center = center;
			sph.m_sphere.Radius = CarConst::WallProbeRadius;
			sph.m_type = KdCollider::TypeBump;

			for (auto& wp : m_wpHitList)
			{
				std::shared_ptr<KdGameObject> obj = wp.lock();
				if (!obj) { continue; }
				std::list<KdCollider::CollisionResult> results;
				if (!obj->Intersects(sph, &results)) { continue; }
				for (const auto& r : results)
				{
					// 面法線が水平に近い(=垂直な壁)ものだけ採用。床(法線が上向き)は無視。
					if (fabsf(r.m_hitNDir.y) > CarConst::WallNormalMaxY) { continue; }

					Math::Vector3 n = r.m_hitDir; n.y = 0.0f;   // 水平の押し戻し方向
					if (n.LengthSquared() < 1e-6f) { continue; }
					n.Normalize();

					m_pos += n * r.m_overlapDistance;           // めり込みぶん押し出す

					const float into = m_vel.Dot(n);            // 壁へ食い込む速度を除去
					if (into < 0.0f) { m_vel -= n * into * (1.0f + CarConst::WallSlideBounce); }
				}
			}
		};

		for (const Probe& p : probes)
		{
			const Math::Vector3 center = m_pos
			                           + Math::Vector3(0.0f, CarConst::WallSphereHeight, 0.0f)
			                           + rightW * p.px + fwdW * p.pz;
			resolveSphere(center);
		}
	}

	//----- 接地(TypeGround)：4輪それぞれ真下へレイ → 高さ＋地形の傾き(CarX風の床判定) -----
	// 各タイヤ位置から真下へレイを飛ばし、接地高さを4点求める。
	// 4点の平均で車体の高さ、前後差でピッチ(坂)、左右差でロール(バンク)を出す。
	{
		// 4輪の車体ローカル配置(px=右, pz=前)。DrawLitのタイヤ配置と揃える。
		struct WheelPos { float px; float pz; };
		const WheelPos wheelPos4[4] =
		{
			{ -m_track,  m_base }, // 0 前左
			{  m_track,  m_base }, // 1 前右
			{ -m_track, -m_base }, // 2 後左
			{  m_track, -m_base }, // 3 後右
		};
		const Math::Vector3 fwdW(sinf(m_yaw), 0.0f, cosf(m_yaw));
		const Math::Vector3 rightW(cosf(m_yaw), 0.0f, -sinf(m_yaw));

		float contactY[4] = { 0,0,0,0 };
		bool  contactHit[4] = { false,false,false,false };
		int   hitCount = 0;

		for (int i = 0; i < 4; ++i)
		{
			const Math::Vector3 wpos = m_pos + rightW * wheelPos4[i].px + fwdW * wheelPos4[i].pz;

			KdCollider::RayInfo ray;
			ray.m_pos   = wpos + Math::Vector3(0.0f, CarConst::GroundRayUp, 0.0f);
			ray.m_dir   = Math::Vector3::Down;
			ray.m_range = CarConst::GroundRayLen;
			ray.m_type  = KdCollider::TypeGround;

			float bestY = -FLT_MAX; bool hit = false;
			for (auto& wp : m_wpHitList)
			{
				std::shared_ptr<KdGameObject> obj = wp.lock();
				if (!obj) { continue; }
				std::list<KdCollider::CollisionResult> results;
				if (!obj->Intersects(ray, &results)) { continue; }
				for (const auto& r : results)
				{
					if (r.m_hitPos.y > bestY) { bestY = r.m_hitPos.y; hit = true; }  // 一番高い面=路面上側
				}
			}
			contactHit[i] = hit;
			contactY[i]   = hit ? bestY : 0.0f;
			if (hit) { ++hitCount; }
		}

		// 接地したタイヤ2つの平均高さを返す(片方だけ接地ならそれを使う)
		auto pairAvg = [&](int a, int b) -> float
		{
			if (contactHit[a] && contactHit[b]) { return (contactY[a] + contactY[b]) * 0.5f; }
			return contactHit[a] ? contactY[a] : contactY[b];
		};

		const float k = std::min(CarConst::GroundFollowSmooth * dt, 1.0f);

		if (hitCount > 0)
		{
			m_onGround = true;

			// 車体高さ＝接地タイヤの平均
			float sum = 0.0f;
			for (int i = 0; i < 4; ++i) { if (contactHit[i]) { sum += contactY[i]; } }
			const float avgY = sum / static_cast<float>(hitCount);

			// 前後差→ピッチ(坂)、左右差→ロール(バンク)。両ペアが接地しているときだけ更新。
			float terrainPitchTarget = m_terrainPitch;
			float terrainRollTarget  = m_terrainRoll;
			if ((contactHit[0] || contactHit[1]) && (contactHit[2] || contactHit[3]))
			{
				const float frontY = pairAvg(0, 1);
				const float rearY  = pairAvg(2, 3);
				terrainPitchTarget = atan2f(frontY - rearY, 2.0f * std::max(m_base, 0.05f));  // 前が高い=登り
			}
			if ((contactHit[0] || contactHit[2]) && (contactHit[1] || contactHit[3]))
			{
				const float leftY  = pairAvg(0, 2);
				const float rightY = pairAvg(1, 3);
				terrainRollTarget = atan2f(leftY - rightY, 2.0f * std::max(m_track, 0.05f));   // 左が高い=右下がり
			}
			terrainPitchTarget = std::clamp(terrainPitchTarget, -CarConst::MaxTerrainTilt, CarConst::MaxTerrainTilt);
			terrainRollTarget  = std::clamp(terrainRollTarget,  -CarConst::MaxTerrainTilt, CarConst::MaxTerrainTilt);

			// メッシュのガタつきで跳ねないよう、高さ・傾きをなめらかに追従
			m_pos.y        += (avgY + CarConst::RideHeight - m_pos.y) * k;
			m_terrainPitch += (terrainPitchTarget - m_terrainPitch) * k;
			m_terrainRoll  += (terrainRollTarget  - m_terrainRoll ) * k;
		}
		else
		{
			// 4輪とも地形が無い＝平地扱いへ戻す
			m_onGround = false;
			m_pos.y        += (CarConst::FallbackY - m_pos.y) * k;
			m_terrainPitch += (0.0f - m_terrainPitch) * k;
			m_terrainRoll  += (0.0f - m_terrainRoll ) * k;
		}
	}

	//===== タイヤの転がり回転(視覚) =====
	const Math::Vector3 fwd(sinf(m_yaw), 0.0f, cosf(m_yaw));
	const float radius   = std::max(m_wheelH, 0.01f);
	const float vLongEnd = fwd.Dot(m_vel);   // 路面上の前後速度
	const float twoPi = 6.2831853f;
	auto wrap = [&](float& ang) { if (ang > twoPi) { ang -= twoPi; } if (ang < -twoPi) { ang += twoPi; } };

	// 前輪は路面速度で転がる
	m_wheelSpinFront += (vLongEnd / radius) * dt;
	wrap(m_wheelSpinFront);
	// 後輪は駆動輪の接地面速度で回る＝アクセル空転で速く回り、サイド中は0でロック
	m_wheelSpinRear += (m_driveSpeed / radius) * dt;
	wrap(m_wheelSpinRear);

	//===== ドリフトスモーク放出(後輪のスリップ量に応じて) =====
	{
		const Math::Vector3 rightV(cosf(m_yaw), 0.0f, -sinf(m_yaw));
		const float vLatEnd = rightV.Dot(m_vel);                 // 横滑り速度
		const float spinSlip = std::max(m_driveSpeed - vLongEnd, 0.0f); // 空転ぶん
		float rearSlip = fabsf(vLatEnd) + spinSlip * 0.5f;
		// サイド中の常時煙は「動いているとき」だけ(停止中に引いても出さない)
		const float carSpeed = m_vel.Length();
		if (handbrake && carSpeed > SmokeConst::MinSpeed) { rearSlip += SmokeConst::HandbrakeBoost; }

		// スリップ量を 0〜1 に正規化して放出レートを決定
		const float slip01 = std::clamp(
			(rearSlip - SmokeConst::SlipThreshold) /
			(SmokeConst::SlipFull - SmokeConst::SlipThreshold), 0.0f, 1.0f);

		// 車速ガード：ほぼ停止しているときは一切煙を出さない
		if (slip01 > 0.0f && carSpeed > SmokeConst::MinSpeed)
		{
			// 端数を蓄積して整数枚に(1輪あたりの枚数)
			m_smokeCarry += SmokeConst::SpawnPerSec * slip01 * dt;
			const int n = static_cast<int>(m_smokeCarry);
			m_smokeCarry -= static_cast<float>(n);

			if (n > 0)
			{
				const Math::Vector3 fwdV(sinf(m_yaw), 0.0f, cosf(m_yaw));
				const Math::Vector3 trail = -m_vel * SmokeConst::TrailFactor; // 進行の逆へ引きずる
				// 後輪(左右)の接地位置から放出
				for (int side = -1; side <= 1; side += 2)
				{
					Math::Vector3 wp = m_pos
					                 + rightV * (static_cast<float>(side) * m_track)
					                 - fwdV   * m_base;
					wp.y = SmokeConst::WheelGroundY;
					m_smoke.Emit(wp, trail, n);
				}
			}
		}
	}

	// スモーク粒の更新(寿命・移動)
	m_smoke.Update(dt);
}

//----------------------------------------------------------
// ドリフトスモーク描画（UnLitパス内でシーンから呼ばれる）
//----------------------------------------------------------
void CarBase::DrawEffect()
{
	m_smoke.SetTint(m_smokeColor);
	m_smoke.DrawEffect();
}

//----------------------------------------------------------
// 当たり判定の可視化（F1トグル）：壁プローブ球（緑）＋接地レイ（橙）
//   シーンの DrawDebug パス（UnLit）内から呼ばれる
//----------------------------------------------------------
void CarBase::DrawDebug()
{
	if (m_debugDraw && m_pDebugWire)
	{
		const Math::Color wallCol(0.1f, 1.0f, 0.2f, 1.0f);   // 壁プローブ=緑
		const Math::Color rayCol (1.0f, 0.55f, 0.1f, 1.0f);  // 接地レイ=橙

		const Math::Vector3 fwdW(sinf(m_yaw), 0.0f, cosf(m_yaw));
		const Math::Vector3 rightW(cosf(m_yaw), 0.0f, -sinf(m_yaw));

		// 壁プローブ球(四隅＋前後端の6個)＝壁の当たり判定に使っている球そのもの
		const float tip = m_base * CarConst::WallProbeTip;
		const float ppx[6] = { -m_track, m_track, -m_track, m_track, 0.0f, 0.0f };
		const float ppz[6] = {  m_base,  m_base, -m_base, -m_base, tip, -tip };
		for (int i = 0; i < 6; ++i)
		{
			const Math::Vector3 c = m_pos
			                      + Math::Vector3(0.0f, CarConst::WallSphereHeight, 0.0f)
			                      + rightW * ppx[i] + fwdW * ppz[i];
			m_pDebugWire->AddDebugSphere(c, CarConst::WallProbeRadius, wallCol);
		}

		// 接地レイ(4輪ぶん)＝床判定に飛ばしている下方向レイ
		const float wpx[4] = { -m_track, m_track, -m_track, m_track };
		const float wpz[4] = {  m_base,  m_base, -m_base, -m_base };
		for (int i = 0; i < 4; ++i)
		{
			const Math::Vector3 wp = m_pos + rightW * wpx[i] + fwdW * wpz[i];
			const Math::Vector3 s  = wp + Math::Vector3(0.0f, CarConst::GroundRayUp, 0.0f);
			const Math::Vector3 e  = s  + Math::Vector3(0.0f, -CarConst::GroundRayLen, 0.0f);
			m_pDebugWire->AddDebugLine(s, e, rayCol);
		}
	}

	// 基底が m_pDebugWire を描画＆クリア
	KdGameObject::DrawDebug();
}

void CarBase::DrawLit()
{
	auto& shader = KdShaderManager::Instance().m_StandardShader;

	// 車体全体(本体＋4輪)を地形の傾きへ合わせる＝坂・バンクで車ごと傾く(CarX風の床判定)
	// 傾きは"車のローカル軸"で掛ける＝ヨー(向き)より先に適用する。後に掛けると
	// ワールド軸基準になり、車が向きを変えるとピッチとロールが入れ替わってしまう。
	// 符号：登りでノーズ上げ／左が高い路面で右下がりになるよう反転。
	const Math::Matrix carWorld =
		Math::Matrix::CreateRotationX(-m_terrainPitch) *   // 前後(坂)：ローカルX(右)軸まわり
		Math::Matrix::CreateRotationZ(-m_terrainRoll)  *   // 左右(バンク)：ローカルZ(前)軸まわり
		Math::Matrix::CreateRotationY(m_yaw) *             // 向き(ヨー)
		Math::Matrix::CreateTranslation(m_pos);

	//===== 車体の行列(サスのロール/ピッチを反映。タイヤは接地したまま) =====
	const Math::Matrix bodyW =
		Math::Matrix::CreateScale(m_bodyScale) *
		Math::Matrix::CreateRotationY(m_bodyYaw) *
		Math::Matrix::CreateRotationX(m_pitchAngle) *  // ピッチ(前後の沈み込み)
		Math::Matrix::CreateRotationZ(m_rollAngle) *   // ロール(左右の傾き)
		carWorld;

	//===== タイヤ4輪の行列を先に計算 =====
	struct WheelDef { float x; float z; bool front; };
	const WheelDef wheels[4] =
	{
		{ -m_track,  m_base, true  }, // 前左
		{  m_track,  m_base, true  }, // 前右
		{ -m_track, -m_base, false }, // 後左
		{  m_track, -m_base, false }, // 後右
	};
	Math::Matrix wheelMat[4];
	for (int i = 0; i < 4; ++i)
	{
		const WheelDef& w = wheels[i];
		const bool leftSide = (w.x < 0.0f);
		// 左側は同じモデルだと外向きの面が内を向くので、180度回して外向きにする
		const Math::Matrix flip = leftSide ? Math::Matrix::CreateRotationY(3.14159265f) : Math::Matrix::Identity;
		const Math::Matrix steerRot = w.front ? Math::Matrix::CreateRotationY(m_steer) : Math::Matrix::Identity;

		// 全体 + 前輪/後輪 のオフセット
		const float ox = m_offX + (w.front ? m_frontOffX : m_rearOffX);
		const float oz = m_offZ + (w.front ? m_frontOffZ : m_rearOffZ);

		// 転がり回転(車軸まわり)。モデルローカルで最初に適用。
		// 左側は flip(Y180) で車軸の向きも反転するため、転がり角を反転して打ち消す
		// (同一車軸のタイヤは左右とも同じ向きに回るのが正しい)。
		float spin = w.front ? m_wheelSpinFront : m_wheelSpinRear;
		if (leftSide) { spin = -spin; }
		const Math::Matrix spinRot = Math::Matrix::CreateRotationX(spin);

		wheelMat[i] =
			spinRot *
			Math::Matrix::CreateScale(m_wheelScale) *
			Math::Matrix::CreateRotationZ(m_camber) *   // キャンバー(flipより前=左右で自動ミラー)
			flip *
			Math::Matrix::CreateRotationY(m_wheelYaw) *
			steerRot *
			Math::Matrix::CreateTranslation(w.x + ox, m_wheelH, w.z + oz) *
			carWorld;
	}

	//===== 本体(通常ライティング) =====
	shader.DrawModel(m_body, bodyW);
	for (const auto& m : wheelMat) { shader.DrawModel(m_wheel, m); }

	//===== アウトライン(原神式：背面押し出し。本体の"後"に描く) =====
	if (m_outlineEnabled)
	{
		const Math::Color oc(m_outlineColor.x, m_outlineColor.y, m_outlineColor.z, 1.0f);
		shader.BeginOutline();
		shader.SetOutlineWidth(m_outlineWidth);
		shader.DrawModel(m_body, bodyW, oc);
		for (const auto& m : wheelMat) { shader.DrawModel(m_wheel, m, oc); }
		shader.EndOutline();
		shader.BeginLit();   // 後続オブジェクトのためにLitへ戻す
	}
}

//----------------------------------------------------------
// HUD（スピード / RPM / ステア角）をバー＋フォントで表示
//   ※シーン側が spriteShader.Begin()～End() 内で呼ぶ
//----------------------------------------------------------
void CarBase::DrawSprite()
{
	auto& sp = KdShaderManager::Instance().m_spriteShader;

	//===== 表示する数値 =====
	const float kmh      = m_vel.Length() * CarConst::HudMsToKmh;
	const float rpm      = m_engineRPM;
	const float steerDeg = m_steer * 57.29578f;

	const float speedRatio = std::clamp(kmh / (m_maxSpeed * CarConst::HudMsToKmh), 0.0f, 1.0f);
	const float rpmRatio   = std::clamp(rpm / CarConst::HudRpmMax, 0.0f, 1.0f);
	const float steerNorm  = std::clamp(m_steer / std::max(m_maxSteerAngle, 1e-4f), -1.0f, 1.0f);

	//===== 色 =====
	const Math::Color colBack (0.05f, 0.05f, 0.07f, 0.6f);   // バー背景
	const Math::Color colText (1.0f, 1.0f, 1.0f, 1.0f);
	const Math::Color colSpeed(0.2f, 0.8f, 1.0f, 1.0f);      // 水色
	const Math::Color colRpm  (0.3f, 1.0f, 0.4f, 1.0f);      // 緑
	const Math::Color colRed  (1.0f, 0.25f, 0.2f, 1.0f);     // レッドゾーン
	const Math::Color colSteer(1.0f, 0.85f, 0.2f, 1.0f);     // 黄

	const int L  = CarConst::HudLeft;
	const int bW = CarConst::HudBarW;
	const int bH = CarConst::HudBarH;

	// 左詰めバー(背景＋値)を描くヘルパ
	auto drawBar = [&](int cy, float ratio, const Math::Color& col)
	{
		sp.DrawBox(L + bW / 2, cy, bW / 2, bH / 2, &colBack, true);
		const int fillW = static_cast<int>(bW * ratio);
		if (fillW > 0) { sp.DrawBox(L + fillW / 2, cy, fillW / 2, bH / 2, &col, true); }
	};

	//===== RPM + ギア(上段) =====
	int y = CarConst::HudBaseY + CarConst::HudRowGap * 2;
	drawBar(y, rpmRatio, (rpm >= CarConst::HudRedline) ? colRed : colRpm);
	sp.DrawFont(Math::Vector2(static_cast<float>(L), static_cast<float>(y + bH / 2 + CarConst::HudTextDY)),
	            &colText, "RPM %4.0f  GEAR %d  %s", rpm, m_gear, (m_clutch < 0.5f) ? "[CLUTCH]" : "");

	//===== SPEED(中段) =====
	y = CarConst::HudBaseY + CarConst::HudRowGap;
	drawBar(y, speedRatio, colSpeed);
	sp.DrawFont(Math::Vector2(static_cast<float>(L), static_cast<float>(y + bH / 2 + CarConst::HudTextDY)),
	            &colText, "SPEED %3.0f km/h", kmh);

	//===== STEER(下段：中央基準) =====
	y = CarConst::HudBaseY;
	sp.DrawBox(L + bW / 2, y, bW / 2, bH / 2, &colBack, true);
	const int cx = L + bW / 2;                                  // バー中央
	sp.DrawBox(cx, y, 1, bH / 2, &colText, true);               // 中央マーク
	const int knob = cx + static_cast<int>(steerNorm * (bW / 2));
	sp.DrawBox(knob, y, 6, bH / 2, &colSteer, true);            // ステア位置
	sp.DrawFont(Math::Vector2(static_cast<float>(L), static_cast<float>(y + bH / 2 + CarConst::HudTextDY)),
	            &colText, "STEER %+4.0f", steerDeg);
}

//----------------------------------------------------------
// 見た目のライブ調整パネル（決まった値は各車種のコンストラクタへ反映する）
//----------------------------------------------------------
void CarBase::DrawTuningImGui()
{
	ImGui::Begin(m_tuningName.c_str());

	// ===== ライブ診断 =====
	{
		const Math::Vector3 fwdD(sinf(m_yaw), 0.0f, cosf(m_yaw));
		const Math::Vector3 rightD(cosf(m_yaw), 0.0f, -sinf(m_yaw));
		const float vLongD = m_vel.Dot(fwdD);
		const float vLatD  = m_vel.Dot(rightD);
		const bool  hb = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
		ImGui::SeparatorText(U8("診断(ライブ)"));
		ImGui::Text(U8("速度 %.2f m/s (%.0f km/h)"), m_vel.Length(), m_vel.Length() * 3.6f);
		ImGui::Text(U8("前後vLong %.2f / 横vLat %.2f"), vLongD, vLatD);
		ImGui::Text(U8("駆動輪速 %.2f / ヨー %.2f"), m_driveSpeed, m_yawRate);
		ImGui::Text(U8("RPM %.0f / ギア %d / クラッチ %.2f"), m_engineRPM, m_gear, m_clutch);
		ImGui::Text(U8("ステア %.1f deg / サイド %s"), m_steer * 57.29578f, hb ? "ON" : "off");
	}

	// 保存 / 読込
	if (ImGui::Button(U8("保存"))) { SaveTuning(); }
	ImGui::SameLine();
	if (ImGui::Button(U8("読込(ファイルから)"))) { LoadTuning(); }
	ImGui::TextDisabled("%s", TuneFilePath().c_str());

	ImGui::SeparatorText(U8("車体"));
	ImGui::DragFloat(U8("車体スケール"), &m_bodyScale, 0.01f, 0.01f, 100.0f);
	ImGui::DragFloat(U8("車体向き(rad)"), &m_bodyYaw, 0.01f);

	ImGui::SeparatorText(U8("タイヤ配置"));
	ImGui::DragFloat(U8("トレッド半幅(左右)"), &m_track, 0.01f);
	ImGui::DragFloat(U8("ホイールベース(前後)"), &m_base, 0.01f);
	ImGui::DragFloat(U8("高さ"), &m_wheelH, 0.01f);

	ImGui::SeparatorText(U8("オフセット"));
	ImGui::DragFloat(U8("全体X(右+)"), &m_offX, 0.01f); ImGui::SameLine();
	ImGui::DragFloat(U8("全体Z(前+)"), &m_offZ, 0.01f);
	ImGui::DragFloat(U8("前輪X"), &m_frontOffX, 0.01f); ImGui::SameLine();
	ImGui::DragFloat(U8("前輪Z"), &m_frontOffZ, 0.01f);
	ImGui::DragFloat(U8("後輪X"), &m_rearOffX, 0.01f); ImGui::SameLine();
	ImGui::DragFloat(U8("後輪Z"), &m_rearOffZ, 0.01f);

	ImGui::SeparatorText(U8("タイヤ見た目"));
	ImGui::DragFloat(U8("タイヤスケール"), &m_wheelScale, 0.01f, 0.01f, 100.0f);
	ImGui::DragFloat(U8("タイヤ向き(rad)"), &m_wheelYaw, 0.01f);
	ImGui::DragFloat(U8("キャンバー(rad)"), &m_camber, 0.005f);

	ImGui::SeparatorText(U8("アウトライン"));
	ImGui::Checkbox(U8("有効"), &m_outlineEnabled);
	ImGui::DragFloat(U8("太さ"), &m_outlineWidth, 0.002f, 0.0f, 1.0f);
	ImGui::ColorEdit3(U8("色"), &m_outlineColor.x);

	ImGui::SeparatorText(U8("ドリフトスモーク"));
	ImGui::ColorEdit3(U8("煙の色"), &m_smokeColor.x);

	// 画面全体のエッジ検出アウトライン(トゥーン輪郭・ポストプロセス)
	ImGui::SeparatorText(U8("画面アウトライン(トゥーン)"));
	{
		auto& pp = KdShaderManager::Instance().m_postProcessShader;
		bool sceneOutline = pp.IsSceneOutlineEnabled();
		if (ImGui::Checkbox(U8("有効##sceneOutline"), &sceneOutline)) { pp.SetSceneOutlineEnabled(sceneOutline); }
		ImGui::DragFloat(U8("太さ(px)##sceneOutline"), &pp.WorkOutlineThickness(), 0.05f, 0.5f, 8.0f);
		ImGui::DragFloat(U8("深度しきい値(シルエット)"), &pp.WorkOutlineDepthThreshold(), 0.01f, 0.01f, 2.0f);
		ImGui::DragFloat(U8("法線しきい値(角)"), &pp.WorkOutlineNormalThreshold(), 0.01f, 0.01f, 1.0f);
		ImGui::DragFloat(U8("濃さ##sceneOutline"), &pp.WorkOutlineEdgeStrength(), 0.02f, 0.0f, 1.0f);
		ImGui::ColorEdit3(U8("色##sceneOutline"), &pp.WorkOutlineColor().x);
	}

	ImGui::SeparatorText(U8("動力"));
	ImGui::DragFloat(U8("エンジン出力(加速)"), &m_enginePower, 0.5f, 0.0f, 200.0f);
	ImGui::DragFloat(U8("ブレーキ/後退力"), &m_brakePower, 0.5f, 0.0f, 200.0f);
	ImGui::DragFloat(U8("最高速"), &m_maxSpeed, 0.5f, 1.0f, 200.0f);
	ImGui::DragFloat(U8("抵抗(転がり/空気)"), &m_drag, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat(U8("横滑りスクラブ抵抗(ドリフト速度抑制)"), &m_scrubDrag, 0.05f, 0.0f, 5.0f);
	ImGui::DragFloat(U8("最大切れ角(rad)"), &m_maxSteerAngle, 0.01f, 0.0f, 1.5f);
	ImGui::DragFloat(U8("ステア追従速度"), &m_steerSpeed, 0.1f, 0.1f, 50.0f);

	ImGui::SeparatorText(U8("タイヤ物理(CarX風)"));
	ImGui::DragFloat(U8("前輪グリップμ"), &m_muFront, 0.01f, 0.1f, 3.0f);
	ImGui::DragFloat(U8("後輪グリップμ"), &m_muRear, 0.01f, 0.1f, 3.0f);
	ImGui::DragFloat(U8("剛性B(切れ味)"), &m_tireB, 0.1f, 1.0f, 30.0f);
	ImGui::DragFloat(U8("形状C"), &m_tireC, 0.01f, 1.0f, 2.0f);
	ImGui::DragFloat(U8("ヨー慣性(小=クイック)"), &m_izz, 0.02f, 0.1f, 10.0f);
	ImGui::DragFloat(U8("ヨー減衰"), &m_yawDamp, 0.02f, 0.0f, 5.0f);
	ImGui::DragFloat(U8("重心高(荷重移動)"), &m_cgHeight, 0.01f, 0.0f, 2.0f);
	ImGui::DragFloat(U8("アクセルで後輪グリップ↓(0-1)"), &m_rearGripThrottleLoss, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat(U8("ハンドブレーキ後輪倍率"), &m_handbrakeGripMul, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat(U8("サイド制動力(引くと減速)"), &m_handbrakeBrake, 0.1f, 0.0f, 20.0f);
	ImGui::DragFloat(U8("サイド中ヨー減衰(回りすぎ防止)"), &m_handbrakeYawDamp, 0.1f, 0.0f, 10.0f);
	ImGui::DragFloat(U8("低速安定(スリップ分母)"), &m_slipEps, 0.1f, 0.1f, 20.0f);
	ImGui::DragFloat(U8("停止付近の横力フェード速度"), &m_lowSpeedGrip, 0.1f, 0.1f, 10.0f);
	ImGui::DragFloat(U8("ドリフト速度維持(0-1,止まらない)"), &m_driftRetain, 0.02f, 0.0f, 1.0f);

	ImGui::SeparatorText(U8("駆動輪の空転(摩擦円)"));
	ImGui::DragFloat(U8("縦タイヤ剛性"), &m_longStiff, 0.2f, 1.0f, 60.0f);
	ImGui::DragFloat(U8("駆動輪の慣性(小=空転しやすい)"), &m_wheelInertia, 0.01f, 0.05f, 5.0f);
	ImGui::DragFloat(U8("惰行の路面追従"), &m_driveRelax, 0.1f, 0.0f, 30.0f);
	ImGui::Text(U8("駆動輪速: %.1f / 車速: %.1f"), m_driveSpeed, m_vel.Length());

	ImGui::SeparatorText(U8("サスペンション(ロール/ピッチ)"));
	ImGui::DragFloat(U8("バネ定数"), &m_suspStiff, 1.0f, 1.0f, 300.0f);
	ImGui::DragFloat(U8("ダンパー"), &m_suspDamp, 0.5f, 0.0f, 80.0f);
	ImGui::DragFloat(U8("ロール量(横G, 符号で向き)"), &m_rollGain, 0.001f, -0.1f, 0.1f);
	ImGui::DragFloat(U8("ピッチ量(前後G, 符号で向き)"), &m_pitchGain, 0.001f, -0.1f, 0.1f);
	ImGui::DragFloat(U8("最大角(rad)"), &m_suspMax, 0.01f, 0.0f, 0.7f);
	ImGui::DragFloat(U8("入力平滑化(小=なめらか)"), &m_accelSmooth, 0.2f, 0.5f, 30.0f);

	ImGui::SeparatorText(U8("オートカウンター(CarX風)"));
	ImGui::Checkbox(U8("有効##counter"), &m_counterSteerEnabled);
	ImGui::DragFloat(U8("カウンター強さ(0-1.3)"), &m_counterAssist, 0.01f, 0.0f, 1.3f);
	ImGui::DragFloat(U8("効き始め速度"), &m_counterMinSpeed, 0.1f, 0.0f, 20.0f);
	ImGui::DragFloat(U8("サイド中カウンター倍率(1=通常)"), &m_handbrakeCounterMul, 0.02f, 0.0f, 1.0f);

	ImGui::SeparatorText(U8("スピン防止アシスト"));
	ImGui::Checkbox(U8("有効##spin"), &m_spinAssistEnabled);
	ImGui::DragFloat(U8("アシスト強さ(大=スピンしにくい)"), &m_spinAssist, 0.1f, 0.0f, 20.0f);
	ImGui::DragFloat(U8("グリップ復帰の滑らかさ(小=なめらか)"), &m_gripRelax, 0.5f, 2.0f, 60.0f);
	ImGui::DragFloat(U8("車体アライン(オフで角度を抜く, 0=OFF)"), &m_bodyAlign, 0.2f, 0.0f, 20.0f);
	ImGui::DragFloat(U8("トランジション補助(振り返し, 0=OFF)"), &m_transitionAssist, 0.1f, 0.0f, 15.0f);

	ImGui::End();
}

//----------------------------------------------------------
// 調整値の保存 / 読込
//----------------------------------------------------------
std::string CarBase::TuneFilePath() const
{
	return "Asset/Data/CarTune_" + m_saveKey + ".txt";
}

std::vector<std::pair<const char*, float*>> CarBase::TuneParamList()
{
	return {
		{ "bodyScale", &m_bodyScale }, { "bodyYaw", &m_bodyYaw },
		{ "wheelScale", &m_wheelScale }, { "wheelYaw", &m_wheelYaw },
		{ "track", &m_track }, { "base", &m_base }, { "wheelH", &m_wheelH }, { "camber", &m_camber },
		{ "offX", &m_offX }, { "offZ", &m_offZ },
		{ "frontOffX", &m_frontOffX }, { "frontOffZ", &m_frontOffZ },
		{ "rearOffX", &m_rearOffX }, { "rearOffZ", &m_rearOffZ },
		{ "enginePower", &m_enginePower }, { "brakePower", &m_brakePower },
		{ "maxSpeed", &m_maxSpeed }, { "drag", &m_drag }, { "scrubDrag", &m_scrubDrag },
		{ "maxSteerAngle", &m_maxSteerAngle }, { "steerSpeed", &m_steerSpeed },
		// CarX風タイヤモデル
		{ "muFront", &m_muFront }, { "muRear", &m_muRear },
		{ "tireB", &m_tireB }, { "tireC", &m_tireC },
		{ "izz", &m_izz }, { "cgHeight", &m_cgHeight }, { "yawDamp", &m_yawDamp },
		{ "rearGripThrottleLoss", &m_rearGripThrottleLoss },
		{ "handbrakeGripMul", &m_handbrakeGripMul }, { "slipEps", &m_slipEps },
		{ "handbrakeSlipEps", &m_handbrakeSlipEps }, { "lowSpeedGrip", &m_lowSpeedGrip },
		{ "handbrakeBrake", &m_handbrakeBrake }, { "handbrakeYawDamp", &m_handbrakeYawDamp },
		// 駆動輪の空転(摩擦円)
		{ "longStiff", &m_longStiff }, { "wheelInertia", &m_wheelInertia }, { "driveRelax", &m_driveRelax },
		{ "driftRetain", &m_driftRetain },
		// サスペンション
		{ "suspStiff", &m_suspStiff }, { "suspDamp", &m_suspDamp },
		{ "rollGain", &m_rollGain }, { "pitchGain", &m_pitchGain }, { "suspMax", &m_suspMax },
		{ "accelSmooth", &m_accelSmooth },
		// オートカウンター
		{ "counterAssist", &m_counterAssist }, { "counterMinSpeed", &m_counterMinSpeed },
		{ "spinAssist", &m_spinAssist }, { "gripRelax", &m_gripRelax },
		{ "gripCatch", &m_gripCatch }, { "bodyAlign", &m_bodyAlign },
		{ "transitionAssist", &m_transitionAssist },
		{ "handbrakeCounterMul", &m_handbrakeCounterMul },
		// アウトライン
		{ "outlineWidth", &m_outlineWidth },
		{ "outlineColR", &m_outlineColor.x }, { "outlineColG", &m_outlineColor.y }, { "outlineColB", &m_outlineColor.z },
		// ドリフトスモーク色
		{ "smokeColR", &m_smokeColor.x }, { "smokeColG", &m_smokeColor.y }, { "smokeColB", &m_smokeColor.z },
	};
}

void CarBase::SaveTuning()
{
	std::ofstream ofs(TuneFilePath());
	if (!ofs) { return; }
	for (const auto& p : TuneParamList()) { ofs << p.first << " " << *p.second << "\n"; }
}

void CarBase::LoadTuning()
{
	std::ifstream ifs(TuneFilePath());
	if (!ifs) { return; }
	auto params = TuneParamList();
	std::string key;
	float val = 0.0f;
	while (ifs >> key >> val)
	{
		for (const auto& p : params) { if (key == p.first) { *p.second = val; break; } }
	}
}
