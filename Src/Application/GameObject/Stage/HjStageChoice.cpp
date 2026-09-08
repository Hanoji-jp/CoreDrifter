#include "HjStageChoice.h"
#include "../../Util/HjSaveFile.h"

#include <fstream>

namespace SC = StageChoiceConst;

namespace
{
	// 範囲の外なら既定へ倒す。
	// 手で書き換えられることも、古い保存を読むこともある
	SC::Kind Clamp(int v)
	{
		if (v < 0 || v >= static_cast<int>(SC::Kind::Count)) { return SC::Fallback; }
		return static_cast<SC::Kind>(v);
	}
}

void HjStageChoice::Load()
{
	HjSaveIStream ifs("stage");
	if (!ifs) { return; }   // まだ無いのは異常ではない

	std::string key;
	int val = 0;
	while (ifs >> key >> val)
	{
		if (key == SC::SaveKey) { m_kind = Clamp(val); }
	}
}

void HjStageChoice::Save() const
{
	HjSaveOStream ofs("stage");
	if (!ofs) { return; }

	ofs << SC::SaveKey << " " << static_cast<int>(m_kind) << "\n";
}

void HjStageChoice::Set(Kind k)
{
	m_kind = Clamp(static_cast<int>(k));

	// 選んだ時点で覚える。
	// 1つの選択で途中の状態が無いので、後から迷いようがない
	Save();
}

const char* HjStageChoice::NameOf(Kind k)
{
	const int i = static_cast<int>(k);
	if (i < 0 || i >= static_cast<int>(SC::Kind::Count)) { return ""; }
	return SC::Names[i];
}

const char* HjStageChoice::NoteOf(Kind k)
{
	const int i = static_cast<int>(k);
	if (i < 0 || i >= static_cast<int>(SC::Kind::Count)) { return ""; }
	return SC::Notes[i];
}

const char* HjStageChoice::Name() const
{
	return NameOf(m_kind);
}
