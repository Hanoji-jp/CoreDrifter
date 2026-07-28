#include "PlayModeScene.h"
#include "../SceneManager.h"
#include "../HjTransition.h"
#include "../../GameObject/UI/PlayModeUI.h"

void PlayModeScene::Event()
{
	if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
	{
		HjTransition::Instance().Go(SceneManager::SceneType::Title);
		return;
	}
	// 決定(モード選択)→ゲームへ(暫定)
	if (auto ui = m_wpUI.lock())
	{
		if (ui->ConsumeActivated())
		{
			// 選択カードが広がってゲームへ(コンテナ・トランスフォーム)
			float dx, dy, w, h;
			ui->GetSelectedCardRect(dx, dy, w, h);
			HjTransition::Instance().GoMorph(SceneManager::SceneType::Game, dx, dy, w, h);
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
