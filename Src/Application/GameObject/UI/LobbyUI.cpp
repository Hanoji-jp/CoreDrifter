#include "LobbyUI.h"
#include "../../Input/HjKeyInput.h"

using namespace UIConst;
using namespace MultiConst;
namespace U = HjUI;

namespace
{
	const char* kFilters[] = { "ALL", "RANKED", "CASUAL", "PRIVATE" };
	constexpr int kFilterCount = 4;

	// 列の左端。等分割にしない。
	// PINGのような短い列に無駄な幅が付くと、全体が間延びする。
	const float kColX[5] = { PadX + 20.0f,  PadX + 700.0f, PadX + 870.0f,
	                         PadX + 1130.0f, PadX + 1270.0f };
	const char* const kCols[5] = { "ROOM NAME", "MODE", "TRACK", "PLAYERS", "PING" };
	constexpr int kColCount = 5;

	// 一覧が空のときも表の高さを保つ。行数で高さが変わると、
	// 下のボタンが上下に動いて押し場所が定まらない
	constexpr int kMinRows = 6;
}

void LobbyUI::Update()
{
	U::BeginInput();

	const int rows = static_cast<int>(m_rooms.size());
	if (rows > 0)
	{
		if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Up))   { m_selected = (m_selected + rows - 1) % rows; }
		if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Down)) { m_selected = (m_selected + 1) % rows; }
	}

	for (int i = 0; i < kFilterCount; ++i) { (void)i; }   // 絞り込みは一覧が入ってから

	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Cancel)) { m_back = true; }
	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Decide)) { m_quickJoin = true; }
	if (HjKeyInput::Instance().Pressed('C'))       { m_create = true; }
}

void LobbyUI::DrawSprite()
{
	DrawFrame("MULTIPLAYER // ROOM LIST", "LOBBY");

	// 一覧に出せる部屋が無いので、代わりに繋ぎ方を出す
	DrawStatRight("CONNECTION", "DIRECT IP");

	DrawGuides(360.0f, 200.0f, 260.0f);
	DrawGuides(1290.0f, 140.0f, 300.0f);
	{
		Math::Color dot = INK; dot.w = DotAlpha;
		U::Deco::DotGrid(1210.0f, 760.0f, 11, 4, DotGap, dot);
	}

	DrawFilters();
	DrawTable();
}

//----------------------------------------------------------
// 絞り込みのチップ。
// 選んでいるものだけ塗り、他は枠だけ。
//----------------------------------------------------------
void LobbyUI::DrawFilters()
{
	float x = PadX;
	for (int i = 0; i < kFilterCount; ++i)
	{
		const float w = U::Measure(FontRow, kFilters[i], 0.0f) / Scale + FilterPadX * 2.0f;
		const bool on = (i == m_filter);

		if (on) { U::RectTL(x, FilterY, w, FilterH, ACID); }
		else    { U::FrameTL(x, FilterY, w, FilterH, 2.0f, INK); }

		U::TextAt(FontRow, x + FilterPadX,
		          FilterY + CenterInBox(FilterH, FontPx(FontRow)), kFilters[i], INK);
		x += w + FilterGap;
	}
}

void LobbyUI::DrawTable()
{
	const int rows = std::max(static_cast<int>(m_rooms.size()), kMinRows);
	const float h = HeadH + RowH * rows;

	U::FrameTL(PadX, TableY, ContentW, h, 2.0f, INK);
	U::TableHead(PadX, TableY, ContentW, kCols, kColX, kColCount);

	if (m_rooms.empty())
	{
		DrawEmpty(TableY + HeadH);
		DrawFooter(TableY + h);
		return;
	}

	for (int i = 0; i < static_cast<int>(m_rooms.size()); ++i)
	{
		const Room& r = m_rooms[i];
		const float ry = TableY + HeadH + i * RowH;
		const bool  sel  = (i == m_selected);
		const bool  full = (r.capacity > 0 && r.players >= r.capacity);

		if (sel)   { U::RectTL(PadX + 2.0f, ry, ContentW - 4.0f, RowH, ACID); }
		if (i > 0) { U::LineD(PadX, ry, PadX + ContentW, ry, 2.0f, INK); }

		// 満員の部屋は薄く落とす。押せない物は押せない見た目にする
		Math::Color ink = INK;
		if (full) { ink.w = 0.45f; }

		// 行の中では、フォントごとに中央へ揃える。
		// 全部を同じ数値で下げると、大きいフォントほど下へはみ出す。
		auto rowY = [ry](int fontId) { return ry + CenterInBox(RowH, FontPx(fontId)); };
		U::TextAt(FontCard, kColX[0], rowY(FontCard), r.name,  ink);
		U::TextAt(FontRow, kColX[1], rowY(FontRow), r.mode,  ink);
		U::TextAt(FontRow, kColX[2], rowY(FontRow), r.track, MUTE);

		char pl[16];
		snprintf(pl, sizeof(pl), "%d / %d", r.players, r.capacity);
		U::TextAt(FontRow, kColX[3], rowY(FontRow), pl, ink);

		char pg[16];
		snprintf(pg, sizeof(pg), "%dMS", r.pingMs);
		// 遅い回線は色相を増やさず、濃い段で警告する
		U::TextAt(FontRow, kColX[4], rowY(FontRow), pg, (r.pingMs > 80) ? ACID_HL : ink);
	}

	DrawFooter(TableY + h);
}

//----------------------------------------------------------
// 一覧が空のとき。
// 「無い」ことと「代わりに何ができるか」を並べて出す。
// 空欄のまま置くと、読み込み中なのか壊れているのか分からない。
//----------------------------------------------------------
void LobbyUI::DrawEmpty(float tableTop)
{
	const float cy = tableTop + RowH * 1.4f;
	U::TextAtC(FontCard, PadX + ContentW * 0.5f, cy, "NO ROOMS FOUND", MUTE);
	U::TextAtC(FontFoot, PadX + ContentW * 0.5f, cy + 34.0f, "MATCHMAKING IS NOT RUNNING  //  CREATE A ROOM AND SHARE YOUR ADDRESS",
	         SUBTXT);
}

void LobbyUI::DrawFooter(float tableBottom)
{
	const float by = tableBottom + FooterGap;

	// 一覧が空なら参加はできない。押せない状態を見た目で示す
	const auto joinKind = m_rooms.empty() ? U::BtnKind::Disabled : U::BtnKind::Primary;
	U::Button(PadX, by, ButtonW, ButtonH, "QUICK JOIN", joinKind, "ENTER");
	U::Button(PadX + ButtonW + ButtonGap, by, ButtonW, ButtonH,
	          "CREATE ROOM", U::BtnKind::Secondary, "C");

	U::Keycap(CanvasW - PadX - 160.0f, by + 12.0f, "ESC", "BACK");
}
