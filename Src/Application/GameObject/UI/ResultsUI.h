#pragma once

#include "MultiUIBase.h"

//==========================================================
// ResultsUI
//   走り終えた後の成績。
//
//   ■ 今は自分1人ぶん
//   相手と競う仕組みがまだ無いので、並ぶのは自分の記録だけ。
//   通信が入れば、同じ表に相手の行が増える。
//   架空の対戦相手を並べると、順位が意味を持たない表になる。
//==========================================================
class ResultsUI : public MultiUIBase
{
public:
	// 表の1行
	struct Row
	{
		const char* driver = "";
		double      score  = 0.0;
		int         combo  = 1;
		bool        self   = false;
	};

	void Update()     override;
	void DrawSprite() override;

	void SetRows(const std::vector<Row>& rows) { m_rows = rows; }

	bool ConsumeBack() { const bool a = m_back; m_back = false; return a; }

private:
	void DrawTable();
	void DrawFooter(float tableBottom);

	std::vector<Row> m_rows;
	bool m_back = false;
};
