#include "ElementsScene.h"
#include "../SceneManager.h"
#include "../HjTransition.h"
#include "../../GameObject/UI/ElementsUI.h"

void ElementsScene::Event()
{
	if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
	{
		HjTransition::Instance().Go(SceneManager::SceneType::Title);
	}
}

void ElementsScene::Init()
{
	m_objList.clear();
	auto ui = std::make_shared<ElementsUI>();
	AddObject(ui);
}
