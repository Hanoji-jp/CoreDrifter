#pragma once

#include "../GameObject/UI/UIConst.h"

// 他のプレイヤーの車の上に出す名前札の定数。
//
// ■ なぜ3Dの文字ではなく2Dで描くか
// 車の上に板を置いて文字を貼ると、カメラの向きによって潰れて読めなくなる。
// 常にこちらを向かせる手もあるが、遠いほど小さくなるので結局読めない。
// 画面の座標へ変換して2Dで描けば、距離に関係なく同じ大きさで読める。
namespace NamePlateConst
{
	// 車の中心からどれだけ上に出すか(m)。
	// 車体の高さより少し上。低いと屋根に埋まり、高いと誰の名前か分からない
	constexpr float HeightOffset = 1.6f;

	// これより遠い相手は出さない(m)。
	// 全部出すと画面が名前で埋まる。近くの相手だけ分かればよい
	constexpr float MaxDistance = 120.0f;

	// 薄くなり始める距離(m)。ここからMaxDistanceにかけて消えていく。
	// 距離で急に消すと、走っているだけで名前が点滅する
	constexpr float FadeStart = 90.0f;

	// 名前に使うフォント。日本語も出せるものを選ぶ
	// (プレイヤー名は日本語でも入力できるようにしてある)
	const int FontId = UIConst::FontCJKSmall;

	// 文字の後ろに敷く板。
	// 明るい路面や空の上だと白文字が読めなくなるので、暗い板を敷く
	constexpr float PadX = 8.0f;    // 文字の左右の余白(デザインpx)
	constexpr float PadY = 3.0f;    // 上下の余白
	constexpr float PlateAlpha = 0.55f;   // 板の濃さ

	// 板の下に出す小さな三角(どの車の名前かを指す)。
	// 名前だけだと、車が近いときにどちらのものか分からない
	constexpr float PointerW = 10.0f;
	constexpr float PointerH = 6.0f;
}
