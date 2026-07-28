#include "TitleScene.h"
#include "../SceneManager.h"
#include "../HjTransition.h"
#include "../../GameObject/UI/TitleMenuUI.h"

void TitleScene::Event()
{
	// ELEMENTS(UIキット)確認用：F5 でいつでも開ける(デバッグ)
	if (GetAsyncKeyState(VK_F5) & 0x8000)
	{
		HjTransition::Instance().Go(SceneManager::SceneType::Elements);
		return;
	}

	// Enter/Space または マウスクリックで決定。選択中メニューに応じて遷移。
	bool decide = (GetAsyncKeyState(VK_RETURN) & 0x8000) || (GetAsyncKeyState(VK_SPACE) & 0x8000);
	int sel = 0;
	if (auto menu = m_wpMenu.lock())
	{
		sel = menu->GetSelected();
		if (menu->ConsumeActivated()) { decide = true; }
	}
	if (decide)
	{
		switch (sel)
		{
		case 0: // PLAY：PLAYバーが伸びて次レイアウトへ変形するモーフ遷移
			HjTransition::Instance().GoMorph(SceneManager::SceneType::PlayMode, 54.0f, 380.0f, 400.0f, 51.0f);
			break;
		case 2: HjTransition::Instance().Go(SceneManager::SceneType::Settings); break;  // SETTINGS
		case 4: PostQuitMessage(0);                                             break;  // QUIT
		default: HjTransition::Instance().Go(SceneManager::SceneType::Game);    break;  // GARAGE/STATS(未実装)→暫定Game
		}
	}
}

void TitleScene::Init()
{
	m_objList.clear();

	auto menu = std::make_shared<TitleMenuUI>();
	menu->Init();
	AddObject(menu);
	m_wpMenu = menu;
}
