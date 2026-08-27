#pragma once

#include "../BaseScene/BaseScene.h"

class LobbyUI;

// 部屋の一覧。ここから部屋を立てるか、相手を探しに行く。
class LobbyScene : public BaseScene
{
public:
	LobbyScene() { Init(); }
	~LobbyScene() {}

private:
	void Event() override;
	void Init()  override;

	std::weak_ptr<LobbyUI> m_wpUI;
};
