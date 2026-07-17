#pragma once

#include "../../Const/CarConst.h"        // 既定値(規約上constヘッダはinclude可)
#include "../../Const/AlignmentConst.h"  // アライメント/サスセッティング既定値
#include "../Effect/DriftSmoke.h"        // ドリフトスモーク(後輪の煙)
#include "../../Input/HjGamePad.h"       // コントローラー入力(XInput)

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
	void DrawLit() override;
	void DrawEffect() override;   // ドリフトスモーク(UnLitパス)
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

protected:
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
	float m_lowSpeedGrip = CarConst::LowSpeedGrip;   // 停止付近でタイヤ横力をフェード
	float m_latSettle    = CarConst::LowLatSettle;   // 低速で横滑り速度を吸収
	float m_handbrakeBrake = CarConst::HandbrakeBrake; // サイド中の常時制動
	float m_handbrakeYawDamp = CarConst::HandbrakeYawDamp; // サイド中のヨー減衰(回りすぎ防止)
	float m_spinRecover  = CarConst::SpinRecover;    // 横向き移動(スピン)の収束
	float m_driftRetain  = CarConst::DriftRetain;    // ドリフト中の速度維持(スクラブ還元)
	float m_handbrakeSlipEps = CarConst::HandbrakeSlipEps;
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

	// オートカウンター(CarX風ステアリングアシスト)
	bool  m_counterSteerEnabled = true;
	float m_counterAssist  = CarConst::CounterAssist;   // 横滑り角を打ち消す割合(0-1+)
	float m_counterMinSpeed = CarConst::CounterMinSpeed; // これ未満の速度では効かせない

	// スピン防止アシスト(スタビリティコントロール)
	bool  m_spinAssistEnabled = true;
	float m_spinAssist = CarConst::SpinAssistStrength;
	float m_handbrakeCounterMul = CarConst::HandbrakeCounterMul; // サイド中のカウンター倍率(1=通常)

	// タイヤ・リラクゼーション(グリップ変化の平滑化)
	float m_gripRelax = CarConst::GripRelax;
	// グリップキャッチ(アクセルオフで入力方向へグリップ復帰)
	float m_gripCatch = CarConst::GripCatch;
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
	Math::Vector3 m_smokeColor = Math::Vector3(1.0f, 1.0f, 1.0f);


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

	// ドリフトスモーク(後輪の煙)
	DriftSmoke    m_smoke;
	float         m_smokeCarry = 0.0f;   // 放出数の端数を蓄積(毎秒レート→整数枚)

	// コントローラー入力(接続時のみアナログ操作を反映)
	HjGamePad     m_pad;

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
	bool          m_prevStyleKey   = false;            // F3(文字エフェクト切替)のエッジ検出
};
