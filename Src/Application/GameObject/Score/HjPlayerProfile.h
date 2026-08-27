#pragma once

#include "../../Const/PlayerConst.h"

//==========================================================
// HjPlayerProfile
//   プレイヤーの累計データ（名前・累計スコア・走行回数）。
//
//   HjRunResult が「1本ぶんの結果」を持つのに対して、
//   こちらは「これまでの積み重ね」を持つ。
//   走り終えるたびに AddRun() で積み、ファイルへ保存する。
//
//   レベルは持たずに累計スコアから毎回計算する。
//   両方を保存すると、片方だけ書き換わったときに食い違うため。
//==========================================================
class HjPlayerProfile
{
public:
	static HjPlayerProfile& Instance()
	{
		static HjPlayerProfile inst;
		return inst;
	}

	// 起動時に1回。ファイルが無ければ既定値のまま
	void Load();
	void Save() const;

	// 1本走り終えたときに呼ぶ（累計へ積んで保存まで行う）
	void AddRun(double score);

	const std::string& GetName() const { return m_name; }
	void SetName(const std::string& name);

	// マルチで自分を見分けるための色。
	// 車のアウトラインと煙に使う。誰がどれか一目で分かるようにするためのもの
	const Math::Vector3& GetColor() const { return m_color; }
	void SetColor(const Math::Vector3& color);

	double GetTotalScore() const { return m_totalScore; }
	int    GetRunCount()   const { return m_runCount; }
	double GetBestScore()  const { return m_bestScore; }

	// 累計スコアから求めるレベル（1〜MaxLevel）
	int GetLevel() const;
	// 次のレベルまでの進み具合（0〜1）。バッジのゲージ表示に使う
	float GetLevelProgress() const;

private:
	HjPlayerProfile() = default;

	// レベル n に到達するのに必要な累計スコア
	static double RequiredExp(int level);

	std::string   m_name  = PlayerConst::DefaultName;
	Math::Vector3 m_color = Math::Vector3(PlayerConst::DefaultColorR,
	                                      PlayerConst::DefaultColorG,
	                                      PlayerConst::DefaultColorB);
	double      m_totalScore = 0.0;
	double      m_bestScore  = 0.0;
	int         m_runCount   = 0;

	HjPlayerProfile(const HjPlayerProfile&) = delete;
	void operator=(const HjPlayerProfile&) = delete;
};
