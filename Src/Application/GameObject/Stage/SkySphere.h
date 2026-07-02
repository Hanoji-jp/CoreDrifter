#pragma once

//==========================================================
// SkySphere
//   galaxybox.gltf を天球として、常にカメラを中心に大きく
//   陰影なし(UnLit)で描画する背景。
//==========================================================
class SkySphere : public KdGameObject
{
public:
	void Init()      override;
	void DrawUnLit() override;

private:
	KdModelWork m_model;
};
