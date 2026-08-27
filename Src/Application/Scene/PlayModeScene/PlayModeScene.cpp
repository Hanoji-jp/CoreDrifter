#include "PlayModeScene.h"
#include "../../Input/HjKeyInput.h"
#include "../SceneManager.h"
#include "../HjTransition.h"
#include "../../GameObject/UI/PlayModeUI.h"

void PlayModeScene::Event()
{
	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Cancel))
	{
		HjTransition::Instance().Go(SceneManager::SceneType::Title);
		return;
	}
	// 決定(モード選択)→ゲームへ(暫定)
	if (auto ui = m_wpUI.lock())
	{
		if (ui->ConsumeActivated())
		{
			// マルチだけは先に部屋の一覧を挟む。
			// 誰と走るかを決めてからでないとゲームへ入れないため。
			const bool multi = (ui->GetSelected() == PlayModeUI::MultiplayerIndex);

			// 選択カードが広がって次の画面へ(コンテナ・トランスフォーム)
			float dx, dy, w, h;
			ui->GetSelectedCardRect(dx, dy, w, h);
			HjTransition::Instance().GoMorph(
				multi ? SceneManager::SceneType::Lobby : SceneManager::SceneType::Game,
				dx, dy, w, h);
		}
	}
}

void PlayModeScene::Init()
{
	m_objList.clear();
	auto ui = std::make_shared<PlayModeUI>();
	AddObject(ui);
	m_wpUI = ui;
}
