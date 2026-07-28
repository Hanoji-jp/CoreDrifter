#include "GameScene.h"
#include"../SceneManager.h"
#include "../../GameObject/Stage/Ground.h"
#include "../../GameObject/Stage/Stage.h"
#include "../../GameObject/Stage/SkySphere.h"
#include "../../GameObject/Car/Silvia.h"
#include "../../GameObject/Camera/ChaseCamera.h"
#include "../../GameObject/Score/DriftScore.h"

void GameScene::Event()
{
	if (GetAsyncKeyState('T') & 0x8000)
	{
		SceneManager::Instance().SetNextScene
		(
			SceneManager::SceneType::Title
		);
	}

	// R=スポーン地点へリスポーン(押した瞬間だけ)
	const bool respawnKey = (GetAsyncKeyState('R') & 0x8000) != 0;
	if (respawnKey && !m_prevRespawnKey)
	{
		if (auto car = m_wpCar.lock()) { car->Respawn(); }
	}
	m_prevRespawnKey = respawnKey;
}

void GameScene::Init()
{
	// ゲーム開始演出：グレースケール→徐々にフルカラーへ戻す
	KdShaderManager::Instance().m_postProcessShader.TriggerColorRestore();

	// 天球(背景の星空)。最初に追加＝背景として描画。
	auto sky = std::make_shared<SkySphere>();
	sky->Init();
	AddObject(sky);

	// 影の範囲を広げる(既定25×25は狭くカメラ近くしか影が出ない。広大マップ向けに拡大)
	KdShaderManager::Instance().WorkAmbientController().SetDirLightShadowArea(
		{ StageConst::ShadowAreaSize, StageConst::ShadowAreaSize }, StageConst::ShadowLightHeight);

	// コースマップ(地形＋当たり判定)
	auto stage = std::make_shared<Stage>();
	stage->Init();
	AddObject(stage);

	// 車（W=前進 / S=ブレーキ・後退 / A,D=ステア / Space=ハンドブレーキ）
	auto car = std::make_shared<Silvia>();
	car->Init();
	// 車が接地・壁判定を飛ばす相手として地形を登録
	car->AddCollisionTarget(stage);
	// 保存済みスポーン位置へ配置(StageConfig.txtから読まれた値)
	car->SetSpawn(stage->GetSpawnPos(), stage->GetSpawnYaw());
	AddObject(car);
	m_wpCar = car;   // Rキーのリスポーン用に保持

	// ドリフト採点＆スコア表示演出(車の速度・向きを参照)
	auto score = std::make_shared<DriftScore>();
	score->SetCar(car);
	AddObject(score);

	// 調整パネル(ImGui)：車のチューニングとマップ配置を1つのコールバックにまとめて登録
	//   ※SetPersistentGuiCallbackは単一スロット(上書き)なので合成して渡す
	KdDebugGUI::Instance().SetPersistentGuiCallback([car, stage]()
	{
		car->DrawImGui();
		stage->DrawTuningImGui();
		KdShaderManager::Instance().m_postProcessShader.DrawFluidTextImGui();

		// 車とステージの両方を触れるこの場所で、スポーン設定の橋渡しボタンを出す。
		ImGui::Begin(U8("ステージ(マップ配置)"));   // 同名Beginで上のパネルへ追記される
		if (ImGui::Button(U8("現在の車位置をスポーンに設定")))
		{
			stage->SetSpawn(car->GetPos(), car->GetYaw());
			stage->SaveConfig();   // 押した時点で即保存
		}
		ImGui::SameLine();
		if (ImGui::Button(U8("スポーンへ移動(R)")))
		{
			car->SetSpawn(stage->GetSpawnPos(), stage->GetSpawnYaw());
		}
		ImGui::End();
	});

	// 追従カメラ
	auto cam = std::make_shared<ChaseCamera>();
	cam->Init();
	cam->SetTarget(car);
	AddObject(cam);
}
