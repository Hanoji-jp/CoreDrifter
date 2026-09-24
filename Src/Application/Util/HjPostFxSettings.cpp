#include "HjPostFxSettings.h"
#include "HjSaveFile.h"

namespace
{
	// 保存先。車やステージの調整とは別にする
	constexpr const char* SettingsPath = "Asset/Data/PostFx.txt";

	// 真偽値も同じ仕組みで扱えるよう、0/1の数値として持つ。
	// 別の仕組みを用意すると、項目を足すときに2か所直すことになる
	float g_sceneOutline = 0.0f;
	float g_smokeOutline = 0.0f;
	float g_halftone     = 0.0f;
}

//----------------------------------------------------------
// 保存する項目。
// SaveとLoadが同じ一覧を使うので、片方だけ直し忘れることがない。
//----------------------------------------------------------
std::vector<std::pair<const char*, float*>> HjPostFxSettings::ParamList()
{
	auto& pp = KdShaderManager::Instance().m_postProcessShader;

	// 真偽値は今の値をいったん取り出しておく。
	// Saveのときはここが書き出され、Loadのときは読んだ値を後で反映する
	g_sceneOutline = pp.IsSceneOutlineEnabled() ? 1.0f : 0.0f;
	g_smokeOutline = pp.IsSmokeOutlineEnabled() ? 1.0f : 0.0f;
	g_halftone     = pp.IsHalftoneEnabled()     ? 1.0f : 0.0f;

	return
	{
		// 画面アウトライン(トゥーン輪郭)
		{ "sceneOutlineOn",     &g_sceneOutline },
		{ "outlineThickness",   &pp.WorkOutlineThickness() },
		{ "outlineDepthTh",     &pp.WorkOutlineDepthThreshold() },
		{ "outlineNormalTh",    &pp.WorkOutlineNormalThreshold() },
		{ "outlineStrength",    &pp.WorkOutlineEdgeStrength() },
		{ "outlineColR",        &pp.WorkOutlineColor().x },
		{ "outlineColG",        &pp.WorkOutlineColor().y },
		{ "outlineColB",        &pp.WorkOutlineColor().z },

		// 煙の輪郭
		{ "smokeOutlineOn",     &g_smokeOutline },
		{ "smokeOutlineThick",  &pp.WorkSmokeOutlineThickness() },
		{ "smokeOutlineAlphaTh",&pp.WorkSmokeOutlineAlphaThreshold() },
		{ "smokeOutlineStr",    &pp.WorkSmokeOutlineEdgeStrength() },
		{ "smokeOutlineColR",   &pp.WorkSmokeOutlineColor().x },
		{ "smokeOutlineColG",   &pp.WorkSmokeOutlineColor().y },
		{ "smokeOutlineColB",   &pp.WorkSmokeOutlineColor().z },

		// ハーフトーン(印刷風の網点)
		{ "halftoneOn",         &g_halftone },
		{ "halftoneScale",      &pp.WorkHalftoneScale() },
		{ "halftoneStrength",   &pp.WorkHalftoneStrength() },
		{ "halftoneDarkBias",   &pp.WorkHalftoneDarkBias() },
	};
}

void HjPostFxSettings::Save() const
{
	// ParamList は今の状態を取り出す都合で非constにしてある
	auto params = const_cast<HjPostFxSettings*>(this)->ParamList();

	HjSaveOStream ofs("postfx", SettingsPath);
	if (!ofs) { return; }
	for (const auto& p : params) { ofs << p.first << " " << *p.second << "\n"; }
}

void HjPostFxSettings::Load()
{
	HjSaveIStream ifs("postfx", SettingsPath);
	if (!ifs) { return; }   // 無ければ既定値のまま

	auto params = ParamList();

	std::string key;
	float value = 0.0f;
	while (ifs >> key >> value)
	{
		for (const auto& p : params)
		{
			if (key == p.first) { *p.second = value; break; }
		}
	}

	// 真偽値は数値として読み込んだだけなので、シェーダーへ反映する。
	// ここを忘れると、ファイルには残っているのに効かない状態になる
	auto& pp = KdShaderManager::Instance().m_postProcessShader;
	pp.SetSceneOutlineEnabled(g_sceneOutline > 0.5f);
	pp.SetSmokeOutlineEnabled(g_smokeOutline > 0.5f);
	pp.SetHalftoneEnabled(g_halftone > 0.5f);
}

void HjPostFxSettings::Update()
{
	if (!m_dirty) { return; }

	// スライダーを掴んでいる間は毎フレーム値が変わる。
	// 手を離してから1回だけ書き出す
	if (ImGui::IsAnyItemActive()) { return; }

	Save();
	m_dirty = false;
}
