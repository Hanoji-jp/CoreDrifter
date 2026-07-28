#include "ElementsUI.h"
#include <utility>

void ElementsUI::DrawSprite()
{
	using namespace UIConst;
	namespace U = HjUI;

	// 背景(紙)
	KdShaderManager::Instance().m_spriteShader.DrawBox(0, 0, ScreenW / 2, ScreenH / 2, &PAPER, true);

	const float PADX = 64.0f, PADY = 48.0f;

	// 見出し ELEMENTS / UI KIT
	const float endX = U::Text(FontHead, PADX, PADY, 47.0f, "ELEMENTS ", INK) / Scale;
	U::Text(FontHead, endX, PADY + 6.0f, 47.0f, "/ UI KIT", ACID);

	// 明滅ドット(右上)
	U::DotFieldTwinkle(1200.0f, 30.0f, 150.0f, 40.0f, DOTS, 0.0f);
	U::DotFieldTwinkle(1370.0f, 30.0f, 90.0f, 40.0f, GREY9, 1.5f);

	// 3列×3行のラベル付きセル
	const float gx = PADX, gy = PADY + 100.0f;
	const float colW = (DesignW - PADX * 2.0f - 96.0f) / 3.0f, colGap = 48.0f;
	const float rowH = 240.0f;
	auto cellX = [&](int c) { return gx + c * (colW + colGap); };
	auto cellY = [&](int r) { return gy + r * rowH; };
	auto head  = [&](int c, int r, const char* t) -> std::pair<float, float>
	{
		const float x = cellX(c), y = cellY(r);
		U::Text(FontSmall, x, y, 12.0f, t, INK);
		U::LineD(x, y + 20.0f, x + colW, y + 20.0f, 2.0f, INK);
		return { x, y + 36.0f };
	};

	// 01 BUTTON
	{ auto [x, y] = head(0, 0, "01 - BUTTON");
	  U::Button(x, y,          220.0f, 46.0f, "PRIMARY",   U::BtnKind::Primary, ">");
	  U::Button(x, y + 58.0f,  220.0f, 46.0f, "SECONDARY", U::BtnKind::Secondary);
	  U::Button(x, y + 116.0f, 120.0f, 46.0f, "GHOST",     U::BtnKind::Ghost);
	  U::Button(x + 134.0f, y + 116.0f, 120.0f, 46.0f, "DISABLED", U::BtnKind::Disabled); }

	// 02 ICON BUTTON
	{ auto [x, y] = head(1, 0, "02 - ICON BUTTON");
	  U::IconButton(x,          y, 52.0f, 0, 0);   // ▶ solid
	  U::IconButton(x + 66.0f,  y, 52.0f, 1, 1);   // ＋ outline
	  U::IconButton(x + 132.0f, y, 52.0f, 2, 2); } // ✕ accent

	// 03 TOGGLE
	{ auto [x, y] = head(2, 0, "03 - TOGGLE");
	  U::Toggle(x, y,          true,  ACID);
	  U::Toggle(x, y + 48.0f,  false, ACID);
	  U::Toggle(x, y + 96.0f,  true,  INK); }  // アクセント(黒塗り)

	// 04 TAB BUTTON
	{ auto [x, y] = head(0, 1, "04 - TAB BUTTON");
	  U::Tab(x, y,          "GAMEPLAY", true);
	  U::Tab(x, y + 44.0f,  "CONTROLS", false);
	  U::Tab(x, y + 88.0f,  "GRAPHICS", false); }

	// 05 STEPPER / SELECT
	{ auto [x, y] = head(1, 1, "05 - STEPPER / SELECT");
	  U::Stepper(x, y,         190.0f, "NORMAL");
	  U::Stepper(x, y + 50.0f, 190.0f, "KM/H"); }

	// 06 KEYCAP
	{ auto [x, y] = head(2, 1, "06 - KEYCAP");
	  U::Keycap(x, y,         "ENTER", "SELECT");
	  U::Keycap(x, y + 42.0f, "ESC", "BACK"); }

	// 07 TAG / BADGE
	{ auto [x, y] = head(0, 2, "07 - TAG / BADGE");
	  U::Badge2(x, y,         "DRIVER01", "LV.23", ACID);
	  U::Badge2(x, y + 44.0f, "TIER", "A", PAPER); }

	// 08 STAT BAR(アニメで伸び縮み)
	{ auto [x, y] = head(1, 2, "08 - STAT BAR");
	  const float t = U::Time();
	  U::StatBar(x, y,         240.0f, "SPEED", 0.6f + 0.18f * (0.5f + 0.5f * std::sin(t * 1.6f)));
	  U::StatBar(x, y + 50.0f, 240.0f, "DRIFT", 0.7f + 0.22f * (0.5f + 0.5f * std::sin(t * 2.1f + 1.0f))); }

	// 09 WINDOW HEADER (4アクセント色)
	{ auto [x, y] = head(2, 2, "09 - WINDOW HEADER");
	  U::WindowHeader(x, y,          260.0f, "CONFIRM",     ACID, INK);
	  U::WindowHeader(x, y + 42.0f,  260.0f, "NOTICE",      PINK, INK);
	  U::WindowHeader(x, y + 84.0f,  260.0f, "DELETE",      BLUE, WHITE);
	  U::WindowHeader(x, y + 126.0f, 260.0f, "INFORMATION", SAND, INK); }
}
