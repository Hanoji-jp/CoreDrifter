#include "HjPlayerProfile.h"

using namespace PlayerConst;

//----------------------------------------------------------
// レベル n に到達するのに必要な累計スコア。
//
// 1レベルごとの必要量を BaseExp から StepExp ずつ増やしていくので、
// 合計は等差数列の和になる。
//   Lv2 に必要 = Base
//   Lv3 に必要 = Base + (Base + Step)
//   …
// 一定量にすると、続けるほど「上がりにくくなっていく手応え」が出ない。
//----------------------------------------------------------
double HjPlayerProfile::RequiredExp(int level)
{
	if (level <= 1) { return 0.0; }

	const double n = static_cast<double>(level - 1);   // 上がった回数
	return n * BaseExp + StepExp * n * (n - 1.0) * 0.5;
}

int HjPlayerProfile::GetLevel() const
{
	int lv = 1;
	while (lv < MaxLevel && m_totalScore >= RequiredExp(lv + 1)) { ++lv; }
	return lv;
}

float HjPlayerProfile::GetLevelProgress() const
{
	const int lv = GetLevel();
	if (lv >= MaxLevel) { return 1.0f; }

	const double cur  = RequiredExp(lv);
	const double next = RequiredExp(lv + 1);
	const double span = next - cur;
	if (span <= 0.0) { return 1.0f; }

	return static_cast<float>(std::clamp((m_totalScore - cur) / span, 0.0, 1.0));
}

void HjPlayerProfile::SetName(const std::string& name)
{
	// ※バイト数で切らないこと。UTF-8 は1文字が複数バイトなので、
	//   途中で切ると文字が壊れて表示できなくなる。
	//   文字数の上限は入力側で見ているので、ここでは空だけ弾く。
	m_name = name;
	if (m_name.empty()) { m_name = DefaultName; }
}

void HjPlayerProfile::SetColor(const Math::Vector3& color)
{
	// 暗すぎると見分けの役に立たないので、下限を持たせる。
	// 真っ黒にされると、アウトラインが背景に溶けて誰の車か分からなくなる
	const float lo = 0.15f;
	m_color.x = std::clamp(color.x, lo, 1.0f);
	m_color.y = std::clamp(color.y, lo, 1.0f);
	m_color.z = std::clamp(color.z, lo, 1.0f);
	Save();
}

void HjPlayerProfile::AddRun(double score)
{
	m_totalScore += score;
	m_bestScore = std::max(m_bestScore, score);
	++m_runCount;

	// 走り終えた時点で保存する。
	// 終了時にまとめて書くと、強制終了で記録が消える。
	Save();
}

void HjPlayerProfile::Load()
{
	std::ifstream ifs(ProfilePath);
	if (!ifs) { return; }   // 無ければ既定値のまま

	std::string key;
	while (ifs >> key)
	{
		// 名前は行末まで読む。
		// >> だと空白で切れるが、入力側で空白を弾いているので実害は無い。
		// ただし将来入力を緩めたときに壊れないよう、読み方はこちらに寄せる。
		if      (key == "name")  { std::getline(ifs >> std::ws, m_name); }
		else if (key == "total") { ifs >> m_totalScore; }
		else if (key == "best")  { ifs >> m_bestScore; }
		else if (key == "runs")  { ifs >> m_runCount; }
		else if (key == "color") { ifs >> m_color.x >> m_color.y >> m_color.z; }
	}
}

void HjPlayerProfile::Save() const
{
	std::ofstream ofs(ProfilePath);
	if (!ofs) { return; }

	// 空の名前は既定値へ戻す(空欄で確定されると誰なのか分からなくなる)
	const std::string safeName = m_name.empty() ? DefaultName : m_name;

	ofs << "name "  << safeName    << "\n";
	ofs << "total " << m_totalScore << "\n";
	ofs << "best "  << m_bestScore  << "\n";
	ofs << "runs "  << m_runCount   << "\n";
}
