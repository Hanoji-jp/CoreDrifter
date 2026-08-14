#pragma once

#include "../../Const/CarConst.h"        // 既定値(規約上constヘッダはinclude可)
#include "../../Const/AlignmentConst.h"  // アライメント/サスセッティング既定値
#include "../Effect/DriftSmoke.h"        // ドリフトスモーク(後輪の煙)
#include "../Effect/DriftNeon.h"         // タイヤ周りのネオン線画(Unbound風)
#include "../Effect/SkidMark.h"          // 路面に残るタイヤ痕
#include "../../Input/HjGamePad.h"       // コントローラー入力(XInput)
#include "../../Audio/HjEngineAudio.h"   // エンジン音(点火グレインの合成)

//==========================================================
// CarBase
//   アーケード・FRドリフト車両の共通基底。
//   挙動(物理)・描画(車体+4輪)・調整パネルを持つ。
//   各車種は CarBase を継承し、コンストラクタで
//   「モデルパス」「性能ステータス」「タイヤ配置」を設定する。
//
//   操作: W=前進 / S=ブレーキ・後退 / A,D=ステア / Space=ハンドブレーキ
//==========================================================
class CarBase : public KdGameObject
{
public:
	void Init()    override;
	void Update()  override;
	// 増えたタイヤ痕を焼き付けマップへ書き込む。
	// レンダーターゲットを差し替えるので、シーンの描画パスに入る前に行う必要がある。
	void PreDraw() override;
	void DrawLit() override;
	void DrawEffect() override;        // ドリフトスモーク(UnLitパス)
	void DrawOverlayEffect() override; // ネオン線画(煙の輪郭処理を通さず加算合成で重ねる)
	void DrawBright() override;   // ネオンのグロー(ポストプロセスでぼかされて光が滲む)

	// 車はオブジェクト単位の視錐台カリングの対象外。
	// 車体そのものは小さいが、煙・タイヤ痕・ネオンは車から遠く離れた位置まで
	// 広がっており、車が画面外に出た瞬間にそれらが丸ごと消えてしまう。
	// (これらは各エフェクト側で粒・区間ごとにカリングしている)
	bool CheckInScreen(const DirectX::BoundingFrustum&) const override { return true; }
	void DrawSprite() override;   // HUD(スピード/RPM/ステア)
	void DrawDebug()  override;   // 当たり判定の可視化(F1でトグル)

	// 追従カメラ等から参照
	Math::Vector3 GetPos()     const override { return m_pos; }
	float         GetYaw()     const          { return m_yaw; }
	Math::Vector3 GetForward() const          { return Math::Vector3(sinf(m_yaw), 0.0f, cosf(m_yaw)); }
	Math::Vector3 GetVel()     const          { return m_vel; }   // ドリフトカメラ用(進行方向)

	// 当たり判定対象(地形など)を登録する。車はこれらへレイ/球判定を飛ばす。
	void AddCollisionTarget(const std::weak_ptr<KdGameObject>& obj) { m_wpHitList.push_back(obj); }

	// スポーン(初期配置)：位置・向きを与え、速度など運動状態をリセット。
	// 与えた値はリスポーン地点として記憶する。
	void SetSpawn(const Math::Vector3& pos, float yaw);
	// 記憶したスポーン地点へ戻す(Rキーのリスポーン)
	void Respawn() { SetSpawn(m_spawnPos, m_spawnYaw); }

	// 調整パネルを外部(シーン)から描画するための公開窓口
	void DrawImGui() { DrawTuningImGui(); }

	// ブースト(ニトロ)演出を発動：車体に一瞬だけアクセントカラーが乗り、
	// 同時にネオンの線画が全方向へ弾ける。将来ニトロ機能から呼ぶ。
	void TriggerBoost();

protected:
	//===== Update() の分割 =====
	// 1フレームの運転操作。キーボードとコントローラーを合成した結果。
	struct DriveInput
	{
		float throttle  = 0.0f;   // -1(ブレーキ/後退) 〜 +1(アクセル)
		float steer     = 0.0f;   // -1(左) 〜 +1(右)
		bool  handbrake = false;
		bool  clutch    = false;
		bool  shiftUp   = false;  // 押した瞬間のみ
		bool  shiftDown = false;
	};

	DriveInput ReadInput();        // 運転操作の読み取り(キーボード＋パッド)
	void UpdateDebugKeys();        // F1〜F4のデバッグトグル(運転とは無関係)

	// 車体アライン：アクセルオフで車体を進行方向へ寄せるアシスト。
	// 物理の積分結果(m_yaw)を直接書き換えるので、アシスト群として切り離してある。
	void UpdateBodyAlignAssist(float dt, float vLong0, bool handbrake);

	// サスペンションのロール/ピッチ。見た目だけで挙動には影響しない。
	void UpdateSuspensionVisual(float dt);

	// エンジン・ギア・クラッチ。結果は m_engineRPM / m_gear / m_clutch /
	// m_driveSpeed / m_driveAccel に入る。
	void UpdateDriveline(float dt, float throttle, bool handbrake,
	                     bool clutchPressed, bool shiftUp, bool shiftDown, float vLong0);

	// 4輪シミュレーション。各輪の荷重・スリップ角・摩擦円からタイヤ力を求め、
	// 車体の速度とヨーへ積分する。挙動の本体。
	void StepTireForces(float dt, float throttle, float steerInput, bool handbrake,
	                    bool clutchPressed, bool accelPressed);

	// 水平移動と壁の押し戻し(サブステップCCD＋リラクゼーション)
	void ResolveWallCollision(float dt);
	// 接地判定と車体の高さ・地形の傾きへの追従
	void UpdateGroundContact(float dt);

	// 物理の結果を見た目へ反映する。タイヤの回転と、走行状態に応じた
	// エフェクト(煙・ネオン・タイヤ痕)の放出。挙動には影響しない。
	void UpdateMotionFeedback(float dt, bool handbrake);

	void DrawTuningImGui();

	// 調整値の保存/読込（車種ごとのファイルへ）
	void SaveTuning();
	void LoadTuning();
	std::string TuneFilePath() const;
	std::vector<std::pair<const char*, float*>> TuneParamList();

	//===== 派生クラスがコンストラクタで設定する =====
	// モデル
	std::string m_bodyPath  = "Asset/Data/Box.gltf";
	std::string m_wheelPath = "Asset/Data/Box.gltf";
	std::string m_tuningName = "Car Tuning";   // 調整パネルのタイトル
	std::string m_saveKey    = "Car";          // 保存ファイルのキー(車種ごと)

	// 性能ステータス
	float m_enginePower     = CarConst::EnginePower;
	float m_brakePower      = CarConst::BrakePower;
	float m_maxSpeed        = CarConst::MaxSpeed;
	float m_drag            = CarConst::Drag;
	float m_scrubDrag       = CarConst::ScrubDrag;   // 横滑りスクラブ抵抗(ドリフト速度の抑制)
	float m_maxSteerAngle   = CarConst::MaxSteerAngle;
	float m_steerSpeed      = CarConst::SteerSpeed;
	float m_steerReturnMul  = CarConst::SteerReturnMul;   // 戻す/逆へ振る時の速さ倍率
	float m_counterRelease  = CarConst::CounterRelease;   // 逆に切った時カウンターを緩める量
	float m_turnRate        = CarConst::TurnRate;
	float m_turnRefSpeed    = CarConst::TurnRefSpeed;
	float m_yawResponse     = CarConst::YawResponse;
	float m_gripTraction    = CarConst::GripTraction;
	float m_driftTraction   = CarConst::DriftTraction;
	float m_throttleGripLoss = CarConst::ThrottleGripLoss;
	float m_minTraction     = CarConst::MinTraction;

	// CarX風スリップアングル・タイヤモデル
	float m_muFront   = CarConst::TireMuFront;
	float m_muRear    = CarConst::TireMuRear;
	float m_tireB     = CarConst::TireStiffB;
	float m_tireC     = CarConst::TireShapeC;
	float m_izz       = CarConst::YawInertia;
	float m_cgHeight  = CarConst::CgHeight;
	float m_rearGripThrottleLoss = CarConst::RearGripThrottleLoss;
	float m_handbrakeGripMul     = CarConst::HandbrakeGripMul;
	float m_slipEps   = CarConst::SlipSpeedEps;
	// CarX系の切り返しを決める3要素
	float m_tireLoadSens  = CarConst::TireLoadSens;      // 荷重感度(荷重が増えるほどμが下がる)
	float m_tireRelaxLen  = CarConst::TireRelaxLength;   // リラクゼーション長(m)
	float m_rollFreq      = CarConst::RollFreq;          // ロールの固有角周波数
	float m_rollDampRatio = CarConst::RollDampRatio;     // ロールの減衰比(1未満で行き過ぎる)
	// 路面の傾き・空力・駆動系
	float m_slopeGravity      = CarConst::SlopeGravity;       // 斜面の重力成分の倍率
	float m_downforceCoef     = CarConst::DownforceCoef;      // ダウンフォース係数
	float m_downforceRearBias = CarConst::DownforceRearBias;  // ダウンフォースの後ろ寄り配分
	float m_lsdLock           = CarConst::LsdLock;            // デフのロック強さ
	float m_bumpLoadGain      = CarConst::BumpLoadGain;       // 段差による荷重変化の強さ
	// ※ここにあった lowSpeedGrip / latSettle / handbrakeBrake / handbrakeYawDamp /
	//   spinRecover / driftRetain / handbrakeSlipEps は、ImGuiと保存には出ていたが
	//   物理側で一度も参照されていなかった(触っても何も起きない)ため削除した。
	//   必要になったら実装と一緒に追加すること。
	float m_yawDamp   = CarConst::YawDamp;

	// 駆動輪の縦スリップ(摩擦円：空転すると横グリップが減って流れる)
	float m_longStiff    = CarConst::LongStiff;
	float m_wheelInertia = CarConst::WheelInertia;
	float m_driveRelax   = CarConst::DriveRelax;

	// サスペンション(バネ・ダンパーで車体をロール/ピッチさせる。見た目)
	float m_suspStiff = CarConst::SuspStiffness;
	float m_suspDamp  = CarConst::SuspDamping;
	float m_rollGain  = CarConst::RollGain;
	float m_pitchGain = CarConst::PitchGain;
	float m_suspMax   = CarConst::SuspMaxAngle;
	float m_accelSmooth = CarConst::SuspAccelSmooth;

	//===== 運転アシスト =====
	// CarXにも同種の設定はあるが、あちらは「切っても物理が成立する」前提で
	// プレイヤーが自由にON/OFFできる。こちらも同じように個別に切れるようにする。
	// すべて切ると、タイヤと荷重だけで走る素の挙動になる。
	bool  m_transitionEnabled = true;   // 振り返しのヨー後押し(物理を経由しない外力)
	bool  m_bodyAlignEnabled  = true;   // アクセルオフで車体を進行方向へ回頭
	bool  m_scrubDragEnabled  = true;   // 横滑り速度の直接減衰(タイヤ力とは別口)

	// オートカウンター(CarX風ステアリングアシスト)
	bool  m_counterSteerEnabled = true;
	float m_counterAssist  = CarConst::CounterAssist;   // 横滑り角を打ち消す割合(0-1+)
	float m_counterMinSpeed = CarConst::CounterMinSpeed; // これ未満の速度では効かせない

	// スピン防止アシスト(スタビリティコントロール)
	bool  m_spinAssistEnabled = true;
	float m_spinAssist = CarConst::SpinAssistStrength;
	float m_handbrakeCounterMul = CarConst::HandbrakeCounterMul; // サイド中のカウンター倍率(1=通常)

	// ※gripRelax(グリップ変化の平滑化)と gripCatch(アクセルオフでグリップ復帰)も
	//   物理側で未参照だったため削除。前者の役割は m_tireRelaxLen が担っている。
	// 車体アライン(アクセルオフで車体を進行方向へ回頭＝角度を抜く。カニ歩き防止)
	float m_bodyAlign = CarConst::BodyAlign;
	// トランジション補助(振り返し。ドリフト中に切った方向へヨーを後押し)
	float m_transitionAssist = CarConst::TransitionAssist;

	// アライメント/サスセッティング(実挙動に効く。既定は中立=現状維持)
	float m_toeFront   = AlignmentConst::ToeFront;    // 前トー(rad, 正=トーイン)
	float m_toeRear    = AlignmentConst::ToeRear;     // 後トー
	float m_ackermann  = AlignmentConst::Ackermann;   // アッカーマン(-1〜1, 0=平行)
	float m_camberGrip = AlignmentConst::CamberGrip;  // キャンバーの横グリップ寄与(0=見た目のみ)
	float m_springF    = AlignmentConst::SpringFront; // 前ばね定数(相対)
	float m_springR    = AlignmentConst::SpringRear;  // 後ばね定数(相対)
	float m_arbF       = AlignmentConst::ArbFront;    // 前スタビ(相対)
	float m_arbR       = AlignmentConst::ArbRear;     // 後スタビ(相対)

	// 見た目(タイヤ配置・向き)
	float m_bodyScale  = CarConst::CarModelScale;
	float m_bodyYaw    = CarConst::CarModelYawOffset;
	float m_wheelScale = CarConst::WheelModelScale;
	float m_wheelYaw   = CarConst::WheelModelYawOffset;
	float m_track      = CarConst::WheelTrack;
	float m_base       = CarConst::WheelBase;
	float m_wheelH     = CarConst::WheelHeight;
	float m_camber     = CarConst::CamberAngle;
	// オフセット(全体 / 前輪 / 後輪 を個別に)
	float m_offX = 0.0f,      m_offZ = 0.0f;       // 4輪全体
	float m_frontOffX = 0.0f, m_frontOffZ = 0.0f;  // 前輪のみ
	float m_rearOffX = 0.0f,  m_rearOffZ = 0.0f;   // 後輪のみ

	// アウトライン(原神式・背面押し出しトゥーン輪郭)
	bool          m_outlineEnabled = true;
	float         m_outlineWidth   = 0.04f;
	Math::Vector3 m_outlineColor    = Math::Vector3(0.0f, 0.0f, 0.0f); // 黒

	// ドリフトスモークの色味(白=通常。NFS Unbound風のカラー煙にもできる)
	// 発生源から離れるほど 色A → 色B へ滑らかにグラデーションする
	Math::Vector3 m_smokeColor    = Math::Vector3(1.0f, 1.0f, 1.0f);  // 手前の色
	Math::Vector3 m_smokeColorB   = Math::Vector3(1.0f, 1.0f, 1.0f);  // 奥の色
	float         m_smokeGradDist = SmokeConst::SmokeGradDist;        // 色Bになりきる距離(m)
	Math::Vector3 m_smokeHiColor  = Math::Vector3(1.0f, 1.0f, 1.0f);  // ハイライトの色

	// ドリフト中に車体をアクセントカラーで塗る演出(NFS Unboundのドライビングエフェクト風)
	Math::Vector3 m_driftTintColor = Math::Vector3(0.78f, 1.0f, 0.16f); // 塗る色
	// 発光中の輪郭の色。車体の塗りとは別に指定できる
	// (縁だけ違う色で光らせたい場合があるため、アクセントカラーとは分ける)
	Math::Vector3 m_boostOutlineColor = Math::Vector3(1.0f, 1.0f, 1.0f);
	float         m_driftTintMax   = 1.0f;   // 最大の塗り具合(0=無効 1=完全に塗り潰す)
	float         m_driftTintSlipDeg = 35.0f;// この横滑り角(度)で塗りが最大になる
	// ネオン線画・粒の色(2色。粒ごとにAとBを混色して散らす)
	Math::Vector3 m_neonColorA = Math::Vector3(NeonFxConst::ColorAR, NeonFxConst::ColorAG, NeonFxConst::ColorAB);
	Math::Vector3 m_neonColorB = Math::Vector3(NeonFxConst::ColorBR, NeonFxConst::ColorBG, NeonFxConst::ColorBB);


private:
	// モデル
	KdModelWork m_body;
	KdModelWork m_wheel;

	// ランタイム状態
	Math::Vector3 m_pos = Math::Vector3::Zero;
	Math::Vector3 m_vel = Math::Vector3::Zero;
	Math::Vector3 m_spawnPos = Math::Vector3::Zero;   // リスポーン地点(SetSpawnで記憶)
	float         m_spawnYaw = 0.0f;
	float         m_yaw     = 0.0f;
	float         m_yawRate = 0.0f;   // ヨー角速度(rad/s)
	float         m_mzFilt  = 0.0f;   // 平滑化したヨーモーメント(タイヤリラクゼーション)
	float         m_steer       = 0.0f;
	float         m_playerSteer = 0.0f;   // プレイヤー入力ぶんの舵角(平滑化)
	float         m_hbCounterFactor = 1.0f; // サイド中カウンター倍率の平滑化(段差カクつき防止)
	float         m_liftCounterFactor = 1.0f; // アクセルオフ時カウンター減衰の平滑化(急スリップ防止)
	float         m_wheelSpinFront = 0.0f;   // 前輪の転がり角(rad)
	float         m_wheelSpinRear  = 0.0f;   // 後輪の転がり角(rad, サイド中はロック)
	float         m_driveSpeed     = 0.0f;   // 駆動輪の接地面速度(m/s, 空転で路面速度を超える)

	// エンジン / ギア / クラッチ
	float         m_engineRPM = CarConst::IdleRPM;
	int           m_gear      = 1;
	float         m_clutch    = 1.0f;        // 1=接続 / 0=切断(サイドで自動的に切れる)
	float         m_driveAccel = 0.0f;       // 今フレームの駆動加速(クラッチ・トルク込み)
	bool          m_reverse    = false;      // 後退ギア(R)に入っているか

	// サスペンション状態(車体のロール/ピッチ)
	float         m_rollAngle  = 0.0f, m_rollVel  = 0.0f;
	float         m_pitchAngle = 0.0f, m_pitchVel = 0.0f;
	float         m_accelLong  = 0.0f, m_accelLat = 0.0f;   // 直近の車体座標加速度
	float         m_accelLongF = 0.0f, m_accelLatF = 0.0f;  // 平滑化した加速度(サス入力)
	float         m_dLongF = 0.0f, m_dLatF = 0.0f;          // 平滑化した荷重移動(急なリフトオフ防止)
	float         m_rollV = 0.0f;        // ロール(横荷重移動)の速度。ばね-ダンパの状態
	float         m_wheelFy[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // 各輪の横力(リラクゼーションで遅らせた値)
	float         m_bumpLoad[4] = { 0.0f, 0.0f, 0.0f, 0.0f };// 段差による各輪の荷重の偏り(-1〜1)
	float         m_driveDiff = 0.0f;    // 左右の駆動輪の回転差の半分(デフ)

	// ドリフトスモーク(後輪の煙)
	DriftSmoke    m_smoke;
	float         m_smokeCarry = 0.0f;   // 放出数の端数を蓄積(毎秒レート→整数枚)
	float         m_smokeCarryFront = 0.0f;   // 同・前輪ぶん(こちらはごく少量)
	float         m_driftTint  = 0.0f;   // 車体のアクセントカラー塗り(0〜1, 平滑化済み)

	// タイヤ周りのネオン線画(Unbound風。リング＋スパーク)
	DriftNeon     m_neon;
	float         m_neonRingCarry  = 0.0f;   // 放出数の端数(リング)
	float         m_neonSparkCarry = 0.0f;   // 放出数の端数(スパーク)

	// 路面に残るタイヤ痕。マップはコースに1枚の共有(SkidMark::Instance)で、
	// ここに持つのは自車ぶんの枠の先頭番号だけ。
	int m_skidBase = -1;

	// コントローラー入力(接続時のみアナログ操作を反映)
	HjGamePad     m_pad;

	// エンジン音。RPMとアクセル開度から波形を組み立てて鳴らす
	HjEngineAudio m_engineAudio;

	// マニュアルシフトのキーボード用エッジ検出(押した瞬間だけ1段送る)
	bool          m_prevKeyShiftUp   = false;
	bool          m_prevKeyShiftDown = false;

	// 当たり判定：地形などの対象(接地レイ・壁球を飛ばす相手)
	std::vector<std::weak_ptr<KdGameObject>> m_wpHitList;
	Math::Vector3 m_groundNormal = Math::Vector3::Up;   // 接地面の法線(後で重力相対に使う)
	bool          m_onGround      = false;              // 今フレーム接地したか
	float         m_terrainPitch  = 0.0f;              // 地形の前後傾き(rad, 4輪レイから推定)
	float         m_terrainRoll   = 0.0f;              // 地形の左右傾き(rad, 4輪レイから推定)

	// ジャンプ/滞空の手触り調整(ImGuiで生調整可)
	float         m_airGravityMul  = CarConst::AirGravityMul; // 滞空重力倍率(大=ズシッと速い/小=フワッと)
	float         m_jumpLaunch     = 1.0f;                    // ランプの打ち上げ強さ倍率
	float         m_landBounce     = CarConst::LandBounce;    // 着地の跳ね返り

	// ジャンプ/滞空(エビス風ジャンプドリフト)：垂直速度・重力・着地を扱う
	bool          m_airborne       = false;   // 滞空中(タイヤ力なし=横向き/スピンを保持して飛ぶ)
	float         m_velY           = 0.0f;    // 垂直速度(m/s, 上+)
	float         m_groundYFilt    = 0.0f;    // 支持面の高さ(低域通過。メッシュ継ぎ目のガタつき除去)
	float         m_prevGroundY    = 0.0f;    // 前フレームの支持面高さ(上昇速度の算出用)
	float         m_supportVelY    = 0.0f;    // 支持面の上昇速度(平滑化。クレストで打ち上がる勢い)
	bool          m_prevGroundValid = false;  // 前フレームに支持面高さが有効だったか
	// 空中の剛体回転(角運動量)：ランプで付いた回転を空中で保持し空力で弾道へ収束
	float         m_pitchRate      = 0.0f;    // ピッチ角速度(rad/s)
	float         m_rollRate       = 0.0f;    // ロール角速度(rad/s)

	// 当たり判定の可視化(F1トグル)：壁プローブ球＋接地レイをワイヤ表示
	bool          m_debugDraw    = false;
	bool          m_prevDebugKey = false;              // F1のエッジ検出
	bool          m_prevOutlineKey = false;            // F2(煙輪郭トグル)のエッジ検出
	float         m_boostFlash    = -1.0f;            // ブースト演出の経過秒(負=発動していない)
	bool          m_prevTintKey   = false;            // F4のエッジ検出
	bool          m_prevStyleKey   = false;            // F3(文字エフェクト切替)のエッジ検出
};
