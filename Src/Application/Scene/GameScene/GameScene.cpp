#include "GameScene.h"
#include"../SceneManager.h"
#include "../../GameObject/Stage/Ground.h"
#include "../../GameObject/Stage/Stage.h"
#include "../../GameObject/Stage/SkySphere.h"
#include "../../GameObject/Car/Silvia.h"
#include "../../GameObject/Camera/ChaseCamera.h"

void GameScene::Event()
{
	if (GetAsyncKeyState('T') & 0x8000)
	{
		SceneManager::Instance().SetNextScene
		(
			SceneManager::SceneType::Title
		);
	}
}

void GameScene::Init()
{
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
	AddObject(car);

	// 調整パネル(ImGui)：車のチューニングとマップ配置を1つのコールバックにまとめて登録
	//   ※SetPersistentGuiCallbackは単一スロット(上書き)なので合成して渡す
	KdDebugGUI::Instance().SetPersistentGuiCallback([car, stage]()
	{
		car->DrawImGui();
		stage->DrawTuningImGui();
	});

	// 追従カメラ
	auto cam = std::make_shared<ChaseCamera>();
	cam->Init();
	cam->SetTarget(car);
	AddObject(cam);
}
