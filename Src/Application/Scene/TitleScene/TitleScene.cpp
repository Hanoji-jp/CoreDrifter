#include "TitleScene.h"
#include "../SceneManager.h"
#include "../../GameObject/UI/TitleMenuUI.h"

void TitleScene::Event()
{
	// Enter/Space で決定。QUIT(index4)なら終了、それ以外はゲームへ。
	if (GetAsyncKeyState(VK_RETURN) & 0x8000 || GetAsyncKeyState(VK_SPACE) & 0x8000)
	{
		int sel = 0;
		if (auto menu = m_wpMenu.lock()) { sel = menu->GetSelected(); }

		if (sel == UIConst::MenuCount - 1)   // QUIT
		{
			PostQuitMessage(0);
			return;
		}

		SceneManager::Instance().SetNextScene(SceneManager::SceneType::Game);
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
