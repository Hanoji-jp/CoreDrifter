#pragma once

// ガードレールの定数。
//
// ■ どこに置くかが本体
// 実際の峠は、どこにでもレールがあるわけではない。
// 外側で、かつ谷になっている所だけ。
//
// 高さマップがあるので自動で出せる。
// 「道の縁から少し外の地形が、これだけ落ちていたら置く」
//
// ■ 当たり判定は持たない
// 将来すべて剛体で処理する方針なので、いまは見た目だけ。
// ここで簡易な壁を入れると、剛体を入れるときに二重になる。
namespace GuardRailConst
{
	//===== 置く場所 =====
	// 道の縁からどれだけ外を見るか(m)。
	// 近すぎると路肩の傾きを拾い、遠すぎると崖を見逃す
	constexpr float LookOut = 6.0f;

	// これだけ落ちていたら置く(m)。
	// 実際のガードレールは、路外へ落ちると危ない所に付いている
	constexpr float DropThreshold = 2.5f;

	// 短すぎる区間は付けない(m)。
	// 数メートルだけの柵は、現実にも無いし見た目も悪い
	constexpr float MinRun = 12.0f;

	// 短い切れ目は繋ぐ(m)。
	// 地形のわずかな起伏で柵が飛び飛びになるのを防ぐ
	constexpr float MaxGap = 20.0f;

	//===== 形 =====
	// 中心線からレールまで(m)。路肩の外端の少し外
	constexpr float Offset = 5.4f;

	// ビームの下端と上端の高さ(m)。路面から測る
	//===== ビーム(波形の板) =====
	// 実物は「Ｗビーム」。断面が波を2つ描いていて、
	// 中央の谷で支柱に留める。
	//
	// ■ なぜ波形なのか
	// 平らな板だと、当たった瞬間に折れる。波を付けると断面の
	// 高さが稼げて、板のまま曲がって衝撃を受け流せる。
	// 見た目の話ではなく、あの形でないとガードレールにならない。
	//
	// 前は3点(下・中・上)で中を膨らませただけだったので、
	// 波が1つしかなく、樋を横にしたように見えていた
	constexpr float BeamBottom = 0.44f;   // 板の下端の高さ(m)
	constexpr float BeamTop    = 0.76f;   // 板の上端の高さ(m)

	// 波の深さ(m)。道側へ出っ張る量
	constexpr float BeamCrest = 0.055f;

	// 上下の縁を裏へ折り返す量(m)。
	// 実物も縁を折って断面を閉じている。折らないと板の厚みが無く、
	// 横から見たときに紙のように見える
	constexpr float BeamLip = 0.022f;

	// 中央の谷の深さ(m)。ここで支柱に留まる
	constexpr float BeamValley = 0.012f;

	//===== 支柱 =====
	// 実物は丸パイプ(φ139.7mm)。四角い柱は無い
	constexpr float PostSpacing = 2.0f;   // 間隔(m)。実物は2mか4m
	constexpr float PostRadius  = 0.070f; // 半径(m)
	constexpr int   PostSides   = 8;      // 何角形で丸を作るか
	constexpr float PostTop     = 0.80f;
	constexpr float PostBottom  = -0.30f; // 少し埋める

	// 支柱を板の裏へ下げる量(m)。
	// 実物も板が手前、支柱が奥。同じ面に置くと、
	// 板から柱が生えているように見える
	constexpr float PostBack = 0.055f;

	//===== 色 =====
	// 白に近い灰色。峠のガードレールは白が多い
	constexpr float BeamR = 0.72f;
	constexpr float BeamG = 0.73f;
	constexpr float BeamB = 0.70f;

	// 支柱は少し暗くする。同じ色だと形が読めない
	// 支柱。亜鉛めっきの灰。板より少し暗い
	constexpr float PostColR = 0.50f;
	constexpr float PostColG = 0.51f;
	constexpr float PostColB = 0.50f;
}
