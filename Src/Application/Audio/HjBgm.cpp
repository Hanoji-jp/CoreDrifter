#include "HjBgm.h"
#include "../Const/BgmConst.h"
#include "HjAudioSettings.h"

void HjBgm::SetPlaying(bool play)
{
	m_wantPlay = play;

	if (!play) { return; }

	// 最初に鳴らすときだけ読み込む。
	// 画面が変わるたびに読み直すと、そのたびに引っかかる
	if (!m_loaded)
	{
		if (m_volume <= 0.0f) { m_volume = BgmConst::Volume; }

		// 音量0で始まるので、このあと Update で上げていく
		m_loaded = m_voice.Play(BgmConst::MenuPath, true);
	}
}

void HjBgm::Update(float dt)
{
	if (!m_loaded || !m_voice.IsValid()) { return; }

	// 鳴らしたい方向へ音量を動かす。
	// 止めるのではなく音量で出し入れするので、曲の位置は保たれる。
	// 走行を終えてメニューへ戻ると、続きから聞こえる
	// 画面が許していて、かつ手で止められていないときだけ鳴らす
	const bool on = m_wantPlay && !m_muted;

	const float target = on ? 1.0f : 0.0f;
	const float time   = on ? BgmConst::FadeInTime : BgmConst::FadeOutTime;

	const float step = dt / std::max(time, 0.01f);
	if (m_gain < target) { m_gain = std::min(m_gain + step, target); }
	else                 { m_gain = std::max(m_gain - step, target); }

	// 設定画面の音楽音量を掛ける。
	// 音源側が自分の音量を持つと、設定を変えるたびに
	// 全部へ配って回ることになるので、鳴らす側が見に来る
	m_voice.SetVolume(m_gain * m_volume * HjAudioSettings::Instance().GetMusic());
}

void HjBgm::Stop()
{
	m_voice.Stop();
	m_loaded   = false;
	m_gain     = 0.0f;
	m_wantPlay = false;
}

void HjBgm::DrawImGui()
{
	if (!ImGui::CollapsingHeader(U8("BGM(メニュー)"))) { return; }

	ImGui::Text(U8("状態: %s   音量 %.2f"),
	            m_wantPlay ? U8("再生") : U8("停止"), m_gain * m_volume);

	if (m_volume <= 0.0f) { m_volume = BgmConst::Volume; }
	if (ImGui::SliderFloat(U8("音量"), &m_volume, 0.0f, 1.0f))
	{
			if (m_voice.IsValid())
		{
			m_voice.SetVolume(m_gain * m_volume * HjAudioSettings::Instance().GetMusic());
		}
	}

	ImGui::TextDisabled(U8("走行中とポーズ中は鳴らしません"));
	ImGui::TextDisabled(U8("(エンジン音とタイヤの音を聞き取れなくするため)"));
}

const char* HjBgm::GetTitle()  const { return BgmConst::MenuTitle; }
const char* HjBgm::GetArtist() const { return BgmConst::MenuArtist; }
