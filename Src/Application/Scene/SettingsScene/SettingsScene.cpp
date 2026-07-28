#include "SettingsScene.h"
#include "../SceneManager.h"
#include "../HjTransition.h"
#include "../../GameObject/UI/SettingsUI.h"

void SettingsScene::Event()
{
	if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
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
