#pragma once

// 車両挙動(アーケード・FRドリフト)と仮表示の定数
namespace CarConst
{
	//===== エンジン / 駆動(後輪駆動) =====
	constexpr float EnginePower   = 22.0f;   // 前進加速度
	constexpr float BrakePower    = 30.0f;   // ブレーキ / 後退の力
	constexpr float MaxSpeed      = 45.0f;   // 最高速度
	constexpr float Drag          = 0.12f;   // 転がり/空気抵抗(速度比例の減速係数 1/s)

	//===== ステアリング =====
	constexpr float MaxSteerAngle = 0.55f;   // 前輪の最大切れ角(rad)
	constexpr float SteerSpeed    = 8.0f;    // ステア入力の追従速度
	constexpr float TurnRate      = 2.6f;    // 車体ヨー角速度(rad/s, 最大切れ角時)
	constexpr float TurnRefSpeed  = 8.0f;    // フル操舵が効き始める速度
	constexpr float YawResponse   = 7.0f;    // ヨー角速度が目標に追従する速さ

	//===== グリップ / ドリフト(旧アーケードモデル・未使用) =====
	constexpr float GripTraction     = 5.5f; // 通常グリップ(速度を車体前方へ寄せる強さ)
	constexpr float DriftTraction    = 1.2f; // ハンドブレーキ時のグリップ(低い=滑る)
	constexpr float ThrottleGripLoss = 2.0f; // アクセルによる後輪グリップ低下(FRらしさ)
	constexpr float MinTraction      = 0.6f; // グリップ下限

	//===== 物理(CarX風スリップアングル・タイヤモデル) =====
	constexpr float TireMuFront   = 1.15f;  // 前輪の摩擦係数(最大横G)
	constexpr float TireMuRear    = 1.05f;  // 後輪の摩擦係数
	constexpr float TireStiffB    = 9.0f;   // 魔法式係数B(コーナリング剛性=切れ味)
	constexpr float TireShapeC    = 1.45f;  // 魔法式係数C(カーブ形状)
	constexpr float YawInertia    = 1.7f;   // ヨー慣性(小さいほどクイックに向き変わる)
	constexpr float CgHeight      = 0.55f;  // 重心高(荷重移動の強さ)
	constexpr float RearGripThrottleLoss = 0.55f; // アクセルで後輪グリップ低下(0-1)
	constexpr float HandbrakeGripMul     = 0.50f; // サイド時の後輪グリップ倍率(ロックのスクラブ摩擦。滑って自然に減速)
	constexpr float HandbrakeSlipEps     = 0.5f;  // サイド中のスリップ分母(小=低速でも滑る)
	constexpr float HandbrakeStopSpeed   = 6.0f;  // サイド+アクセルオフでこの速度以下は停止アシスト(回転/横滑り収束)
	constexpr float HandbrakeYawDamp     = 2.2f;  // サイド中のヨー減衰(1/s, 回りっぱなし防止。大=すぐ収まる)
	constexpr float HandbrakeBrake       = 0.5f;  // サイド中の常時制動(1/s, 引けば減速して止まる)
	constexpr float SlipSpeedEps  = 2.2f;   // 低速時スリップ角の安定化(分母下限)
	constexpr float LowSpeedGrip  = 1.5f;   // この速度以下で前輪横力をフェード(停止中は旋回不可)
	constexpr float LowLatSettle  = 12.0f;  // 低速時に横滑り速度を吸収する強さ(止まり際の横流れ防止)
	constexpr float SpinRecover   = 6.0f;   // 横向き移動(スピン)時に横速度と回転を収束させる強さ(全輪スクラブ)
	constexpr float DriftRetain   = 0.6f;   // ドリフト中に横スクラブで失う速度を進行方向へ戻す割合(0-1, 止まらない)
	constexpr float YawDamp       = 0.6f;   // ヨー角速度の減衰(1/s, スピン収束)
	constexpr float Gravity       = 9.81f;
	constexpr int   PhysicsSubsteps = 4;    // 物理サブステップ数(安定化)

	//===== 駆動輪の縦スリップ(空転/ロック=摩擦円で横グリップを食う) =====
	constexpr float LongStiff     = 22.0f;  // 縦タイヤ剛性(大=空転が収束しやすい)
	constexpr float WheelInertia  = 0.8f;   // 駆動輪の慣性(小=空転しやすい=低速で滑れる)
	constexpr float DriveRelax    = 6.0f;   // アクセルオフ時に路面速度へ戻る速さ

	//===== サスペンション(バネ・ダンパーで車体をロール/ピッチ。見た目) =====
	constexpr float SuspStiffness = 30.0f;  // バネ定数(大=硬い/戻り速い)
	constexpr float SuspDamping   = 18.0f;  // ダンパー(大=揺れが早く収まる。臨界以上=跳ねない)
	constexpr float RollGain      = 0.012f; // 横G→ロール角の係数(符号でリーン向き反転)
	constexpr float PitchGain     = 0.007f; // 前後G→ピッチ角の係数
	constexpr float SuspMaxAngle  = 0.20f;  // ロール/ピッチの最大角(rad, 約11度)
	constexpr float SuspAccelSmooth = 6.0f; // 加速度入力の平滑化(小=なめらか/鈍い)

	//===== オートカウンター(CarX風ステアリングアシスト) =====
	constexpr float CounterAssist   = 0.85f; // 横滑り角を前輪で打ち消す割合(1=完全カウンター)
	constexpr float CounterMinSpeed = 3.0f;  // これ未満の速度ではアシストを効かせない

	//===== スピン防止アシスト(スタビリティコントロール) =====
	constexpr float SpinAssistThreshold = 0.45f; // この横滑り角(rad,約26度)を超えたらヨーを抑える
	constexpr float SpinAssistStrength  = 5.0f;  // 抑える強さ(大=スピンしにくい)
	constexpr float HandbrakeCounterMul = 0.5f;  // サイド中のオートカウンター倍率(1=通常, 小=サイドで流しやすい)

	//===== タイヤ・リラクゼーション(グリップ変化を滑らかに) =====
	constexpr float GripRelax = 14.0f;  // ヨー応答の平滑化(1/s, 小=なめらか/遅い, 大=即応/カクつく)

	//===== グリップキャッチ(アクセルオフで入力方向へグリップ復帰) =====
	constexpr float GripCatch = 6.0f;   // アクセルオフ時に速度をタイヤの向きへ寄せる速さ(0=OFF)

	//===== 車体アライン(アクセルオフで車体の向きを進行方向へ寄せる＝角度を抜く) =====
	constexpr float BodyAlign = 2.5f;   // アクセルオフ時に車体を進行方向へ回頭する速さ(0=OFF, カニ歩き防止)

	//===== トランジション補助(振り返し) =====
	constexpr float TransitionAssist = 2.5f; // ドリフト中に切った方向へヨーを後押し(0=OFF, 振り返しのキレ)

	//===== 車両モデル(silvia_body) =====
	constexpr float CarModelScale     = 1.0f;               // ボディの表示スケール(要調整)
	constexpr float CarModelYawOffset = 3.14159265f;        // ボディの向き補正(前後が逆なので180度)

	//===== タイヤ(独立モデル Car_Wheel) =====
	constexpr float WheelModelScale     = 1.0f;    // タイヤモデルのスケール
	constexpr float WheelModelYawOffset = 0.0f;    // タイヤモデルの向き補正(必要なら)
	constexpr float WheelTrack          = 0.72f;   // 左右トレッドの半幅(中心から)
	constexpr float WheelBase           = 1.27f;   // 前後ホイールベース(中心から)
	constexpr float WheelHeight         = 0.24f;   // タイヤ中心の高さ
	constexpr float CamberAngle         = 0.10f;   // キャンバー角(rad, 左右で逆に付く)
	// 4輪全体のオフセット(車体の原点とズレてる分を合わせる。+X=右, +Z=前)
	constexpr float WheelCenterOffsetX  = 0.0f;
	constexpr float WheelCenterOffsetZ  = 0.0f;

	//===== 仮モデル(Box)の見た目 =====
	constexpr float BodyScaleX   = 0.9f;   // 車幅
	constexpr float BodyScaleY   = 0.5f;   // 車高
	constexpr float BodyScaleZ   = 2.0f;   // 車長
	constexpr float BodyOffsetY  = 0.5f;   // 車体を持ち上げる量
	constexpr float WheelScale   = 0.35f;  // タイヤの大きさ
	constexpr float WheelOffsetX = 0.85f;  // タイヤの左右位置
	constexpr float WheelOffsetZ = 1.35f;  // タイヤの前後位置
	constexpr float WheelOffsetY = 0.1f;   // タイヤの高さ

	//===== オービットカメラ(車の周りをマウスで回転) =====
	constexpr float CamDistance      = 8.0f;   // 初期の距離
	constexpr float CamLookAtOffsetY = 1.0f;   // 注視点の高さ
	constexpr float CamFollow        = 8.0f;   // 注視点(車)追従の滑らかさ
	constexpr float CamFov           = 60.0f;  // 視野角(度)
	constexpr float CamOrbitSensitivity = 0.006f; // マウス回転感度
	constexpr float CamZoomSpeed     = 1.2f;   // ホイールズーム速度
	constexpr float CamMinDistance   = 2.5f;   // 最小距離
	constexpr float CamMaxDistance   = 30.0f;  // 最大距離
	constexpr float CamPitchMin      = -0.2f;  // 下限(見下ろし側)
	constexpr float CamPitchMax      = 1.4f;   // 上限(真上近く)
	constexpr float CamInitPitch     = 0.35f;  // 初期の見下ろし角
	constexpr float CamInitYaw       = 3.14159265f; // 初期は車の後方から
	// ドリフトカメラ：進行方向の後ろから見る
	constexpr float CamDriftBias     = 0.6f;   // 進行方向を向く度合い(0=車の向き, 1=進行方向)
	constexpr float CamYawFollow     = 4.0f;   // カメラ向きの追従速度(1/s)
	constexpr float CamMinTravelSpeed = 2.0f;  // この速度以上で進行方向を採用(低速は車の向き)

	//===== エンジン / ギア / クラッチ =====
	constexpr float IdleRPM      = 900.0f;    // アイドル回転
	constexpr float MaxRPM       = 8000.0f;   // レブリミット
	constexpr float ShiftUpRPM   = 7000.0f;   // オートシフトアップ回転
	constexpr float ShiftDownRPM = 3000.0f;   // オートシフトダウン回転
	constexpr float ClutchSpeed  = 6.0f;      // クラッチ断続の速さ(1/s)
	constexpr float RevUp        = 7000.0f;   // クラッチ切断時アクセルで上がる回転(RPM/s)
	constexpr float RevDown      = 4500.0f;   // クラッチ切断時アクセルオフで下がる回転(RPM/s)
	constexpr float FinalDrive   = 3.9f;      // ファイナル(最終減速比)
	constexpr float RpmLinkSpeed = 9.0f;      // クラッチ接続時にエンジン回転が駆動系へ追従する速さ
	constexpr float RpmPerRadSec = 9.5493f;   // rad/s → RPM (60/2π)
	constexpr int   GearCount    = 5;         // 前進ギア段数
	// ギア比(index0は未使用。1速が一番大きい=トルク大/低速)
	constexpr float GearRatios[GearCount + 1] = { 0.0f, 3.20f, 2.00f, 1.40f, 1.05f, 0.82f };
	// トルクカーブ(中回転でピーク)：torque = 1 - k*(rpmN - peak)^2
	constexpr float TorquePeakN  = 0.55f;     // ピーク回転(0-1正規化)
	constexpr float TorqueFall   = 2.2f;      // ピークから外れたときの落ち具合
	constexpr float TorqueMin    = 0.35f;     // トルク下限

	//===== HUD(スピード/RPM/ステア表示) =====
	constexpr float HudRpmIdle     = 800.0f;    // アイドルRPM
	constexpr float HudRpmPerSpeed = 140.0f;    // 駆動輪速(m/s)→RPM係数(空転で跳ねる)
	constexpr float HudRpmMax      = 8000.0f;   // 最大RPM(レブ)
	constexpr float HudRedline     = 6800.0f;   // レッドゾーン開始RPM
	constexpr float HudMsToKmh     = 3.6f;      // m/s→km/h

	// レイアウト(画面中心原点, +x右/+y上, 1280x720)
	constexpr int HudLeft    = -610;   // HUD左端X
	constexpr int HudBaseY   = -210;   // 最下段のY
	constexpr int HudBarW    = 280;    // バーの幅
	constexpr int HudBarH    = 18;     // バーの高さ
	constexpr int HudRowGap  = 52;     // 段の縦間隔
	constexpr int HudTextDY  = 4;      // バー上のテキストのYオフセット

	//===== 地面(仮) =====
	constexpr float GroundScaleXZ = 100.0f;   // 地面の広さ
	constexpr float GroundScaleY  = 1.0f;     // 地面の厚み
}
