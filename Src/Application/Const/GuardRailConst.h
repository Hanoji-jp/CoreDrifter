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
	constexpr float BeamLow  = 0.45f;
	constexpr float BeamHigh = 0.75f;

	// ビームの真ん中の折れ(m)。
	// 本物は断面がW字。真ん中を外へ張り出させて、それらしくする
	constexpr float BeamBulge = 0.04f;

	//===== 支柱 =====
	// 何メートルおきに立てるか
	constexpr float PostSpacing = 3.0f;

	// 支柱の太さ(m)と、路面からの高さ(m)
	constexpr float PostHalf   = 0.05f;
	constexpr float PostTop    = 0.80f;
	constexpr float PostBottom = -0.25f;   // 少し埋める

	//===== 色 =====
	// 白に近い灰色。峠のガードレールは白が多い
	constexpr float BeamR = 0.72f;
	constexpr float BeamG = 0.73f;
	constexpr float BeamB = 0.70f;

	// 支柱は少し暗くする。同じ色だと形が読めない
	constexpr float PostR = 0.45f;
	constexpr float PostG = 0.46f;
	constexpr float PostB = 0.45f;
}
