#include "SettingsScene.h"
#include "../../Input/HjKeyInput.h"
#include "../SceneManager.h"
#include "../HjTransition.h"
#include "../../GameObject/UI/SettingsUI.h"

void SettingsScene::Event()
{
	if (HjKeyInput::Instance().Pressed(HjKeyInput::Key::Cancel))
	{
		HjTransition::Instance().Go(SceneManager::SceneType::Title);
	}
}

void SettingsScene::Init()
{
	m_objList.clear();
	auto ui = std::make_shared<SettingsUI>();
	ui->Init();
	AddObject(ui);
}
