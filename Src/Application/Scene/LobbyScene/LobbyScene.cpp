#include "LobbyScene.h"
#include "../SceneManager.h"
#include "../HjTransition.h"
#include "../../GameObject/UI/LobbyUI.h"

void LobbyScene::Event()
{
	auto ui = m_wpUI.lock();
	if (!ui) { return; }

	if (ui->ConsumeBack())
	{
		HjTransition::Instance().Go(SceneManager::SceneType::PlayMode);
		return;
	}
	if (ui->ConsumeCreate())
	{
		HjTransition::Instance().Go(SceneManager::SceneType::Room);
		return;
	}
	if (ui->ConsumeQuickJoin())
	{
		HjTransition::Instance().Go(SceneManager::SceneType::Matchmaking);
	}
}

void LobbyScene::Init()
{
	m_objList.clear();
	auto ui = std::make_shared<LobbyUI>();

	// 一覧に入れる部屋は無い。
	// マッチングサーバーが無い以上、他人が立てた部屋を見つける手段が無いため。
	// 偽の行を置くと、押しても入れない部屋が居座ることになる。
	AddObject(ui);
	m_wpUI = ui;
}
