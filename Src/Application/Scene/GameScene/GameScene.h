#pragma once

#include"../BaseScene/BaseScene.h"

class GameScene : public BaseScene
{
public :

	GameScene()  { Init(); }
	~GameScene() {}

private:

	void Event() override;
	void Init()  override;

	// Rキーのリスポーン用に車を保持(所有はシーンのオブジェクトリスト側)
	std::weak_ptr<class CarBase> m_wpCar;
	bool m_prevRespawnKey = false;   // Rキーのエッジ検出
};
