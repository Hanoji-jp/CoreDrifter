#pragma once

// 走行中のHUDの配置。デザイン座標(1536x864, 左上原点)。
// 数値はデザイン指定そのまま。yは文字の「上端」。
//
// ■ 映像の上に置くので、色の役割が反転する
// メニュー画面は紙が背景で墨が文字。走行画面は映像が背景なので、
// 文字を紙色にして映像の上に浮かせる。囲みは作らない。
// パネルで塗り潰すと、その面積ぶん走っている絵が見えなくなる。
//
// ■ 塗るのはアシッドだけ
// 罫線とかぎ括弧で位置を示し、塗りは「今それが起きている」ものだけに使う。
// 全部を塗ると、どれが今の値なのか分からなくなる。
namespace RunHudConst
{
	constexpr float CanvasW = 1536.0f, CanvasH = 864.0f;
	constexpr float PadX = 64.0f;
	constexpr float CenterX = 768.0f;

	//===== 画面の枠 =====
	// 四隅のかぎ括弧だけで枠を示す。中身を囲わないので映像を塞がない
	constexpr float BracketInset = 40.0f, BracketLen = 110.0f, BracketPx = 2.0f;
	constexpr float CenterTickTop0 = 40.0f,  CenterTickTop1 = 96.0f;
	constexpr float CenterTickBot0 = 768.0f, CenterTickBot1 = 824.0f;

	//===== 左上：走行の情報 =====
	// ※中身は仮。何本目・コース名・区間を持つ仕組みはまだ無い
	constexpr float RunLabelY = 78.0f;
	constexpr float RunNumX = 46.0f, RunNumY = 64.0f, RunNumPx = 44.0f;
	constexpr float RunTotalY = 82.0f;
	constexpr float RunRuleY = 122.0f, RunRuleW = 230.0f, RunRuleH = 2.0f;
	constexpr float CourseY = 138.0f, SectionY = 166.0f;

	//===== 下中央：ドリフト角 =====
	// スコアと入れ替えて下へ置く。
	// 角度は操作しながら追い続ける値なので、車の近く(画面の下側)にある方が
	// 視線の移動が短くて済む。
	//
	// ■ 中央を0にして、滑っている側へ伸ばす
	// 片側だけのバーは「伸びるほど良い」に見えるが、角度は多いほど
	// 良い値ではない。中央から左右に振れば、どちらへ滑っているかも同時に出る。
	// 切り返しではバーが中央を通って反対側へ抜ける。
	//
	// 目盛りは全部同じ高さ・同じ色にする。
	// 印を立てたり色を変えたりすると、そこに意味があるように見えてしまう。
	// 実際に伸び具合を読むのに要るのは、どこまで埋まっているかだけ。
	constexpr float AngleLabelY = 742.0f, AngleValueY = 772.0f, AngleValuePx = 44.0f;

	constexpr int   AngleTicksPerSide = 12;   // 片側の本数
	constexpr float AngleTickW = 6.0f, AngleTickGap = 3.0f, AngleTickY = 824.0f;
	constexpr float AngleTickH = 16.0f;

	// 端まで振れる角度(度)
	constexpr float AngleFullDeg = 70.0f;

	// 外側へ行くほど赤くする。
	// 深い角度は戻せなくなる領域なので、同じ色のまま伸びるより、
	// 端へ近づくほど警告の色になった方が「行き過ぎ」が体で分かる。
	// 内側は白のままにして、色が付き始める位置を持たせる。
	const Math::Color AngleHot = { 0.92f, 0.18f, 0.12f, 1.0f };
	// この割合(端を1とする)を超えてから色が付き始める
	constexpr float AngleHotStart = 0.35f;
	// 色の乗り方。1より大きいと端の手前まで白が残り、端で一気に赤くなる
	constexpr float AngleHotCurve = 1.6f;

	//===== 右上 =====
	// 何も置かない。
	// 採点の内訳は走り終えた後に読むものなので、RESULTSへ置く。
	// 走行中に出しても操作へ反映できず、動かない表示は目が無視する。

	//===== 左下：走行ラインと通過点 =====
	// ※中身は仮。コースの形も通過判定もまだ無い

	//===== 上中央：スコアとチェーン倍率 =====
	// 角度と入れ替えて上へ置く。
	// スコアは結果であって操作対象ではないので、常時追う必要がない。
	constexpr float ScoreLabelY = 56.0f, ScoreValueY = 72.0f, ScoreValuePx = 58.0f;
	// ── スコアが伸びたときの演出 ──
	// 数字が大きくなるほど、判定文字と同じ文字流体化を強く掛ける。
	// 数字が増えるだけだと「どれだけ凄いのか」が伝わらない。
	// 見た目が変わると、伸びていること自体が手応えになる。
	//
	// 段は採点のしきい値をそのまま使う。画面ごとに別の基準を持つと、
	// 「NICEが出る点」と「見た目が変わる点」がずれて混乱する。
	constexpr int   ScoreFxStyleLow  = 2;   // 炎
	constexpr int   ScoreFxStyleHigh = 1;   // 虹スモーク(最上位)
	// 効果の強さ。下限から上限へ向けて上げていく
	constexpr float ScoreFxMinAlpha = 0.35f;
	// 焼いた画像には上下に余白が入る(文字の高さの1/8ずつ)。
	// 枠の縦横比はこれを含めた実寸で決める。含めないと横へ潰れる。
	constexpr float ScoreFxPadRatio = 1.25f;

	// 枠の大きさの倍率。
	// 数字は文字テクスチャの一部しか占めておらず、上下に大きな空きがある。
	// 計算だけでは普通の表示と同じ大きさに揃わないので、ここで合わせる。
	// 縦横比は実寸から出しているので、この値を変えても比率は崩れない。
	constexpr float ScoreFxSizeMul = 2.356f;
	// 炎/虹スモークの尾(トレイル)の長さ。既定の0.05だと文字際にしか出ず、
	// 炎が小さく見える。文字自体の大きさ(枠のサイズ)とは別のパラメータなので、
	// これを伸ばせば文字を大きくせずに炎だけ大きく見せられる。
	constexpr float ScoreFxDilate = 0.22f;

	// 調整用。F6で常時表示に切り替える。
	// 点が伸びるまで待たないと見た目を確かめられないと、調整にならない。
	constexpr int   ScoreFxDebugKey = VK_F6;
	// 常時表示のときに使う見本の数値。
	// 桁数で幅が変わるので、1桁で合わせると実際とずれる
	constexpr int   ScoreFxSampleScore = 0;

	// 枠を少し下げる。炎は右上へなびくので、詰めるとラベルに掛かる
	constexpr float ScoreFxDropY = 1.75f;

	// 倍率は数字の周りの輪で出す。
	// 「CHAIN」と書かなくても、輪が減っていれば時間の残りだと分かる。
	// 文字で説明するより、減っていく形そのものが説明になる。
	constexpr float ChainRingCy = 179.0f;   // 輪の中心
	constexpr float ChainRingR  = 34.0f;    // 半径
	constexpr float ChainRingPx = 5.0f;     // 輪の太さ
	constexpr float ChainTextPx = 26.0f;    // 中の数字

	//===== 右下：回転計・速度・ギア =====
	// 画面の下端に寄りすぎていたので、まとめて持ち上げる。ku
	// 回転計・速度・単位・ギアは互いの位置関係で読むものなので、
	// 1つだけ動かすと関係が崩れる。ここを変えれば全部が一緒に動く。
	constexpr float RightBlockLift = 26.0f;

	constexpr int   TachTicks = 30;
	constexpr float TachTickW = 5.0f, TachTickGap = 4.0f;
	constexpr float TachY = 676.0f - RightBlockLift;
	// 目盛りは右へ行くほど高くする。回転が上がるほど視線が引っ張られる
	constexpr float TachTickBaseH = 10.0f, TachTickRiseH = 0.8f, TachMaxH = 34.0f;
	constexpr int   TachRedTicks = 6;      // 右端の何本を上限域とするか
	constexpr float TachDimAlpha = 0.35f;  // まだ回っていないぶん

	// 速度の右端を、画面の右端からどれだけ内側に置くか。
	// 小さくするほど右へ寄る。KM/H(UnitX=84)より内側にすると重なる。
	//
	// ※右揃えの計算には文字テクスチャの右側の余白が含まれる。
	//   細い字(1など)ほど余白の割合が大きく、見た目はさらに左へ寄る。
	constexpr float SpeedRightPad = 60.0f;
	constexpr float SpeedY = 716.0f - RightBlockLift, SpeedPx = 126.0f;
	// KM/H・GEARの左端を、画面の右端からどれだけ内側に置くか。
	// 小さくするほど右へ寄る。速度(SpeedRightPad)より小さくしないと、
	// 速度の数字と重なる。
	constexpr float UnitX = 54.0f;
	// KM/H と GEAR の文字だけをさらに持ち上げる。
	// ギアの数字は動かさないので、ここを増やすとラベルと数字の間が開く。
	constexpr float UnitLabelLift = 16.0f;

	constexpr float UnitY = 740.0f - RightBlockLift - UnitLabelLift;
	constexpr float GearLabelY = 772.0f - RightBlockLift - UnitLabelLift;
	constexpr float GearValueY = 788.0f - RightBlockLift;
	constexpr float GearValuePx = 44.0f;

	//===== 透明度 =====
	// 映像の上なので、輪郭がはっきりする程度に落とす。
	// 不透明にすると画面が塞がり、薄すぎると背景に負けて読めない
	constexpr float DimAlpha   = 0.75f;   // 小さいラベル
	constexpr float FaintAlpha = 0.30f;   // 罫線・消えている目盛り
}
