#pragma once

// 道に付ける物の定数。ガードレールと路面標示。
//
// ■ なぜこれを先に作るか
// 地形が単色でも、縁にガードレールが立って白線が引かれていれば、
// 山道として読める。
//
// 逆に、地面をどれだけ丁寧に塗っても、その2つが無いと
// 「起伏のある地面」にしか見えない。道だと分かる手掛かりが要る。
//
// ■ スプラインに沿って置く
// 道の中心線は既にある。そこから横へずらして並べるだけで、
// 曲がりにも勾配にも自動で付いてくる。
namespace RoadDecoConst
{
	//===== ガードレール =====
	// 路肩の外へ置く。中心からの距離(m)
	constexpr float RailOffset = 4.9f;

	// 支柱の高さ(m)と、レール板の高さ・厚み
	constexpr float RailPostHeight = 0.75f;
	constexpr float RailBeamTop    = 0.72f;   // 板の上端(路面から)
	constexpr float RailBeamBottom = 0.38f;   // 板の下端
	constexpr float RailBeamOut    = 0.06f;   // 板の張り出し(厚み)

	// 支柱の間隔(m)と太さ
	constexpr float PostSpacing = 4.0f;
	constexpr float PostHalf    = 0.05f;

	// レールを置く刻み(m)。板は連続なので細かくする
	constexpr float RailStep = 2.0f;

	// 谷側にだけ置くか。
	// 実際の峠は、落ちる側にしか無いことが多い。
	// ただし判断には地形が要るので、まずは両側に置く
	constexpr bool RailBothSides = true;

	//===== 路面標示 =====
	// 中央線。路面より少し浮かせて置く。
	// 同じ高さだと、どちらが手前か決まらずちらつく
	constexpr float MarkLift = 0.02f;

	// 中央線の幅(m)
	constexpr float CenterLineWidth = 0.15f;

	// 破線の長さと間隔(m)。
	// 実際の道路標示に近い比率にすると、速度感が出る
	constexpr float DashLength = 4.0f;
	constexpr float DashGap    = 6.0f;

	// 外側線。路肩との境目に引く実線
	constexpr bool  UseEdgeLine    = true;
	constexpr float EdgeLineOffset = 3.35f;   // 中心からの距離
	constexpr float EdgeLineWidth  = 0.12f;

	//===== 色 =====
	// ガードレール。塗装した鋼なので、明るい灰色
	constexpr float RailR = 0.72f, RailG = 0.73f, RailB = 0.70f;
	// 支柱は少し暗く。同じ色だと立体に見えない
	constexpr float PostR = 0.45f, PostG = 0.46f, PostB = 0.44f;

	// 標示は白。少し落として、光ったように見えないようにする
	constexpr float MarkR = 0.88f, MarkG = 0.88f, MarkB = 0.84f;
}
