#include "PlayModeUI.h"

namespace
{
	// 暫定4モード(企画=ドリフト峠サンドボックス)＋マルチ(P2Pで棚上げ中=SOON)。
	struct Mode { const char* title; const char* sub; bool soon; };
	const Mode kModes[4] = {
		{ "FREE ROAM",   "EXPLORE THE PASS",  false },
		{ "TIME ATTACK", "BEAT THE CLOCK",    false },
		{ "DRIFT TRIAL", "SCORE YOUR SLIDES", false },
		{ "MULTIPLAYER", "P2P - COMING SOON", true  },
	};
	const int kCount = 4;

	// カード配置(デザイン座標)。4枚が横1列に収まる幅。
	const float kCardY = 280.0f, kCardH = 340.0f, kCardGap = 40.0f;
	const float kGridX = 64.0f;
	const float kCardW = (1536.0f - kGridX * 2.0f - kCardGap * 3.0f) / 4.0f;   // = 322
	float CardX(int i) { return kGridX + i * (kCardW + kCardGap); }
}

void PlayModeUI::GetSelectedCardRect(float& dx, float& dy, float& w, float& h) const
{
	dx = CardX(m_sel); dy = kCardY; w = kCardW; h = kCardH;
}

void PlayModeUI::Update()
{
	const bool lf = (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0 || (GetAsyncKeyState('A') & 0x8000) != 0;
	const bool rt = (GetAsyncKeyState(VK_RIGHT)& 0x8000) != 0 || (GetAsyncKeyState('D') & 0x8000) != 0;
	if (lf && !m_prevL) { m_sel = (m_sel + kCount - 1) % kCount; }
	if (rt && !m_prevR) { m_sel = (m_sel + 1) % kCount; }
	m_prevL = lf; m_prevR = rt;

	// マウス：カードにホバーで選択、クリックで決定
	HjUI::BeginInput();
	bool decide = false;
	for (int i = 0; i < kCount; ++i)
	{
		if (HjUI::Hover(CardX(i), kCardY, kCardW, kCardH)) { m_sel = i; }
		if (HjUI::Clicked(CardX(i), kCardY, kCardW, kCardH)) { m_sel = i; decide = true; }
	}
	if ((GetAsyncKeyState(VK_RETURN) & 0x8000) || (GetAsyncKeyState(VK_SPACE) & 0x8000)) { decide = true; }

	// SOON(マルチ=棚上げ中)は決定できない
	if (decide && !kModes[m_sel].soon) { m_activated = true; }
}

void PlayModeUI::DrawSprite()
{
	using namespace UIConst;
	namespace U = HjUI;
	auto& sp = KdShaderManager::Instance().m_spriteShader;
	sp.DrawBox(0, 0, ScreenW / 2, ScreenH / 2, &PAPER, true);

	const float T = U::Time();

	// ── 装飾＋モーション案(6種すべて) ──
	{
		// ① ドットのトゥインクル：フィールドごとに位相をずらして明滅
		auto twinkleDots = [&](float x, float y, float w, float h, float phase) {
			Math::Color c = DOTS; c.w = 0.14f + 0.28f * (0.5f + 0.5f * std::sin(T * 2.2f + phase));
			U::DotField(x, y, w, h, c);
		};
		twinkleDots(1150.0f, 56.0f, 150.0f, 90.0f, 0.0f);
		twinkleDots(64.0f,   662.0f, 120.0f, 60.0f, 1.3f);
		twinkleDots(1330.0f, 648.0f, 150.0f, 96.0f, 2.1f);
		twinkleDots(556.0f,  120.0f, 80.0f, 44.0f, 3.4f);
		twinkleDots(1060.0f, 700.0f, 96.0f, 44.0f, 0.8f);

		// ③ 十字マークの点滅(＋少し浮遊)
		auto crossBlink = [&](float x, float y, float r, float phase) {
			Math::Color c = INK; c.w = 0.35f + 0.65f * std::fabs(std::sin(T * 3.0f + phase));
			const float fy = std::sin(T * 1.5f + phase) * 4.0f;   // ④フロート
			U::LineD(x - r, y + fy, x + r, y + fy, 2.5f, c); U::LineD(x, y - r + fy, x, y + r + fy, 2.5f, c);
		};
		crossBlink(150.0f, 150.0f, 8.0f, 0.0f);  crossBlink(900.0f, 96.0f, 8.0f, 1.0f);
		crossBlink(1400.0f, 300.0f, 9.0f, 2.0f); crossBlink(240.0f, 560.0f, 6.0f, 3.0f);

		// ⑥ マーキー(横スクロール。黒boxは無しで墨色テキストのみ)
		{
			const char* mq = "DRIFT PROJECT  //  SELECT YOUR MODE  //  BUILT TO SLIDE  //  ";
			const float span = 620.0f;
			float sx = 70.0f - std::fmod(T * 90.0f, span);
			for (int r = 0; r < 4; ++r) { U::Text(FontTiny, sx + r * span, 838.0f, 10.0f, mq, INK); }
		}
	}

	// 見出し
	const float ex = U::Text(FontHead, 64.0f, 48.0f, 47.0f, "SELECT ", INK) / Scale;
	U::Text(FontHead, ex, 48.0f, 47.0f, "MODE", ACID);
	U::Text(FontFoot, 64.0f, 120.0f, 11.0f, "// CHOOSE YOUR RUN", SUBTXT);

	// 4枚のモードカード(選択=アシッド塗り)
	for (int i = 0; i < kCount; ++i)
	{
		const float x = CardX(i);
		const bool on = (i == m_sel);
		if (on)
		{
			U::RectTL(x, kCardY, kCardW, kCardH, INK, true);   // 黒地
			// 中身の演出：流れる斜めハザードストライプ
			const float rx = x + 6.0f, ry = kCardY + 64.0f, rw = kCardW - 12.0f, rh = kCardH - 190.0f;
			U::FxStripes(rx, ry, rw, rh, ACID, T);
		}
		U::FrameTL(x, kCardY, kCardW, kCardH, 3.0f, INK);
		// ② 選択カードのパルス：外側に脈動する枠
		if (on)
		{
			const float p = 4.0f + 4.0f * std::sin(T * 5.0f);
			U::FrameTL(x - p, kCardY - p, kCardW + p * 2.0f, kCardH + p * 2.0f, 2.0f, INK);
		}
		// 番号(④ 上下フロート)。選択(黒地)は白、非選択は墨。
		const float nfy = std::sin(T * 1.8f + i) * 5.0f;
		char no[8]; sprintf_s(no, sizeof(no), "0%d", i + 1);
		U::Text(FontTab, x + 22.0f, kCardY + 22.0f + nfy, 20.0f, no, on ? WHITE : INK);
		// SOONバッジ(マルチ=棚上げ中)
		if (kModes[i].soon)
		{
			U::RectTL(x + kCardW - 76.0f, kCardY + 22.0f, 54.0f, 24.0f, INK, true);
			U::Text(FontTiny, x + kCardW - 66.0f, kCardY + 27.0f, 10.0f, "SOON", ACID);
		}
		// タイトル(アウトライン=中空)。選択(黒地)は縁アシッド・中黒、非選択は縁墨・中紙。
		U::TextOutline(FontCard, x + 18.0f, kCardY + kCardH - 102.0f, 30.0f, kModes[i].title, on ? ACID : INK, on ? INK : PAPER);
		U::Text(FontFoot, x + 22.0f, kCardY + kCardH - 40.0f, 11.0f, kModes[i].sub, on ? WHITE : SUBTXT);
	}

	// フッター
	U::Keycap(64.0f, 700.0f, "ENTER", "START");
	U::Keycap(300.0f, 700.0f, "ESC", "BACK");
}
