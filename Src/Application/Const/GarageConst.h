#pragma once

// 車を選ぶ画面(GARAGE)の定数。
//
// ■ 元絵は設計案のHTML
// 「ゲームデザイン実装方法/Drift Project UI.dc.html」の SCREEN 3。
// 座標はあちらの CSS をそのまま写してある。
//
//   ・画面は 1536x864、余白は上下44・左右64
//   ・左の列 200px、すきま 56px、残りが中央
//   ・台も車も、中央の左端からの相対で置く
//
// 数字を勝手に丸めないこと。元絵と突き合わせたときに
// どこを変えたのか分からなくなる。
//
// ■ なぜ独立した画面にするか
// これまで車を選べるのは走行中のTABメニューだけだった。
// あれは弄るための画面で、しかも「車種」と「車体のモデル」が
// 同じ階層に並んでいて意味の段が違う。
//
// 上が「どの車に乗るか」、下が「その車の見た目を差し替える」。
// 車庫を分ければ、選んだ瞬間に場面ごと作り直せるので、
// 「車種を変えても見た目が変わらない」も起きない。
namespace GarageConst
{
	//===== 画面の余白 =====
	constexpr float PadX = 64.0f;
	constexpr float PadY = 44.0f;

	// 版面の右端(デザイン座標)。UIConst::DesignW(1536) から余白を引いた位置
	constexpr float DesignRight = 1536.0f - PadX;

	//===== 文字の大きさ =====
	// ここに並ぶ px は、すべて元絵(HTML)の font-size をそのまま写したもの。
	//
	// ■ 渡し方に注意
	// HjUI::Text の第4引数は縦位置を決める値で、文字の大きさは変えない。
	// 大きさはフォントIDで決まっているので、そこへ元絵の数字を入れても
	// 何も起きない(入れた大きさで描かれたつもりになるだけ)。
	//
	// 大きさを変えたいときは HjUI::TextScaled を使う。
	// あちらの targetPx はデザイン座標なので、元絵の値をそのまま渡せる。
	//
	// ■ どのフォントを伸ばすか
	// 焼いてある大きさより大きく出すとドットが割れる。
	// 近くて大きいものを縮めるほうが綺麗なので、太さ(weight)が
	// 合うものの中から大きめを選ぶ。
	//   FontTitle 125/900 / FontHead 47/900 / FontTab 17/800
	//   FontRow 14/700 / FontFoot 11/700

	//===== 見出し =====
	// 元絵は 56px/900。FontTitle(125/900)を縮めて使う
	constexpr float HeadPx = 56.0f;

	// 見出しが占める高さ。行送りは Archivo のおおよそ 1.15 倍
	constexpr float HeadBlockH = 64.0f;

	//===== 車の一覧(左の列) =====
	// 元絵はブランド(NEXUS/KATANA…)を並べているが、
	// この作品に作り話のブランドは無い。選べる車をそのまま並べる
	constexpr float ListW    = 200.0f;   // 列の幅
	constexpr float ListGap  = 56.0f;    // 列と中央のすきま
	constexpr float ListStep = 60.0f;    // 行送り(項目40 + すきま20)
	constexpr float ListRowH = 40.0f;    // 1行の高さ(上下の詰め9 + 文字18)
	constexpr float ListPx   = 18.0f;
	constexpr float ListPadX = 12.0f;    // 選択中の塗りが文字からはみ出す量
	constexpr float ListPadY = 9.0f;     // 同じく上下

	// 記号と名前の間。元絵は記号24 + すきま10
	constexpr float ListIconW = 34.0f;

	//===== 諸元表(左の列の下) =====
	// 一覧が2台しかないと左が空く。
	//
	// 棒は「他と比べてどうか」しか言わないので、実寸を並べて補う。
	// 数字は車ごとに違うものだけにする。全車で同じ値を並べても
	// 面が埋まるだけで、選ぶ手がかりにならない
	constexpr float SpecY       = 340.0f;
	constexpr float SpecHeadGap = 26.0f;   // 見出しから1行目まで
	constexpr float SpecStep    = 34.0f;
	constexpr float SpecPx      = 13.0f;
	constexpr float SpecHeadPx  = 12.0f;
	constexpr float SpecLineY   = 20.0f;   // 行の上端から罫線まで

	//===== 中央 =====
	// 左の列を避けた位置から始まる
	constexpr float StageX = PadX + ListW + ListGap;      // 320
	constexpr float StageY = PadY + HeadBlockH + 44.0f;   // 152

	// 一覧も中央と同じ高さから始まる(元絵は同じ行に並ぶ)
	constexpr float ListY = StageY;

	//===== 車名と型 =====
	constexpr float NamePx  = 20.0f;
	constexpr float ModelPx = 96.0f;   // 元絵は line-height .9 の大見出し

	constexpr float ModelY = StageY + 24.0f;   // 車名の行の下

	// 等級の札。型名の下に10px空けて置く
	constexpr float TierY = StageY + 119.0f;
	constexpr float TierH = 31.0f;

	// 札の中。元絵は padding 5px 12px と 5px 16px
	constexpr float TierPx   = 14.0f;
	constexpr float TierPadL = 12.0f;   // 「TIER」側
	constexpr float TierPadR = 16.0f;   // 等級側

	//===== 暗い上に置く色 =====
	// 画面全体が3Dの空間になったので、文字も枠も暗い上に乗る。
	// 墨のままでは読めないので、明暗を入れ替える。
	//
	// アシッド緑(UIConst::ACID)だけはそのまま使う。
	// あれは明るい色なので、暗い上でもよく見える
	constexpr float InkCol[3]  = { 0.94f, 0.94f, 0.93f };   // 主の文字・枠
	constexpr float SubCol[3]  = { 0.58f, 0.58f, 0.57f };   // 小さい文字
	constexpr float LineCol[3] = { 0.78f, 0.78f, 0.77f };   // 罫線

	// 棒の地。暗い上では、墨の地は背景と見分けが付かない
	constexpr float BarBackAlpha = 0.22f;

	//===== 車を手で回す場所 =====
	// 画面のどこでも回せると、一覧や帯を押したときも回ってしまう。
	// 左の列と下の帯を避けた真ん中だけを掴めるようにする
	constexpr float SpinAreaX = StageX;
	constexpr float SpinAreaY = PadY + 90.0f;
	constexpr float SpinAreaW = DesignRight - SpinAreaX;
	constexpr float SpinAreaH = 470.0f;

	//===== 画面 =====
	// 車庫は画面全体がこの空間。デザインの寸法と同じ比にする
	constexpr float ScreenAspect = 1536.0f / 864.0f;

	//===== 車庫の部屋(3D) =====
	// B1 "NIGHT PIT"。
	// 数値の出どころは ゲームデザイン実装方法/exports/drift_garage_scene.h。
	// 向こうを直したら、こちらも合わせること。
	//
	// ■ 単位はメートル。ただし比を掛けて使う
	// 渡された寸法は実寸(部屋 14m × 10m × 5m)。
	// そのまま置くと、モデルの単位が違う車では部屋だけが巨大になる。
	// 車の包む球を測って「実車ならこれくらい」で割った比を全部に掛ける。
	// 比が 1 なら、渡された寸法そのまま
	constexpr float RefCarRadius = 2.45f;   // 実寸のS15を包む球の半径(m)

	// ■ 奥行きの符号
	// 渡された図は +Z が手前(カメラ側)。この枠組みは +Z が奥。
	// ここに書く Z は、渡された値の符号を反転させたもの
	constexpr float RoomW     = 14.0f;   // X -7 .. +7
	constexpr float RoomD     = 10.0f;   // Z -5 .. +5
	constexpr float RoomH     = 5.0f;
	constexpr float BackWallZ = 5.0f;    // 渡された値は -5

	// 床と壁だけに足す余白。
	//
	// 壁の面で画面に入る幅は片側 7.47m。
	// 部屋の幅 14m(片側 7m)では、左右の端に何も無い帯が出る。
	// 付属物の位置は渡されたとおりで、地だけを広げる
	constexpr float RoomMargin = 1.5f;

	//===== 色 =====
	// 渡された16進をそのまま 0〜1 に直したもの。
	// 見える明るさは光が乗った後の値なので、下の StageLit で合わせる
	constexpr float FloorCol[3]   = { 0.200f, 0.196f, 0.184f };   // #33322F コンクリ
	constexpr float WallCol[3]    = { 0.149f, 0.149f, 0.141f };   // #262624 塗装
	constexpr float SlatCol[3]    = { 0.231f, 0.227f, 0.216f };   // #3B3A37 羽根の面
	constexpr float SlatGapCol[3] = { 0.173f, 0.169f, 0.157f };   // #2C2B28 羽根の隙間
	constexpr float FrameCol[3]   = { 0.290f, 0.286f, 0.271f };   // #4A4945 枠
	constexpr float PaperCol[3]   = { 0.945f, 0.941f, 0.925f };   // #F1F0EC 蛍光灯・白線
	constexpr float AcidCol[3]    = { 0.812f, 0.878f, 0.129f };   // #CFE021 輪・車止め

	// 全部の塗り色に掛ける。
	// 明る過ぎ・暗過ぎは、まずここ一箇所で合わせる
	constexpr float StageLit = 1.00f;

	//===== シャッター =====
	// 羽根は本当に出っ張らせる。模様として描くと、
	// 横から光が当たったときに平らなことが分かってしまう
	constexpr float ShutterY     = 1.70f;
	constexpr float ShutterZ     = 4.95f;
	constexpr float ShutterW     = 6.00f;
	constexpr float ShutterH     = 3.40f;
	constexpr float ShutterPitch = 0.12f;    // 羽根の間隔
	constexpr float ShutterDepth = 0.012f;   // 羽根の出っ張り
	constexpr float ShutterFrame = 0.12f;    // 枠の太さ

	//===== 通用口 =====
	// 枠だけ。引っ込めてあるので、中は影になって見えない
	constexpr float DoorX      = -5.80f;
	constexpr float DoorY      =  1.05f;
	constexpr float DoorZ      =  4.95f;
	constexpr float DoorW      =  1.20f;
	constexpr float DoorH      =  2.10f;
	constexpr float DoorRecess =  0.10f;
	constexpr float DoorFrame  =  0.10f;

	//===== 蛍光灯 =====
	// 壁の高い所に5本。
	//
	// ■ 高さを渡された値から下げてある
	// 渡された値は Y 4.6。カメラを指定どおり(0,1.6,7.5)/縦画角38°に置くと、
	// 壁の面で画面に入るのは Y 4.28 まで。4.6 は枠の外へ出る。
	// 「上端に蛍光灯が並ぶ」ことが仕上がりの条件なので、入る高さにした
	constexpr int   TubeCount = 5;
	constexpr float TubeX[TubeCount] = { -4.8f, -2.4f, 0.0f, 2.4f, 4.8f };
	constexpr float TubeY     = 4.20f;   // 渡された値は 4.6
	constexpr float TubeZ     = 4.90f;
	constexpr float TubeLen   = 1.80f;
	constexpr float TubeThick = 0.06f;

	// 光って見えるまで色を持ち上げる量と、滲ませる強さ
	constexpr float TubeLit  = 5.20f;
	constexpr float TubeGlow = 0.50f;

	// 壁を照らすぶん。
	//
	// 渡された指示は「光らせるだけ、光源にはしない」。
	// ただし主光は真上からなので、壁には何も当たらない。
	// 灯りを消したままだとシャッターも通用口も文字も沈んで見えなくなる。
	// 点光は影を落とさないので、「影を落とさない」条件は満たしている
	constexpr float TubeLightCol[3] = { 0.80f, 0.80f, 0.76f };
	constexpr float TubeLightR     = 7.0f;   // 届く範囲(m)

	//===== 壁の文字 =====
	// 塗ってある "PIT 02" だけ。
	//
	// 前は題字を壁一面に出していたが、車名(S15)と重なって両方読めなかった。
	// 壁の文字はこれ1つに絞る
	constexpr const char* WallTextStr = "PIT 02";
	constexpr float WallTextX     = 4.60f;
	constexpr float WallTextY     = 3.10f;
	constexpr float WallTextZ     = 4.98f;
	constexpr float WallTextH     = 0.60f;   // 大文字の高さ
	constexpr float WallTextAlpha = 0.18f;

	//===== 床の白線 =====
	// 駐車枠。奥へ向かって狭まるので、奥行きはこれが一番効く
	constexpr float LineW      = 0.12f;
	constexpr float LineAlpha  = 0.55f;
	constexpr float BayLineX   = 5.50f;   // 左右2本。奥まで通す
	constexpr float CrossLineZ = 3.20f;   // 横切る1本。渡された値は -3.2

	//===== 車止め =====
	// 台の奥。つや消しのアシッドで、光らせない
	constexpr float StopY = 0.06f;
	constexpr float StopZ = 3.40f;
	constexpr float StopW = 3.60f;
	constexpr float StopH = 0.12f;
	constexpr float StopD = 0.25f;

	//===== 回す台 =====
	// 床に埋めた円盤と、縁の光る輪
	constexpr float TurnY      = 0.01f;
	constexpr float TurnOuterD = 5.60f;
	constexpr float RingBandW  = 0.14f;
	constexpr int   TurnSeg    = 96;      // 円周の分割数
	constexpr float DiscMul    = 1.18f;   // 床より少し明るい鉄板

	// 輪。白へ飛ばすとUIの主色と別の色になるので、控えめに持ち上げる
	constexpr float RingLit  = 2.60f;
	constexpr float RingGlow = 0.40f;

	//===== 回り方 =====
	// ゆっくり回り続ける。Q/E で回している間はそちらが勝ち、
	// 離すと 0.4 秒かけて自動へ戻る
	constexpr float SpinDegPerSec  = 15.0f;   // 24秒で1周
	constexpr float ManualEaseSec  = 0.40f;
	constexpr float CarStartYawDeg = -35.0f;

	//===== カメラ(固定) =====
	// 揺らさない。車庫は「据えた1台を見る」画面で、
	// カメラが動くと見たい角度で止められない
	constexpr float CamY       = 1.60f;
	constexpr float CamZ       = -7.50f;   // 渡された値は +7.5
	constexpr float CamTargetY = 0.70f;
	constexpr float CamFovDeg  = 38.0f;    // 縦画角

	//===== 光 =====
	// 主光は台の真上から1つだけ。影を落とすのはこれだけ。
	//
	// ■ 真下へ向けきらない
	// 完全に真下だと、影の向きが decide できず深度マップの基準が作れない。
	// わずかに傾けて、影は車の真下に残す
	constexpr float KeyLightY      = 4.80f;
	constexpr float KeyLightDir[3] = { 0.06f, -1.0f, 0.10f };
	constexpr float KeyLightCol[3] = { 0.95f, 0.95f, 0.92f };

	// 真上から当てる灯り。円錐で切ってあるので、台の周りだけが明るい
	constexpr float KeySpotCol[3]  = { 1.45f, 1.45f, 1.40f };
	constexpr float KeySpotOuter   = 52.0f;   // 円錐の外側(度)
	constexpr float KeySpotInner   = 26.0f;   // 同じく内側
	constexpr float KeySpotRange   = 12.0f;

	// 環境光。低く、灰緑に寄せる(#1C1C1A)
	constexpr float AmbientCol[3]  = { 0.30f, 0.30f, 0.28f };

	// 車体に映り込む環境。上=蛍光灯 / 横=壁 / 下=床。
	// これが無いと、いくら光を当てても車体が塗った面にしかならない
	constexpr float EnvUp[3]   = { 0.34f, 0.34f, 0.32f };
	constexpr float EnvSide[3] = { 0.12f, 0.12f, 0.115f };
	constexpr float EnvDown[3] = { 0.10f, 0.10f, 0.095f };

	// 明るさの抽出のしきい値。
	// 下げ過ぎると輪が白へ飛んで、UIの主色と違う色になる
	constexpr float BloomThreshold = 0.90f;

	//===== 車の輪郭 =====
	// 暗い部屋では輪郭を強く出すと縁取りが浮くので、弱めに掛ける
	constexpr float PortraitOutlineMul = 0.45f;

	//===== 手で回す =====
	// 引きずった量とキーの速さ
	constexpr float DragYawPerPx = 0.45f;   // 引きずった1pxあたり(度)
	constexpr float KeyYawSpeed  = 80.0f;   // キーで回す速さ(度/秒)
	//===== 性能の棒(右) =====
	// 元絵: right:0 top:60 width:300、項目のすきま20
	//   ラベル14px + 下に5px + 棒16px = 37、すきま20 → 行送り57
	constexpr float StatW     = 300.0f;
	constexpr float StatBarH  = 16.0f;
	constexpr float StatStep  = 57.0f;
	constexpr float StatY     = StageY + 60.0f;
	constexpr float StatBarDy = 21.0f;   // ラベルの上端から棒の上端まで
	constexpr float StatPx    = 14.0f;

	//===== 中央の高さ =====
	// 元絵の min-height。台(90+320=410)も車(40+380=420)も収まる。
	// 下の帯はここから下げて置く
	constexpr float StageH = 470.0f;

	//===== 下の帯(車のサムネイル) =====
	constexpr float StripY   = StageY + StageH + 36.0f;   // 658
	constexpr float StripH   = 120.0f;
	constexpr float ArrowW   = 36.0f;
	constexpr float ArrowPx  = 20.0f;   // 矢印の文字。元絵と同じ
	constexpr float ArrowGap = 12.0f;   // 矢印と札の間
	constexpr float StripGap = 14.0f;   // 札どうしの間

	// 札1枚の幅の上限。
	//
	// 元絵は5台前提で帯を等分していた。台数が少ないまま等分すると
	// 1枚が画面幅いっぱいまで伸びて、車の札ではなく色の帯に見える。
	// 5台のときの幅で頭打ちにして、余りは中央へ寄せる
	constexpr float StripCellMax = 251.0f;

	// 札の中の型名。札の下端からの位置
	constexpr float StripTextUp = 26.0f;

	//===== 下端のキー案内 =====
	constexpr float KeyY = StripY + StripH + 28.0f;   // 806
	constexpr float KeyGap = 30.0f;   // 案内どうしのすきま

	//===== 右端のトンボ =====
	// 印刷の裁ち切り位置に見立てた十字。
	// 画面が紙の一部を切り出したものだ、という体裁を作る
	constexpr float TickX    = 1500.0f;
	constexpr float TickLen  = 16.0f;
	constexpr float TickHalf = 8.0f;    // 縦棒の長さの半分
	constexpr float TickPx   = 1.4f;    // 線の太さ

	constexpr float TickYs[] = { 200.0f, 440.0f, 600.0f };
	constexpr int   TickCount = static_cast<int>(sizeof(TickYs) / sizeof(TickYs[0]));

	//===== 見出しの右 =====
	// 選べる車が何台あるか。
	//
	// 元絵はここに所持金(CREDITS ¥125,000)を出していたが、
	// この作品に通貨は無い。無いものの桁だけ置くと、
	// 後で「増えない」と誤解を生む
	constexpr float CountLabelY = PadY + 4.0f;
	constexpr float CountValueY = PadY + 22.0f;
	constexpr float CountLabelPx = 12.0f;
	constexpr float CountValuePx = 24.0f;

	//===== 性能の見せ方 =====
	// 実際の性能値を 0〜1 へ均すときの範囲。
	//
	// 車が2台しかないので、両方の間で正規化すると
	// 常に「片方が満タン、片方が空」になって差が読めない。
	// 決めた範囲に対する位置で出す
	constexpr float SpeedMin = 40.0f,  SpeedMax = 90.0f;    // m/s
	constexpr float PowerMin = 6.0f,   PowerMax = 18.0f;
	constexpr float BrakeMin = 10.0f,  BrakeMax = 26.0f;
	constexpr float GripMin  = 1.00f,  GripMax  = 1.80f;    // 摩擦係数
	constexpr float DriftMin = 0.85f,  DriftMax = 1.15f;    // 後/前の摩擦の比(小さいほど出る)

	//===== 車の入れ替え =====
	// 選び直したときに瞬時に差し替わると、絵が切り替わっただけで、
	// 車が入れ替わったようには見えない。
	//
	// カメラを振って、一番速く動いている所で差し替える。
	// 動きに紛れるので、差し替えの瞬間そのものは見えない
	// カメラは固定なので、振ったり引いたりはしない。
	// 差し替わったことは、輪がひと突き強くなることで伝える
	constexpr float SwapTime      = 0.52f;   // 掛かる時間(秒)
	constexpr float SwapFlash     = 2.40f;   // 入れ替わる瞬間の明るさの倍率
	constexpr float SwapFlashTime = 0.34f;   // 明るさが戻るまで(秒)

	//===== 画面の動き =====
	// 切り替えた瞬間に棒と表が跳ぶと、別の画面に差し替わったように見える。
	// 追いかける形にすると、同じ画面のまま中身が入れ替わって見える
	constexpr float UiAnimTime  = 0.46f;   // 切り替えに掛ける時間(秒)
	constexpr float BarStagger  = 0.07f;   // 棒を1本ずつずらす量(秒)
	constexpr float SpecStagger = 0.045f;  // 諸元表の行をずらす量(秒)
	constexpr float SpecSlide   = 26.0f;   // 行が横から入ってくる量(px)
	constexpr float NameSlide   = 34.0f;   // 型名が横から入ってくる量(px)
	constexpr float SelFollow   = 18.0f;   // 選択の地が追いつく速さ(1/秒)

}
