#include "SceneManager.h"

#include "BaseScene/BaseScene.h"
#include "TitleScene/TitleScene.h"
#include "GameScene/GameScene.h"
#include "SettingsScene/SettingsScene.h"
#include "ElementsScene/ElementsScene.h"
#include "PlayModeScene/PlayModeScene.h"
#include "HjTransition.h"
#include "../GameObject/UI/HjUI.h"

void SceneManager::PreUpdate()
{
	// シーン切替
	if (m_currentSceneType != m_nextSceneType)
	{
		ChangeScene(m_nextSceneType);
	}

	// モーフ遷移中はシーンを止める(遷移後に動きが二重になるのを防ぐ)
	if (HjTransition::Instance().FreezesScene()) { return; }
	m_currentScene->PreUpdate();
}

void SceneManager::Update()
{
	// UIアニメの共有クロックを進める(全シーン共通)
	HjUI::Tick(KdFPSController::GetDt());

	// モーフ遷移中はシーン更新を止める(遷移中に動くゲームと遷移後の動きが二重に見えるのを防ぐ)
	if (!HjTransition::Instance().FreezesScene()) { m_currentScene->Update(); }
	// パネルワイプ遷移を進める(カバー完了時に自動でシーン切替を予約)
	HjTransition::Instance().Update(KdFPSController::GetDt());
}

void SceneManager::PostUpdate()
{
	if (HjTransition::Instance().FreezesScene()) { return; }
	m_currentScene->PostUpdate();
}

void SceneManager::PreDraw()
{
	m_currentScene->PreDraw();
}

void SceneManager::Draw()
{
	m_currentScene->Draw();
}

void SceneManager::DrawSprite()
{
	m_currentScene->DrawSprite();
	// パネルワイプ遷移のオーバーレイを最前面に
	HjTransition::Instance().Draw();
}

void SceneManager::DrawDebug()
{
	m_currentScene->DrawDebug();
}

const std::list<std::shared_ptr<KdGameObject>>& SceneManager::GetObjList()
{
	return m_currentScene->GetObjList();
}

void SceneManager::AddObject(const std::shared_ptr<KdGameObject>& _obj)
{
	m_currentScene->AddObject(_obj);
}

void SceneManager::ChangeScene(SceneType _sceneType)
{
	// 次のシーンを作成し、現在のシーンにする
	switch (_sceneType)
	{
	case SceneType::Title:
		m_currentScene = std::make_shared<TitleScene>();
		break;
	case SceneType::Game:
		m_currentScene = std::make_shared<GameScene>();
		break;
	case SceneType::Settings:
		m_currentScene = std::make_shared<SettingsScene>();
		break;
	case SceneType::Elements:
		m_currentScene = std::make_shared<ElementsScene>();
		break;
	case SceneType::PlayMode:
		m_currentScene = std::make_shared<PlayModeScene>();
		break;
	}

	// 現在のシーン情報を更新
	m_currentSceneType = _sceneType;
}
