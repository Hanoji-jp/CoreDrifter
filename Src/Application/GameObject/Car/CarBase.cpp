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
	m_neon.Init();
	m_skid.Init();

	// 当たり判定の可視化用ワイヤフレーム(F1でトグル)
	m_pDebugWire = std::make_unique<KdDebugWireFrame>();

	// 調整パネル(DrawImGui)はシーン側でステージ用パネルと合成して登録する
}

void CarBase::SetSpawn(const Math::Vector3& pos, float yaw)
{
	// 位置・向きを設定し、運動状態(速度・ヨー・駆動輪回転など)をリセット。
	// 初期配置とリスポーンの両方に使う。
	m_pos      = pos;
	m_yaw      = yaw;
	m_spawnPos = pos;   // リスポーン地点として記憶
	m_spawnYaw = yaw;
	m_vel      = Math::Vector3::Zero;
	m_yawRate  = 0.0f;
	m_steer    = 0.0f;
	m_playerSteer = 0.0f;
	m_driveSpeed  = 0.0f;
	m_engineRPM   = CarConst::IdleRPM;
	m_gear        = 1;
	m_clutch      = 1.0f;
	m_reverse     = false;
	// 滞空/垂直状態もリセット(空中でリスポーンしても即接地判定へ)
	m_airborne        = false;
	m_velY            = 0.0f;
	m_supportVelY     = 0.0f;
	m_prevGroundValid = false;
	m_pitchRate       = 0.0f;
	m_rollRate        = 0.0f;
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

	// F2：煙シルエット輪郭のON/OFFトグル(消え方の切り分け用。OFFで従来の直描き)
	const bool outlineKey = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
	if (outlineKey && !m_prevOutlineKey)
	{
		auto& pp = KdShaderManager::Instance().m_postProcessShader;
		pp.SetSmokeOutlineEnabled(!pp.IsSmokeOutlineEnabled());
	}
	m_prevOutlineKey = outlineKey;

	// F4：ブースト(ニトロ)演出を発動。車体に一瞬だけ色が乗り、同時に線画が弾ける。
	// ※ニトロ機能が入るまでのデバッグ用トリガー
	const bool tintKey = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
	if (tintKey && !m_prevTintKey) { TriggerBoost(); }
	m_prevTintKey = tintKey;

	// F3：文字エフェクトのスタイルを切り替え(0=グラデ 1=虹スモ 2=炎 3=ドット 4=縞 5=色収差 6=3D 7=ワープ)
	const bool styleKey = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
	if (styleKey && !m_prevStyleKey)
	{
		KdShaderManager::Instance().m_postProcessShader.CycleFluidStyle();
	}
	m_prevStyleKey = styleKey;

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

	//===== 後退ギア(R)の断続 =====
	// ほぼ停止中にS(throttle<0)を踏んだら後退へ入る。W(throttle>0)か
	// 前進し始めたら解除。前進中のSは通常どおりブレーキ(後退には入らない)。
	if (m_reverse)
	{
		if (throttle > 0.0f || vLong0 > CarConst::ReverseEngageSpeed) { m_reverse = false; }
	}
	else
	{
		if (throttle < 0.0f && vLong0 < CarConst::ReverseEngageSpeed) { m_reverse = true; }
	}
	// 駆動方向へアクセルを踏んでいるか(前進=W / 後退=S)。空転リラックスの判定に使う。
	const bool accelPressed = m_reverse ? (throttle < 0.0f) : (throttle > 0.0f);

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

	// 前後ロール剛性配分(スプリング+スタビ)。横方向の荷重移動を前後どちらへ多く振るか。
	//   フロント固い→フロント荷重移動大→フロントが逃げてアンダー。リア固い→オーバー(お尻が出る)。
	//   既定は前後同値+スタビ0 → 0.5(=従来の50:50、挙動そのまま)。
	const float rollStiffF     = std::max(m_springF + m_arbF, 1e-3f);
	const float rollStiffR     = std::max(m_springR + m_arbR, 1e-3f);
	const float frontRollShare = rollStiffF / (rollStiffF + rollStiffR);

	for (int s = 0; s < sub; ++s)
	{
		const Math::Vector3 forward(sinf(m_yaw), 0.0f, cosf(m_yaw));
		const Math::Vector3 right(cosf(m_yaw), 0.0f, -sinf(m_yaw));
		const float vLong = m_vel.Dot(forward);
		const float vLat  = m_vel.Dot(right);
		const float r     = m_yawRate;

		// 滞空中はタイヤ接地なし＝タイヤ力を一切かけない。水平速度もヨーもそのまま保持して
		// 飛ぶ＝横向き・スピンをキープしたまま宙を舞う「ジャンプドリフト」になる。
		// エアコントロール：ステアで機首のヨーだけわずかに調整して着地姿勢を作れる。
		if (m_airborne)
		{
			m_yawRate += steerInput * CarConst::AirSteerControl * h;
			m_yawRate -= m_yawRate * std::min(CarConst::AirYawDamp * h, 1.0f);
			m_yaw     += m_yawRate * h;
			continue;
		}

		// 荷重移動(前後G→前後、横G→左右)。基準は各輪0.25。
		// サスの反応時間ぶん"なめらかに"移す＝アクセルオフでリアが一気に抜けない(急スリップ防止)。
		const float dLongRaw = std::clamp(aLongPrev * m_cgHeight / (2.0f * halfBase  * g), -0.35f, 0.35f);
		const float dLatRaw  = std::clamp(aLatPrev  * m_cgHeight / (2.0f * halfTrack * g), -0.35f, 0.35f);
		m_dLongF += (dLongRaw - m_dLongF) * std::min(10.0f * h, 1.0f);
		m_dLatF  += (dLatRaw  - m_dLatF ) * std::min(10.0f * h, 1.0f);
		const float dLong = m_dLongF;
		const float dLat  = m_dLatF;

		// 駆動(後輪合計)とブレーキ(全輪)
		float engineTotal, brakeEach;
		if (m_reverse)
		{
			// 後退ギア：Sで後ろへ駆動(後退速度に上限)。ブレーキは掛けない。
			// throttle<0 なので engineTotal は負＝m_driveSpeedが負へ動き車が後退する。
			engineTotal = (vLong > -CarConst::MaxReverseSpeed) ? (CarConst::ReversePower * throttle) : 0.0f;
			brakeEach   = 0.0f;
		}
		else
		{
			engineTotal = (throttle > 0.0f) ? (m_driveAccel * throttle) : 0.0f;
			brakeEach   = (throttle < 0.0f) ? (m_brakePower * throttle * 0.25f) : 0.0f;
		}

		float sumLong = 0.0f, sumLat = 0.0f, sumMz = 0.0f;
		float rearReaction = 0.0f;   // 後輪の縦力合計(駆動輪回転の反力)

		for (int i = 0; i < 4; ++i)
		{
			const WheelDef& w = wheelDef[i];
			// 接地点速度(車体座標)：本体速度＋ヨー回転成分
			const float vlx = vLong - r * w.px;   // 縦
			const float vly = vLat  + r * w.pz;   // 横

			// 各輪の実舵角＝(前輪: アッカーマン適用の操舵 + 前トー) / (後輪: 後トーのみ)。
			//   トーは正=トーイン(左右が内側を向く)。アッカーマンはイン側を多く/アウト側を少なく切る。
			const bool  left    = (w.px < 0.0f);
			const float toeSign = left ? +1.0f : -1.0f;   // トーイン方向(左輪は+へ、右輪は-へ)
			float wsteer;
			if (w.front)
			{
				const bool inner = (m_steer * w.px > 0.0f);   // 旋回内側の前輪
				wsteer = m_steer * (inner ? (1.0f + m_ackermann) : (1.0f - m_ackermann))
				       + m_toeFront * toeSign;
			}
			else
			{
				wsteer = m_toeRear * toeSign;
			}
			const float wcs = cosf(wsteer);
			const float wsn = sinf(wsteer);

			// 接地点速度をホイール座標へ回転(全輪。後輪もトーの分だけ回る)
			const float wLong =  vlx * wcs + vly * wsn;
			const float wLat  = -vlx * wsn + vly * wcs;

			// 荷重：加速で後・ブレーキで前、旋回で外側(左右)へ移動。
			//   横方向はロール剛性配分(frontRollShare)で前後の移動量を変える＝アンダー/オーバー調整。
			const float latCoef = w.front ? frontRollShare : (1.0f - frontRollShare);
			float load = 0.25f
			           + (w.front ? -dLong : dLong) * 0.5f
			           + (left    ?  dLat  : -dLat) * latCoef;
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
			// キャンバー：ネガキャン(|camber|)に応じて横グリップを増す(荷重が乗った外輪ほど効く)
			Fy *= (1.0f + m_camberGrip * fabsf(m_camber));

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

			// ホイール座標→車体座標(各輪の実舵角ぶん回す。後輪もトーの分だけ回る)
			const float FcarLong = Fx * wcs - Fy * wsn;
			const float FcarLat  = Fx * wsn + Fy * wcs;

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
		if (handbrake)                            { m_driveSpeed = 0.0f; }                                            // ロック
		// 駆動方向へ踏んでいない(惰行) or クラッチ切断中はフリーホイール＝駆動輪が路面速へ緩和する。
		// これを入れないと、切ったはずの高い駆動輪回転が凍結して残り、m_driveSpeed>接地速で
		// 幽霊の駆動力(m_longStiff*(m_driveSpeed-wLong))が出続けて「クラッチ踏みながら加速」する。
		// ※後退中はaccelPressed=(S踏み)なので、後退駆動を打ち消さない。
		else if (!accelPressed || clutchPressed)  { m_driveSpeed += (vLong - m_driveSpeed) * std::min(m_driveRelax * h, 1.0f); } // 路面速へ

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
	// ※後退中(m_reverse)や実際に後退している時(vLong0<0)は無効。さもないと車体を
	//   「進行方向=真後ろ」へ向けようと180度回頭し続けて、その場でグルグル回る＆
	//   真っ直ぐバックできなくなる。前進時だけ効かせる。
	if (!handbrake && !m_reverse && !m_airborne && vLong0 > 0.0f && m_bodyAlign > 0.0f && alignEngage > 0.01f)
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

	//===== 位置更新＋壁判定：サブステップCCD＋リラクゼーション(車ゲー的に堅牢) =====
	// 1フレームの水平移動を球半径以下に小刻み分割して進め、毎ステップ壁へ押し戻す。
	// ・すり抜け防止：ステップ毎に判定するので高速でも薄い壁を飛び越えない。
	// ・角/複数壁のめり込み：1ステップ内で数回リラクゼーションして収束させる。
	// ・擦り滑り：現在速度で刻むので、壁で殺した速度が次ステップに反映されて壁沿いに滑る。
	// 上下(Y)は下方の接地レイで別途決める(この後)。
	{
		// 車体近似プローブ(車体ローカル：px=右, pz=前)。四隅＋前後端の6点。
		// 傾き(ピッチ/ロール/ヨー)込みで配置＝坂で誤って床に当たらない。DrawLitのcarWorldと同順。
		const Math::Matrix tiltRot =
			Math::Matrix::CreateRotationX(-m_terrainPitch) *
			Math::Matrix::CreateRotationZ(-m_terrainRoll) *
			Math::Matrix::CreateRotationY(m_yaw);

		struct Probe { float px; float pz; };
		const float tip = m_base * CarConst::WallProbeTip;
		const Probe probes[6] =
		{
			{ -m_track,  m_base }, {  m_track,  m_base },  // 前左・前右
			{ -m_track, -m_base }, {  m_track, -m_base },  // 後左・後右
			{  0.0f,     tip    }, {  0.0f,    -tip    },  // 前端・後端(中央)
		};

		// 全プローブで壁を1回押し戻す。押し戻しが起きたら true(反復継続の判断用)。
		auto resolvePass = [&]() -> bool
		{
			bool hitAny = false;
			for (const Probe& p : probes)
			{
				const Math::Vector3 localOff(p.px, CarConst::WallSphereHeight, p.pz);
				const Math::Vector3 center = m_pos + Math::Vector3::TransformNormal(localOff, tiltRot);

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
						// 垂直な壁(法線が水平)だけ採用。床(上向き法線)は接地側で処理。
						if (fabsf(r.m_hitNDir.y) > CarConst::WallNormalMaxY) { continue; }
						Math::Vector3 n = r.m_hitDir; n.y = 0.0f;   // 水平の押し戻し方向
						if (n.LengthSquared() < 1e-6f) { continue; }
						n.Normalize();

						// めり込みぶん押し出す(1回の暴発を防ぐため上限クランプ)
						const float push = std::min(r.m_overlapDistance, CarConst::WallMaxPush);
						m_pos += n * push;

						// 壁へ食い込む速度成分だけ除去＝接線方向へ滑る
						const float into = m_vel.Dot(n);
						if (into < 0.0f) { m_vel -= n * into * (1.0f + CarConst::WallSlideBounce); }
						hitAny = true;
					}
				}
			}
			return hitAny;
		};

		// 移動量を球半径の一定割合以下に分割(すり抜け防止のCCD相当)
		const Math::Vector3 disp0 = m_vel * dt;
		const float dist = sqrtf(disp0.x * disp0.x + disp0.z * disp0.z);
		const float maxStep = CarConst::WallProbeRadius * CarConst::WallSubStepFactor;
		const int steps = std::clamp(
			static_cast<int>(ceilf(dist / std::max(maxStep, 0.001f))), 1, CarConst::WallMaxSubSteps);
		const float subDt = dt / static_cast<float>(steps);

		for (int s = 0; s < steps; ++s)
		{
			m_pos += m_vel * subDt;   // 現在速度で刻む(壁で殺した速度が次ステップに効く)
			for (int it = 0; it < CarConst::WallRelaxIters; ++it)
			{
				if (!resolvePass()) { break; }
			}
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

		//----- 垂直ダイナミクス：地面に吸着(ジャンプ/打ち上げ/壁での浮き 無し・既存の車ゲー的) -----
		// 真下に地形がある間は常に接地。ランプ・クレスト・壁ズレでも打ち上がらない。
		// 坂のピッチ/ロール(車体が地形に沿って傾く)は残す。地面が無い時だけ重力で落下(崖)。
		const float gY = CarConst::Gravity * m_airGravityMul;
		m_velY -= gY * dt;
		const float yBallistic = m_pos.y + m_velY * dt;

		if (hitCount > 0)
		{
			// 接地面の高さ(接地タイヤ平均)＋ライドハイト
			float sum = 0.0f;
			for (int i = 0; i < 4; ++i) { if (contactHit[i]) { sum += contactY[i]; } }
			const float avgY    = sum / static_cast<float>(hitCount);
			const float groundY = avgY + CarConst::RideHeight;

			// 支持面高さを低域通過(メッシュ継ぎ目のガタつき除去)
			if (!m_prevGroundValid) { m_groundYFilt = groundY; }
			m_groundYFilt += (groundY - m_groundYFilt) * k;
			m_prevGroundValid = true;

			// 坂のピッチ(登降)・ロール(バンク)へ追従＝車体が地形に沿って傾く(これは残す)
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
			m_terrainPitch += (terrainPitchTarget - m_terrainPitch) * k;
			m_terrainRoll  += (terrainRollTarget  - m_terrainRoll ) * k;

			// 常に地面へ吸着＝ジャンプ/打ち上げ/壁での浮き 無し。垂直速度・角速度はゼロ。
			m_pos.y     = m_groundYFilt;
			m_velY      = 0.0f;
			m_pitchRate = 0.0f;
			m_rollRate  = 0.0f;
			m_onGround  = true;
			m_airborne  = false;
		}
		else
		{
			// 真下に地形なし＝崖から落下。重力で落ちつつ、車体は水平へ戻す(派手な回転はしない)。
			m_pos.y = yBallistic;
			m_prevGroundValid = false;
			const float lv = std::min(CarConst::AirLevelSmooth * dt, 1.0f);
			m_terrainPitch += (0.0f - m_terrainPitch) * lv;
			m_terrainRoll  += (0.0f - m_terrainRoll ) * lv;
			m_pitchRate = 0.0f;
			m_rollRate  = 0.0f;
			m_onGround  = false;
			m_airborne  = true;
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

		// タイヤ痕：煙と同じ後輪接地点に毎フレーム点を渡す。
		// (実際に点が増えるのは前の点から一定距離離れた時だけなので、低速でも密集しない)
		// 滞空中・ほぼ停止中は痕を切り、次に接地した時は新しい痕として始める。
		{
			const Math::Vector3 fwdS(sinf(m_yaw), 0.0f, cosf(m_yaw));
			const bool canMark = (carSpeed > SmokeConst::MinSpeed) && m_onGround;

			// 前輪の摩擦：舵を切った向きに対してどれだけ横へ滑っているか(スクラブ)。
			// 前輪の位置ではヨー回転ぶんの横速度が加わるので、それを足してから
			// 操舵角ぶん回してタイヤ座標系に直す。前輪は駆動しないので空転は考えない。
			const float vLatFront  = vLatEnd + m_yawRate * m_base;
			const float frontScrub = fabsf(vLatFront * cosf(m_steer) - vLongEnd * sinf(m_steer));
			const float frontSlip01 = std::clamp(
				(frontScrub - SkidMarkConst::FrontSlipThreshold) /
				std::max(SkidMarkConst::FrontSlipFull - SkidMarkConst::FrontSlipThreshold, 1e-4f),
				0.0f, 1.0f) * SkidMarkConst::FrontAlphaMul;

			// 前輪の幅方向は操舵で向きが変わる＝車の右方向を舵角ぶん回したもの
			const Math::Vector3 rightF(cosf(m_yaw + m_steer), 0.0f, -sinf(m_yaw + m_steer));

			for (int side = -1; side <= 1; side += 2)
			{
				const int rearIdx  = (side < 0) ? 0 : 1;
				const int frontIdx = (side < 0) ? 2 : 3;
				if (!canMark) { m_skid.Cut(rearIdx); m_skid.Cut(frontIdx); continue; }

				const Math::Vector3 lat = rightV * (static_cast<float>(side) * m_track);

				// 後輪：幅方向はタイヤの回転軸＝車の右方向
				Math::Vector3 wpR = m_pos + lat - fwdS * m_base;
				wpR.y = m_pos.y + SmokeConst::WheelGroundY;
				m_skid.Emit(rearIdx, wpR, rightV, slip01);

				// 前輪
				Math::Vector3 wpF = m_pos + lat + fwdS * m_base;
				wpF.y = m_pos.y + SmokeConst::WheelGroundY;
				m_skid.Emit(frontIdx, wpF, rightF, frontSlip01);
			}

			// 前輪が擦れているときは、痕と同じ判定でごく少量だけ煙も出す。
			// 後輪と同じ計算式に前輪ぶんの倍率を掛けるだけなので、
			// 速度が上がっても路面に対する煙の密度は後輪と揃う。
			if (canMark && frontSlip01 > 0.0f)
			{
				const float rateF = (SmokeConst::MeshSpawnPerSec
				                   + SmokeConst::MeshSpawnPerMeter * carSpeed)
				                  * SmokeConst::FrontSpawnMul;
				m_smokeCarryFront += rateF * frontSlip01 * dt;
				const int nf = static_cast<int>(m_smokeCarryFront);
				m_smokeCarryFront -= static_cast<float>(nf);

				if (nf > 0)
				{
					const Math::Vector3 trailF = -m_vel * SmokeConst::TrailFactor;
					for (int side = -1; side <= 1; side += 2)
					{
						Math::Vector3 wp = m_pos
						                 + rightV * (static_cast<float>(side) * m_track)
						                 + fwdS   * m_base;
						wp.y = m_pos.y + SmokeConst::WheelGroundY;
						const Math::Vector3 outward = rightV * static_cast<float>(side);
						// 大きさを大幅に落として、砂埃のような小さな粒にする
						m_smoke.Emit(wp, trailF, outward, nf, SmokeConst::FrontSizeMul);
					}
				}
			}
			else
			{
				m_smokeCarryFront = 0.0f;
			}
		}

		// 車速ガード：ほぼ停止しているときは一切煙を出さない。滞空中もタイヤ非接地なので出さない。
		if (slip01 > 0.0f && carSpeed > SmokeConst::MinSpeed && m_onGround)
		{
			// 端数を蓄積して整数枚に(1輪あたりの枚数)
			// 毎秒ぶん＋進んだ距離ぶん。距離ぶんを足すことで、速度が上がっても
			// 路面に並ぶ粒の間隔が広がらず、煙の帯が途切れなくなる。
			const float rate = SmokeConst::MeshSpawnPerSec
			                 + SmokeConst::MeshSpawnPerMeter * carSpeed;
			m_smokeCarry += rate * slip01 * dt;
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
					// 地形追従する車のY(m_pos.y)基準に接地点のわずかな浮きを足す。
					// (固定値で上書きすると坂を登っても煙が原点高さに取り残される)
					wp.y = m_pos.y + SmokeConst::WheelGroundY;
					// 外向き＝その後輪から見て車体の外側(左輪なら左、右輪なら右)
					const Math::Vector3 outward = rightV * static_cast<float>(side);
					m_smoke.Emit(wp, trail, outward, n);
				}
			}

			// ネオン線画(リング＋スパーク)も同じスリップ量で後輪から放出
			m_neonRingCarry  += NeonFxConst::RingPerSec  * slip01 * dt;
			m_neonSparkCarry += NeonFxConst::SparkPerSec * slip01 * dt;
			const int nRing  = static_cast<int>(m_neonRingCarry);
			const int nSpark = static_cast<int>(m_neonSparkCarry);
			m_neonRingCarry  -= static_cast<float>(nRing);
			m_neonSparkCarry -= static_cast<float>(nSpark);

			if (nRing > 0 || nSpark > 0)
			{
				const Math::Vector3 fwdN(sinf(m_yaw), 0.0f, cosf(m_yaw));
				for (int side = -1; side <= 1; side += 2)
				{
					Math::Vector3 wp = m_pos
					                 + rightV * (static_cast<float>(side) * m_track)
					                 - fwdN   * m_base;
					wp.y = m_pos.y + SmokeConst::WheelGroundY;
					// axis=タイヤの回転軸(車の右方向)＝リングがホイール面に沿う
					m_neon.Emit(wp, rightV, m_vel, nRing, nSpark);
				}
			}
		}
	}

	// ブースト演出：フェードさせず、パッと色が乗ってパッと戻す(本家の切り替わり方)
	if (m_boostFlash >= 0.0f)
	{
		m_boostFlash += dt;
		if (m_boostFlash < NeonFxConst::BoostFlashHold)
		{
			m_driftTint = 1.0f;
		}
		else
		{
			m_driftTint  = 0.0f;
			m_boostFlash = -1.0f;   // 終了
		}
	}
	else
	{
		m_driftTint = 0.0f;
	}

	// スモーク粒の更新(寿命・移動)
	m_smoke.Update(dt);
	m_neon.Update(dt);
	m_skid.Update(dt);
}

//----------------------------------------------------------
// ネオン線画の描画（煙の合成が済んだ後にシーンへ直接重ねる）
//   DrawEffectに置くと煙専用RTへ入ってシルエット輪郭が乗り、
//   細い線が輪郭に塗り潰されて中身が見えなくなる。
//----------------------------------------------------------
void CarBase::DrawOverlayEffect()
{
	m_neon.SetColors(m_neonColorA, m_neonColorB);
	m_neon.DrawEffect();
}

//----------------------------------------------------------
// 輝度パス：ネオンのグローを輝度RTへ描く。
// ポストプロセス(LightBloom)が4段階のガウスぼかしを掛けて画面へ加算するので、
// 光が周囲へ本当に滲み、レーザー光線のような見た目になる。
//----------------------------------------------------------
void CarBase::DrawBright()
{
	m_neon.SetColors(m_neonColorA, m_neonColorB);
	m_neon.DrawBrightPass();
}

//----------------------------------------------------------
// ブースト(ニトロ)演出を発動
//   車体に一瞬だけアクセントカラーが乗り、同時にネオンの線画が全方向へ弾ける。
//----------------------------------------------------------
void CarBase::TriggerBoost()
{
	m_boostFlash = 0.0f;   // フラッシュ開始

	// 線画は地面に沿って走らせるので、足元寄りの高さから湧かせる
	const Math::Vector3 burstPos = m_pos + Math::Vector3(0.0f, NeonFxConst::BoostSpawnY, 0.0f);
	m_neon.Burst(burstPos, m_vel);
}

//----------------------------------------------------------
// ドリフトスモーク描画（UnLitパス内でシーンから呼ばれる）
//----------------------------------------------------------
void CarBase::DrawEffect()
{
	m_smoke.SetTint(m_smokeColor);
	m_smoke.SetTintB(m_smokeColorB);
	m_smoke.SetGradDist(m_smokeGradDist);
	m_smoke.SetHighlight(m_smokeHiColor);
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

		// 壁プローブ球(四隅＋前後端の6個)＝壁の当たり判定に使っている球そのもの。
		// 物理側と同じく地形の傾き(ピッチ/ロール)込みで配置＝坂で車体に沿う。
		const Math::Matrix tiltRot =
			Math::Matrix::CreateRotationX(-m_terrainPitch) *
			Math::Matrix::CreateRotationZ(-m_terrainRoll) *
			Math::Matrix::CreateRotationY(m_yaw);
		const float tip = m_base * CarConst::WallProbeTip;
		const float ppx[6] = { -m_track, m_track, -m_track, m_track, 0.0f, 0.0f };
		const float ppz[6] = {  m_base,  m_base, -m_base, -m_base, tip, -tip };
		for (int i = 0; i < 6; ++i)
		{
			const Math::Vector3 localOff(ppx[i], CarConst::WallSphereHeight, ppz[i]);
			const Math::Vector3 c = m_pos + Math::Vector3::TransformNormal(localOff, tiltRot);
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
	// タイヤ痕は路面の一部なので、車体より先にLitパスで描いて路面の光を受けさせる。
	// 深度書き込みをしないので、この後のシーン輪郭抽出に拾われて線が引かれることもない。
	m_skid.SetColor(m_skidColor);
	m_skid.DrawEffect();

	KdShaderManager::Instance().ChangeRasterizerState(KdRasterizerState::CullNone);
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

	//===== 本体(通常ライティング)。両面描画＝裏面も出す(Blender同様) =====
	// CullNone(表裏カリング無効)を本体・4輪の描画の"間だけ"有効化し、直後に元へ戻す。
	// ※裏面は法線が逆向きなので、Litシェーダー側で SV_IsFrontFace により法線を反転して
	//   正しく陰影を付けている(そうしないと裏面が真っ黒になる)。

	// ドリフト中は車体をアクセントカラーで塗り潰す(陰影・輪郭は残る)。
	// DrawModelは描画後に定数バッファを既定へ戻すので、描画のたびに設定し直す。
	// 塗りには煙と同じ網点マスクを重ねて質感を揃える(周期・濃さも煙の値をそのまま使う)。
	const float tintAmt = m_driftTint * m_driftTintMax;
	shader.SetTint(tintAmt, m_driftTintColor,
	               SmokeConst::SmokePatScale, SmokeConst::SmokePatStrength);
	shader.DrawModel(m_body, bodyW);
	for (const auto& m : wheelMat)
	{
		shader.SetTint(tintAmt, m_driftTintColor,
		               SmokeConst::SmokePatScale, SmokeConst::SmokePatStrength);
		shader.DrawModel(m_wheel, m);
	}
	KdShaderManager::Instance().UndoRasterizerState();

	//===== アウトライン(原神式：背面押し出し。本体の"後"に描く) =====
	if (m_outlineEnabled)
	{
		// ブースト中は輪郭もアクセントカラーへ寄せて太らせる＝縁が発光して見える
		// (本家は車体の塗りだけでなく、シルエットの縁が強く光る)
		// 寄せる先は車体の塗りとは別指定。縁だけ違う色で光らせられる
		const Math::Vector3 ocv = Math::Vector3::Lerp(m_outlineColor, m_boostOutlineColor, tintAmt);
		const Math::Color oc(ocv.x, ocv.y, ocv.z, 1.0f);
		shader.BeginOutline();
		shader.SetOutlineWidth(m_outlineWidth * (1.0f + tintAmt * 1.6f));
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
	// ギア表示：後退中は "R"、前進はギア段数
	char gearBuf[8];
	if (m_reverse) { gearBuf[0] = 'R'; gearBuf[1] = '\0'; }
	else           { snprintf(gearBuf, sizeof(gearBuf), "%d", m_gear); }
	sp.DrawFont(Math::Vector2(static_cast<float>(L), static_cast<float>(y + bH / 2 + CarConst::HudTextDY)),
	            &colText, "RPM %4.0f  GEAR %s  %s", rpm, gearBuf, (m_clutch < 0.5f) ? "[CLUTCH]" : "");

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
	// 煙のグラデーション：発生源から離れるほど 色A → 色B へ滑らかに変わる
	ImGui::ColorEdit3(U8("煙の色A(手前)"), &m_smokeColor.x);
	ImGui::ColorEdit3(U8("煙の色B(奥)"),   &m_smokeColorB.x);
	ImGui::SliderFloat(U8("グラデ距離(m)"), &m_smokeGradDist, 0.5f, 30.0f);
	ImGui::ColorEdit3(U8("煙のハイライト色"), &m_smokeHiColor.x);

	// ブースト(ニトロ)発動時：一瞬だけ車体に色が乗り、線画が弾ける
	ImGui::SeparatorText(U8("ブースト演出(ニトロ)"));
	ImGui::ColorEdit3(U8("アクセントカラー"), &m_driftTintColor.x);
	ImGui::ColorEdit3(U8("発光中の輪郭色"), &m_boostOutlineColor.x);
	ImGui::SliderFloat(U8("塗り具合(最大)"), &m_driftTintMax, 0.0f, 1.0f);
	// パーティクル(線・丸)の色。粒ごとにAとBを混色して散らす
	ImGui::ColorEdit3(U8("パーティクル色A"), &m_neonColorA.x);
	ImGui::ColorEdit3(U8("パーティクル色B"), &m_neonColorB.x);
	if (ImGui::Button(U8("発動テスト(F4)"))) { TriggerBoost(); }
	ImGui::SameLine();
	ImGui::Text(U8("現在の塗り %.2f"), m_driftTint * m_driftTintMax);

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

	// 画面全体のハーフトーン(印刷風の網点)
	ImGui::SeparatorText(U8("ハーフトーン(印刷風)"));
	{
		auto& pp = KdShaderManager::Instance().m_postProcessShader;
		bool halftone = pp.IsHalftoneEnabled();
		if (ImGui::Checkbox(U8("有効##halftone"), &halftone)) { pp.SetHalftoneEnabled(halftone); }
		ImGui::DragFloat(U8("網点の周期(px)"), &pp.WorkHalftoneScale(), 0.1f, 2.0f, 40.0f);
		ImGui::DragFloat(U8("濃さ##halftone"), &pp.WorkHalftoneStrength(), 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat(U8("暗部に寄せる量"), &pp.WorkHalftoneDarkBias(), 0.02f, 0.0f, 1.0f);
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

	ImGui::SeparatorText(U8("アライメント / セッティング(挙動に効く)"));
	ImGui::DragFloat(U8("前トー(rad, +イン/-アウト)"), &m_toeFront, 0.002f, -0.2f, 0.2f);
	ImGui::DragFloat(U8("後トー(rad, +イン/-アウト)"), &m_toeRear, 0.002f, -0.2f, 0.2f);
	ImGui::DragFloat(U8("アッカーマン(0=平行/-=アンチ)"), &m_ackermann, 0.01f, -1.0f, 1.0f);
	ImGui::DragFloat(U8("キャンバー横グリップ寄与(0=見た目のみ)"), &m_camberGrip, 0.02f, 0.0f, 3.0f);
	ImGui::DragFloat(U8("前ばね定数(相対)"), &m_springF, 0.05f, 0.1f, 5.0f);
	ImGui::DragFloat(U8("後ばね定数(相対)"), &m_springR, 0.05f, 0.1f, 5.0f);
	ImGui::DragFloat(U8("前スタビ(相対)"), &m_arbF, 0.05f, 0.0f, 5.0f);
	ImGui::DragFloat(U8("後スタビ(相対)"), &m_arbR, 0.05f, 0.0f, 5.0f);
	ImGui::Text(U8("ロール剛性 前:後 = %.0f%% : %.0f%%  (前↑=アンダー/後↑=オーバー)"),
	            (m_springF + m_arbF) / std::max((m_springF + m_arbF) + (m_springR + m_arbR), 1e-3f) * 100.0f,
	            (m_springR + m_arbR) / std::max((m_springF + m_arbF) + (m_springR + m_arbR), 1e-3f) * 100.0f);

	ImGui::SeparatorText(U8("ジャンプ/滞空(エビス風ジャンプドリフト)"));
	ImGui::DragFloat(U8("重力倍率(大=ズシッ/小=フワッ)"), &m_airGravityMul, 0.02f, 0.2f, 4.0f);
	ImGui::DragFloat(U8("打ち上げ強さ倍率"), &m_jumpLaunch, 0.05f, 0.0f, 4.0f);
	ImGui::DragFloat(U8("着地の跳ね返り(0=吸収)"), &m_landBounce, 0.01f, 0.0f, 0.8f);
	ImGui::Text(U8("状態: %s  垂直速度 %.1f m/s"), m_airborne ? U8("滞空") : U8("接地"), m_velY);

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
		// アライメント/セッティング(トー・アッカーマン・キャンバー寄与・前後ロール剛性)
		{ "toeFront", &m_toeFront }, { "toeRear", &m_toeRear },
		{ "ackermann", &m_ackermann }, { "camberGrip", &m_camberGrip },
		{ "springF", &m_springF }, { "springR", &m_springR },
		{ "arbF", &m_arbF }, { "arbR", &m_arbR },
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
		// 色B(奥側)。旧キー smokeColB は色Aの青成分なので、区別できる名前にする。
		{ "smokeCol2R", &m_smokeColorB.x }, { "smokeCol2G", &m_smokeColorB.y }, { "smokeCol2B", &m_smokeColorB.z },
		{ "smokeGradDist", &m_smokeGradDist },
		{ "smokeHiR", &m_smokeHiColor.x }, { "smokeHiG", &m_smokeHiColor.y }, { "smokeHiB", &m_smokeHiColor.z },
		{ "tintR", &m_driftTintColor.x }, { "tintG", &m_driftTintColor.y }, { "tintB", &m_driftTintColor.z },
		{ "tintMax", &m_driftTintMax }, { "tintSlipDeg", &m_driftTintSlipDeg },
		// 発光中の輪郭色(車体の塗りとは独立)
		{ "boostOutR", &m_boostOutlineColor.x }, { "boostOutG", &m_boostOutlineColor.y },
		{ "boostOutB", &m_boostOutlineColor.z },
		{ "neonAR", &m_neonColorA.x }, { "neonAG", &m_neonColorA.y }, { "neonAB", &m_neonColorA.z },
		{ "neonBR", &m_neonColorB.x }, { "neonBG", &m_neonColorB.y }, { "neonBB", &m_neonColorB.z },
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
