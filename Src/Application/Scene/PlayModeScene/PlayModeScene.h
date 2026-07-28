#pragma once

#include "../BaseScene/BaseScene.h"

class PlayModeUI;

// プレイモード選択画面。ESCでタイトルへ戻る。決定でゲームへ。
class PlayModeScene : public BaseScene
{
public:
	PlayModeScene() { Init(); }
	~PlayModeScene() {}

private:
	void Event() override;
	void Init()  override;

	std::weak_ptr<PlayModeUI> m_wpUI;
};
