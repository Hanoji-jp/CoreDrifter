#pragma once

#include "HjUI.h"

//==========================================================
// ElementsUI
//   Drift Project UI の SCREEN5「ELEMENTS / UI KIT」。
//   HjUI の再利用ウィジェットで12種の部品カタログを描く。
//==========================================================
class ElementsUI : public KdGameObject
{
public:
	void DrawSprite() override;
};
