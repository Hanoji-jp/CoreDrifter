#pragma once

// 車両挙動(アーケード・FRドリフト)と仮表示の定数
namespace CarConst
{
	//===== エンジン / 駆動(後輪駆動) =====
	constexpr float EnginePower   = 22.0f;   // 前進加速度
	constexpr float BrakePower    = 30.0f;   // ブレーキ / 後退の力
	constexpr float MaxSpeed      = 45.0f;   // 最高速度

	//===== 後退(バックギア) =====
	constexpr float ReversePower       = 9.0f;   // 後退の駆動加速(前進より弱く)
	constexpr float MaxReverseSpeed    = 10.0f;  // 後退の最高速(m/s)
	// この「実際の速さ」(m/s)未満でSを踏み続けると後退ギアへ入る。
	//
	// ※車体前方の速度で見てはいけない。ドリフト中は車体が横を向くので
	//   前方成分だけが小さくなり、実際には高速で滑っているのに
	//   「止まっている」と誤判定する。そうなるとブレーキが後退駆動へ
	//   置き換わって、踏んでも止まらなくなる。
	constexpr float ReverseEngageSpeed = 1.5f;
	// 後退へ入るまでSを踏み続ける時間(秒)。
	// 一瞬でも条件を満たしたら入る作りだと、停止寸前のブレーキ中に
	// ギアが勝手に切り替わって制動が抜ける
	constexpr float ReverseEngageHold  = 0.30f;
	// 後退中にこの前進速度(m/s)を超えたら後退を解除する。
	// 入る条件と同じ値にすると、境目で行ったり来たりする
	constexpr float ReverseExitSpeed   = 2.0f;

	// ブレーキがこの速度(m/s)以下で弱まり始める。
	// 一定の力をかけ続けると、止まったあとに車が後ろへ這い出す
	constexpr float BrakeFadeSpeed     = 0.9f;
	constexpr float Drag          = 0.12f;   // 転がり/空気抵抗(速度比例の減速係数 1/s)
	constexpr float ScrubDrag     = 0.5f;    // 横滑りスクラブ抵抗(小=ツルツル滑る/ダート感。大=路面を削る/アスファルト感)

	//===== ステアリング =====
	constexpr float MaxSteerAngle = 0.55f;   // 前輪の最大切れ角(rad)
	// 舵が動く速さ。「最大切れ角の何倍を1秒で動けるか」。
	// 8なら端から端(最大切れ角の2倍)まで約0.25秒。
	//
	// ※1次遅れ(指数)で寄せてはいけない。
	//   最初だけ速くて後はじわじわ近づき、いつまでも目標に届かない。
	//   ゴムで引っ張られるような手ごたえになり、今どこまで切れているかが
	//   分からなくなる。実際のステアリングは一定の速さで動いて、
	//   目標に着いたらそこで止まる。
	constexpr float SteerSpeed    = 8.0f;
	// 舵を戻す/反対側へ振る時の追従速度の倍率。1.0で切り込みと同じ＝無効。
	constexpr float SteerReturnMul = 1.0f;

	//===== タイヤの実挙動(CarX系の切り返しはこの3つで決まる) =====

	// ① 荷重感度：実タイヤは荷重が増えるほど摩擦係数が下がる。
	//    これが無いと、荷重が左右へ移っても軸全体のグリップ合計が変わらないため、
	//    荷重移動が挙動にほとんど効かない＝振っても抜けない。
	//    0=無効(荷重に比例、従来通り) / 大きいほど荷重移動でグリップを失う。
	constexpr float TireLoadSens = 0.30f;

	// ② リラクゼーション長(m)：横力は舵を切った瞬間には立ち上がらず、
	//    タイヤがこの距離ぶん転がって初めて定常値に達する。
	//    この遅れが「振ってから食うまでの間」を作り、その隙に車が回る。
	//    実車のタイヤで0.3〜0.8m程度。小さいほど反応が鋭い。
	constexpr float TireRelaxLength = 0.55f;

	// ③ ロールのばね-ダンパ特性。1次遅れだと目標へ滑らかに寄るだけだが、
	//    実車は切り返しで反対側へ勢いよく倒れ込み、行き過ぎてから戻る。
	//    この行き過ぎ(オーバーシュート)が「振った瞬間に荷重が抜けて出る」感触になる。
	constexpr float RollFreq      = 9.0f;   // 固有角周波数(rad/s)。大きいほど機敏
	// 減衰比。1未満で行き過ぎる。低すぎると切り返しの後もロールが揺れ続け、
	// 荷重＝グリップが小刻みに変わってドリフト角が定まらなくなる。
	// 「振った時は行き過ぎるが、すぐ収まる」あたりが扱いやすい。
	constexpr float RollDampRatio = 0.78f;

	//===== 路面の傾き・空力・駆動系 =====

	// ④ 斜面の重力成分の倍率(1=物理どおり, 0=無効)。
	//    路面の傾きに沿って車を引く力。これが無いと下り坂で加速せず、
	//    登りで失速せず、バンクを使ったコーナリングも成立しない。
	//    峠が舞台なら挙動への影響が最も大きい要素。
	constexpr float SlopeGravity = 1.0f;

	// ⑤ ダウンフォース係数。荷重の増加量 = この値 × 速度^2。
	//    0.00035 なら 180km/h(50m/s)で荷重が約1.9倍。
	//    高速ほどグリップが増す＝速度域で手触りが変わる。
	constexpr float DownforceCoef = 0.00035f;
	// 前後配分(0.5=前後均等, 大きいほど後ろ寄り)。後ろ寄りだと高速で安定する
	constexpr float DownforceRearBias = 0.58f;

	// ⑥ デフ(LSD)のロック強さ(1/s)。左右の駆動輪の回転差を戻す速さ。
	//    大きいほど溶接デフ(左右直結)に近く、リアが一体で流れる＝ドリフト向き。
	//    小さいとオープンデフで、内輪が空転して前へ進まなくなる。
	constexpr float LsdLock = 12.0f;

	// ⑦ 段差による荷重変化の強さ。各輪の路面高さのばらつきをサスの縮みとみなす。
	//    解析的な荷重移動だけだと、縁石や轍を踏んでも荷重が一切動かない。
	//    ただし路面はメッシュなので、輪ごとにレイが当たる三角形が切り替わるたび
	//    接地高さが小刻みに動く。それをそのまま荷重にすると、グリップが常時
	//    揺れてドリフト角が定まらず、微調整が効かなくなる。
	//    ・不感帯でメッシュ由来の細かいガタつきを捨てる
	//    ・追従を遅くして本物の段差(縁石・轍)だけが残るようにする
	constexpr float BumpLoadGain   = 0.12f;  // 0=無効
	constexpr float SuspTravel     = 0.12f;  // サスのストローク(m)。これで割って正規化
	constexpr float BumpLoadSmooth = 6.0f;   // 荷重変化の追従速度(1/s)。小さいほど滑らか
	constexpr float BumpDeadzone   = 0.20f;  // これ未満のばらつきは路面ノイズとして無視(0〜1)
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
	// カウンターと逆へ舵を入れた時、アシストを緩める量(0=緩めない, 1=完全に抜く)
	constexpr float CounterRelease  = 0.0f;
	constexpr float CounterMinSpeed = 3.0f;  // これ未満の速度ではアシストを効かせない

	// オートカウンターが動ける速さ(最大切れ角の何倍/秒)。
	//
	// カウンターの元になる横滑り角は、速度から毎フレーム計算した生の値。
	// 段差・タイヤの緩和・摩擦円の頭打ちで細かく震えるので、
	// そのまま舵へ入れると見た目のタイヤがカクカク動く。
	// プレイヤーの舵と同じく動ける量に上限を付けて、震えを均す。
	//
	// プレイヤーより速く設定すること。アシストの役目はスライドを
	// 素早く捕まえることなので、遅くすると当て舵が間に合わなくなる。
	constexpr float CounterRate = 14.0f;

	//===== スピン防止アシスト(スタビリティコントロール) =====
	constexpr float SpinAssistThreshold = 0.45f; // この横滑り角(rad,約26度)を超えたらヨーを抑える
	constexpr float SpinAssistStrength  = 5.0f;  // 抑える強さ(大=スピンしにくい)
	// 回っている向きへこれ以上舵を当てていたら「意図した回転」とみなす。
	// ドリフト中はカウンター(回転と逆)を当てているので誤検出しない。
	// ドーナツや360度は回転と同じ向きへ舵を入れ続けるので、そこで区別できる。
	constexpr float SpinIntentSteer = 0.25f;
	constexpr float HandbrakeCounterMul = 0.5f;  // サイド中のオートカウンター倍率(1=通常, 小=サイドで流しやすい)

	//===== タイヤ・リラクゼーション(グリップ変化を滑らかに) =====
	constexpr float GripRelax = 14.0f;  // ヨー応答の平滑化(1/s, 小=なめらか/遅い, 大=即応/カクつく)

	//===== グリップキャッチ(アクセルオフで入力方向へグリップ復帰) =====
	constexpr float GripCatch = 6.0f;   // アクセルオフ時に速度をタイヤの向きへ寄せる速さ(0=OFF)

	//===== 車体アライン(アクセルオフで車体の向きを進行方向へ寄せる＝角度を抜く) =====
	constexpr float BodyAlign = 2.5f;   // アクセルオフ時に車体を進行方向へ回頭する速さ(0=OFF, カニ歩き防止)

	//===== トランジション補助(振り返し) =====
	constexpr float TransitionAssist = 2.5f; // ドリフト中に切った方向へヨーを後押し(0=OFF, 振り返しのキレ)
	// 今の回転と逆へ振った時だけ掛ける追加倍率。1.0で無効。
	constexpr float TransitionFlickMul = 1.0f;
	// 補助が効き始める横滑り角(rad)と、効き切るまでの幅
	constexpr float TransitionSlipMin  = 0.15f;
	constexpr float TransitionSlipBand = 0.01f;

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
	// 初期の距離。近いほど速度感が出て、車の姿勢の変化も読み取りやすい。
	// FOVが速度とドリフトで開く(下のGain)ぶん、映る範囲は走行中さらに広がる。
	// そのぶんも見込んで基準は控えめに取る。
	constexpr float CamDistance      = 5.8f;
	constexpr float CamLookAtOffsetY = 1.0f;   // 注視点の高さ
	constexpr float CamFollow        = 8.0f;   // 注視点(車)追従の滑らかさ
	constexpr float CamFov           = 60.0f;  // 視野角(度)
	// 近クリップ距離。既定の0.01は近すぎて、遠クリップ2000との比が20万:1になり、
	// 深度バッファ(24bit・非線形)の精度がほとんど近距離側へ食われてしまう。
	// 結果、空のような遠景でDoF/フォグが深度を読むと、量子化の段差が
	// 同心円状の縞(バンディング)として見える。
	// 車のカメラは数m以内に何かが描かれることが無いので、近クリップを
	// 大きく取っても支障が無く、遠距離側の精度を大きく稼げる。
	constexpr float CamNearClip      = 0.5f;
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
	// 動的カメラ(演出④：速度でFOVが開く・ドリフトで寄る)
	// FOVを開くと視野が広がる＝車が小さく遠くに見える。
	// 距離を詰めてもここが大きいと引いた印象が戻ってしまう。
	constexpr float CamFovSpeedGain  = 12.0f;  // 最高速で開くFOV量(度)
	constexpr float CamFovDriftGain  = 10.0f;  // ドリフト最大で開くFOV量(度)
	constexpr float CamDriftPull     = 1.3f;   // ドリフト時に寄る距離(m)
	constexpr float CamDynSmooth     = 5.0f;   // FOV/距離の追従速度(1/s)
	constexpr float CamDriftSlipDeg  = 40.0f;  // この横滑り角(度)でドリフト演出フル
	// ドリフト方向へカメラを傾ける(視線軸まわりのロール)。スライドに持っていかれる臨場感が出る。
	constexpr float CamDriftRoll = 0.22f;  // 横滑り角1radあたりのロール量(符号を反転すると傾く向きが逆)
	constexpr float CamMaxRoll   = 0.15f;  // ロールの上限(rad, ≈8.6度)
	// 注視点を進行方向へ前出しする距離(m)。車を画面中央に置くとドリフト中に
	// 進行方向が見えづらいので、行き先寄りに画面を振る。速度に比例して伸ばす。
	constexpr float CamLookAhead = 3.5f;

	//===== 被写界深度(DoF) =====
	// 手前(車・路面)は常にくっきり、遠くの背景だけ柔らかくぼかして奥行きを出す。
	// ぼかしすぎるとコース先が読めなくなり、ドリフトゲームとしては致命的なので、
	// 「効いているのが分かる程度」に留める。
	//
	// ※ForeRange/BackRangeは「そこまで届けば急にぼける」幅であって、
	//   「そこまでは絶対に鮮明」という意味ではない。ぼけの強さは
	//   1-(焦点との距離÷Range)^2 で決まり、距離がRangeに近づくほど
	//   なだらかにぼけていく。だからForeRangeを焦点距離と同じくらいの
	//   大きさにすると、焦点よりずっと手前にある車まで一緒にぼける
	//   (焦点との差がほぼForeRange幅いっぱいになるため)。
	//   手前側は絶対にぼかしたくないので、ForeRangeは焦点距離よりも
	//   一桁以上大きく取り、近距離側のぼけを事実上無効化する。
	//
	//   FocusDistance … ここが最も鮮明になる基準距離(m)
	//   ForeRange     … 手前側のぼけやすさ。大きいほど手前はぼけない
	//   BackRange     … 焦点より奥。この値ぶん奥から徐々にぼけ始め、
	//                    FocusDistance+BackRange×2あたりで最大にぼける
	constexpr float CamDofFocusDistance = 150.0f;
	constexpr float CamDofForeRange     = 100000.0f;  // 手前側は実質常に鮮明
	constexpr float CamDofBackRange     = 120.0f;      // 150m〜奥が徐々にぼける

	//===== エンジン / ギア / クラッチ =====
	constexpr float IdleRPM      = 900.0f;    // アイドル回転
	constexpr float MaxRPM       = 8000.0f;   // レブリミット
	constexpr float ShiftUpRPM   = 7000.0f;   // オートシフトアップ回転
	constexpr float ShiftDownRPM = 3000.0f;   // オートシフトダウン回転
	constexpr float ClutchSpeed  = 6.0f;      // クラッチ断続の速さ(1/s)
	// クラッチ切断時の空ぶかし。
	// ※一定の割合で上げ下げすると回転計が直線的に動き、機械仕掛けに見える。
	//   実機はクランクの角加速度が (トルク − 摩擦) ÷ 慣性 で決まり、
	//   摩擦は回転数の2乗におおむね比例する(ポンプ損失・かき混ぜ抵抗・油の粘性)。
	//   だから低回転では一気に吹け上がり、レッド手前で急激に鈍る。
	constexpr float RevUp        = 11000.0f;  // 全開・最大トルク時の上昇率(RPM/s)
	constexpr float RevDown      = 9000.0f;   // 摩擦による下降率の係数(RPM/s)
	// 摩擦。回転が上がるほど強くなる＝上は伸びず、下は速く落ちる
	constexpr float RevFrictionBase = 0.10f;  // アイドル付近でも掛かる分
	constexpr float RevFrictionRpm  = 1.15f;  // 回転の2乗で増える分
	constexpr float FinalDrive   = 3.9f;      // ファイナル(最終減速比)
	// ── 空転によるグリップ低下(滑り比の下り坂) ──
	// 摩擦円は「縦横をどう配分するか」しか決めない。
	// つまり空転しても、その輪が出せる力の総量は変わらないままになる。
	// 実タイヤは滑り比がピークを超えると摩擦係数そのものが落ちていくので、
	// 踏むほど後輪が失われ、旋回に使える力が減って半径が広がる。
	// これがドリフト中にアクセルで外へ膨らませる操作の正体で、
	// これが無いと踏んでも角度が変わるだけで線が広がらない。
	constexpr float SpinPeakSlip  = 0.18f;   // この滑り比までは摩擦が最大
	constexpr float SpinFallSlip  = 1.30f;   // ここまで滑ると落ち切る
	// 落ち切ったときに失う割合。大きすぎるとドリフト中に前へ進まなくなる
	constexpr float SpinGripFall  = 0.28f;

	// ── 空転の上限 ──
	// 完全に滑り切ったタイヤは、それ以上速く回しても駆動力が増えない。
	// 余った出力は熱とタイヤの摩耗になり、回転として蓄えられるわけではない。
	//
	// 上限が無いと、摩擦円で頭打ちになった時点で回転を止めるものが無くなり、
	// 駆動輪がはずみ車のように回転を溜め込む。そしてグリップが戻った瞬間に
	// それを一気に放出して、不自然な加速になる。
	constexpr float MaxSlipRatio = 0.55f;   // 路面速に対して何割まで多く回れるか
	constexpr float MaxSlipBase  = 4.0f;    // 停止からの発進ぶん(m/s)

	constexpr float RpmLinkSpeed = 9.0f;      // クラッチ接続時にエンジン回転が駆動系へ追従する速さ

	// クラッチが繋がる瞬間、回転差はクラッチが滑って埋める。
	// このとき「速い側が遅い側を引っ張る」向きを守ること。
	// エンジンの方が速ければエンジンが駆動輪を回す(＝ホイールスピン)。
	// 向きを取り違えると、サイドを離した瞬間に回転がアイドルまで落ちる。
	// サイド中は駆動輪をロックしていて、しかもドリフト中は車が横を向いていて
	// 前後速度が小さいので、駆動輪側の回転はほぼアイドル扱いになるため。
	constexpr float ClutchGrabSpeed = 7.0f;   // エンジンが駆動輪を引き上げる速さ(1/s)
	// エンジンが勝っているときの落ち方。滑っている間は少ししか落ちない
	constexpr float ClutchSlipDrop  = 0.22f;
	constexpr float RpmPerRadSec = 9.5493f;   // rad/s → RPM (60/2π)
	constexpr int   GearCount    = 5;         // 前進ギア段数
	// ギア比(index0は未使用。1速が一番大きい=トルク大/低速)
	constexpr float GearRatios[GearCount + 1] = { 0.0f, 3.20f, 2.00f, 1.40f, 1.05f, 0.82f };
	// 駆動力に掛けるギア比の基準(この比のとき enginePower がそのまま効く)。
	// 低いギア(比が大)ほど加速が強く、高いギアほど弱く=最高速寄りになる。2速基準。
	constexpr float DriveRefRatio = 2.00f;
	// トルクカーブ(中回転でピーク)：torque = 1 - k*(rpmN - peak)^2
	constexpr float TorquePeakN  = 0.55f;     // ピーク回転(0-1正規化)
	constexpr float TorqueFall   = 2.2f;      // ピークから外れたときの落ち具合
	constexpr float TorqueMin    = 0.35f;     // トルク下限
	// レブ手前でトルクを絞る＝ギアが「頭打ち」になり、上へ伸ばすにはシフトアップが要る。
	// これでギアごとの速度域がハッキリ分かれて体感できる。
	constexpr float RevCutStart  = 0.88f;     // この回転(0-1)からトルクが落ち始める
	constexpr float RevCutEnd    = 1.00f;     // レッド(ここでトルクほぼ0)

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

	//===== 接地・当たり判定(地形マップに乗せる) =====
	constexpr float GroundRayUp   = 3.0f;    // 接地レイの発射を車体からどれだけ上げるか(m)
	constexpr float GroundRayLen  = 60.0f;   // 接地レイの長さ(m, これより下に地形が無ければ未接地)
	constexpr float RideHeight    = 0.0f;    // 接地面から車体原点を浮かせる高さ(m)
	constexpr float BodyRadius    = 1.0f;    // 壁(TypeBump)判定に使う車体球の半径(m)
	constexpr float FallbackY     = 0.0f;    // 地形が見つからない時の仮のY(平地扱い)

	// 壁の当たり判定(物理応答)：法線フィルタで路面を除外し、垂直な面だけ壁として押し戻す。
	// 車体を複数の球で近似(カプセル/凸包相当)＝実車ゲームのボディ衝突に近い形。
	constexpr float WallSphereHeight = 0.9f;  // 壁判定球の中心高さ(m, 路面に触れない高さに上げる)
	constexpr float WallProbeRadius  = 0.55f; // 車体近似の各プローブ球の半径(m)
	constexpr float WallProbeTip     = 1.15f; // 前後端プローブの位置(m_base比。車体の張り出しをカバー)
	constexpr float WallNormalMaxY   = 0.3f;  // 面法線のYがこれを超えたら床とみなし壁押し戻ししない
	                                          // 小さいほど急坂も"床"扱い。0.3≒72°より急な面だけ壁＝坂登りのガクガク回避
	constexpr float WallSlideBounce  = 0.0f;  // 壁の反発(0=跳ねず擦る/CarX的, 上げると跳ね返る)
	// 車ゲー的な堅牢化：サブステップ(すり抜け防止)＋リラクゼーション(角のめり込み解消)
	constexpr float WallSubStepFactor = 0.5f; // 1ステップの最大移動＝半径×これ(小さいほど貫通しにくい/重い)
	constexpr int   WallMaxSubSteps   = 12;   // サブステップ上限(高速時の分割数の上限)
	constexpr int   WallRelaxIters    = 4;    // 1ステップ内の押し戻し反復回数(角・複数壁の収束)
	constexpr float WallMaxPush       = 0.5f; // 1回の押し出し上限(m。暴発防止)

	// サイドブレーキ中はスピン防止アシストを弱める＝リアが自由に回り込んで
	// "ブワッと広がる"サイドドリフトが出せる(引いてるときは意図的に出してるため)。
	constexpr float HandbrakeSpinAssistMul = 0.15f;  // サイド中のスピン防止の効き(0=完全解除, 1=通常)

	// CarX風の床判定：4輪レイで路面の高さ＋傾き(坂・バンク)に車を合わせる
	constexpr float GroundFollowSmooth = 12.0f;  // 接地Y・傾きの追従速度(1/s, 大=キビキビ・小=ふわっと)
	constexpr float MaxTerrainTilt     = 0.6f;   // 地形追従の最大傾き(rad, 急斜面での見た目暴れを抑える)

	//===== ジャンプ / 滞空(エビス風ジャンプドリフト) =====
	// ランプの勢いで宙に浮き、横向き・スピンを保持したまま飛んで着地する。
	constexpr float AirGravityMul   = 1.0f;   // 滞空中の重力倍率(Gravityに掛ける。上げると重い/キビキビ)
	constexpr float AirLaunchEps    = 0.03f;  // 支持面よりこの高さ(m)上で滞空とみなす
	constexpr float MaxLaunchVelY   = 12.0f;  // ランプで打ち上がる垂直速度の上限(m/s, メッシュ暴れ対策)
	constexpr float MaxLandVelY     = 40.0f;  // 落下側の垂直速度クランプ
	constexpr float LandBounce      = 0.12f;  // 着地時の跳ね返り(0=吸収, 1=完全反発)
	constexpr float SupportVelSmooth = 22.0f; // 支持面の上昇速度の平滑化(1/s, 大=鋭いランプで強く飛ぶ)
	constexpr float AirYawDamp      = 0.3f;   // 滞空中のヨー減衰(1/s, 無制御スピン防止。小=回転を保つ)
	constexpr float AirSteerControl = 1.2f;   // 滞空中のエアコントロール(ステアで機首のヨーを微調整 rad/s)
	// 空中姿勢=剛体の角運動量(物理)。ランプで付いた回転(角速度)を空中で保持し、
	// 空力(矢羽根効果)で機首がだんだん進行方向=弾道へ収束する。着地で角速度は解消。
	constexpr float AirAeroAlign    = 6.0f;   // 空力で機首を弾道へ揃える復元トルク(大=すぐ整う)
	constexpr float AirAeroDamp     = 2.5f;   // 空中の角速度の空気減衰(1/s, 大=すぐ安定/小=よく回る)
	constexpr float AirMaxPitch     = 0.9f;   // 空中ピッチ姿勢の安全クランプ(rad, ≈51°)
	constexpr float AirLaunchSpin   = 1.0f;   // 離陸時に引き継ぐ回転(角運動量)の強さ倍率
	// 既存の車ゲー的に「地面吸着＝ジャンプ/浮き無し」にする際の、崖落下中の水平戻し速度
	constexpr float AirLevelSmooth  = 3.0f;   // 崖から落ちている間に車体を水平へ戻す速さ(1/s)
}
