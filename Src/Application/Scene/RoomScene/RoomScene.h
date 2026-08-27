#pragma once

#include "../BaseScene/BaseScene.h"

class RoomUI;

// マルチプレイのルーム画面。
// 戻るでプレイモード選択へ、開始でゲームへ。
class RoomScene : public BaseScene
{
public:
	RoomScene() { Init(); }
	~RoomScene() {}

private:
	void Event() override;
	void Init()  override;

	std::weak_ptr<RoomUI> m_wpUI;
};
