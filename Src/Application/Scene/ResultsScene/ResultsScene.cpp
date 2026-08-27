#include "ResultsScene.h"
#include "../SceneManager.h"
#include "../HjTransition.h"
#include "../../GameObject/UI/ResultsUI.h"
#include "../../GameObject/Score/HjRunResult.h"

void ResultsScene::Event()
{
	auto ui = m_wpUI.lock();
	if (!ui) { return; }

	if (ui->ConsumeBack())
	{
		HjTransition::Instance().Go(SceneManager::SceneType::PlayMode);
	}
}

void ResultsScene::Init()
{
	m_objList.clear();
	auto ui = std::make_shared<ResultsUI>();

	// 走り終えた時の点数を持ってくる。
	// シーンを切り替えるとオブジェクトは作り直されるので、
	// 値は HjRunResult に預けてある。
	const HjRunResult& run = HjRunResult::Instance();

	ResultsUI::Row me;
	me.driver = "YOU";
	me.score  = run.HasRun() ? run.GetTotal() : 0.0;
	me.combo  = run.HasRun() ? run.GetBestCombo() : 1;
	me.self   = true;

	// ※対戦相手は仮。競う仕組みがまだ無い。
	//   表の並びと棒の見え方を先に確かめるために置いている。
	//   通信が入ったら、同じ表に本物の行が増えるだけで済む。
	std::vector<ResultsUI::Row> rows = {
		me,
		{ "KAZE_9",  13610.0, 7, false },
		{ "MIRA.S",  12480.0, 6, false },
		{ "NOCTURN", 10905.0, 5, false },
		{ "V0RTEX",   9740.0, 4, false },
	};

	// 点数の高い順に並べ替える。順位は表示の都合ではなく中身で決める
	std::sort(rows.begin(), rows.end(),
	          [](const ResultsUI::Row& a, const ResultsUI::Row& b) { return a.score > b.score; });

	ui->SetRows(rows);

	AddObject(ui);
	m_wpUI = ui;
}
