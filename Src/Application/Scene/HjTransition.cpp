#include "HjTransition.h"
#include "../GameObject/UI/HjUI.h"

void HjTransition::Go(SceneManager::SceneType target)
{
	if (Busy()) { return; }
	if (target == SceneManager::Instance().GetCurrentType()) { return; }
	m_target = target;
	m_morph  = false;
	m_phase  = Phase::Cover;
	m_t      = 0.0f;
}

void HjTransition::GoMorph(SceneManager::SceneType target, float dx, float dy, float w, float h)
{
	if (Busy()) { return; }
	if (target == SceneManager::Instance().GetCurrentType()) { return; }
	m_target = target;
	m_morph  = true;
	m_srcX = dx; m_srcY = dy; m_srcW = w; m_srcH = h;
	// カード全画面パンチ：カバーで選択カードを一気に全画面へ拡大(黒で埋める)、
	// カバー完了時にシーン切替(裏でロード)、リベールで黒を引いてゲームを見せる。
	m_phase  = Phase::Cover;
	m_t      = 0.0f;
}

// easeInOut(cubic-bezier(.7,0,.2,1)相当)
float HjTransition::Ease(float x)
{
	return (x < 0.5f) ? (4.0f * x * x * x)
					  : (1.0f - std::pow(-2.0f * x + 2.0f, 3.0f) * 0.5f);
}

const char* HjTransition::TargetName() const
{
	switch (m_target)
	{
	case SceneManager::SceneType::Title:    return "MENU";
	case SceneManager::SceneType::Game:     return "DRIVE";
	case SceneManager::SceneType::Settings: return "SETTINGS";
	case SceneManager::SceneType::Elements: return "ELEMENTS";
	default: return "";
	}
}

void HjTransition::Update(float dt)
{
	if (m_phase == Phase::Idle) { return; }
	// 切替＋ロード(重いフレーム)の間は進めずに待つ。でないと拡大が一瞬で飛ぶ。
	if (m_skipFrames > 0) { --m_skipFrames; return; }
	m_t += dt / m_dur;
	if (m_t < 1.0f) { return; }
	m_t = 0.0f;
	if (m_phase == Phase::Cover)
	{
		// 覆った瞬間にシーン切替→リベールへ。次フレームはロードで重くなるので
		// 数フレーム進めずに待ってから黒を引く(でないと一瞬で飛ぶ)。
		SceneManager::Instance().SetNextScene(m_target);
		m_phase = Phase::Reveal;
		m_skipFrames = 12;   // ロード＋最初の数フレームを黒でしっかり隠す
	}
	else
	{
		m_phase = Phase::Idle;
	}
}

void HjTransition::Draw()
{
	if (m_phase == Phase::Idle) { return; }
	const float e = Ease(m_t);

	auto& sp = KdShaderManager::Instance().m_spriteShader;
	sp.Begin();

	// ── モーフ(カード全画面パンチ) ──
	if (m_morph)
	{
		if (m_phase == Phase::Cover)
		{
			// 選択カードが一気に全画面へ拡大して黒で埋める(パンチ)。下は選択画面。
			auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
			const float hx = lerp(m_srcX, 0.0f, e);
			const float hy = lerp(m_srcY, 0.0f, e);
			const float hw = lerp(m_srcW, UIConst::DesignW, e);
			const float hh = lerp(m_srcH, UIConst::DesignH, e);
			HjUI::RectTL(hx, hy, hw, hh, UIConst::INK, true);
		}
		else   // Reveal
		{
			const bool switched = (SceneManager::Instance().GetCurrentType() == m_target);
			if (m_skipFrames > 0 || !switched)
			{
				// ロード中(重いフレーム)は全画面黒のまま待つ
				HjUI::RectTL(0.0f, 0.0f, UIConst::DesignW, UIConst::DesignH, UIConst::INK, true);
			}
			else
			{
				// 黒を引いて(フェードアウト)グレースケールのゲームを見せる。色は徐々に戻る。
				Math::Color c = UIConst::INK; c.w = 1.0f - e;
				HjUI::RectTL(0.0f, 0.0f, UIConst::DesignW, UIConst::DesignH, c, true);
			}
		}
		sp.End();
		return;
	}

	// ── ワイプ(パネル形状。幅/横位置をアニメ) ──
	const float coverW = (m_phase == Phase::Cover) ? e * UIConst::DesignW : UIConst::DesignW;
	const float offX   = (m_phase == Phase::Reveal) ? e * UIConst::DesignW : 0.0f;
	HjUI::RectTL(offX, 0.0f, coverW, UIConst::DesignH, UIConst::ACID, true);

	// 次シーン名(黒)。パネルに乗ってスライド＋フェードイン/アウト。
	const float labelPx = 122.0f;
	const float baseX   = 120.0f;
	const float slide   = (m_phase == Phase::Cover) ? (1.0f - e) * 28.0f : e * 28.0f;
	const float lx      = offX + baseX + ((m_phase == Phase::Cover) ? -slide : slide);
	// カバー中はフェードイン(0→1)、リベール中はフェードアウト(1→0)
	Math::Color labelCol = UIConst::INK;
	labelCol.w = (m_phase == Phase::Cover) ? e : (1.0f - e);
	HjUI::Text(UIConst::FontTitle, lx, UIConst::DesignH * 0.5f - labelPx * 0.5f, labelPx, TargetName(), labelCol);
	sp.End();
}
