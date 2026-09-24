#include "CarBase.h"
#include "../../Const/RigidCarConst.h"
#include "../../Util/HjSaveFile.h"
#include "../../Util/HjProfiler.h"
#include "../../Util/HjPostFxSettings.h"
#include "../../Audio/HjAudioSpace.h"
#include "../../Mod/HjModCatalog.h"
#include "../Stage/HjHeightField.h"
#include "../Stage/HjRoad.h"

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

	// 差し替えが選ばれていれば、その見た目にする。
	// ※ここは標準を読んだ後。失敗しても標準のまま残るので、
	//   MODのファイルが消えていてもゲームは動く。
	//
	// 通信で来た他人の車は読まない。
	// 保存ファイルは車種ごとなので、読むと相手の車まで
	// 自分の差し替えになってしまう
	if (m_useModChoice) { LoadModChoice(); }

	// ドリフトスモーク初期化
	m_smoke.Init();
	m_neon.Init();
	m_engineAudio.Init();
	m_tireAudio.Init();
	// タイヤ痕のマップはコースに1枚の共有。自車ぶんの枠だけ確保する
	SkidMark::Instance().Init();
	m_skidBase = SkidMark::Instance().AllocTrails();

	// 当たり判定の可視化用ワイヤフレーム(F1でトグル)
	m_pDebugWire = std::make_unique<KdDebugWireFrame>();

	//===== 剛体で走らせる =====
	// 既定はこちら。
	//
	// 旧モデルは3自由度の平面で、姿勢は4輪のレイから推定していた。
	// 片輪が浮く・転倒する・縁石で跳ねる、が原理的に出せない。
	//
	// 調整パネルで切り替えれば旧モデルにも戻せる
	SetRigid(true);

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
	// タイヤの横力とロールの状態も戻す(前の走りの力が残ったまま復帰しないように)
	m_rollV  = 0.0f;
	m_dLatF  = 0.0f;
	m_dLongF = 0.0f;
	for (float& fy : m_wheelFy)   { fy = 0.0f; }
	for (float& bl : m_bumpLoad)  { bl = 0.0f; }
	m_driveDiff = 0.0f;
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

	//===== 剛体も置き直す =====
	// m_pos へ代入するだけでは剛体は前の場所に残る。
	//
	// 起動直後は原点に置いたままになり、そこの地形へ
	// 埋まった状態で走り出す。サスが縮みきった反力で
	// 打ち上げられて、車が上へ飛んでいく
	if (m_useRigid) { m_rigid.Place(pos, yaw); }
}

//----------------------------------------------------------
// 運転操作の読み取り。キーボード(デジタル)とコントローラー(アナログ)を合成する。
//   W/S=アクセル・ブレーキ / A,D=ステア / Space=サイド
//   左Shift=クラッチ / E=シフトアップ / Q=シフトダウン
//   パッド：左スティックX=ステア / RT,LT=アクセル,ブレーキ
//           A=サイド / LB=クラッチ / RB,X=シフト
//----------------------------------------------------------
CarBase::DriveInput CarBase::ReadInput()
{
	DriveInput in;

	if (GetAsyncKeyState('W') & 0x8000) { in.throttle += 1.0f; }
	if (GetAsyncKeyState('S') & 0x8000) { in.throttle -= 1.0f; }
	if (GetAsyncKeyState('A') & 0x8000) { in.steer    -= 1.0f; }
	if (GetAsyncKeyState('D') & 0x8000) { in.steer    += 1.0f; }
	in.handbrake = (GetAsyncKeyState(VK_SPACE)  & 0x8000) != 0;
	in.clutch    = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0;

	// シフトは押した瞬間だけ反応させる(押しっぱなしで連続シフトしない)
	const bool keyShiftUp   = (GetAsyncKeyState('E') & 0x8000) != 0;
	const bool keyShiftDown = (GetAsyncKeyState('Q') & 0x8000) != 0;
	in.shiftUp   = keyShiftUp   && !m_prevKeyShiftUp;
	in.shiftDown = keyShiftDown && !m_prevKeyShiftDown;
	m_prevKeyShiftUp   = keyShiftUp;
	m_prevKeyShiftDown = keyShiftDown;

	// コントローラー(接続時のみアナログ操作を加算)
	m_pad.Update();
	if (m_pad.IsConnected())
	{
		const float padThrottle = m_pad.RightTrigger() - m_pad.LeftTrigger();
		if (fabsf(padThrottle) > 0.0f) { in.throttle += padThrottle; }
		in.steer += m_pad.LeftStickX();

		if (m_pad.IsButtonDown(PadConst::HandbrakeButtons))   { in.handbrake = true; }
		if (m_pad.IsButtonDown(PadConst::ClutchButton))       { in.clutch    = true; }
		if (m_pad.IsButtonPressed(PadConst::ShiftUpButton))   { in.shiftUp   = true; }
		if (m_pad.IsButtonPressed(PadConst::ShiftDownButton)) { in.shiftDown = true; }

		in.throttle = std::clamp(in.throttle, -1.0f, 1.0f);
		in.steer    = std::clamp(in.steer,    -1.0f, 1.0f);
	}

	return in;
}

//----------------------------------------------------------
// デバッグ用のトグルキー。運転操作とは無関係なのでここに隔離する。
//----------------------------------------------------------
void CarBase::UpdateDebugKeys()
{
	// 押した瞬間だけ反応させるヘルパ
	auto pressed = [](int vk, bool& prev)
	{
		const bool now = (GetAsyncKeyState(vk) & 0x8000) != 0;
		const bool hit = now && !prev;
		prev = now;
		return hit;
	};

	// F1：当たり判定の可視化
	if (pressed(VK_F1, m_prevDebugKey)) { m_debugDraw = !m_debugDraw; }

	// F2：煙シルエット輪郭のON/OFF(消え方の切り分け用。OFFで従来の直描き)
	if (pressed(VK_F2, m_prevOutlineKey))
	{
		auto& pp = KdShaderManager::Instance().m_postProcessShader;
		pp.SetSmokeOutlineEnabled(!pp.IsSmokeOutlineEnabled());
	}

	// F3：文字エフェクトのスタイル切り替え
	if (pressed(VK_F3, m_prevStyleKey))
	{
		KdShaderManager::Instance().m_postProcessShader.CycleFluidStyle();
	}

	// F4：ブースト(ニトロ)演出を発動 ※ニトロ機能が入るまでのデバッグ用トリガー
	if (pressed(VK_F4, m_prevTintKey)) { TriggerBoost(); }
}

//----------------------------------------------------------
// 車体アライン(アシスト)：アクセルを離したら車体の向きを進行方向へ寄せる。
// カウンターを当てて速度が入力方向へ向いた後、車体もそっちを向いて走り出す
// (これが無いと横を向いたまま進む＝カニ歩きになる)。
// アクセルON中は何もしない＝物理どおりドリフト維持(スロットルコントロール可)。
// アクセルを抜くほど強く効き、二値でなく踏み加減に連続。
//
// ※m_yaw を直接書き換える＝物理の積分結果を上書きするアシスト。
//   CarXにはこの処理は無いので、切ると挙動は純粋な物理寄りになる。
//----------------------------------------------------------
void CarBase::UpdateBodyAlignAssist(float dt, float vLong0, bool handbrake)
{
	if (!m_bodyAlignEnabled) { return; }

	const float alignEngage = 1.0f - m_liftCounterFactor;   // オフに近いほど大きい

	// 後退中(m_reverse)や実際に後退している時(vLong0<0)は無効。さもないと車体を
	// 「進行方向=真後ろ」へ向けようと180度回頭し続けて、その場でグルグル回る＆
	// 真っ直ぐバックできなくなる。前進時だけ効かせる。
	if (handbrake || m_reverse || m_airborne) { return; }
	if (vLong0 <= 0.0f || m_bodyAlign <= 0.0f || alignEngage <= 0.01f) { return; }

	if (m_vel.Length() <= 2.0f) { return; }

	const float velYaw = atan2f(m_vel.x, m_vel.z);   // 進行方向のワールドヨー
	float d = velYaw - m_yaw;
	while (d >  3.14159265f) { d -= 6.2831853f; }
	while (d < -3.14159265f) { d += 6.2831853f; }

	const float k = std::min(m_bodyAlign * alignEngage * dt, 1.0f);

	// 剛体は姿勢をクォータニオンで持つので、ヨーだけ回す。
	// m_yaw へ書いても、次のフレームで剛体の値に上書きされる
	if (m_useRigid)
	{
		m_rigid.RotateYaw(d * k);
		m_rigid.DampYaw(k);
		return;
	}

	m_yaw     += d * k;               // 車体を進行方向へ回頭
	m_yawRate -= m_yawRate * k;       // 余分な回転を抑えて収束
}

//----------------------------------------------------------
// 舵角を決める
//
// オートカウンター(CarX風)：横滑り方向へ前輪を自動で当て続ける。
// 車体の横滑り角(sideslip)＝進行方向と車体前方の角度。ドリフト中は前輪が
// 進行方向を向く＝カウンターになる。プレイヤー入力はこれに足し引きする。
//
// ■ 下回りとは切り離してある
// 平面モデルでも剐体でも、舵の作り方は同じでなければいけない。
// ここがドリフトの手ごたえそのものだから
//----------------------------------------------------------
void CarBase::UpdateSteerAngle(float dt, float steerInput, float throttle,
                              bool handbrake, float vLong0, float vLat0, float speedNow)
{
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
	// アクセルを抜いたときにカウンターを抜く挙動。既定は切ってある。
	// 舵に触っていないのに前輪の角度が変わると、手ごたえが読めなくなる。
	// アクセルオフでリアがグリップを取り戻すのは荷重移動で既に起きているので、
	// ここまでやると二重に効く。
	// (m_liftCounterFactor 自体は「アクセルオフで進行方向へ回頭」でも使うので残す)
	if (m_liftCounterEnabled) { autoCounter *= m_liftCounterFactor; }

	//===== ステア(CarX参考のドリフトアシスト) =====
	// プレイヤー入力は平滑化。オートカウンターは"即時"反映してスライドを素早く捕まえる。
	// これがCarXの「勝手に当て舵してドリフトを維持できる」手触りの核心。
	const float steerFade    = 1.0f / (1.0f + speedNow * 0.03f);   // 高速で舵角を絞る
	const float playerTarget = steerInput * m_maxSteerAngle * steerFade;

	// 舵を戻す/反対側へ振る時は速く動かす。切り込みと同じ速さで戻すと、
	// 逆ステの位置から反対のロックまで舵が旅する間にタイミングを逃す。
	float steerRate = m_steerSpeed;
	const bool returning = (playerTarget * m_playerSteer < 0.0f)          // 逆側へ振る
	                    || (fabsf(playerTarget) < fabsf(m_playerSteer));  // 中央へ戻す
	if (returning) { steerRate *= m_steerReturnMul; }

	// 舵は一定の速さで動いて、目標に着いたらそこで止まる。
	//
	// ここを1次遅れ(指数)で寄せると、最初だけ速くて後はじわじわ近づき、
	// いつまでも目標に届かない。ゴムで引っ張られるような手ごたえになり、
	// 今どこまで切れているのかが分からなくなる。
	// 実車のステアリングは腕が動かせる速さで動き、握った位置で止まる。
	{
		const float step = steerRate * m_maxSteerAngle * dt;   // このフレームで動ける量
		const float diff = playerTarget - m_playerSteer;
		m_playerSteer += std::clamp(diff, -step, step);
	}

	// 振り返しの意思があるならカウンターを緩める。
	// アシストは横滑り角を掴んで当て舵を当て続けるので、そのままだと
	// 「反対へ向けたい入力」と綱引きになり、切り返しが鈍る。
	if (autoCounter * steerInput < 0.0f)
	{
		const float release = m_counterRelease * std::min(fabsf(steerInput), 1.0f);
		autoCounter *= 1.0f - release;
	}

	// カウンターも動ける量に上限を付けて追わせる。
	//
	// 元になる横滑り角は速度から毎フレーム計算した生の値で、
	// 段差・タイヤの緩和・摩擦円の頭打ちで細かく震える。
	// そのまま舵へ入れると見た目のタイヤがカクカク動く。
	// プレイヤーの舵より速いレートにしてあるので、スライドの捕まえは鈍らない。
	{
		const float step = CarConst::CounterRate * m_maxSteerAngle * dt;
		const float diff = autoCounter - m_autoCounter;
		m_autoCounter += std::clamp(diff, -step, step);
	}

	m_steer = m_playerSteer + m_autoCounter;
	// 舵角の上限。過剰に切ると前輪の横力がcos(舵角)で消えて食わなくなるので、
	// maxSteerを控えめにした上で1.3倍までに収める(捕捉に十分＋前輪が効く範囲)。
	const float steerLimit = m_maxSteerAngle * 1.3f;
	m_steer = std::clamp(m_steer, -steerLimit, steerLimit);
}

//----------------------------------------------------------
// 振り返しの後押し
//
// ドリフト中(横滑りあり)に舵を切った方向へヨーを後押し＝
// 反対側へパッと振り替えやすい。
// 通常グリップ走行(横滑り小)では効かない。
//
// 硬い閾値で切ると、振り返しの途中(横滑りが0を通る瞬間)に
// 補助が消えてそこだけ動きが止まる。滑らかに立ち上げて谷を作らない
//----------------------------------------------------------
float CarBase::TransitionYawBoost(float steerInput, float vLong0, float vLat0) const
{
	if (!m_transitionEnabled || m_transitionAssist <= 0.0f) { return 0.0f; }
	if (vLong0 <= 0.5f) { return 0.0f; }

	const float ss = atan2f(vLat0, fabsf(vLong0) + 1.0f);   // 横滑り角

	const float gain = std::clamp(
		(fabsf(ss) - CarConst::TransitionSlipMin) / CarConst::TransitionSlipBand,
		0.0f, 1.0f);

	// 今の回転と逆へ振った瞬間だけ、追加でキレを足す
	const float flick = (steerInput * m_yawRate < 0.0f)
		                  ? CarConst::TransitionFlickMul : 1.0f;

	return steerInput * m_transitionAssist * gain * flick;
}

//----------------------------------------------------------
// スピン防止アシスト
//
// 横滑り角が大きい時、"スライドを深める向き(スピンアウト)"の
// ヨーだけを抑える。戻す向き(アクセルオフ＋逆ハンでのリカバリー)は邪魔しない
//----------------------------------------------------------
float CarBase::SpinAssistRate(float steerInput, bool handbrake,
                             float vLong, float vLat, float yawRate) const
{
	if (!m_spinAssistEnabled) { return 0.0f; }

	const float ss    = atan2f(vLat, fabsf(vLong) + 1.0f);   // 横滑り角(符号付)
	const float ssAbs = fabsf(ss);

	if (ssAbs <= CarConst::SpinAssistThreshold) { return 0.0f; }

	// ヨーと横滑りが同符号＝スピンアウト方向。逆符号＝リカバリー方向(抑えない)
	if (yawRate * vLat <= 0.0f) { return 0.0f; }

	// 回っている向きへ舵を当て続けている＝プレイヤーが狙って回している。
	// ドリフト中はカウンター(回転と逆)を当てているので、ここには入らない。
	// この区別が無いと、狙って回そうとしてもアシストに止められる
	const bool intentional = (steerInput * yawRate > 0.0f) &&
		                       (fabsf(steerInput) > CarConst::SpinIntentSteer);

	if (intentional) { return 0.0f; }

	// サイド中はアシストを弱めてリアを自由に回り込ませる(広がり感)
	const float assist = m_spinAssist
		                 * (handbrake ? CarConst::HandbrakeSpinAssistMul : 1.0f);

	return (ssAbs - CarConst::SpinAssistThreshold) * assist;
}

//----------------------------------------------------------
// 後退ギア(R)の断続
//----------------------------------------------------------
void CarBase::UpdateReverseGear(float dt, float throttle, bool handbrake, float vLong0)
{
	// ほぼ停止中にSを踏み続けたら後退へ入る。Wか前進し始めたら解除。
	// 前進中のSは通常どおりブレーキ。
	//
	// ■ 判定に「実際の速さ」を使う
	// 車体前方の速度(vLong0)で見ると、ドリフト中に誤って後退へ入る。
	// 横を向いている間は前方成分が小さくなるので、実際には高速で
	// 滑っていても停止扱いになってしまう。
	// 後退へ入るとブレーキが後退駆動に置き換わる(brakeEach=0)ため、
	// 「サイドを引いて思い切りブレーキしても止まらず前へ進む」という
	// 挙動になっていた。
	//
	// ■ サイド中は入らない
	// サイドブレーキは減速とドリフトの操作であって、後退の意思ではない。
	//
	// ■ 少し踏み続けさせる
	// 一瞬でも条件を満たしたら入る作りだと、停止寸前のブレーキ中に
	// ギアが勝手に切り替わって制動が抜ける。
	if (m_reverse)
	{
		if (throttle > 0.0f || vLong0 > CarConst::ReverseExitSpeed)
		{
			m_reverse = false;
			m_reverseHold = 0.0f;
		}
	}
	else
	{
		const bool wantReverse = (throttle < 0.0f)
		                      && !handbrake
		                      && (m_vel.Length() < CarConst::ReverseEngageSpeed);

		m_reverseHold = wantReverse ? (m_reverseHold + dt) : 0.0f;
		if (m_reverseHold >= CarConst::ReverseEngageHold) { m_reverse = true; }
	}
}

//----------------------------------------------------------
// サスペンションのロール/ピッチ。車体を傾ける見た目だけの処理で、
// タイヤの荷重やグリップには一切影響しない(荷重移動は物理側で別に解いている)。
//   横G  → ロール (コーナーで外傾)
//   前後G→ ピッチ (加速で後沈み / ブレーキで前ダイブ)
//----------------------------------------------------------
//----------------------------------------------------------
// 通信で受け取った状態を見た目へ流し込む。
// 物理は一切進めない(他人の車は本人のPCが答えを出している)。
//----------------------------------------------------------
void CarBase::ApplyVisualState(const Math::Vector3& pos, float yaw, const Math::Vector3& vel,
                               float steer, float spinFront, float spinRear)
{
	m_pos   = pos;
	m_yaw   = yaw;
	m_vel   = vel;          // カメラとHUDが進行方向を読むので入れておく
	m_steer = steer;

	m_wheelSpinFront = spinFront;
	m_wheelSpinRear  = spinRear;
}

//----------------------------------------------------------
// レブの効き具合
//
// トルクを絞る側と、音を潰す側の両方が同じ値を読む。
// 別々に書くと、片方だけ直して音と挙動がずれる
//----------------------------------------------------------
float CarBase::RevCut() const
{
	const float rpmN = m_engineRPM / CarConst::MaxRPM;

	if (rpmN <= CarConst::RevCutStart) { return 0.0f; }

	return std::clamp(
		(rpmN - CarConst::RevCutStart) /
		std::max(CarConst::RevCutEnd - CarConst::RevCutStart, 1e-4f), 0.0f, 1.0f);
}

//----------------------------------------------------------
// エンジン音
//
// 回転数とアクセル開度を渡すだけで、点火の間隔と音量が決まる。
// 後退中はSがアクセルなので、踏み込み量として符号を落とす
//----------------------------------------------------------
void CarBase::UpdateEngineAudio(float dt, float throttle)
{
	const float open = std::clamp(fabsf(throttle), 0.0f, 1.0f);

	m_engineAudio.Update(dt, m_engineRPM, open, CarConst::MaxRPM, RevCut());
}

//----------------------------------------------------------
// 音の3D
//
// 聴取点はカメラ、音源は車。
// エンジンは車の後ろ(マフラー)、タイヤは接地面から鳴らす。
// 位置を与えないと常に耳元で同じ大きさに聞こえ、距離と方向の
// 手がかりが無いまま＝実在しない音になる
//----------------------------------------------------------
void CarBase::PlaceAudio()
{
	HjAudioSpace::Instance().UpdateListener();

	const Math::Vector3 fwd(sinf(m_yaw), 0.0f, cosf(m_yaw));

	m_engineAudio.Apply3D(m_pos - fwd * m_base + Math::Vector3(0.0f, 0.3f, 0.0f));
	m_tireAudio.Apply3D(m_pos - fwd * m_base * 0.5f);
}

//----------------------------------------------------------
// タイヤ痕と煙。
//
// ※自分の車(CarBase::UpdateMotionFeedback)と同じ形の処理が並ぶが、
//   あちらは物理の途中の値を大量に使っており、切り出すと
//   挙動そのものに触ることになる。こちらは「滑り量」だけを
//   入力にした短い版で、同じ定数を使うので絵は揃う。
//----------------------------------------------------------
void CarBase::EmitTireFx(float dt, float speed, float slipRear01, float slipFront01,
                           bool onGround)
{
	auto& skid = SkidMark::Instance();

	const Math::Vector3 fwd(sinf(m_yaw), 0.0f, cosf(m_yaw));
	const Math::Vector3 right(cosf(m_yaw), 0.0f, -sinf(m_yaw));
	// 前輪は舵の向きぶん回っている。痕の幅方向がここで変わる
	const Math::Vector3 rightF(cosf(m_yaw + m_steer), 0.0f, -sinf(m_yaw + m_steer));

	// 止まっている・浮いているときは痕を切る。
	// 切らないと、次に接地したときに離れた点同士が線で繋がってしまう
	const bool canMark = (speed > SmokeConst::MinSpeed) && onGround;

	for (int side = -1; side <= 1; side += 2)
	{
		const int rearIdx  = m_skidBase + ((side < 0) ? 0 : 1);
		const int frontIdx = m_skidBase + ((side < 0) ? 2 : 3);

		if (!canMark) { skid.Cut(rearIdx); skid.Cut(frontIdx); continue; }

		const Math::Vector3 lat = right * (static_cast<float>(side) * m_track);

		Math::Vector3 wpR = m_pos + lat - fwd * m_base;
		wpR.y = m_pos.y + SmokeConst::WheelGroundY;
		skid.Emit(rearIdx, wpR, right, slipRear01);

		Math::Vector3 wpF = m_pos + lat + fwd * m_base;
		wpF.y = m_pos.y + SmokeConst::WheelGroundY;
		skid.Emit(frontIdx, wpF, rightF, slipFront01);
	}

	if (!canMark) { m_smokeCarry = 0.0f; m_smokeCarryFront = 0.0f; return; }

	// 煙。毎秒ぶん＋進んだ距離ぶんで数を決める。
	// 距離ぶんを足すのは、速度が上がっても粒の間隔が広がらないようにするため
	const float rate = SmokeConst::MeshSpawnPerSec
	                 + SmokeConst::MeshSpawnPerMeter * speed;

	// 後輪
	if (slipRear01 > 0.0f)
	{
		m_smokeCarry += rate * slipRear01 * dt;
		const int n = static_cast<int>(m_smokeCarry);
		m_smokeCarry -= static_cast<float>(n);

		if (n > 0)
		{
			const Math::Vector3 trail = -m_vel * SmokeConst::TrailFactor;
			for (int side = -1; side <= 1; side += 2)
			{
				Math::Vector3 wp = m_pos + right * (static_cast<float>(side) * m_track)
				                 - fwd * m_base;
				wp.y = m_pos.y + SmokeConst::WheelGroundY;
				m_smoke.Emit(wp, trail, right * static_cast<float>(side), n);
			}
		}
	}
	else { m_smokeCarry = 0.0f; }

	// 前輪。擦れているときだけ、砂埃くらいの小さな粒を少量
	if (slipFront01 > 0.0f)
	{
		m_smokeCarryFront += rate * SmokeConst::FrontSpawnMul * slipFront01 * dt;
		const int nf = static_cast<int>(m_smokeCarryFront);
		m_smokeCarryFront -= static_cast<float>(nf);

		if (nf > 0)
		{
			const Math::Vector3 trailF = -m_vel * SmokeConst::TrailFactor;
			for (int side = -1; side <= 1; side += 2)
			{
				Math::Vector3 wp = m_pos + right * (static_cast<float>(side) * m_track)
				                 + fwd * m_base;
				wp.y = m_pos.y + SmokeConst::WheelGroundY;
				m_smoke.Emit(wp, trailF, right * static_cast<float>(side), nf,
				             SmokeConst::FrontSizeMul);
			}
		}
	}
	else { m_smokeCarryFront = 0.0f; }
}

void CarBase::UpdateEffectParticles(float dt)
{
	m_smoke.Update(dt);
	m_neon.Update(dt);
}

//----------------------------------------------------------
// 通信で受け取った傾きを反映する。
// 位置や向きとは別にしてあるのは、こちらは「見た目だけ」で、
// 当たり判定にも進行方向にも関わらないため。
//----------------------------------------------------------
void CarBase::ApplyVisualTilt(float terrainPitch, float terrainRoll,
                              float bodyPitch, float bodyRoll)
{
	m_terrainPitch = terrainPitch;
	m_terrainRoll  = terrainRoll;
	m_pitchAngle   = bodyPitch;
	m_rollAngle    = bodyRoll;
}

void CarBase::ApplyVisualRotation(const Math::Quaternion& rot)
{
	m_netRotation    = rot;
	m_useNetRotation = true;
}

void CarBase::ApplyLookColors(const Math::Vector3& outline,
                              const Math::Vector3& smokeA, const Math::Vector3& smokeB,
                              const Math::Vector3& accent,
                              const Math::Vector3& neonA,  const Math::Vector3& neonB,
                              const Math::Vector3& smokeHi, float smokeGradDist)
{
	// 持ち主が調整パネルで設定した色をそのまま入れる。
	// 通信側で色を決めると、せっかく詰めた配色が上書きされる
	m_outlineColor   = outline;
	m_smokeColor     = smokeA;
	m_smokeColorB    = smokeB;
	m_driftTintColor = accent;
	m_neonColorA     = neonA;
	m_neonColorB     = neonB;
	m_smokeHiColor   = smokeHi;
	m_smokeGradDist  = smokeGradDist;
}

void CarBase::StopAudio()
{
	m_engineAudio.Stop();
	m_tireAudio.Stop();
}

void CarBase::UpdateSuspensionVisual(float dt)
{
	// 加速度入力を平滑化(空転やアクセルのガタつきで跳ねないように)
	const float aSmooth = std::min(m_accelSmooth * dt, 1.0f);
	m_accelLatF  += (m_accelLat  - m_accelLatF ) * aSmooth;
	m_accelLongF += (m_accelLong - m_accelLongF) * aSmooth;

	const float rollTarget  = std::clamp( m_accelLatF  * m_rollGain,  -m_suspMax, m_suspMax);
	const float pitchTarget = std::clamp(-m_accelLongF * m_pitchGain, -m_suspMax, m_suspMax);

	// ばね-ダンパで目標角へ寄せる
	{
		const float acc = m_suspStiff * (rollTarget - m_rollAngle) - m_suspDamp * m_rollVel;
		m_rollVel += acc * dt;  m_rollAngle += m_rollVel * dt;
	}
	{
		const float acc = m_suspStiff * (pitchTarget - m_pitchAngle) - m_suspDamp * m_pitchVel;
		m_pitchVel += acc * dt; m_pitchAngle += m_pitchVel * dt;
	}
}

//----------------------------------------------------------
// エンジン / ギア / クラッチ（マニュアルトランスミッション）。
// プレイヤーがギアとクラッチを手動操作する。サイドブレーキでもクラッチは切れる(CarX挙動)。
// クラッチが切れている間はエンジンが駆動系から切り離され、アクセルで空ぶかしできる。
// 結果は m_engineRPM / m_gear / m_clutch / m_driveSpeed / m_driveAccel に入る。
//----------------------------------------------------------
void CarBase::UpdateDriveline(float dt, float throttle, bool handbrake,
                              bool clutchPressed, bool shiftUp, bool shiftDown, float vLong0)
{
	const float radius = std::max(m_wheelH, 0.01f);

	// トルクカーブ(中回転ピーク)。空ぶかしの上昇率にも使うので先に求める。
	// 回転数は前フレームの値を使うが、1フレームぶんの遅れは体感に出ない。
	const float rpmN   = m_engineRPM / CarConst::MaxRPM;
	const float dd     = rpmN - CarConst::TorquePeakN;
	float torque = std::clamp(1.0f - CarConst::TorqueFall * dd * dd, CarConst::TorqueMin, 1.0f);

	// 手動シフト(押した瞬間のみ1段。クラッチの有無に関係なく入る＝簡易化)
	if ((shiftUp   && m_gear < CarConst::GearCount) ||
	    (shiftDown && m_gear > 1))
	{
		if (shiftUp) { m_gear++; } else { m_gear--; }

		// シンクロ機構がギア比の差を一瞬で吸収する体で、
		// エンジン回転数を新しいギア比の分だけ即座に付け替える。
		//
		// ここを更新しないと、例えば1速レッドゾーンから5速へ飛んだ瞬間、
		// 「5速換算では低回転のはずの場面に、1速の高回転が残る」という
		// 矛盾した状態になる。下の駆動処理はこれを「エンジンが車輪を
		// 空転させて引っ張っている」(サイドを離した直後と同じ状況)と
		// 誤認し、5速の分母が小さいぶん「エンジン回転に釣り合う速度」が
		// 異常に大きく計算されて、そこへ向かって駆動輪速度が急加速で
		// 引っ張られる。結果、爆発的な加速として現れる。
		if (!handbrake && !clutchPressed)
		{
			const float trNew = CarConst::GearRatios[m_gear] * CarConst::FinalDrive;
			m_engineRPM = std::clamp(
				fabsf(m_driveSpeed / radius) * trNew * CarConst::RpmPerRadSec,
				CarConst::IdleRPM, CarConst::MaxRPM);
		}
	}

	// クラッチ切断＝サイドブレーキ or クラッチボタン
	const bool clutchOut = handbrake || clutchPressed;
	const float clutchTarget = clutchOut ? 0.0f : 1.0f;
	m_clutch += (clutchTarget - m_clutch) * std::min(CarConst::ClutchSpeed * dt, 1.0f);

	if (!clutchOut)
	{
		// クラッチ接続：駆動輪は地面と一緒に転がる＝車速より遅くならない
		if (vLong0 > 0.0f && m_driveSpeed < vLong0) { m_driveSpeed = vLong0; }

		const float tr = CarConst::GearRatios[m_gear] * CarConst::FinalDrive;

		// 地面から決まる駆動輪の回転。
		// マニュアルなので、高いギアで低速だとRPMが落ち、低いギアで高速だと吹け上がる。
		const float groundRPM = std::clamp(fabsf(m_driveSpeed / radius) * tr * CarConst::RpmPerRadSec,
		                                   CarConst::IdleRPM, CarConst::MaxRPM);
		// 今のエンジン回転から見た駆動輪の速さ
		const float engineDriveSpeed = m_engineRPM / (CarConst::RpmPerRadSec * tr) * radius;

		// クラッチが繋がるとき、回転差はクラッチが滑って埋まる。
		// 大事なのは「速い側が遅い側を引っ張る」という向き。
		if (m_engineRPM > groundRPM)
		{
			// エンジンが勝っている：エンジンが駆動輪を回す＝ホイールスピン。
			// サイドを離した直後がまさにこれで、回転は落ちずに後輪が空転する。
			// ここを逆向きにすると、サイド中は駆動輪をロックしていて
			// しかもドリフト中は車が横を向いていて前後速度が小さいので、
			// 駆動輪側がほぼアイドル扱いになり、回転がそこまで引き落とされる。
			m_driveSpeed += (engineDriveSpeed - m_driveSpeed) *
			                std::min(CarConst::ClutchGrabSpeed * m_clutch * dt, 1.0f);
			// エンジン側は滑っているぶんだけ落ちる(急に同期させない)
			m_engineRPM += (groundRPM - m_engineRPM) *
			               std::min(CarConst::RpmLinkSpeed * CarConst::ClutchSlipDrop * dt, 1.0f);
		}
		else
		{
			// 駆動輪の方が速い＝エンジンが押し回される(エンジンブレーキ側)
			m_engineRPM += (groundRPM - m_engineRPM) * std::min(CarConst::RpmLinkSpeed * dt, 1.0f);
		}

		// レブリミッター：駆動輪速はレッド回転相当を超えられない(無限空転防止)
		const float maxDrive = CarConst::MaxRPM / (CarConst::RpmPerRadSec * tr) * radius;
		m_driveSpeed = std::clamp(m_driveSpeed, -maxDrive, maxDrive);
	}
	else
	{
		// クラッチ切断：ギア固定。アクセルで回転上昇。
		// アクセルを抜けばアイドルへ落ちる(サイドを引いていても同じ)。
		// クランクの角加速度 = (トルク − 摩擦) ÷ 慣性。
		// 一定の割合で上げ下げすると回転計が直線的に動き、機械仕掛けに見える。
		// 摩擦は回転数の2乗におおむね比例する(ポンプ損失・かき混ぜ抵抗・油の粘性)。
		// だから低回転では一気に吹け上がり、レッド手前で急激に鈍る。
		const float rn = std::clamp(m_engineRPM / CarConst::MaxRPM, 0.0f, 1.2f);
		const float friction = CarConst::RevFrictionBase + CarConst::RevFrictionRpm * rn * rn;

		if (throttle > 0.0f)
		{
			// トルクカーブがそのまま吹け上がりの形になる。
			// レブ手前でトルクが絞られるので、頭打ちも自然に出る。
			m_engineRPM += (CarConst::RevUp * torque * throttle
			              - CarConst::RevDown * friction) * dt;
		}
		else
		{
			// 落ちる方も摩擦で決まる＝高回転ほど速く落ち、アイドル付近で粘る。
			//
			//
			// ※サイドブレーキで落ち方を変えてはいけない。
			//   サイドは後輪に効くもので、クラッチが切れていれば
			//   エンジンとは切り離されている。実車では回転の落ち方に
			//   影響する余地がない。
			//   ドリフト中に回転が保たれているのは車の性質ではなく、
			//   運転手がアクセルを煽っているから。操作で解決する話なので、
			//   ここで手心を加える必要はない。
			m_engineRPM -= CarConst::RevDown * friction * dt;
		}
		m_engineRPM = std::clamp(m_engineRPM, CarConst::IdleRPM, CarConst::MaxRPM);
	}


	// レブ手前でトルクを絞る(頭打ち)。RevCutStart→RevCutEndで1→0へ。
	// これでギアが上限に張り付き、伸ばすにはシフトアップが必要＝ギア差が体感できる。
	const float revCut = RevCut();
	torque *= (1.0f - revCut);   // レッドでトルク0

	const float gearFactor = CarConst::GearRatios[m_gear] / CarConst::DriveRefRatio;
	m_driveAccel = m_enginePower * torque * m_clutch * gearFactor;

	UpdateEngineAudio(dt, throttle);
}

//----------------------------------------------------------
// 4輪シミュレーション：各輪の荷重・スリップ角・摩擦円からタイヤ力を求め、
// 車体の速度とヨーへ積分する。この関数が挙動の本体。
// サブステップに分けて解く(1フレームで一気に進めると高速時に破綻するため)。
//----------------------------------------------------------
void CarBase::StepTireForces(float dt, float throttle, float steerInput, bool handbrake,
                             bool clutchPressed, bool accelPressed)
{
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

	// 横方向の荷重移動＝ロール。1次遅れで追わせると目標へ滑らかに寄るだけだが、
	// 実車は切り返しで反対側へ勢いよく倒れ込み、行き過ぎてから戻る。
	// ばね-ダンパで解いてこの行き過ぎを再現する＝振った瞬間に内輪の荷重が
	// 一瞬抜け、後輪がグリップを失って出る(CarXの切り返しの正体)。
	{
		const float wn   = m_rollFreq;
		const float zeta = m_rollDampRatio;
		m_rollV += (wn * wn * (dLatRaw - m_dLatF) - 2.0f * zeta * wn * m_rollV) * h;
		m_dLatF += m_rollV * h;
		m_dLatF  = std::clamp(m_dLatF, -0.45f, 0.45f);
	}
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

	// ダウンフォースで増える接地荷重(4輪合計ぶん。静止時の合計1.0に足す)
	const float dfLoad = m_downforceCoef * (vLong * vLong + vLat * vLat);

	float sumLong = 0.0f, sumLat = 0.0f, sumMz = 0.0f;
	float rearReaction = 0.0f;   // 後輪の縦力合計(駆動輪回転の反力)
	float rearFxL = 0.0f, rearFxR = 0.0f;   // 左右の後輪の縦力(デフの差回転に使う)

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

		// 段差・轍：各輪の路面高さのばらつきをサスの縮みとみなして荷重へ足す。
		// 解析的な荷重移動だけだと、縁石を踏んでも荷重が一切動かない。
		load += m_bumpLoad[i] * m_bumpLoadGain;

		// ダウンフォース：速度の2乗で接地荷重が増える。
		// 高速ほどグリップが上がり、速度域で手触りが変わる。
		load += dfLoad * (w.front ? (1.0f - m_downforceRearBias) : m_downforceRearBias) * 0.5f;

		load = std::clamp(load, 0.02f, 1.20f);

		// 荷重感度：実タイヤは荷重が増えるほど摩擦係数が下がる。
		// これが無いと Dmax が荷重に比例するだけで、左右へ荷重が移っても
		// 軸のグリップ合計が変わらず、荷重移動が挙動に効かない。
		// 下がる側の損失が上がる側の得より大きくなるので、
		// 「荷重が大きく動いた軸は総合的にグリップを失う」＝振ると抜ける。
		const float mu0  = w.front ? m_muFront : m_muRear;
		const float loadRatio = load / 0.25f;   // 1.0が基準(4輪均等)
		const float mu   = std::max(mu0 * (1.0f - m_tireLoadSens * (loadRatio - 1.0f)), 0.05f);
		float Dmax = mu * load * g;   // この輪の摩擦上限(縦横で共有=摩擦円)
		// サイドブレーキ：後輪をロックすると横グリップが激減してリアが外へ流れる。
		// これがハンドブレーキドリフトの核心。倍率0.5=グリップ半減(ImGuiで調整可)。
		if (handbrake && !w.front) { Dmax *= m_handbrakeGripMul; }

		// この輪の駆動輪回転(デフの差ぶん左右で違う)。
		// 空転の深さを見るために、横力より前に求めておく。
		float wheelDrive = 0.0f;
		if (!w.front)
		{
			wheelDrive = m_driveSpeed + (left ? -m_driveDiff : +m_driveDiff);

			// 空転によるグリップ低下。
			// 摩擦円は縦横の「配分」しか決めないので、それだけだと
			// 空転してもその輪が出せる力の総量は変わらない。
			// 実タイヤは滑り比がピークを超えると摩擦係数そのものが落ちる。
			// 踏むほど後輪が失われ、旋回に使える力が減って半径が広がる＝外へ膨らむ。
			// これが無いと、踏んでも角度が変わるだけで線が広がらない。
			const float slipRatio = fabsf(wheelDrive - wLong) / std::max(fabsf(wLong), 2.0f);
			if (slipRatio > CarConst::SpinPeakSlip)
			{
				const float t = std::clamp((slipRatio - CarConst::SpinPeakSlip) /
				                           std::max(CarConst::SpinFallSlip - CarConst::SpinPeakSlip, 1e-3f),
				                           0.0f, 1.0f);
				Dmax *= (1.0f - CarConst::SpinGripFall * t);
			}
		}

		// 横力：スリップ角から
		const float denom = fabsf(wLong) + m_slipEps;
		const float alpha = atan2f(wLat, denom);
		float Fy = latForce(alpha, Dmax);
		// キャンバー：ネガキャン(|camber|)に応じて横グリップを増す(荷重が乗った外輪ほど効く)
		Fy *= (1.0f + m_camberGrip * fabsf(m_camber));

		// リラクゼーション長：横力は舵を切った瞬間には立ち上がらず、
		// タイヤがこの距離ぶん転がって初めて定常値に達する。
		// 追従の速さを「時間」ではなく「進んだ距離」で決めるのが要点で、
		// 低速ほど food立ち上がりが遅くなる実車の挙動もそのまま出る。
		// この遅れが振ってから食うまでの"間"を作り、その隙に車が回る。
		if (m_tireRelaxLen > 1e-3f)
		{
			const float travel = (fabsf(wLong) + 0.5f) * h;   // このステップで転がった距離
			const float k = std::clamp(travel / m_tireRelaxLen, 0.0f, 1.0f);
			m_wheelFy[i] += (Fy - m_wheelFy[i]) * k;
			Fy = m_wheelFy[i];
		}

		// 縦力：後輪=駆動スリップ、全輪=ブレーキ
		//
		// ブレーキはタイヤの回転を止める力なので、進行方向の逆へ効く。
		// 向きを見ずに一定の力をかけ続けると、止まったあとも押し続けて
		// 車が後ろへ這い出す。止まりかけたら弱めて、そこで止める。
		float Fx = 0.0f;
		if (brakeEach != 0.0f)
		{
			const float fade = std::clamp(fabsf(wLong) / CarConst::BrakeFadeSpeed, 0.0f, 1.0f);
			Fx = -std::copysign(fabsf(brakeEach), wLong) * fade;
		}
		float rearFxTraction = 0.0f;
		if (!w.front)
		{
			// デフ：左右の駆動輪はそれぞれ違う速さで回る。
			// m_driveSpeed が左右の平均、m_driveDiff がその差の半分。
			// 旋回中は外輪が速く回る必要があり、その差をデフが許す。
			// (wheelDrive は空転の深さを見るために上で求めてある)
			rearFxTraction = m_longStiff * (wheelDrive - wLong);   // 駆動/空転
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
		if (!w.front)
		{
			rearReaction += FcarLong;
			if (left) { rearFxL = FcarLong; } else { rearFxR = FcarLong; }
		}
	}

	// 空気/転がり抵抗(前後)
	sumLong += -m_drag * vLong;
	// タイヤスクラブ抵抗(横滑り)：ドリフト中の横方向の滑りはタイヤが摩擦で
	// 削り取ってエネルギーを失う。これが無いと横滑り速度が総速度|v|に乗って
	// 「ドリフトの方が直線グリップより速い」不具合になる。横滑りぶんを減衰させる。
	if (m_scrubDragEnabled) { sumLat += -m_scrubDrag * vLat; }

	// 斜面の重力成分。路面の傾きに沿って車を引く。
	// これが無いと下り坂で加速せず、登りで失速せず、バンクも効かない。
	//   m_terrainPitch … 前が高い(登り)で正 → 後ろへ引かれる
	//   m_terrainRoll  … 左が高いで正       → 右へ引かれる
	sumLong += -g * sinf(m_terrainPitch) * m_slopeGravity;
	sumLat  +=  g * sinf(m_terrainRoll)  * m_slopeGravity;

	const float aLong = sumLong;
	const float aLat  = sumLat;
	aLongPrev = aLong; aLatPrev = aLat;
	m_accelLong = aLong;   // サスペンション用に保持
	m_accelLat  = aLat;

	// 駆動輪の回転ダイナミクス：エンジン - 後輪縦反力
	const float wheelI = std::max(m_wheelInertia, 0.05f);
	m_driveSpeed += (engineTotal - rearReaction) * (h / wheelI);

	// デフの差回転：左右の後輪に掛かる縦力の差が回転差を広げ、
	// デフのロックがそれを戻す。ロックが強いほど左右直結(溶接デフ)に近づき、
	// リアが一体で流れる＝ドリフト向き。弱いと内輪が空転して前へ進まない。
	m_driveDiff += (rearFxL - rearFxR) * (h / (2.0f * wheelI));
	m_driveDiff -= m_driveDiff * std::min(m_lsdLock * h, 1.0f);

	if (handbrake)                            { m_driveSpeed = 0.0f; m_driveDiff = 0.0f; }                         // ロック
	// 駆動方向へ踏んでいない(惰行) or クラッチ切断中はフリーホイール＝駆動輪が路面速へ緩和する。
	// これを入れないと、切ったはずの高い駆動輪回転が凍結して残り、m_driveSpeed>接地速で
	// 幽霊の駆動力(m_longStiff*(m_driveSpeed-wLong))が出続けて「クラッチ踏みながら加速」する。
	// ※後退中はaccelPressed=(S踏み)なので、後退駆動を打ち消さない。
	else if (!accelPressed || clutchPressed)  { m_driveSpeed += (vLong - m_driveSpeed) * std::min(m_driveRelax * h, 1.0f); } // 路面速へ

	// 空転の上限。
	// 完全に滑り切ったタイヤは、それ以上速く回しても駆動力が増えない。
	// 余った出力は熱とタイヤの摩耗になり、回転として蓄えられるわけではない。
	//
	// 上限が無いと、摩擦円で頭打ちになった時点で回転を止めるものが無くなり、
	// 駆動輪がはずみ車のように回転を溜め込む。そしてグリップが戻った瞬間に
	// 溜めたぶんを一気に放出して、不自然な加速になる。
	// 停止からの発進ができるよう、下駄(MaxSlipBase)を履かせておく。
	{
		const float slipCap = fabsf(vLong) * (1.0f + CarConst::MaxSlipRatio) + CarConst::MaxSlipBase;
		m_driveSpeed = std::clamp(m_driveSpeed, -slipCap, slipCap);
	}

	// 積分(ヨーはタイヤ力に即応＝グリップは一瞬でかかる。切り返しがキレる)
	m_vel     += (forward * aLong + right * aLat) * h;
	m_yawRate += (sumMz / std::max(m_izz, 0.05f)) * h;
	m_yawRate -= m_yawRate * std::min(m_yawDamp * h, 1.0f);

	// スピン防止アシスト。
	// 横滑りを深める向きのヨーだけを抑える
	{
		const float rate = SpinAssistRate(steerInput, handbrake, vLong, vLat, m_yawRate);

		if (rate > 0.0f) { m_yawRate -= m_yawRate * std::min(rate * h, 1.0f); }
	}
	m_yaw     += m_yawRate * h;

	// 最高速クランプ
	const float sp = m_vel.Length();
	if (sp > m_maxSpeed) { m_vel *= (m_maxSpeed / sp); }
}
}

//----------------------------------------------------------
// 水平移動と壁の押し戻し。
// 1フレームの移動を球半径以下に小刻み分割して進め、毎ステップ壁へ押し戻す。
	// 1フレームの水平移動を球半径以下に小刻み分割して進め、毎ステップ壁へ押し戻す。
	// ・すり抜け防止：ステップ毎に判定するので高速でも薄い壁を飛び越えない。
	// ・角/複数壁のめり込み：1ステップ内で数回リラクゼーションして収束させる。
	// ・擦り滑り：現在速度で刻むので、壁で殺した速度が次ステップに反映されて壁沿いに滑る。
	// 上下(Y)は下方の接地レイで別途決める(この後)。
//----------------------------------------------------------
void CarBase::ResolveWallCollision(float dt)
{
	HjScopedTimer _t(U8(" └ 壁の判定"));

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

					// どの地形ノードに当たったかを覚える。
					// 当たり判定の調整で「今ぶつかっている物」を名指しするために使う
					m_lastWallNode = r.m_hitNodeIndex;
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

//----------------------------------------------------------
// 接地判定と車体の高さ・傾き。
	// 各タイヤ位置から真下へレイを飛ばし、接地高さを4点求める。
	// 4点の平均で車体の高さ、前後差でピッチ(坂)、左右差でロール(バンク)を出す。
//----------------------------------------------------------
void CarBase::UpdateGroundContact(float dt)
{
	HjScopedTimer _t(U8(" └ 接地の判定"));

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

		// 道を先に見る。
		//
		// 高さマップは真上から見た格子なので、路面のカントや
		// 中央の盛り上がりを表現できない。
		// 道の上なら、スプラインの断面から求めたほうが正しい
		if (m_pRoad)
		{
			float rh = 0.0f;
			Math::Vector3 rn = Math::Vector3::Up;
			if (m_pRoad->SampleAt(wpos.x, wpos.z, rh, rn))
			{
				contactHit[i] = true;
				contactY[i]   = rh;
				++hitCount;
				continue;
			}
		}

		// 地形の格子があれば、そこから直接取る。
		//
		// 光線を飛ばすのは、どの面に当たるか分からないから。
		// 格子なら位置からどのマス目かが決まるので、探す必要がない。
		// 4点読んで混ぜるだけで済む
		if (m_pHeightField)
		{
			const float h = m_pHeightField->HeightAt(wpos.x, wpos.z);
			if (h > TerrainConst::OutsideHeight)
			{
				contactHit[i] = true;
				contactY[i]   = h;
				++hitCount;
				continue;
			}
		}

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

		// 段差・轍による荷重の偏り。
		// 車体は「4輪の平均を通り、傾きに沿った平面」に乗っていると考え、
		// 各輪の実際の路面高さがその平面からどれだけ外れているかを見る。
		// 高く飛び出た輪はサスが縮む＝荷重が乗り、へこんだ輪は荷重が抜ける。
		// 解析的な荷重移動だけだと、縁石を踏んでも荷重が一切動かない。
		{
			// 各輪の車体ローカル位置(px=右, pz=前)。0,1=前 / 2,3=後 / 偶数=左
			const float lpx[4] = { -m_track, m_track, -m_track, m_track };
			const float lpz[4] = {  m_base,  m_base, -m_base, -m_base };
			const float tp = tanf(m_terrainPitch);   // 前が高い
			const float tr = tanf(m_terrainRoll);    // 左が高い
			const float sm = std::min(CarConst::BumpLoadSmooth * dt, 1.0f);

			for (int i = 0; i < 4; ++i)
			{
				float target = 0.0f;
				if (contactHit[i])
				{
					const float planeY = avgY + lpz[i] * tp - lpx[i] * tr;
					float d = std::clamp((contactY[i] - planeY) / CarConst::SuspTravel, -1.0f, 1.0f);

					// 不感帯：メッシュの三角形が切り替わるたびに出る細かいガタつきを捨てる。
					// これが無いと路面ノイズがそのまま荷重の揺れになり、
					// グリップが常時ふらついてドリフト角が定まらない。
					const float dz = CarConst::BumpDeadzone;
					if (fabsf(d) <= dz) { d = 0.0f; }
					else                { d = (d > 0.0f) ? (d - dz) : (d + dz); }
					target = d;
				}
				m_bumpLoad[i] += (target - m_bumpLoad[i]) * sm;
			}
		}

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

//----------------------------------------------------------
// タイヤの見た目の回転と、走行状態から出すエフェクト(煙・ネオン・タイヤ痕)の放出。
// 挙動には影響しない。スリップ量など、物理の結果を読んで演出へ渡すだけ。
//----------------------------------------------------------
void CarBase::UpdateMotionFeedback(float dt, bool handbrake, float throttle)
{
	HjScopedTimer _t(U8(" └ 演出の放出"));

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
	m_slipRear01 = slip01;   // 通信で相手へ送る(相手の画面でも同じ量の煙が出る)

	// タイヤのスキール音とブレーキ鳴き。
	// 煙・タイヤ痕とまったく同じ滑り量で駆動するので、
	// 「煙が出ている＝鳴いている」が必ず一致する。
	// ブレーキは前進中にSを踏んでいる量(後退中は駆動なので鳴らさない)。
	{
		const float brake01 = (!m_reverse && throttle < 0.0f) ? fabsf(throttle) : 0.0f;
		m_tireAudio.Update(dt, slip01, carSpeed, brake01, m_onGround);
	}

	PlaceAudio();

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
		m_slipFront01 = frontSlip01;   // 同上

		// 前輪の幅方向は操舵で向きが変わる＝車の右方向を舵角ぶん回したもの
		const Math::Vector3 rightF(cosf(m_yaw + m_steer), 0.0f, -sinf(m_yaw + m_steer));

		for (int side = -1; side <= 1; side += 2)
		{
			const int rearIdx  = m_skidBase + ((side < 0) ? 0 : 1);
			const int frontIdx = m_skidBase + ((side < 0) ? 2 : 3);
			if (!canMark) { SkidMark::Instance().Cut(rearIdx); SkidMark::Instance().Cut(frontIdx); continue; }

			const Math::Vector3 lat = rightV * (static_cast<float>(side) * m_track);

			// 後輪：幅方向はタイヤの回転軸＝車の右方向
			Math::Vector3 wpR = m_pos + lat - fwdS * m_base;
			wpR.y = m_pos.y + SmokeConst::WheelGroundY;
			SkidMark::Instance().Emit(rearIdx, wpR, rightV, slip01);

			// 前輪
			Math::Vector3 wpF = m_pos + lat + fwdS * m_base;
			wpF.y = m_pos.y + SmokeConst::WheelGroundY;
			SkidMark::Instance().Emit(frontIdx, wpF, rightF, frontSlip01);
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

}

void CarBase::Update()
{
	HjScopedTimer _t(U8("車の物理"));

	const float dt = KdFPSController::GetDt();
	if (dt <= 0.0f) { return; }

	// 観戦中など、走行を止めているとき。
	//
	// 入力を切るだけでは惰性で走り続けるので、見ていない間に
	// 崖から落ちたり壁に刺さったりする。速度と回転ごと止める。
	// 車輪の空転も止めないと、置いたまま音が鳴り続ける。
	if (m_halted)
	{
		m_vel        = Math::Vector3::Zero;
		m_yawRate    = 0.0f;
		m_driveSpeed = 0.0f;
		m_driveDiff  = 0.0f;
		m_velY       = 0.0f;

		// 接地だけは続ける。止めた瞬間に地形へめり込んだままになるのを防ぐ。
		//
		// ただしすり抜け中は取らない。取ると、持ち上げた車が
		// 毎フレーム地面へ引き戻されて浮かない
		if (!m_noClip) { UpdateGroundContact(dt); }
		return;
	}

	UpdateDebugKeys();

	const DriveInput in = ReadInput();

	// 追走のCPUが手本として借りる。物理へ渡す前の値をそのまま覚える。
	//
	// 剛体へ渡すのもこれなので、分岐より先に入れる。
	// 後ろに置くと、剛体が前フレームの入力を読み続けて何も効かない
	m_lastInput = in;
	m_handbrakeNow = in.handbrake;   // 通信で相手へ送るので覚えておく

	//===== 剛体で走らせる =====
	// 旧モデルは3自由度の平面モデルで、姿勢は4輪のレイから
	// 推定していた。片輪が浮く・転倒する、が原理的に出せない
	if (m_useRigid)
	{
		UpdateRigid(dt);
		return;
	}
	float throttle       = in.throttle;
	float steerInput     = in.steer;
	bool  handbrake      = in.handbrake;
	bool  clutchPressed  = in.clutch;
	bool  shiftUp        = in.shiftUp;
	bool  shiftDown      = in.shiftDown;

	// 進行方向に対する前後・横の速さ。舵も駆動も後退判定もここを見る
	const Math::Vector3 fwd0(sinf(m_yaw), 0.0f, cosf(m_yaw));
	const Math::Vector3 right0(cosf(m_yaw), 0.0f, -sinf(m_yaw));
	const float vLong0 = m_vel.Dot(fwd0);
	const float vLat0  = m_vel.Dot(right0);
	const float speedNow = m_vel.Length();

	UpdateSteerAngle(dt, steerInput, throttle, handbrake, vLong0, vLat0, speedNow);

	// 振り返しの後押し。切った向きへヨーを足す
	m_yawRate += TransitionYawBoost(steerInput, vLong0, vLat0) * dt;

	UpdateDriveline(dt, throttle, handbrake, clutchPressed, shiftUp, shiftDown, vLong0);

	UpdateReverseGear(dt, throttle, handbrake, vLong0);

	// 駆動方向へアクセルを踏んでいるか(前進=W / 後退=S)。空転リラックスの判定に使う。
	const bool accelPressed = m_reverse ? (throttle < 0.0f) : (throttle > 0.0f);

	StepTireForces(dt, throttle, steerInput, handbrake, clutchPressed, accelPressed);

	UpdateBodyAlignAssist(dt, vLong0, handbrake);
	UpdateSuspensionVisual(dt);

	ResolveWallCollision(dt);
	UpdateGroundContact(dt);

	UpdateMotionFeedback(dt, handbrake, throttle);
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
	HjScopedTimer _t(U8("煙の描画"));

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

//----------------------------------------------------------
// 増えたタイヤ痕をマークマップへ焼き込む。
// 1フレームで焼くのは進んだ数センチぶんだけなので、痕がどれだけ長く
// なっても、車が何台いても、ここのコストは変わらない。
//----------------------------------------------------------
void CarBase::PreDraw()
{
	HjScopedTimer _t(U8("タイヤ痕の焼付"));

	SkidMark::Instance().BakePending();
}

//----------------------------------------------------------
// 車体と4輪の行列を組む
//
// DrawLit と車庫の見せ札(HjCarPortrait)の両方から呼ぶ。
// 見せ札側へ写すと、調整パネルでオフセットを触るたびに
// 走行中の車と車庫の車で位置が食い違う
//----------------------------------------------------------
void CarBase::BuildPose(const Math::Matrix& carWorld,
                        Math::Matrix& outBody, Math::Matrix outWheel[4]) const
{
	//===== 車体の行列(サスのロール/ピッチを反映。タイヤは接地したまま) =====
	outBody =
		Math::Matrix::CreateScale(m_bodyScale) *
		Math::Matrix::CreateRotationY(m_bodyYaw) *
		// 車体モデルだけをずらす。タイヤは接地したままにしたいので、
		// 車ごと動かすことはしない
		Math::Matrix::CreateTranslation(m_bodyOffset) *
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

		outWheel[i] =
			spinRot *
			Math::Matrix::CreateScale(m_wheelScale) *
			Math::Matrix::CreateRotationZ(m_camber) *   // キャンバー(flipより前=左右で自動ミラー)
			flip *
			Math::Matrix::CreateRotationY(m_wheelYaw) *
			steerRot *
			Math::Matrix::CreateTranslation(w.x + ox, m_wheelH, w.z + oz) *
			carWorld;
	}

}

void CarBase::DrawLit()
{
	auto& shader = KdShaderManager::Instance().m_StandardShader;

	// ※タイヤ痕はここでは描かない。焼き付けマップへ書き溜めてあり、
	//   路面のシェーダーが引いて色を暗くする(Stage側で ApplyToShader を呼ぶ)。

	KdShaderManager::Instance().ChangeRasterizerState(KdRasterizerState::CullNone);
	// 車体全体(本体＋4輪)を地形の傾きへ合わせる＝坂・バンクで車ごと傾く(CarX風の床判定)
	// 傾きは"車のローカル軸"で掛ける＝ヨー(向き)より先に適用する。後に掛けると
	// ワールド軸基準になり、車が向きを変えるとピッチとロールが入れ替わってしまう。
	// 符号：登りでノーズ上げ／左が高い路面で右下がりになるよう反転。
	// 通信で姿勢を受け取っているなら、合成済みのものをそのまま使う。
	// 角度ごとに補間すると3軸が同時に動いたときに経路がずれて車体が揺れる
	const Math::Matrix carRot = m_useNetRotation
		? Math::Matrix::CreateFromQuaternion(m_netRotation)
		: (Math::Matrix::CreateRotationX(-m_terrainPitch) *   // 前後(坂)：ローカルX(右)軸まわり
		   Math::Matrix::CreateRotationZ(-m_terrainRoll)  *   // 左右(バンク)：ローカルZ(前)軸まわり
		   Math::Matrix::CreateRotationY(m_yaw));            // 向き(ヨー)

	const Math::Matrix carWorld = carRot * Math::Matrix::CreateTranslation(m_pos);

	Math::Matrix bodyW;
	Math::Matrix wheelMat[4];
	BuildPose(carWorld, bodyW, wheelMat);

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

		// ブースト中は太らせる＝縁が発光して見える
		const float baseW = m_outlineWidth * (1.0f + tintAmt * 1.6f);

		shader.BeginOutline();

		// 1本にまとめて描く。車体と4輪で同じことをするので
		auto drawAll = [&](float width, const Math::Color& col)
		{
			shader.SetOutlineWidth(width);
			shader.DrawModel(m_body, bodyW, col);
			for (const auto& m : wheelMat) { shader.DrawModel(m_wheel, m, col); }
		};

		if (CarConst::OutlineTwoLayer)
		{
			// 外側(色)を先。
			// 後に描いたほうが上に乗るので、内側の黒が色の内周を
			// 上書きして二層に見える。逆にすると色が黒を覆って1本になる
			drawAll(baseW * CarConst::OutlineOuterMul, oc);

			// 内側(黒)。
			// 色だけだと、明るい路面や空を背にしたとき車体との境目が溶ける。
			// 黒を1本入れると背景が何色でも形が立つ
			const Math::Color inner(CarConst::OutlineInnerR,
			                        CarConst::OutlineInnerG,
			                        CarConst::OutlineInnerB, 1.0f);
			drawAll(baseW, inner);
		}
		else
		{
			drawAll(baseW, oc);
		}

		shader.EndOutline();
		shader.BeginLit();   // 後続オブジェクトのためにLitへ戻す
	}
}


//----------------------------------------------------------
// 見た目のライブ調整パネル（決まった値は各車種のコンストラクタへ反映する）
//----------------------------------------------------------
//----------------------------------------------------------
// 見た目の差し替え(調整パネルの中の1区画)
//
// ここに置くのは、大きさや向きの調整がすぐ下にあるから。
// 外から持ってきたモデルは、そのままでは乗らないことがほとんどで、
// 差し替えた直後に必ず調整することになる。
// 画面を分けると、選ぶたびに行き来することになる。
//----------------------------------------------------------
void CarBase::DrawModImGui()
{
	ImGui::SeparatorText(U8("見た目の差し替え(MOD)"));

	auto& cat = HjModCatalog::Instance();

	// 最初に開いたときだけ調べる。
	// フォルダを見に行くのは遅いので、毎フレームやらない
	if (!cat.IsScanned()) { cat.Rescan(); }

	if (ImGui::Button(U8("一覧を更新")))
	{
		cat.Rescan();
	}
	ImGui::SameLine();
	ImGui::TextDisabled(U8("Mods/Body, /Wheel に置く"));

	if (cat.SkippedCount() > 0)
	{
		// 黙って外すと、置いたのに出てこない理由が分からない
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
		                   U8("%d 件は形式が違うか大きすぎるため外しました"),
		                   cat.SkippedCount());
	}

	// 直前の結果。次に何か選ぶまで出しておく
	static HjModLoader::Result s_last = HjModLoader::Result::Ok;

	// 1つ分の選択欄。車体とホイールで中身が同じなので、まとめる
	auto pick = [&](const char* label, HjModCatalog::Kind kind,
	                const std::string& cur,
	                HjModLoader::Result (CarBase::*apply)(const std::string&))
	{
		const auto& list = cat.List(kind);

		// いま選ばれているものを見出しに出す。
		//
		// その車のモデルなら、そのモデル名を出す。
		// 「標準」と呼び分けると、別物のように見える。
		// シルビアの車体はシルビアのモデルというだけで、特別ではない
		const std::string ownName = (kind == HjModCatalog::Kind::Body)
			? GetOwnBodyName() : GetOwnWheelName();

		const bool stock = (cur == ModConst::StockMark);
		const char* now  = stock ? ownName.c_str()
		                         : (cat.IndexOf(kind, cur) >= 0
		                            ? list[cat.IndexOf(kind, cur)].name.c_str()
		                            : U8("(見つかりません)"));

		ImGui::Text("%s : %s", label, now);

		ImGui::PushID(label);

		if (ImGui::Button(U8("この車のモデルへ戻す")))
		{
			s_last = (this->*apply)(ModConst::StockMark);
			SaveModChoice();
		}

		if (list.empty())
		{
			ImGui::TextDisabled(U8("  置かれているモデルがありません"));
			ImGui::PopID();
			return;
		}

		// 候補を並べる。数が多いと縦に伸びきってしまうので、
		// 高さを決めた枠の中で送る
		if (ImGui::BeginListBox("##list", ImVec2(-FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * 4.5f)))
		{
			for (size_t i = 0; i < list.size(); ++i)
			{
				const bool sel = (list[i].path == cur);
				if (ImGui::Selectable(list[i].name.c_str(), sel))
				{
					s_last = (this->*apply)(list[i].path);

					// 選んだ時点で覚える。
					// 別に「保存」を押させると、押し忘れて next 起動で戻る
					SaveModChoice();
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s  (%d KB)", list[i].path.c_str(), list[i].sizeKb);
				}
			}
			ImGui::EndListBox();
		}

		ImGui::PopID();
	};

	pick(U8("車体"),   HjModCatalog::Kind::Body,  m_bodyModPath,  &CarBase::SetBodyModel);
	pick(U8("ホイール"), HjModCatalog::Kind::Wheel, m_wheelModPath, &CarBase::SetWheelModel);

	if (s_last != HjModLoader::Result::Ok)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
		                   HjModLoader::Message(s_last));
	}

	ImGui::TextDisabled(U8("向きや大きさは下の「見た目」で合わせる"));
}

void CarBase::DrawTuningImGui()
{
	//===== 走らせ方 =====
	// 旧モデルは3自由度の平面モデル。姿勢は4輪のレイから推定していた。
	// 剛体は姿勢そのものを持つので、片輪が浮くし転倒もする。
	//
	// 詰め終わるまで見比べる必要があるので、切り替えられるようにしてある
	{
		bool rigid = m_useRigid;
		if (ImGui::Checkbox(U8("剛体で走らせる(6自由度)"), &rigid))
		{
			SetRigid(rigid);
		}

		if (m_useRigid)
		{
			ImGui::SameLine();
			ImGui::TextDisabled(U8("接地 %d / 4"), m_rigid.GroundedCount());

			ImGui::TextDisabled(U8("横滑り %.1f 度 / 前 %.2f 後 %.2f"),
			                    m_rigid.SlipAngle() * 57.2958f,
			                    m_rigid.SlipFront(), m_rigid.SlipRear());

			if (m_rigid.IsFlipped()) { ImGui::TextDisabled(U8("転倒中")); }

			// 調整値は毎フレーム渡しているので、触ればそのまま効く。
			// 回転と段はエンジン側(旧モデル)が持っている
			if (m_reverse)
			{
				ImGui::TextDisabled(U8("%.0f rpm / R"), m_engineRPM);
			}
			else
			{
				ImGui::TextDisabled(U8("%.0f rpm / %d速"), m_engineRPM, m_gear);
			}
		}

		ImGui::Separator();
	}

	// ※ウィンドウは開かない。Hierarchy が用意した Inspector の中へ描く。
	//   自前で Begin すると、選ぶたびに別ウィンドウが現れて
	//   位置もドッキング状態もバラバラになる。

	// ===== 見た目の差し替え(MOD) =====
	DrawModImGui();

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
	// 車体モデルだけをずらす(タイヤは接地したまま)
	ImGui::DragFloat3(U8("車体の位置(x/高さ/前後)"), &m_bodyOffset.x, 0.005f);

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
	//
	// ここの値はシェーダー側が持っていて、車の調整値ではない。
	// そのままだと保存されず、次に起動したときに戻ってしまうので、
	// 触ったことを設定側へ伝えて自動的に書き出させる。
	ImGui::SeparatorText(U8("画面アウトライン(トゥーン)"));
	{
		auto& pp = KdShaderManager::Instance().m_postProcessShader;
		bool changed = false;

		bool sceneOutline = pp.IsSceneOutlineEnabled();
		if (ImGui::Checkbox(U8("有効##sceneOutline"), &sceneOutline))
		{
			pp.SetSceneOutlineEnabled(sceneOutline);
			changed = true;
		}
		changed |= ImGui::DragFloat(U8("太さ(px)##sceneOutline"), &pp.WorkOutlineThickness(), 0.05f, 0.5f, 8.0f);
		changed |= ImGui::DragFloat(U8("深度しきい値(シルエット)"), &pp.WorkOutlineDepthThreshold(), 0.01f, 0.01f, 2.0f);
		changed |= ImGui::DragFloat(U8("法線しきい値(角)"), &pp.WorkOutlineNormalThreshold(), 0.01f, 0.01f, 1.0f);
		changed |= ImGui::DragFloat(U8("濃さ##sceneOutline"), &pp.WorkOutlineEdgeStrength(), 0.02f, 0.0f, 1.0f);
		changed |= ImGui::ColorEdit3(U8("色##sceneOutline"), &pp.WorkOutlineColor().x);

		if (changed) { HjPostFxSettings::Instance().NotifyChanged(); }
	}

	// 画面全体のハーフトーン(印刷風の網点)
	ImGui::SeparatorText(U8("ハーフトーン(印刷風)"));
	{
		auto& pp = KdShaderManager::Instance().m_postProcessShader;
		bool changed = false;

		bool halftone = pp.IsHalftoneEnabled();
		if (ImGui::Checkbox(U8("有効##halftone"), &halftone))
		{
			pp.SetHalftoneEnabled(halftone);
			changed = true;
		}
		changed |= ImGui::DragFloat(U8("網点の周期(px)"), &pp.WorkHalftoneScale(), 0.1f, 2.0f, 40.0f);
		changed |= ImGui::DragFloat(U8("濃さ##halftone"), &pp.WorkHalftoneStrength(), 0.01f, 0.0f, 1.0f);
		changed |= ImGui::DragFloat(U8("暗部に寄せる量"), &pp.WorkHalftoneDarkBias(), 0.02f, 0.0f, 1.0f);

		if (changed) { HjPostFxSettings::Instance().NotifyChanged(); }
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
	ImGui::DragFloat(U8("低速安定(スリップ分母)"), &m_slipEps, 0.1f, 0.1f, 20.0f);

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

	// エンジン音は鳴らしながら詰めるものなので、走行中に触れる位置に出す
	m_engineAudio.DrawImGui();
	m_tireAudio.DrawImGui();
	HjAudioSpace::Instance().DrawImGui();

	// アシストの一括操作。すべて切ると、タイヤと荷重だけで走る素の挙動になる。
	// CarXも同種の設定を持つが、あちらは「切っても物理が成立する」前提なので、
	// ここでも個別に切れるようにして比較できるようにしてある。
	ImGui::SeparatorText(U8("運転アシスト(まとめて切替)"));
	if (ImGui::Button(U8("すべてOFF(素の物理)")))
	{
		m_counterSteerEnabled = false;
		m_spinAssistEnabled   = false;
		m_transitionEnabled   = false;
		m_bodyAlignEnabled    = false;
		m_scrubDragEnabled    = false;
	}
	ImGui::SameLine();
	if (ImGui::Button(U8("すべてON")))
	{
		m_counterSteerEnabled = true;
		m_spinAssistEnabled   = true;
		m_transitionEnabled   = true;
		m_bodyAlignEnabled    = true;
		m_scrubDragEnabled    = true;
	}
	ImGui::Checkbox(U8("振り返しのヨー後押し"), &m_transitionEnabled);
	ImGui::Checkbox(U8("アクセルオフで進行方向へ回頭"), &m_bodyAlignEnabled);
	ImGui::Checkbox(U8("横滑り速度の直接減衰"), &m_scrubDragEnabled);

	ImGui::SeparatorText(U8("オートカウンター(CarX風)"));
	ImGui::Checkbox(U8("有効##counter"), &m_counterSteerEnabled);
	ImGui::Checkbox(U8("アクセルオフでカウンターを抜く"), &m_liftCounterEnabled);
	ImGui::DragFloat(U8("カウンター強さ(0-1.3)"), &m_counterAssist, 0.01f, 0.0f, 1.3f);
	ImGui::DragFloat(U8("逆に切った時カウンターを緩める(0-1)"), &m_counterRelease, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat(U8("効き始め速度"), &m_counterMinSpeed, 0.1f, 0.0f, 20.0f);
	ImGui::DragFloat(U8("サイド中カウンター倍率(1=通常)"), &m_handbrakeCounterMul, 0.02f, 0.0f, 1.0f);

	ImGui::SeparatorText(U8("スピン防止アシスト"));
	ImGui::Checkbox(U8("有効##spin"), &m_spinAssistEnabled);
	ImGui::DragFloat(U8("アシスト強さ(大=スピンしにくい)"), &m_spinAssist, 0.1f, 0.0f, 20.0f);

	ImGui::SeparatorText(U8("タイヤの実挙動(切り返しの手触り)"));
	ImGui::DragFloat(U8("荷重感度(大=荷重移動でグリップを失う)"), &m_tireLoadSens, 0.01f, 0.0f, 0.8f);
	ImGui::DragFloat(U8("リラクゼーション長(m, 小=反応が鋭い)"), &m_tireRelaxLen, 0.02f, 0.05f, 2.0f);
	ImGui::DragFloat(U8("ロールの速さ(rad/s)"), &m_rollFreq, 0.2f, 2.0f, 25.0f);
	ImGui::DragFloat(U8("ロールの減衰比(1未満で行き過ぎる)"), &m_rollDampRatio, 0.02f, 0.15f, 1.5f);

	ImGui::SeparatorText(U8("路面の傾き・空力・駆動系"));
	ImGui::DragFloat(U8("斜面の重力(1=物理どおり, 0=無効)"), &m_slopeGravity, 0.02f, 0.0f, 2.0f);
	ImGui::DragFloat(U8("ダウンフォース係数"), &m_downforceCoef, 0.00002f, 0.0f, 0.002f, "%.5f");
	ImGui::DragFloat(U8("ダウンフォース後ろ配分(0.5=均等)"), &m_downforceRearBias, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat(U8("デフのロック(大=溶接デフ/小=オープン)"), &m_lsdLock, 0.5f, 0.0f, 60.0f);
	ImGui::DragFloat(U8("段差の荷重変化(0=無効)"), &m_bumpLoadGain, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat(U8("車体アライン(オフで角度を抜く, 0=OFF)"), &m_bodyAlign, 0.2f, 0.0f, 20.0f);
	ImGui::DragFloat(U8("トランジション補助(振り返し, 0=OFF)"), &m_transitionAssist, 0.1f, 0.0f, 15.0f);
	ImGui::DragFloat(U8("舵を戻す速さの倍率"), &m_steerReturnMul, 0.05f, 1.0f, 6.0f);

}

//----------------------------------------------------------
// 調整値の保存 / 読込
//----------------------------------------------------------
//----------------------------------------------------------
// 道から、拡張子なしのファイル名だけ取り出す
//
// MOD の候補と同じ形で並べるために使う
//----------------------------------------------------------
std::string CarBase::AssetName(const std::string& path)
{
	size_t begin = path.find_last_of("/\\");
	begin = (begin == std::string::npos) ? 0 : (begin + 1);

	const size_t dot = path.find_last_of('.');
	const size_t end = (dot == std::string::npos || dot < begin) ? path.size() : dot;

	return path.substr(begin, end - begin);
}

std::string CarBase::TuneFilePath() const
{
	return "Asset/Data/CarTune_" + m_saveKey + ".txt";
}

std::vector<std::pair<const char*, float*>> CarBase::TuneParamList()
{
	std::vector<std::pair<const char*, float*>> list = {
		{ "bodyScale", &m_bodyScale }, { "bodyYaw", &m_bodyYaw },
		{ "wheelScale", &m_wheelScale }, { "wheelYaw", &m_wheelYaw },
		{ "track", &m_track }, { "base", &m_base }, { "wheelH", &m_wheelH }, { "camber", &m_camber },
		{ "offX", &m_offX }, { "offZ", &m_offZ },
		{ "bodyOffX", &m_bodyOffset.x }, { "bodyOffY", &m_bodyOffset.y },
		{ "bodyOffZ", &m_bodyOffset.z },
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
		// 駆動輪の空転(摩擦円)
		{ "longStiff", &m_longStiff }, { "wheelInertia", &m_wheelInertia }, { "driveRelax", &m_driveRelax },
		// サスペンション
		{ "suspStiff", &m_suspStiff }, { "suspDamp", &m_suspDamp },
		{ "rollGain", &m_rollGain }, { "pitchGain", &m_pitchGain }, { "suspMax", &m_suspMax },
		{ "accelSmooth", &m_accelSmooth },
		// オートカウンター
		{ "counterAssist", &m_counterAssist }, { "counterMinSpeed", &m_counterMinSpeed },
		{ "spinAssist", &m_spinAssist }, { "bodyAlign", &m_bodyAlign },
		{ "transitionAssist", &m_transitionAssist },
		{ "steerReturnMul", &m_steerReturnMul }, { "counterRelease", &m_counterRelease },
		// タイヤの実挙動(切り返しの手触りを決める)
		{ "tireLoadSens", &m_tireLoadSens }, { "tireRelaxLen", &m_tireRelaxLen },
		{ "rollFreq", &m_rollFreq }, { "rollDampRatio", &m_rollDampRatio },
		// ※エンジン音のパラメータはこの下で m_engineAudio から追加する
		// 路面の傾き・空力・駆動系
		{ "slopeGravity", &m_slopeGravity },
		{ "downforceCoef", &m_downforceCoef }, { "downforceRearBias", &m_downforceRearBias },
		{ "lsdLock", &m_lsdLock }, { "bumpLoadGain", &m_bumpLoadGain },
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

	// エンジン音の調整値は音側が持っているので、そこから受け取って足す。
	// ここへ手書きで並べると、パラメータを増やすたびに追加漏れが起きる。
	m_engineAudio.CollectTuneParams(list);
	m_tireAudio.CollectTuneParams(list);

	return list;
}

//----------------------------------------------------------
// 見た目の差し替え(MOD)
//----------------------------------------------------------
std::string CarBase::ModFilePath() const
{
	return std::string(ModConst::SavePrefix) + m_saveKey + ModConst::SaveSuffix;
}

HjModLoader::Result CarBase::SetBodyModel(const std::string& path)
{
	// 標準へ戻す。派生クラスがコンストラクタで入れた道が残っているので、
	// それを読み直せばよい
	if (path.empty() || path == ModConst::StockMark)
	{
		m_body.SetModelData(m_bodyPath);
		m_bodyModPath = ModConst::StockMark;
		return HjModLoader::Result::Ok;
	}

	const HjModLoader::Result r = HjModLoader::Load(m_body, path);
	if (r == HjModLoader::Result::Ok) { m_bodyModPath = path; }
	return r;
}

HjModLoader::Result CarBase::SetWheelModel(const std::string& path)
{
	if (path.empty() || path == ModConst::StockMark)
	{
		m_wheel.SetModelData(m_wheelPath);
		m_wheelModPath = ModConst::StockMark;
		return HjModLoader::Result::Ok;
	}

	const HjModLoader::Result r = HjModLoader::Load(m_wheel, path);
	if (r == HjModLoader::Result::Ok) { m_wheelModPath = path; }
	return r;
}

std::vector<CarBase::AppearanceParam> CarBase::AppearanceParamList()
{
	// 走行中のメニューから触ってよいものだけ。
	// 順番はそのまま画面に並ぶので、合わせる手順どおりに置く。
	// まず全体の大きさと向き、次に車体の位置、最後にタイヤの配置。
	//
	// 左の名前は保存に使うので変えないこと。
	// 右の名前は画面に出るだけなので、いつ変えてもよい
	return {
		{ "bodyScale",  U8("車体の大きさ"),   &m_bodyScale },
		{ "bodyYaw",    U8("車体の向き"),     &m_bodyYaw },
		{ "bodyOffY",   U8("車体の高さ"),     &m_bodyOffset.y },
		{ "bodyOffZ",   U8("車体の前後"),     &m_bodyOffset.z },
		{ "bodyOffX",   U8("車体の左右"),     &m_bodyOffset.x },

		{ "wheelScale", U8("タイヤの大きさ"), &m_wheelScale },
		{ "wheelYaw",   U8("タイヤの向き"),   &m_wheelYaw },
		{ "camber",     U8("キャンバー"),     &m_camber },

		{ "track",      U8("左右の幅"),       &m_track },
		{ "base",       U8("前後の長さ"),     &m_base },
		{ "wheelH",     U8("タイヤの高さ"),   &m_wheelH },

		{ "offX",       U8("4輪まとめて左右"), &m_offX },
		{ "offZ",       U8("4輪まとめて前後"), &m_offZ },
		{ "frontOffX",  U8("前輪だけ左右"),   &m_frontOffX },
		{ "frontOffZ",  U8("前輪だけ前後"),   &m_frontOffZ },
		{ "rearOffX",   U8("後輪だけ左右"),   &m_rearOffX },
		{ "rearOffZ",   U8("後輪だけ前後"),   &m_rearOffZ },
	};
}

void CarBase::SaveModChoice() const
{
	HjSaveOStream ofs("mod/" + m_saveKey, ModFilePath().c_str());
	if (!ofs) { return; }

	// 1行1項目。読み込み側が名前で拾うので、順番は問わない
	ofs << ModConst::KeyBody  << " " << m_bodyModPath  << "\n";
	ofs << ModConst::KeyWheel << " " << m_wheelModPath << "\n";
}

void CarBase::LoadModChoice()
{
	HjSaveIStream ifs("mod/" + m_saveKey, ModFilePath().c_str());
	if (!ifs) { return; }

	std::string key;
	std::string val;
	while (ifs >> key >> val)
	{
		// 読み込めなくても止めない。
		// MODのファイルが消えていることは普通に起きるので、
		// そのときは標準のまま進む
		if (key == ModConst::KeyBody)       { SetBodyModel(val); }
		else if (key == ModConst::KeyWheel) { SetWheelModel(val); }
	}
}

void CarBase::SaveTuning()
{
	HjSaveOStream ofs("tune/" + m_saveKey, TuneFilePath().c_str());
	if (!ofs) { return; }
	for (const auto& p : TuneParamList()) { ofs << p.first << " " << *p.second << "\n"; }
}

void CarBase::LoadTuning()
{
	HjSaveIStream ifs("tune/" + m_saveKey, TuneFilePath().c_str());
	if (!ifs) { return; }
	auto params = TuneParamList();
	std::string key;
	float val = 0.0f;
	while (ifs >> key >> val)
	{
		for (const auto& p : params) { if (key == p.first) { *p.second = val; break; } }
	}
}
//----------------------------------------------------------
// 素のまま描く
//
// 影の元になる深度を書くのに使う。
// 深度マップの生成では色も縁取りも要らないので、
// ポーズを組んでモデルを流すだけ
//----------------------------------------------------------
void CarBase::DrawPortraitPlain(const Math::Matrix& world)
{
	auto& shader = KdShaderManager::Instance().m_StandardShader;

	Math::Matrix bodyW;
	Math::Matrix wheelMat[4];
	BuildPose(world, bodyW, wheelMat);

	shader.DrawModel(m_body, bodyW);
	for (const auto& m : wheelMat) { shader.DrawModel(m_wheel, m); }
}


//----------------------------------------------------------
// 車庫の見せ札として描く
//
// 走行中の DrawLit と分ける理由:
//   ・ドリフトの塗り(SetTint)と輪郭は走っている時の演出で、
//     止まっている絵に乗せると何も起きていないのに光って見える
//   ・タイヤ痕・煙・当たり判定の線は絵に要らない
//
// 姿勢の値(ピッチ・ロール・切れ角・転がり)は、この車を
// Update していないので既定のまま=直立・直進で組まれる
//----------------------------------------------------------
void CarBase::DrawPortrait(const Math::Matrix& world, float outlineMul,
                           const Math::Color& col)
{
	auto& shader = KdShaderManager::Instance().m_StandardShader;

	Math::Matrix bodyW;
	Math::Matrix wheelMat[4];
	BuildPose(world, bodyW, wheelMat);

	// 裏面も出す。走行中と同じ扱いにしないと、
	// 同じモデルなのに車庫でだけ穴が空いて見える
	KdShaderManager::Instance().ChangeRasterizerState(KdRasterizerState::CullNone);

	shader.DrawModel(m_body, bodyW, col);
	for (const auto& m : wheelMat) { shader.DrawModel(m_wheel, m, col); }

	KdShaderManager::Instance().UndoRasterizerState();

	//===== アウトライン(走行中と同じ背面押し出し) =====
	// これが無いと、車庫でだけ縁の無いのっぺりした絵になる。
	// 走っている時と同じ描き味で見せないと、選んだ車の印象が変わる。
	//
	// ブースト中の発光は入れない。止まった絵で縁が光ると、
	// 何も起きていないのに何か起きているように見える
	//
	// 太さだけは呼ぶ側から絞れるようにする。走行中と同じ太さで
	// 大きく写すと、背面を押し出す作りの都合で面の切れ目に線が溜まり、
	// 車体の内側まで色が散る
	if (m_outlineEnabled && outlineMul > 0.0f)
	{
		shader.BeginOutline();

		auto drawAll = [&](float width, const Math::Color& col)
		{
			shader.SetOutlineWidth(width * outlineMul);
			shader.DrawModel(m_body, bodyW, col);
			for (const auto& m : wheelMat) { shader.DrawModel(m_wheel, m, col); }
		};

		const Math::Color oc(m_outlineColor.x, m_outlineColor.y, m_outlineColor.z, 1.0f);

		if (CarConst::OutlineTwoLayer)
		{
			// 外側(色)を先。後に描いたほうが上に乗るので、
			// 内側の黒が色の内周を上書きして二層に見える
			drawAll(m_outlineWidth * CarConst::OutlineOuterMul, oc);

			const Math::Color inner(CarConst::OutlineInnerR,
			                        CarConst::OutlineInnerG,
			                        CarConst::OutlineInnerB, 1.0f);
			drawAll(m_outlineWidth, inner);
		}
		else
		{
			drawAll(m_outlineWidth, oc);
		}

		shader.EndOutline();
	}
}

//----------------------------------------------------------
// 見せ札のためにモデルだけ読む
//
// Init() は音・タイヤ痕の確保・当たり判定の線まで用意する。
// 絵を出すだけの車にそれをやらせると、走ってもいない車が
// 共有のタイヤ痕マップの枠を食う
//----------------------------------------------------------
void CarBase::LoadPreviewModels()
{
	// 順番は Init() と同じにする。
	//
	// 先に標準を読み、その上へ差し替えを載せる。
	// 逆にすると、差し替えのモデルを HjModLoader ではなく
	// 素の読み込みに通すことになる(MODはAsset/の外にあるので壊れる)
	m_body.SetModelData(m_bodyPath);
	m_wheel.SetModelData(m_wheelPath);

	// 保存済みの調整値を読む。
	//
	// 車体スケールもタイヤ位置もここに入っている。
	// 読まないと、走行中の車と車庫の車で大きさも佇まいも別物になる
	LoadTuning();

	// 車庫で選んだ車と、走り出した車の見た目が違うと選んだ意味がない
	if (m_useModChoice) { LoadModChoice(); }
}

//----------------------------------------------------------
// 車体が占める大きさを測る
//
// 車ごとにモデルの単位も m_bodyScale もばらばらなので、
// カメラの距離を決め打ちにすると、車を替えるたびに
// 画面に映る大きさが変わる。
// 実際に読んだモデルから測って、カメラ側で合わせる
//----------------------------------------------------------
bool CarBase::GetBodyBounds(Math::Vector3& outCenter, float& outRadius) const
{
	const auto data = m_body.GetData();
	if (!data) { return false; }

	bool any = false;
	Math::Vector3 lo, hi;

	for (const auto& node : data->GetOriginalNodes())
	{
		if (!node.m_spMesh) { continue; }

		const auto& box = node.m_spMesh->GetBoundingBox();

		// 箱の8隅を全部ノードの行列に通す。
		//
		// 中心だけ運んで辺の長さをそのまま使うと、ノード側に縮尺が
		// 入っているモデル(書き出し時にcm単位のまま親で縮めたもの等)で
		// 大きさを取り違える。縮小されているモデルほど過大に見積もり、
		// カメラがその分だけ遠のいて豆粒になる
		for (int k = 0; k < 8; ++k)
		{
			const Math::Vector3 corner = {
				box.Center.x + ((k & 1) ? box.Extents.x : -box.Extents.x),
				box.Center.y + ((k & 2) ? box.Extents.y : -box.Extents.y),
				box.Center.z + ((k & 4) ? box.Extents.z : -box.Extents.z),
			};

			const Math::Vector3 w = Math::Vector3::Transform(corner, node.m_worldTransform);

			if (!any) { lo = w; hi = w; any = true; }
			else
			{
				lo = Math::Vector3::Min(lo, w);
				hi = Math::Vector3::Max(hi, w);
			}
		}
	}

	if (!any) { return false; }

	// 描くときと同じ拡大と位置ずらしを掛ける
	outCenter = (lo + hi) * 0.5f * m_bodyScale + m_bodyOffset;
	outRadius = ((hi - lo) * 0.5f * m_bodyScale).Length();
	return true;
}

//----------------------------------------------------------
// 剛体で走らせるかを切り替える
//
// 入り切りで座標系が変わる。旧モデルは接地面の高さで位置を持ち、
// 剛体は重心の高さで持つので、いまの位置を渡し直す
//----------------------------------------------------------
void CarBase::SetRigid(bool on)
{
	if (m_useRigid == on) { return; }

	m_useRigid = on;

	if (on)
	{
		ApplyRigidSetup();
		m_rigid.Place(m_pos, m_yaw);

		// 駆動輪の回転を引き継ぐ。捨てると切り替えた瞬間に失速する
		m_rigid.SetDrive(m_driveSpeed, m_driveDiff);
	}
	else
	{
		// 旧モデルへ戻す。姿勢は捨てて向きだけ引き継ぐ
		m_useNetRotation = false;
		m_vel     = Math::Vector3::Zero;
		m_velY    = 0.0f;
		m_yawRate = 0.0f;
	}
}

//----------------------------------------------------------
// 調整値を剛体へ渡す
//
// 名前も単位も旧モデルのまま渡す。
// 剛体側で読み替えると、調整パネルのどのつまみが何に効くのか
// 分からなくなる
//----------------------------------------------------------
void CarBase::ApplyRigidSetup()
{
	HjCarRigid::Setup su;

	su.track = m_track;
	su.base  = m_base;

	//----- タイヤ -----
	su.muFront = m_muFront;
	su.muRear  = m_muRear;
	su.tireB   = m_tireB;
	su.tireC   = m_tireC;
	su.tireLoadSens = m_tireLoadSens;
	su.tireRelaxLen = m_tireRelaxLen;
	su.slipEps      = m_slipEps;
	su.camber     = m_camber;
	su.camberGrip = m_camberGrip;
	su.handbrakeGripMul = m_handbrakeGripMul;

	//----- アライメント -----
	su.toeFront  = m_toeFront;
	su.toeRear   = m_toeRear;
	su.ackermann = m_ackermann;

	//----- 駆動輪 -----
	su.longStiff    = m_longStiff;
	su.wheelInertia = m_wheelInertia;
	su.driveRelax   = m_driveRelax;
	su.lsdLock      = m_lsdLock;

	//----- 抵抗 -----
	su.drag      = m_drag;
	su.scrubDrag = m_scrubDrag;
	su.scrubDragEnabled = m_scrubDragEnabled;

	//----- 力 -----
	su.brakePower = m_brakePower;
	su.maxSpeed   = m_maxSpeed;
	su.yawDamp    = m_yawDamp;

	//----- 空力 -----
	su.downforceCoef     = m_downforceCoef;
	su.downforceRearBias = m_downforceRearBias;

	//----- サス -----
	su.springF = m_springF;
	su.springR = m_springR;
	su.arbF    = m_arbF;
	su.arbR    = m_arbR;

	m_rigid.SetSetup(su);
}

//----------------------------------------------------------
// 剛体で1フレーム進める
//
// ■ 役割分担
//   舵・エンジン・変速・アシスト … ここ(旧モデルと同じ処理)
//   姿勢・接地・タイヤの力        … 剛体
//
// ドリフトの手ざわりは平面モデル側で作り込んであるので、
// 下回りを剛体に替えても、ここを通す限り乗り味は変わらない。
//
// ■ 書き戻し
// 見た目・音・通信・エフェクトは m_pos / m_vel / m_yaw を見ている。
// そこを剛体が埋めれば、下回りを入れ替えても上は動く。
// 姿勢だけは3つの角では表せないので、通信で使っている
// クォータニオンの経路をそのまま借りる
//----------------------------------------------------------
void CarBase::UpdateRigid(float dt)
{
	// 調整値を渡す。切り替えた時だけだと、調整しても走りが変わらない
	ApplyRigidSetup();

	const DriveInput in = m_lastInput;

	const float throttle      = in.throttle;
	const float steerInput    = in.steer;
	const bool  handbrake     = in.handbrake;
	const bool  clutchPressed = in.clutch;

	//===== 進行方向に対する前後・横の速さ =====
	// 舵も駆動も後退判定もここを見る。
	// 旧モデルと同じ意味の値でないと、アシストが噛み合わない
	m_yaw = m_rigid.Yaw();

	const Math::Vector3 fwd0(sinf(m_yaw), 0.0f, cosf(m_yaw));
	const Math::Vector3 right0(cosf(m_yaw), 0.0f, -sinf(m_yaw));

	Math::Vector3 vFlat = m_rigid.Vel();
	vFlat.y = 0.0f;

	const float vLong0   = vFlat.Dot(fwd0);
	const float vLat0    = vFlat.Dot(right0);
	const float speedNow = vFlat.Length();

	// アシストは m_vel / m_yawRate を見て向きを決める。
	// 映してから呼ばないと、前フレームの値で判断する
	m_vel     = vFlat;
	m_yawRate = m_rigid.YawRate();

	//===== 舵 =====
	// オートカウンターを含む。結果は m_steer に入る
	UpdateSteerAngle(dt, steerInput, throttle, handbrake, vLong0, vLat0, speedNow);

	//===== 振り返しの後押し =====
	m_rigid.AddYawRate(TransitionYawBoost(steerInput, vLong0, vLat0) * dt);

	//===== 駆動系 =====
	// 駆動輪の回転(m_driveSpeed)は、エンジン側とタイヤ側の両方が触る。
	// クラッチはこちらが、路面からの反力は剛体が解くので、
	// 行き来させないと閉じない
	m_driveSpeed = m_rigid.DriveSpeed();
	m_driveDiff  = m_rigid.DriveDiff();

	UpdateDriveline(dt, throttle, handbrake, clutchPressed,
	                in.shiftUp, in.shiftDown, vLong0);

	UpdateReverseGear(dt, throttle, handbrake, vLong0);

	m_rigid.SetDrive(m_driveSpeed, m_driveDiff);

	//===== 剛体を進める =====
	HjCarRigid::Input ri;
	ri.throttle     = throttle;
	ri.steer        = m_steer;
	ri.handbrake    = handbrake;
	ri.driveAccel   = m_driveAccel;
	ri.reverse      = m_reverse;
	ri.accelPressed = m_reverse ? (throttle < 0.0f) : (throttle > 0.0f);
	ri.clutchOut    = handbrake || clutchPressed;

	m_rigid.Step(ri, dt);

	m_driveSpeed = m_rigid.DriveSpeed();
	m_driveDiff  = m_rigid.DriveDiff();

	//===== スピン防止 =====
	// 旧モデルはタイヤ力の刻みごとに掛けていた。
	// こちらは1フレームに1回なので、同じ割合を dt で掛ける
	{
		const float rate = SpinAssistRate(steerInput, handbrake,
		                                  vLong0, vLat0, m_rigid.YawRate());

		if (rate > 0.0f) { m_rigid.DampYaw(std::min(rate * dt, 1.0f)); }
	}

	//===== 転倒からの復帰 =====
	// ひっくり返ったまま動けなくなるので、一定時間で戻す
	if (m_rigid.ConsumeNeedReset())
	{
		// 位置は接地面の高さで渡す約束。
		// Pos() は重心なので、そのまま渡すと戻すたびに
		// 重心の高さぶん浮き上がっていく
		m_rigid.Place(m_rigid.Pos() - Math::Vector3::Up * RigidCarConst::CgHeight,
		              m_rigid.Yaw());
	}

	//===== 結果を旧モデルの変数へ映す =====
	// 位置は接地面の高さで持つ。旧モデルと揃えないと、
	// カメラもエフェクトも車体の中へ潜る
	m_pos = m_rigid.Pos() - Math::Vector3::Up * RigidCarConst::CgHeight;

	m_vel   = m_rigid.Vel();
	m_velY  = m_vel.y;
	m_vel.y = 0.0f;

	m_yaw      = m_rigid.Yaw();
	m_yawRate  = m_rigid.YawRate();

	m_onGround = (m_rigid.GroundedCount() > 0);
	m_airborne = !m_onGround;

	//===== 空中のエアコントロール =====
	// 舵で機首のヨーだけ調整して着地姿勢を作れる
	if (m_airborne)
	{
		m_rigid.AddYawRate(steerInput * CarConst::AirSteerControl * dt);
		m_rigid.DampYaw(std::min(CarConst::AirYawDamp * dt, 1.0f));
	}

	//===== 車体アライン =====
	// 中で剛体のヨーを回す。映したあとでないと向きがずれる
	{
		const Math::Vector3 fwdEnd(sinf(m_yaw), 0.0f, cosf(m_yaw));
		UpdateBodyAlignAssist(dt, m_vel.Dot(fwdEnd), handbrake);
	}

	// アシストでヨーが動いたので取り直す
	m_yaw     = m_rigid.Yaw();
	m_yawRate = m_rigid.YawRate();

	// 姿勢はクォータニオンで渡す。
	// 3つの角では、転倒した姿勢を表せない
	ApplyVisualRotation(m_rigid.Rot());

	//===== 車輪の見た目 =====
	// 前輪は路面速度で転がる。後輪は駆動輪の接地面速度で回る＝
	// アクセル空転で速く回り、サイド中は0でロック
	{
		const Math::Vector3 fwdEnd(sinf(m_yaw), 0.0f, cosf(m_yaw));

		const float radius   = std::max(m_wheelH, 0.01f);
		const float vLongEnd = m_vel.Dot(fwdEnd);

		const float twoPi = 6.2831853f;

		auto wrap = [&](float& a)
		{
			if (a >  twoPi) { a -= twoPi; }
			if (a < -twoPi) { a += twoPi; }
		};

		m_wheelSpinFront += (vLongEnd / radius) * dt;
		m_wheelSpinRear  += (m_driveSpeed / radius) * dt;

		wrap(m_wheelSpinFront);
		wrap(m_wheelSpinRear);
	}

	//===== 見た目と音 =====
	const float speed = m_vel.Length();

	m_slipRear01  = m_rigid.SlipRear();
	m_slipFront01 = m_rigid.SlipFront();

	EmitTireFx(dt, speed, m_slipRear01, m_slipFront01, m_onGround);

	//===== 音 =====
	// 旧モデルでは UpdateMotionFeedback が鳴らしているが、
	// 剛体はそこを通らないので無音になっていた。
	// 音が無いと、段が変わったことも耳で分からない
	UpdateEngineAudio(dt, throttle);

	// ブレーキ鳴きは前進中にSを踏んでいる量。
	// 後退中のSは駆動なので鳴らさない
	const float brake01 = (!m_reverse && throttle < 0.0f) ? fabsf(throttle) : 0.0f;

	m_tireAudio.Update(dt, m_slipRear01, speed, brake01, m_onGround);

	PlaceAudio();

	UpdateEffectParticles(dt);
}

//----------------------------------------------------------
// 位置を直に置く
//
// 枠組みの SetPos はワールド行列へ書くだけなので、
// 車が実際に使う m_pos には届かない。
//
// 剛体で走らせているときは、そちらの重心も合わせる。
// 合わせないと、次のフレームに剛体の位置へ引き戻される
//----------------------------------------------------------
void CarBase::SetPos(const Math::Vector3& pos)
{
	m_pos = pos;

	if (m_useRigid)
	{
		// 剛体は重心の高さで持っている
		m_rigid.Place(pos, m_yaw);
	}

	// 枠組み側も揃えておく。
	// 当たり判定の相手としてワールド行列を見る所がある
	KdGameObject::SetPos(pos);
}
