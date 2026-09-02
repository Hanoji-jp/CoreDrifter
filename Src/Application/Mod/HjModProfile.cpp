#include "HjModProfile.h"

#include <fstream>
#include "json.hpp"

using Json = nlohmann::json;

//----------------------------------------------------------
void HjModProfile::Load()
{
	m_map.clear();
	m_dirty = false;

	std::ifstream ifs(ModConst::ProfilePath);
	if (!ifs) { return; }   // まだ無いのは異常ではない

	// 壊れたファイルで落とさない。
	// 手で書き換えられることを前提にしておく
	Json root = Json::parse(ifs, nullptr, false);
	if (root.is_discarded() || !root.is_object()) { return; }

	for (auto it = root.begin(); it != root.end(); ++it)
	{
		if (!it.value().is_object()) { continue; }

		Entry e;
		for (auto f = it.value().begin(); f != it.value().end(); ++f)
		{
			// 数でないものは読み飛ばす。
			// 手で書き換えたときに文字が混じることがある
			if (!f.value().is_number()) { continue; }
			e[f.key()] = f.value().get<float>();
		}
		m_map[it.key()] = e;
	}
}

//----------------------------------------------------------
void HjModProfile::Save() const
{
	Json root = Json::object();

	for (const auto& kv : m_map)
	{
		Json obj = Json::object();
		for (const auto& f : kv.second) { obj[f.first] = f.second; }
		root[kv.first] = obj;
	}

	std::ofstream ofs(ModConst::ProfilePath);
	if (!ofs) { return; }

	// 字下げして書く。手で開いて直せるほうが、
	// 合わない値を1つ消したいときに早い
	ofs << root.dump(ModConst::JsonIndent) << "\n";

	m_dirty = false;
}

//----------------------------------------------------------
bool HjModProfile::Has(const std::string& path) const
{
	return m_map.find(path) != m_map.end();
}

HjModProfile::Entry HjModProfile::Get(const std::string& path) const
{
	const auto it = m_map.find(path);
	if (it == m_map.end()) { return Entry(); }
	return it->second;
}

void HjModProfile::Set(const std::string& path, const Entry& e)
{
	m_map[path] = e;
	m_dirty = true;
}

void HjModProfile::Erase(const std::string& path)
{
	if (m_map.erase(path) > 0) { m_dirty = true; }
}
