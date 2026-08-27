#include "ToastUI.h"
#include "HjToastQueue.h"
#include "HjUiVisibility.h"

using namespace UIConst;
using namespace ToastConst;
namespace U = HjUI;

void ToastUI::Update()
{
	const float dt = KdFPSController::GetDt();
	auto& items = HjToastQueue::Instance().Items();

	// 古いものから消す。消える途中は下の行が繰り上がる
	for (auto it = items.begin(); it != items.end(); )
	{
		it->age += dt;
		if (it->age > SlideTime + HoldTime + FadeTime) { it = items.erase(it); }
		else                                           { ++it; }
	}
}

void ToastUI::DrawSprite()
{
	if (!HjUiVisibility::Instance().showToast) { return; }

	auto& items = HjToastQueue::Instance().Items();

	float y = Y;
	for (const auto& t : items)
	{
		// 出てくる：左から滑り込む。位置が動くと視界の端でも気付ける
		const float slide = std::clamp(t.age / SlideTime, 0.0f, 1.0f);
		const float x = X - (1.0f - slide) * SlideDist;

		// 消える：最後だけ薄くする。急に消えると見落とす
		const float left = (SlideTime + HoldTime + FadeTime) - t.age;
		const float fade = std::clamp(left / FadeTime, 0.0f, 1.0f);
		const float a = slide * fade;

		// 背景。映像が透ける程度に留める
		Math::Color bg = INK; bg.w = BgAlpha * a;
		U::RectTL(x, y, W, H, bg);
		Math::Color edge = WHITE; edge.w = a;
		U::FrameTL(x, y, W, H, 2.0f, edge);

		// 種別のチップ。良い出来事だけアシッドで塗る
		Math::Color chipInk = WHITE; chipInk.w = a;
		const float kw = U::Chip(x + PadX, y + CenterInBox(H, ChipH),
		                         t.kicker, t.hot, chipInk);

		// 本文と数値。数値は右端に置くので、本文が伸びても位置が動かない。
		// 本文の幅は、数値の手前までに収める(はみ出すと重なる)。
		Math::Color tc = WHITE; tc.w = a;
		Math::Color vc = t.hot ? ACID : WHITE; vc.w = a;

		const float valueW = (t.value[0] != 0)
			? U::Measure(FontTab, t.value, 0.0f) / Scale : 0.0f;

		const float textX = x + PadX + kw + PadX;
		const float textY = y + CenterInBox(H, FontPx(FontTab));

		U::PushClip(textX, y, (x + W - PadX - valueW - ValueGap) - textX, H);
		U::TextAt(FontTab, textX, textY, t.text, tc);
		U::PopClip();

		if (valueW > 0.0f)
		{
			U::TextAtR(FontTab, x + W - PadX, textY, t.value, vc);
		}

		y += H + Gap;
	}
}
