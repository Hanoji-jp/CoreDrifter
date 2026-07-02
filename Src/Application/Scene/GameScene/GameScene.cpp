#include "GameScene.h"
#include"../SceneManager.h"
#include "../../GameObject/Stage/Ground.h"
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

	// 仮の地面
	auto ground = std::make_shared<Ground>();
	ground->Init();
	AddObject(ground);

	// 車（W=前進 / S=ブレーキ・後退 / A,D=ステア / Space=ハンドブレーキ）
	auto car = std::make_shared<Silvia>();
	car->Init();
	AddObject(car);

	// 追従カメラ
	auto cam = std::make_shared<ChaseCamera>();
	cam->Init();
	cam->SetTarget(car);
	AddObject(cam);
}
