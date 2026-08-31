#include "HjModMenu.h"

#include "HjUI.h"
#include "../Car/CarBase.h"
#include "../../Mod/HjModCatalog.h"
#include "../../Mod/HjModProfile.h"
#include "../../Input/HjKeyInput.h"

namespace MM = ModMenuConst;

namespace
{
	// 数値を桁を決めて文字にする。
	// %g だと 0.001 が 1e-03 になって読めない
	std::string NumText(float v)
	{
		char buf[32] = {};
		snprintf(buf, sizeof(buf), "%.3f", v);
		return buf;
	}
}

//----------------------------------------------------------
void HjModMenu::Init()
{
	// ※描画の種類は立てない。DrawSprite は種別に関わらず呼ばれる。
	//   ここで Lit などを指定すると、2Dしか描かないのに
	//   3Dの描画順にも並ぶことになる。

	// 一覧はここで1回だけ調べる。
	// フォルダを見に行くのは遅いので、開くたびにはやらない
	HjModCatalog::Instance().Rescan();

	// モデルごとの合わせ込みを読む。
	// 差し替えたときに、前に合わせた値がそのまま戻るように
	HjModProfile::Instance().Load();
}

//----------------------------------------------------------
// 行を組む
//----------------------------------------------------------
void HjModMenu::BuildRows()
{
	m_rows.clear();

	auto car = m_wpCar.lock();
	if (!car) { return; }

	switch (m_page)
	{
	case Page::Root:      BuildRoot(car);         break;
	case Page::BodyList:  BuildList(car, true);   break;
	case Page::WheelList: BuildList(car, false);  break;
	case Page::Adjust:    BuildAdjust(car);       break;
	}

	// 選んでいる行が消えることがある(一覧を更新した後など)。
	// はみ出したままにすると、決定を押したときに何も起きない
	if (m_index >= static_cast<int>(m_rows.size()))
	{
		m_index = m_rows.empty() ? 0 : static_cast<int>(m_rows.size()) - 1;
	}
}

void HjModMenu::BuildRoot(std::shared_ptr<CarBase>& car)
{
	auto& cat = HjModCatalog::Instance();

	// いま何を使っているかを、潜る前に見せる。
	// 潜らないと分からないと、確かめるだけで往復することになる
	auto nameOf = [&](HjModCatalog::Kind kind, const std::string& cur) -> std::string
	{
		if (cur == ModConst::StockMark) { return U8("標準"); }

		const int i = cat.IndexOf(kind, cur);
		if (i < 0) { return U8("見つかりません"); }
		return cat.List(kind)[i].name;
	};

	Row body;
	body.kind  = RowKind::Submenu;
	body.label = U8("車体のモデル");
	body.value = nameOf(HjModCatalog::Kind::Body, car->GetBodyModPath());
	body.to    = Page::BodyList;
	m_rows.push_back(body);

	Row wheel;
	wheel.kind  = RowKind::Submenu;
	wheel.label = U8("ホイールのモデル");
	wheel.value = nameOf(HjModCatalog::Kind::Wheel, car->GetWheelModPath());
	wheel.to    = Page::WheelList;
	m_rows.push_back(wheel);

	Row adj;
	adj.kind  = RowKind::Submenu;
	adj.label = U8("向きと大きさを合わせる");
	adj.to    = Page::Adjust;
	m_rows.push_back(adj);

	Row scan;
	scan.kind  = RowKind::Action;
	scan.act   = ActionKind::Rescan;
	scan.label = U8("一覧を読み直す");
	scan.value = U8("Asset/Mods");
	m_rows.push_back(scan);
}

void HjModMenu::BuildList(std::shared_ptr<CarBase>& car, bool body)
{
	auto& cat = HjModCatalog::Instance();
	const auto kind = body ? HjModCatalog::Kind::Body : HjModCatalog::Kind::Wheel;
	const std::string& cur = body ? car->GetBodyModPath() : car->GetWheelModPath();

	// 先頭は必ず「標準」。
	// 差し替えて形が崩れたとき、一番上に戻り道が無いと不安になる
	Row stock;
	stock.kind   = RowKind::Choice;
	stock.label  = U8("標準");
	stock.choice = -1;
	if (cur == ModConst::StockMark) { stock.value = U8("使用中"); }
	m_rows.push_back(stock);

	const auto& list = cat.List(kind);
	for (size_t i = 0; i < list.size(); ++i)
	{
		Row r;
		r.kind   = RowKind::Choice;
		r.label  = list[i].name;
		r.choice = static_cast<int>(i);

		// 大きさを添える。重いモデルを選ぶ前に気づけるように
		if (list[i].path == cur) { r.value = U8("使用中"); }
		else                     { r.value = std::to_string(list[i].sizeKb) + " KB"; }

		m_rows.push_back(r);
	}

	if (list.empty())
	{
		Row none;
		none.kind  = RowKind::Action;
		none.label = body ? U8("Asset/Mods/Body に置く")
		                  : U8("Asset/Mods/Wheel に置く");
		none.value = U8("空");
		m_rows.push_back(none);
	}
}

void HjModMenu::BuildAdjust(std::shared_ptr<CarBase>& car)
{
	// 車が持っている調整値をそのまま並べる。
	// ここで別に値を持つと、開発用パネル(F2)と食い違う
	for (const auto& p : car->AppearanceParamList())
	{
		Row r;
		r.kind   = RowKind::Slider;
		r.label  = p.first;
		r.target = p.second;
		r.value  = NumText(*p.second);

		// 名前で動かす幅を決める。
		// 大きさは100倍のモデルまで来るので広く、
		// 向きは1周ぶん、位置は車の寸法ぶんあれば足りる
		// 名前で動かす幅を決める。
		// ※「車体の大きさ」と「車体の高さ」を取り違えないよう、
		//   先に「大きさ」を見てから位置へ倒す
		const std::string label = p.first;
		if (label.find(U8("向き")) != std::string::npos)
		{
			r.min = MM::AngleMin; r.max = MM::AngleMax; r.step = MM::AngleStep;
		}
		else if (label.find(U8("大きさ")) != std::string::npos)
		{
			r.min = MM::ScaleMin; r.max = MM::ScaleMax; r.step = MM::ScaleStep;
		}
		else
		{
			r.min = MM::OffsetMin; r.max = MM::OffsetMax; r.step = MM::OffsetStep;
		}

		m_rows.push_back(r);
	}

	// 標準の車は、車の調整(CarTune)が受け持つ。
	// こちらで書き出す先が無いので、保存の行は出さない
	if (ProfileKey().empty()) { return; }

	// 触った値はすぐには書かない。
	// 合わせている最中は行き過ぎたり戻したりするので、
	// 途中の値を毎回書くと「やっぱり前のほうが良かった」が効かない
	Row save;
	save.kind  = RowKind::Action;
	save.act   = ActionKind::SaveProf;
	save.label = U8("この形で覚える");
	if (HjModProfile::Instance().IsDirty()) { save.value = U8("未保存"); }
	m_rows.push_back(save);

	Row revert;
	revert.kind  = RowKind::Action;
	revert.act   = ActionKind::RevertProf;
	revert.label = U8("覚えた所まで戻す");
	m_rows.push_back(revert);

	Row reset;
	reset.kind  = RowKind::Action;
	reset.act   = ActionKind::ResetProf;
	reset.label = U8("合わせ込みを捨てる");
	m_rows.push_back(reset);
}

//----------------------------------------------------------
// モデルごとの合わせ込み
//----------------------------------------------------------
std::string HjModMenu::ProfileKey() const
{
	auto car = m_wpCar.lock();
	if (!car) { return ""; }

	// 車体とホイールをまとめて1つの鍵にする。
	// 別々に持つと、組み合わせを変えるたびに片方だけ合っている、
	// という中途半端な状態になる
	const std::string& b = car->GetBodyModPath();
	const std::string& w = car->GetWheelModPath();

	// どちらも標準なら記録しない。車の調整が受け持つ
	if (b == ModConst::StockMark && w == ModConst::StockMark) { return ""; }

	return b + "|" + w;
}

void HjModMenu::ApplyProfile(const std::string& path)
{
	auto car = m_wpCar.lock();
	if (!car || path.empty()) { return; }

	// まだ合わせていないモデルは、車のいまの値のままにする。
	// ここで既定値へ倒すと、差し替えた瞬間に車が潰れて、
	// 何が起きたのか分からなくなる
	if (!HjModProfile::Instance().Has(path)) { return; }

	const auto e = HjModProfile::Instance().Get(path);
	const auto params = car->AppearanceParamList();

	// 並びは AppearanceParamList と揃えてある。
	// 名前で引くと、文言を直した瞬間に効かなくなる
	const float vals[] = {
		e.bodyScale, e.bodyYaw, e.wheelScale, e.wheelYaw,
		e.track, e.base, e.wheelH,
		e.bodyOffY, e.bodyOffZ, e.bodyOffX,
	};

	const size_t n = std::min(params.size(), std::size(vals));
	for (size_t i = 0; i < n; ++i) { *params[i].second = vals[i]; }
}

void HjModMenu::CaptureProfile()
{
	auto car = m_wpCar.lock();
	if (!car) { return; }

	const std::string key = ProfileKey();
	if (key.empty()) { return; }

	const auto params = car->AppearanceParamList();

	HjModProfile::Entry e;
	float* const dst[] = {
		&e.bodyScale, &e.bodyYaw, &e.wheelScale, &e.wheelYaw,
		&e.track, &e.base, &e.wheelH,
		&e.bodyOffY, &e.bodyOffZ, &e.bodyOffX,
	};

	const size_t n = std::min(params.size(), std::size(dst));
	for (size_t i = 0; i < n; ++i) { *dst[i] = *params[i].second; }

	HjModProfile::Instance().Set(key, e);
}

void HjModMenu::SaveProfile()
{
	CaptureProfile();
	HjModProfile::Instance().Save();
}

void HjModMenu::RevertProfile()
{
	// ファイルの中身へ戻す。
	// 手元の触りかけを捨てるので、読み直してから当てる
	HjModProfile::Instance().Load();
	ApplyProfile(ProfileKey());
}

//----------------------------------------------------------
// 入力
//----------------------------------------------------------
bool HjModMenu::Repeat(int vk, float& timer, bool pressed, bool down)
{
	(void)vk;

	// 押した瞬間は必ず1回通す
	if (pressed)
	{
		timer = 0.0f;
		return true;
	}

	if (!down)
	{
		timer = 0.0f;
		return false;
	}

	// 押し続けている。
	// すぐ連射すると細かい調整ができないので、少し待ってから
	timer += KdFPSController::GetDt();
	if (timer < MM::HoldDelay) { return false; }

	// 一定の間隔で拾う。
	// フレームごとに1回だと、フレームレートで速さが変わってしまう
	const float interval = 1.0f / MM::HoldRate;
	if (timer >= MM::HoldDelay + interval)
	{
		timer -= interval;
		return true;
	}
	return false;
}

void HjModMenu::UpdateInput()
{
	auto& key = HjKeyInput::Instance();

	// 上下で選ぶ。端で折り返す
	if (!m_rows.empty())
	{
		const int n = static_cast<int>(m_rows.size());
		if (key.Pressed(HjKeyInput::Key::Up))   { m_index = (m_index - 1 + n) % n; }
		if (key.Pressed(HjKeyInput::Key::Down)) { m_index = (m_index + 1) % n; }
	}

	// 送り。選んでいる行が枠から出ないところまで動かす
	if (m_index < m_top) { m_top = m_index; }
	if (m_index >= m_top + MM::VisibleRows) { m_top = m_index - MM::VisibleRows + 1; }

	// 左右で数値を動かす。
	// 押しっぱなしを拾わないと、100倍のモデルを合わせるのに時間がかかる
	const bool lP = key.Pressed(HjKeyInput::Key::Left);
	const bool rP = key.Pressed(HjKeyInput::Key::Right);
	const bool lD = key.Down(HjKeyInput::Key::Left);
	const bool rD = key.Down(HjKeyInput::Key::Right);

	if (Repeat(VK_LEFT,  m_holdL, lP, lD)) { StepValue(-1); }
	if (Repeat(VK_RIGHT, m_holdR, rP, rD)) { StepValue(+1); }

	if (key.Pressed(HjKeyInput::Key::Decide)) { Decide(); }

	// 戻る。
	// ESC は場面の側がポーズに使っているので、ここでは取らない。
	// 取ると、閉じたつもりがポーズも開く、という重なりが起きる
	if (key.Pressed(VK_BACK)) { GoBack(); }
}

void HjModMenu::Decide()
{
	if (m_rows.empty()) { return; }

	auto car = m_wpCar.lock();
	if (!car) { return; }

	const Row& r = m_rows[m_index];

	switch (r.kind)
	{
	case RowKind::Submenu:
		m_page  = r.to;
		m_index = 0;
		m_top   = 0;
		m_last  = HjModLoader::Result::Ok;
		break;

	case RowKind::Choice:
	{
		auto& cat = HjModCatalog::Instance();
		const bool body = (m_page == Page::BodyList);
		const auto kind = body ? HjModCatalog::Kind::Body : HjModCatalog::Kind::Wheel;

		// -1 は「標準へ戻す」
		const std::string path = (r.choice < 0)
			? std::string(ModConst::StockMark)
			: cat.List(kind)[r.choice].path;

		m_last = body ? car->SetBodyModel(path) : car->SetWheelModel(path);

		// どのモデルを使うかは選んだ時点で覚える。
		// これは1つの選択で、途中の状態が無いので、後から迷いようがない
		car->SaveModChoice();

		// このモデルに合わせ込みがあれば当てる。
		// 無ければ車のいまの値のまま(差し替えた瞬間に潰れないように)
		ApplyProfile(ProfileKey());
		break;
	}

	case RowKind::Action:
		switch (r.act)
		{
		case ActionKind::Rescan:     HjModCatalog::Instance().Rescan(); break;
		case ActionKind::SaveProf:   SaveProfile();                     break;
		case ActionKind::RevertProf: RevertProfile();                   break;
		case ActionKind::ResetProf:
			HjModProfile::Instance().Erase(ProfileKey());
			HjModProfile::Instance().Save();
			break;
		case ActionKind::None:       break;
		}
		break;

	case RowKind::Slider:
		// 決定では何もしない。数値は左右で動かす
		break;
	}
}

void HjModMenu::StepValue(int dir)
{
	if (m_rows.empty()) { return; }

	Row& r = m_rows[m_index];
	if (r.kind != RowKind::Slider || !r.target) { return; }

	*r.target = std::clamp(*r.target + r.step * dir, r.min, r.max);

	// ※ここではファイルへ書かない。
	//   合わせている最中は行き過ぎたり戻したりするので、
	//   途中の値を毎回書くと「やっぱり前のほうが良かった」が効かない。
	//   手元に覚えておいて、書き出すのは「この形で覚える」を選んだときだけ
	CaptureProfile();
}

void HjModMenu::GoBack()
{
	if (m_page == Page::Root)
	{
		// 入口で戻るを押したら閉じる
		m_open = false;
		return;
	}

	m_page  = Page::Root;
	m_index = 0;
	m_top   = 0;
	m_last  = HjModLoader::Result::Ok;
}

//----------------------------------------------------------
void HjModMenu::Update()
{
	auto& key = HjKeyInput::Instance();
	const float dt = KdFPSController::GetDt();

	// 開閉。
	// ※文字入力中は取らない。名前を打っている最中の TAB で
	//   メニューが開くと、打った文字がどこへ行ったか分からなくなる
	if (key.Pressed(VK_TAB) && !key.IsImeComposing())
	{
		m_open = !m_open;
		if (m_open)
		{
			m_page  = Page::Root;
			m_index = 0;
			m_top   = 0;
			m_last  = HjModLoader::Result::Ok;
		}
	}

	// 横から出し入れする。一瞬で現れると、
	// 何が起きたのか分からないまま視界の左が埋まる
	const float target = m_open ? 1.0f : 0.0f;
	const float speed  = (MM::SlideSec > 0.0f) ? (dt / MM::SlideSec) : 1.0f;
	if (m_slide < target) { m_slide = std::min(m_slide + speed, target); }
	else                  { m_slide = std::max(m_slide - speed, target); }

	if (!m_open) { return; }

	// 開いている間だけ組み直す。
	// 候補が増減しても、表示と中身がずれない
	BuildRows();
	UpdateInput();
}

//----------------------------------------------------------
// 描画
//----------------------------------------------------------
void HjModMenu::DrawSprite()
{
	if (m_slide <= 0.0f) { return; }

	// 閉じている途中も描く。位置と濃さで出し入れを見せる
	const float ox = MM::SlideFrom * (1.0f - m_slide);
	const float x  = MM::PanelX + ox;

	const int shown = std::min(static_cast<int>(m_rows.size()), MM::VisibleRows);
	const float bodyH = MM::RowH * static_cast<float>(std::max(shown, 1));
	const float h = MM::HeadH + bodyH + MM::FootH;

	// 地
	Math::Color panel = MM::Panel;
	panel.w *= m_slide;
	HjUI::RectTL(x, MM::PanelY, MM::PanelW, h, panel);

	// 見出し
	Math::Color head = MM::Head;
	head.w *= m_slide;
	HjUI::RectTL(x, MM::PanelY, MM::PanelW, MM::HeadH, head);

	Math::Color on   = MM::TextOff; on.w   *= m_slide;
	Math::Color dim  = MM::TextDim; dim.w  *= m_slide;
	Math::Color line = MM::Line;    line.w *= m_slide;

	HjUI::TextAt(UIConst::FontCJKHead, x + MM::PadX, MM::PanelY + 18.0f,
	             PageTitle(), on);

	// 入口以外は、戻れることを見出しの右へ出す。
	// 潜ったあとに戻り方が分からないと、閉じるしかなくなる
	if (m_page != Page::Root)
	{
		HjUI::TextAtR(UIConst::FontFoot, x + MM::PanelW - MM::PadX,
		              MM::PanelY + 22.0f, U8("BS"), dim);
	}

	HjUI::LineD(x, MM::PanelY + MM::HeadH, x + MM::PanelW, MM::PanelY + MM::HeadH,
	            1.0f, line);

	// 行
	const float rowsY = MM::PanelY + MM::HeadH;
	for (int i = 0; i < shown; ++i)
	{
		const int idx = m_top + i;
		if (idx >= static_cast<int>(m_rows.size())) { break; }

		const Row& r = m_rows[idx];
		const float ry = rowsY + MM::RowH * static_cast<float>(i);
		const bool sel = (idx == m_index);

		Math::Color label = on;

		if (sel)
		{
			// 面で塗る。枠だけだと、走行画面の上では背景に紛れる
			Math::Color fill = MM::RowSel;
			fill.w *= m_slide;
			HjUI::RectTL(x, ry, MM::PanelW, MM::RowH, fill);

			label = MM::TextOn;
			label.w *= m_slide;
		}
		else
		{
			// 選んでいない行の左に細い柱。
			// 潜れる行かどうかを、記号を増やさずに示す
			if (r.kind == RowKind::Submenu)
			{
				Math::Color bar = MM::TextDim;
				bar.w *= m_slide * 0.6f;
				HjUI::RectTL(x, ry, MM::MarkW, MM::RowH, bar);
			}
		}

		HjUI::TextAt(UIConst::FontCJKRow, x + MM::PadX + MM::MarkW,
		             ry + 9.0f, r.label.c_str(), label);

		// 右に値。選択行は塗りの上なので同じ色で、
		// それ以外は落として、行の頭が読みやすいようにする
		if (!r.value.empty())
		{
			Math::Color v = sel ? label : dim;
			HjUI::TextAtR(UIConst::FontCJKRow, x + MM::PanelW - MM::ValueRightPad,
			              ry + 9.0f, r.value.c_str(), v);
		}
		// 数値の行は、左右で動かせることを選択中だけ示す
		else if (sel && r.kind == RowKind::Slider)
		{
			HjUI::TextAtR(UIConst::FontCJKRow, x + MM::PanelW - MM::ValueRightPad,
			              ry + 9.0f, NumText(r.target ? *r.target : 0.0f).c_str(), label);
		}
	}

	// 足元
	const float footY = rowsY + bodyH;
	HjUI::LineD(x, footY, x + MM::PanelW, footY, 1.0f, line);

	// 読み込めなかった理由。次に何か選ぶまで出す。
	// 出さないと「押したのに変わらない」だけになる
	if (m_last != HjModLoader::Result::Ok)
	{
		Math::Color warn = MM::Warn;
		warn.w *= m_slide;
		HjUI::TextAt(UIConst::FontFoot, x + MM::PadX, footY + 11.0f,
		             HjModLoader::Message(m_last), warn);
		return;
	}

	// 何番目か。送っているときに、どのあたりを見ているのかが分かる
	if (!m_rows.empty())
	{
		const std::string pos = std::to_string(m_index + 1) + " / "
		                      + std::to_string(m_rows.size());
		HjUI::TextAt(UIConst::FontFoot, x + MM::PadX, footY + 11.0f,
		             pos.c_str(), dim);
	}

	HjUI::TextAtR(UIConst::FontFoot, x + MM::PanelW - MM::PadX, footY + 11.0f,
	              U8("TAB CLOSE"), dim);
}

//----------------------------------------------------------
const char* HjModMenu::PageTitle() const
{
	switch (m_page)
	{
	case Page::BodyList:  return U8("車体のモデル");
	case Page::WheelList: return U8("ホイールのモデル");
	case Page::Adjust:    return U8("向きと大きさ");
	case Page::Root:      break;
	}
	return U8("MOD MENU");
}
