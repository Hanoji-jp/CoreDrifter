#include "MatchmakingScene.h"
#include "../SceneManager.h"
#include "../HjTransition.h"
#include "../../GameObject/UI/MatchmakingUI.h"

void MatchmakingScene::Event()
{
	auto ui = m_wpUI.lock();
	if (!ui) { return; }

	if (ui->ConsumeCancel())
	{
		HjTransition::Instance().Go(SceneManager::SceneType::Lobby);
	}
}

void MatchmakingScene::Init()
{
	m_objList.clear();
	auto ui = std::make_shared<MatchmakingUI>();

	// 見つかった人数は通信側が入れる。まだ繋がる相手はいない。
	AddObject(ui);
	m_wpUI = ui;
}
