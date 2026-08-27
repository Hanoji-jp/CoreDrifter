#pragma once

#include "../BaseScene/BaseScene.h"

class ResultsUI;

// 走り終えた後の成績。走行中にESCで来る。
class ResultsScene : public BaseScene
{
public:
	ResultsScene() { Init(); }
	~ResultsScene() {}

private:
	void Event() override;
	void Init()  override;

	std::weak_ptr<ResultsUI> m_wpUI;
};
