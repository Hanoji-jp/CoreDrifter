#include "ElementsScene.h"
#include "../../Input/HjKeyInput.h"
#include "../SceneManager.h"
#include "../HjTransition.h"
#include "../../GameObject/UI/ElementsUI.h"

void ElementsScene::Event()
{
	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Cancel))
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
