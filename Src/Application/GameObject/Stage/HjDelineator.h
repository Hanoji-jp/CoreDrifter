#pragma once

class HjRoad;

//==========================================================
// HjDelineator
//   視線誘導標(デリニエータ)。自作なので Hj 接頭辞。
//
//   ■ 先を読むための道具
//   峠の暗い山道で情報を出しているのは、白線とこれの2つ。
//   カーブの外側に等間隔で並ぶので、まだ路面が見えていない段階で
//   「この先どちらへどれだけ曲がるか」が読める。
//
//   ■ カーブにだけ置く
//   直線に並べても意味が無いうえ、数が増えて重くなる。
//   曲がりの強さで間隔も詰める(実際の道路もそうしている)。
//
//   ■ 1枚のメッシュにまとめる
//   1本ずつ描くと数千回の描画命令になる。
//   動かないので、変換済みの頂点として詰めてしまう
//==========================================================
class HjDelineator : public KdGameObject
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

	int GetPostCount() const { return m_posts; }

private:
	// 1本ぶんを積む
	void AddPost(const Math::Vector3& base, const Math::Vector3& right,
	             float side,
	             std::vector<KdMeshVertex>& verts,
	             std::vector<KdMeshFace>& faces);

	std::shared_ptr<KdMesh> m_spMesh;
	std::vector<KdMaterial> m_materials;

	bool m_visible = true;
	int  m_posts   = 0;
};
