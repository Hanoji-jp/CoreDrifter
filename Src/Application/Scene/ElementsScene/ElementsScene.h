#pragma once

#include "../BaseScene/BaseScene.h"

// ELEMENTS / UI KIT画面。ESCでタイトルへ戻る。
class ElementsScene : public BaseScene
{
public:
	ElementsScene() { Init(); }
	~ElementsScene() {}

private:
	void Event() override;
	void Init()  override;
};
