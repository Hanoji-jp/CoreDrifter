#pragma once

//==========================================================
// HjRunResult
//   1本走った結果を、次の画面(RESULTS)へ持ち越すための入れ物。
//
//   シーンを切り替えるとオブジェクトは全部作り直されるので、
//   走り終えた時点の点数はどこかへ預けておかないと消える。
//   採点の仕組みそのものはDriftScoreが持っているので、
//   ここは「終わった時の値」だけを預かる。
//==========================================================
class HjRunResult
{
public:
	static HjRunResult& Instance()
	{
		static HjRunResult inst;
		return inst;
	}

	// 走り終えたときに呼ぶ
	void Record(double total, int bestCombo)
	{
		m_total     = total;
		m_bestCombo = bestCombo;
		m_hasRun    = true;
	}

	double GetTotal()     const { return m_total; }
	int    GetBestCombo() const { return m_bestCombo; }
	// 一度も走っていない場合は、結果の代わりにその旨を出す
	bool   HasRun()       const { return m_hasRun; }

private:
	HjRunResult() = default;

	double m_total     = 0.0;
	int    m_bestCombo = 1;
	bool   m_hasRun    = false;

	HjRunResult(const HjRunResult&) = delete;
	void operator=(const HjRunResult&) = delete;
};
