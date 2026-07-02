#pragma once

//==========================================================
// Ground
//   仮の地面。Box.gltf を大きく平たくして敷くだけ（平地テスト用）。
//   後で重力相対の曲面地形に置き換える。
//==========================================================
class Ground : public KdGameObject
{
public:
	void Init()    override;
	void DrawLit() override;

private:
	KdModelWork m_model;   // 仮: Box
};
