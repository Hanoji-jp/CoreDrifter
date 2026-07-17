#pragma once

#include"../BaseScene/BaseScene.h"

class TitleMenuUI;

class TitleScene : public BaseScene
{
public :

	TitleScene()  { Init(); }
	~TitleScene() {}

private :

	void Event() override;
	void Init()  override;

	// メニューUI(選択番号の参照用に保持)
	std::weak_ptr<TitleMenuUI> m_wpMenu;
};
