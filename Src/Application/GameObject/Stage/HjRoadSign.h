#pragma once

class HjRoad;

//==========================================================
// HjRoadSign
//   道路標識(カーブ注意)。自作なので Hj 接頭辞。
//
//   ■ 図柄まで形で作る
//   黄色の菱形と黒い矢印なので、板と多角形を組めば標識になる。
//   テクスチャを焼く仕組みを待たずに出せる。
//
//   ■ カーブの手前に置く
//   標識は「これから起きること」を伝えるものなので、
//   カーブの中に置いても意味が無い。入口から手前へ下げる。
//
//   ■ 1枚のメッシュにまとめる
//   柱と板と図柄を別々に描くと、1本で何回もの描画命令になる。
//   動かないので変換済みの頂点として詰める
//==========================================================
class HjRoadSign : public KdGameObject
{
public:
	// 道から組む。道を作り直したら、こちらも呼ぶ
	void Build(const HjRoad& road);

	void DrawLit() override;
	void GenerateDepthMapFromLight() override;

	// 道と同じで数百メートルにわたるので、球では収まらない
	bool CheckInScreen(const DirectX::BoundingFrustum&) const override { return true; }

	void SetVisible(bool on) { m_visible = on; }
	bool IsVisible() const { return m_visible; }

	int GetSignCount() const { return m_signs; }

private:
	// 1本ぶんを積む。
	//   base  : 柱の根元
	//   face  : 板が向く向き(道の上流へ)
	//   right : 板の横方向
	//   toRight : そのカーブが右へ曲がっているか
	void AddSign(const Math::Vector3& base,
	             const Math::Vector3& face, const Math::Vector3& right,
	             bool toRight,
	             std::vector<KdMeshVertex>& verts,
	             std::vector<KdMeshFace>& faces);

	std::shared_ptr<KdMesh> m_spMesh;
	std::vector<KdMaterial> m_materials;

	bool m_visible = true;
	int  m_signs   = 0;
};
