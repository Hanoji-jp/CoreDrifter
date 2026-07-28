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

	// 天球はカメラを常に包んでいるので視錐台カリングの対象外。
	// (既定判定だと原点まわりの小さな球と見なされ、空を向いていない時に消える)
	bool CheckInScreen(const DirectX::BoundingFrustum&) const override { return true; }

private:
	KdModelWork m_model;
};
