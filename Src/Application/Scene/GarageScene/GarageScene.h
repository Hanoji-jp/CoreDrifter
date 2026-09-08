#pragma once

#include "../BaseScene/BaseScene.h"

class GarageUI;
class HjCarPortrait;

//==========================================================
// GarageScene
//   車を選ぶ画面。タイトルの GARAGE から入る。
//
//   ■ なぜ独立した画面にするか
//   これまで車を選べるのは走行中のTABメニューだけだった。
//   あれは弄るための画面で、選んでも車は作り直されないので
//   見た目が変わらなかった。
//
//   ここで選べば、決定した時点で覚えて戻るだけ。
//   次に走り出すときには選んだ車で始まる。
//==========================================================
class GarageScene : public BaseScene
{
public:
	GarageScene() { Init(); }
	~GarageScene() {}

private:
	void Event() override;
	void Init()  override;

	std::weak_ptr<GarageUI>      m_wpUI;
	std::weak_ptr<HjCarPortrait> m_wpPortrait;
};
