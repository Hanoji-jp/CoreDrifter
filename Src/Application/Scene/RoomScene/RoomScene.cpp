#include "RoomScene.h"
#include "../SceneManager.h"
#include "../HjTransition.h"
#include "../../GameObject/UI/RoomUI.h"

void RoomScene::Event()
{
	auto ui = m_wpUI.lock();
	if (!ui) { return; }

	// 戻る。UI側で段階を1つ戻し切っていたら画面ごと戻す
	if (ui->ConsumeBack())
	{
		HjTransition::Instance().Go(SceneManager::SceneType::Lobby);
		return;
	}

	// 開始(ホスト)/接続(参加)。
	// ※通信が入るまでは、どちらもそのままゲームへ入るだけ。
	//   繋がる前に画面だけ先に触れるようにしておく。
	if (ui->ConsumeStart())
	{
		HjTransition::Instance().Go(SceneManager::SceneType::Game);
	}
}

void RoomScene::Init()
{
	m_objList.clear();
	auto ui = std::make_shared<RoomUI>();
	AddObject(ui);
	m_wpUI = ui;
}
