#include "HjAudioSettings.h"

namespace
{
	constexpr const char* SettingsPath = "Asset/Data/AudioSettings.txt";
}

std::vector<std::pair<const char*, float*>> HjAudioSettings::ParamList()
{
	return
	{
		{ "sfx",     &m_sfx     },
		{ "music",   &m_music   },
		{ "ambient", &m_ambient },
	};
}

void HjAudioSettings::Load()
{
	std::ifstream ifs(SettingsPath);
	if (!ifs) { return; }   // 無ければ既定値のまま

	auto params = ParamList();

	std::string key;
	float value = 0.0f;
	while (ifs >> key >> value)
	{
		for (const auto& p : params)
		{
			if (key == p.first) { *p.second = std::clamp(value, 0.0f, 1.0f); break; }
		}
	}
}

void HjAudioSettings::Save() const
{
	// ParamList は値の場所を返す都合で非constにしてある
	auto params = const_cast<HjAudioSettings*>(this)->ParamList();

	std::ofstream ofs(SettingsPath);
	if (!ofs) { return; }
	for (const auto& p : params) { ofs << p.first << " " << *p.second << "\n"; }
}

// 触った時点で保存する。
// 保存ボタンを押させると、押し忘れて「設定したのに戻っている」になる
void HjAudioSettings::SetSfx(float v)     { m_sfx     = std::clamp(v, 0.0f, 1.0f); Save(); }
void HjAudioSettings::SetMusic(float v)   { m_music   = std::clamp(v, 0.0f, 1.0f); Save(); }
void HjAudioSettings::SetAmbient(float v) { m_ambient = std::clamp(v, 0.0f, 1.0f); Save(); }
