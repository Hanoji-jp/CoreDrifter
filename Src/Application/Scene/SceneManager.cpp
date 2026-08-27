#include "SceneManager.h"
#include "../Util/HjProfiler.h"
#include "../GameObject/UI/HjCursor.h"
#include "../Input/HjKeyInput.h"

#include "BaseScene/BaseScene.h"
#include "TitleScene/TitleScene.h"
#include "GameScene/GameScene.h"
#include "SettingsScene/SettingsScene.h"
#include "ElementsScene/ElementsScene.h"
#include "PlayModeScene/PlayModeScene.h"
#include "RoomScene/RoomScene.h"
#include "LobbyScene/LobbyScene.h"
#include "MatchmakingScene/MatchmakingScene.h"
#include "ResultsScene/ResultsScene.h"
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
	HjScopedTimer _t(U8("シーン更新"));

	// UIアニメの共有クロックを進める(全シーン共通)
	HjUI::Tick(KdFPSController::GetDt());

	// モーフ遷移中はシーン更新を止める(遷移中に動くゲームと遷移後の動きが二重に見えるのを防ぐ)
	if (!HjTransition::Instance().FreezesScene()) { m_currentScene->Update(); }
	// パネルワイプ遷移を進める(カバー完了時に自動でシーン切替を予約)
	HjTransition::Instance().Update(KdFPSController::GetDt());

	// 自前のマウスポインタ。位置と、動かしていない時間を見る
	HjCursor::Instance().Update(KdFPSController::GetDt());
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

	// ポインタは何よりも手前。遷移のパネルの上にも出す
	// (画面が切り替わる最中もマウスは動かせるため)
	HjCursor::Instance().Draw();
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
	case SceneType::Room:
		m_currentScene = std::make_shared<RoomScene>();
		break;
	case SceneType::Lobby:
		m_currentScene = std::make_shared<LobbyScene>();
		break;
	case SceneType::Matchmaking:
		m_currentScene = std::make_shared<MatchmakingScene>();
		break;
	case SceneType::Results:
		m_currentScene = std::make_shared<ResultsScene>();
		break;
	}

	// 遷移に使ったキーを消化する。
	// これが無いと、ENTER を押したまま次の画面へ入った瞬間に
	// 同じ ENTER が新規入力として拾われ、画面をいくつも飛ばしてしまう。
	// 1箇所で入れておけば、どの画面遷移でも自動的に防げる。
	HjKeyInput::Instance().ConsumeAll();

	// 現在のシーン情報を更新
	m_currentSceneType = _sceneType;
}
