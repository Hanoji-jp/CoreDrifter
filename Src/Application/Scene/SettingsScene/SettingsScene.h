#pragma once

#include "../BaseScene/BaseScene.h"

// SETTINGS画面。ESCでタイトルへ戻る。
class SettingsScene : public BaseScene
{
public:
	SettingsScene() { Init(); }
	~SettingsScene() {}

private:
	void Event() override;
	void Init()  override;
};
