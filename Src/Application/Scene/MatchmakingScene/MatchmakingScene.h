#pragma once

#include "../BaseScene/BaseScene.h"

class MatchmakingUI;

// 相手を探している画面。取り消すと部屋の一覧へ戻る。
class MatchmakingScene : public BaseScene
{
public:
	MatchmakingScene() { Init(); }
	~MatchmakingScene() {}

private:
	void Event() override;
	void Init()  override;

	std::weak_ptr<MatchmakingUI> m_wpUI;
};
