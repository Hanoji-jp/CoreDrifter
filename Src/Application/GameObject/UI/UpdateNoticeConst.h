#pragma once

#include "UIConst.h"

// タイトル画面の左下に出す、更新の知らせ。
//
// ■ どういう見せ方か
// 文字を1行置いて、その下に細い帯。
// 帯は明るさを行き来させて、そこに何かあることを伝える。
//
// ■ なぜ点滅ではなく明るさの行き来か
// パッと消えて出る点滅は、視界の端で起きると気が散る。
// タイトルは眺めている時間が長いので、
// 明るさを滑らかに往復させて、うるさくならないようにする。
//
// ■ なぜ左下か
// 右下は再生中の曲、右上は装飾、中央は3Dの窓が占めている。
// 左下は地図を外して空いたまま。
namespace UpdateNoticeConst
{
	//===== 試し表示 =====
	// 見た目を決めるための仮の設定。
	//
	// 本物の更新はGitHubにリリースを置かないと起きないので、
	// 置く前に見た目を見たいときはここを true にする。
	// 位置や明るさが決まったら false に戻すこと。
	//
	// ※true のままだと、更新が無くても出続ける
	constexpr bool ForceShow = true;
	constexpr const char* ForceText   = "新しい版があります";
	constexpr const char* ForceFromVer = "v1.0.0";
	constexpr const char* ForceToVer   = "v1.1.0";

	//===== 置き場所(デザイン座標 1536x864) =====
	constexpr float X = 64.0f;
	constexpr float TextY = 752.0f;

	// 文字と帯の間。詰めすぎると1つの塊に見えて、
	// 帯が下線に見えてしまう
	constexpr float BarY = 790.0f;
	constexpr float BarW = 244.0f;
	constexpr float BarH = 6.0f;

	// 帯の下に出す小さな添え字(版の番号)
	constexpr float SubY = 806.0f;

	//===== 明るさの行き来 =====
	// 1往復にかける時間(秒)。速いと急かされている感じになり、
	// 遅いと止まって見える
	constexpr float PulseSec = 1.9f;

	// 明るさの下限と上限。
	// 0まで落とすと消えたように見えて、点滅と変わらなくなる
	constexpr float GlowMin = 0.28f;
	constexpr float GlowMax = 1.0f;

	// 帯の後ろに敷く薄い下地。
	// これが無いと、明るさが下がったときに帯そのものが消える
	constexpr float TrackAlpha = 0.16f;

	// 受け取り中は、進んだぶんだけ帯を伸ばす。
	// そのときは行き来を止める(進み具合が読めなくなるため)
	constexpr float BusyGlow = 0.92f;

	//===== 出るまでの間 =====
	// 起動してすぐ出すと、画面が組み上がる前に現れてちらつく。
	// 少し置いてから、すっと出す
	constexpr float AppearDelay = 0.6f;
	constexpr float AppearSec   = 0.45f;

	//===== 色 =====
	// 主色(アシッド緑)。タイトルの他の要素と同じ色を使う。
	// ここだけ別の色にすると、知らせだけが浮く
	const Math::Color Glow = UIConst::ACID;
	const Math::Color Text = UIConst::INK;
	const Math::Color Sub  = UIConst::MUTE;
}
