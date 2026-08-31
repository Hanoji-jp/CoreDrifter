#include "HjModProfile.h"

#include <fstream>
#include "json.hpp"

using Json = nlohmann::json;

namespace
{
	// 項目の名前と、Entry のどこに入れるか。
	// 読みと書きで同じ一覧を使う。別々に書くと、
	// 片方に足してもう片方を忘れる、が必ず起きる
	using Field = std::pair<const char*, float HjModProfile::Entry::*>;

	const std::vector<Field>& Fields()
	{
		static const std::vector<Field> list = {
			{ "bodyScale",  &HjModProfile::Entry::bodyScale  },
			{ "bodyYaw",    &HjModProfile::Entry::bodyYaw    },
			{ "wheelScale", &HjModProfile::Entry::wheelScale },
			{ "wheelYaw",   &HjModProfile::Entry::wheelYaw   },
			{ "track",      &HjModProfile::Entry::track      },
			{ "base",       &HjModProfile::Entry::base       },
			{ "wheelH",     &HjModProfile::Entry::wheelH     },
			{ "bodyOffY",   &HjModProfile::Entry::bodyOffY   },
			{ "bodyOffZ",   &HjModProfile::Entry::bodyOffZ   },
			{ "bodyOffX",   &HjModProfile::Entry::bodyOffX   },
		};
		return list;
	}
}

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
		for (const auto& f : Fields())
		{
			const auto v = it.value().find(f.first);
			if (v != it.value().end() && v->is_number())
			{
				e.*(f.second) = v->get<float>();
			}
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
		for (const auto& f : Fields())
		{
			obj[f.first] = kv.second.*(f.second);
		}
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
