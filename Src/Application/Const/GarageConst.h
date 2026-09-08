#pragma once

// 車を選ぶ画面(GARAGE)の定数。
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

	//===== 車の一覧(左の列) =====
	// 選べる車をそのまま並べる。作り話のブランドは持たない
	constexpr float ListY    = PadY + 100.0f;
	constexpr float ListStep = 46.0f;
	constexpr float ListPadX = 12.0f;   // 選択中の塗りが文字からはみ出す量

	//===== 車の台(中央) =====
	constexpr float StageX = PadX + 256.0f;
	constexpr float StageY = PadY + 100.0f;

	// 等級の札
	constexpr float TierY = StageY + 136.0f;
	constexpr float TierH = 32.0f;

	// アシッドの台。斜めに切る。
	// 傾きは幅に対する割合。左上を内へ、右下を外へ。
	//
	// 車が乗るので大きく取る。小さいと車が台からはみ出して、
	// 地面ではなく車の後ろの板に見える
	constexpr float SlabX     = StageX + 150.0f;
	constexpr float SlabY     = StageY + 60.0f;
	constexpr float SlabW     = 760.0f;
	constexpr float SlabH     = 470.0f;
	constexpr float SlabShear = 0.06f;

	//===== 車の絵を貼る枠 =====
	// 台より少し大きく取って、上へずらして乗せる。
	// 台の内側へ収めると、車が箱庭に置かれたように見える。
	// 前輪が手前の縁を踏み越えているほうが地面に乗って見える
	constexpr float CarX = SlabX - 40.0f;
	constexpr float CarY = SlabY - 110.0f;
	constexpr float CarW = 840.0f;
	constexpr float CarH = 525.0f;   // CarW と PortraitW:PortraitH を揃える

	//===== 車の絵 =====
	// 台へ貼る絵の解像度。
	//
	// 貼る枠より粗いと縁がぼける。台と同じ縦横比にしておかないと、
	// 引き伸ばした時に車が縦長になる
	constexpr int PortraitW = 1024;
	constexpr int PortraitH = 640;

	// カメラ。車の全長を基準に引く
	constexpr float PortraitFovDeg = 26.0f;   // 望遠寄り。広角だと手前が膨らんで模型に見える
	constexpr float PortraitPitch  = 12.0f;   // 見下ろす角度(度)。真横だと車高が読めない

	// 距離は決め打ちにしない。
	// 車ごとにモデルの単位も車体スケールも違うので、固定にすると
	// 車を替えるたびに画面に映る大きさが変わる。
	// 実際のモデルの大きさから、毎回この余白になる距離を出す
	constexpr float PortraitFill = 1.78f;   // 画面の高さに対して車が占める割合

	// 測れなかったときの距離(m)。モデルが読めていない時だけ使う
	constexpr float PortraitDistFallback = 12.0f;

	// 回る速さ(度/秒)。眺めている間に一周してしまわない程度
	constexpr float PortraitSpinDeg = 8.0f;

	// 光の向きと色。走行中の空とは別に、絵として当てる
	constexpr float PortraitLightDir[3] = { -0.4f, -0.7f, 0.55f };
	constexpr float PortraitLightCol[3] = {  1.0f,  0.98f, 0.94f };
	constexpr float PortraitAmbient[3]  = {  0.42f, 0.43f, 0.46f };

	//===== 性能の棒(右) =====
	constexpr float StatW    = 300.0f;
	constexpr float StatBarH = 16.0f;
	constexpr float StatStep = 56.0f;
	constexpr float StatY    = StageY + 60.0f;

	//===== 下の帯(車のサムネイル) =====
	constexpr float StripY   = 640.0f;
	constexpr float StripH   = 120.0f;
	constexpr float ArrowW   = 36.0f;
	constexpr float StripGap = 14.0f;

	//===== 下端のキー案内 =====
	constexpr float KeyY = 792.0f;

	//===== 背景のハーフトーン =====
	// 何もない所を「余白」ではなく「地」に見せるための点。
	//
	// ■ タイトルと同じ明滅を使う
	// タイトルは HjUI::DotFieldTwinkle で、位置と時間で位相をずらした
	// 波を点の上に流している。ここだけ静止した点にすると、
	// 画面を移った瞬間に「止まった」ように見えて、同じ作品に見えない。
	//
	// 大きさは点の数で持つ。幅で持つと、点の間隔(UIConst::DotCell)は
	// 画面の解像度に付いて回るので、解像度が変わった瞬間に
	// 端が欠けたり一列多く出たりする
	struct DotPatch
	{
		float x, y;      // 左上(デザイン座標)
		int   cols, rows;
		float alpha;     // 濃さ。全部同じにすると壁紙になって図案に見えない
		float phase;     // 波の位相。ずらさないと全部が一斉に光って点滅になる
		bool  grey;      // 明るい灰で打つか。隣り合う塊を2色に分ける
	};

	// 位相はタイトルと同じ 0.7 刻み
	constexpr DotPatch Dots[] =
	{
		{   70.0f, 560.0f, 11, 8, 0.40f, 0.0f, false },
		{  250.0f, 560.0f,  6, 8, 0.22f, 0.7f, true  },   // 隣と2色で組にする
		{  620.0f, 120.0f,  9, 6, 0.35f, 1.4f, false },   // 車の台の裏。模様の上に車が乗る
		{ 1200.0f, 740.0f, 15, 3, 0.38f, 2.1f, false },
	};

	constexpr int DotCount = static_cast<int>(sizeof(Dots) / sizeof(Dots[0]));

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
	// 元案はここに所持金を出していたが、この作品に通貨は無い。
	// 無いものの桁だけ置くと、後で「増えない」と誤解を生む
	constexpr float CountLabelY = PadY + 4.0f;
	constexpr float CountValueY = PadY + 22.0f;
	constexpr float CountLabelPx = 12.0f;
	constexpr float CountValuePx = 22.0f;

	//===== 背景のぐにゃぐにゃ =====
	// タイヤ痕を重ねたような、不規則な閉曲線。
	//
	// 楕円の大小で代用しないこと。同心円は「的」に見えて、
	// 走った跡には見えない。歪んでいることそのものが図案
	constexpr float LoopCx = 1050.0f;
	constexpr float LoopCy = 430.0f;

	// 1秒に回る角度(度)。ゆっくり回っているのが分かる程度
	constexpr float LoopSpinDeg = 3.0f;

	// 1つの曲線を何点で描くか。
	// 少ないと角ばって、線ではなく多角形に見える
	constexpr int LoopSteps = 8;

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
}
