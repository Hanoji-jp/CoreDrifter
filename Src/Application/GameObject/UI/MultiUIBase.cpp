#include "MultiUIBase.h"

using namespace UIConst;
using namespace MultiConst;
namespace U = HjUI;

//----------------------------------------------------------
// 薄いガイド線。途中で切って印を置く。
// 通しで引くと画面を分断してしまうので、切れ目を作って
// 「区切りではなく補助線」であることを示す。
//----------------------------------------------------------
void MultiUIBase::DrawGuides(float x, float breakTop, float breakBottom)
{
	Math::Color guide = INK; guide.w = GuideAlpha;
	Math::Color tick  = INK; tick.w  = TickAlpha;

	U::Deco::Guide(x, 0.0f, breakTop, guide);
	U::Deco::Guide(x, breakBottom, CanvasH, guide);
	U::Deco::CropTick(x, breakTop, tick);
	U::Deco::CropTick(x, breakBottom, tick);
}

//----------------------------------------------------------
// 背景と見出し。
//----------------------------------------------------------
void MultiUIBase::DrawFrame(const char* kicker, const char* title)
{
	auto& sp = KdShaderManager::Instance().m_spriteShader;
	sp.DrawBox(0, 0, ScreenW / 2, ScreenH / 2, &PAPER, true);

	// 右上のアシッドの小片。画面の系統を示す印で、情報は持たない
	U::RectTL(1400.0f, 60.0f, 34.0f, 70.0f, ACID);

	U::TextAt(FontFoot, PadX, KickerY, kicker, SUBTXT);
	U::TextAtTracked(FontHead, PadX, TitleY, title, INK, -0.03f * 60.0f);
}

void MultiUIBase::DrawStatRight(const char* label, const char* value)
{
	U::TextAtR(FontFoot, CanvasW - PadX, KickerY, label, SUBTXT);
	U::TextAtR(FontTab, CanvasW - PadX, KickerY + 34.0f, value, INK);
}
