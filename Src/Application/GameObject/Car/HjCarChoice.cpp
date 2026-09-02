#include "HjCarChoice.h"

#include "Silvia.h"
#include "Nsx.h"

#include <fstream>

namespace CC = CarChoiceConst;

namespace
{
	// 範囲の外なら既定へ倒す。
	// 手で書き換えられることも、古い保存を読むこともある
	CC::Kind Clamp(int v)
	{
		if (v < 0 || v >= static_cast<int>(CC::Kind::Count)) { return CC::Fallback; }
		return static_cast<CC::Kind>(v);
	}
}

void HjCarChoice::Load()
{
	std::ifstream ifs(CC::SavePath);
	if (!ifs) { return; }   // まだ無いのは異常ではない

	std::string key;
	int val = 0;
	while (ifs >> key >> val)
	{
		if (key == CC::SaveKey) { m_kind = Clamp(val); }
	}
}

void HjCarChoice::Save() const
{
	std::ofstream ofs(CC::SavePath);
	if (!ofs) { return; }

	ofs << CC::SaveKey << " " << static_cast<int>(m_kind) << "\n";
}

void HjCarChoice::Set(Kind k)
{
	m_kind = Clamp(static_cast<int>(k));

	// 選んだ時点で覚える。
	// 1つの選択で途中の状態が無いので、後から迷いようがない
	Save();
}

const char* HjCarChoice::NameOf(Kind k)
{
	const int i = static_cast<int>(k);
	if (i < 0 || i >= static_cast<int>(CC::Kind::Count)) { return ""; }
	return CC::Names[i];
}

const char* HjCarChoice::Name() const
{
	return NameOf(m_kind);
}

std::shared_ptr<CarBase> HjCarChoice::Create(Kind k)
{
	// ここだけが車種を知っている。
	// 場面ごとに分岐を書くと、車を足すたびに全部直すことになる
	switch (k)
	{
	case CC::Kind::Nsx:    return std::make_shared<Nsx>();
	case CC::Kind::Silvia: break;
	case CC::Kind::Count:  break;
	}
	return std::make_shared<Silvia>();
}
