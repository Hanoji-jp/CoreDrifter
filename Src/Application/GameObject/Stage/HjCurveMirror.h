#pragma once

class HjRoad;

//==========================================================
// HjCurveMirror
//   カーブミラー。自作なので Hj 接頭辞。
//
//   ■ 峠の絵で一番「そこにある」もの
//   他の飾りは無くても道に見えるが、見通しの悪いヘアピンに
//   ミラーが無いと、道として不自然に見える。
//
//   ■ 立つのはカーブの外側
//   見通しを塞いでいるのは内側(山側)の斜面。その向こうを見るには、
//   塞いでいるものの反対側に立てる必要がある。
//
//   ■ 双面鏡
//   鏡を2枚付ける。1本で両方向から使えるので、
//   向きをどちらに決めるか悩まなくて済む
//==========================================================
class HjCurveMirror : public KdGameObject
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

	int GetCount() const { return m_count; }

private:
	// 1本ぶん。柱＋鏡2枚
	void AddMirror(const Math::Vector3& base,
	               const Math::Vector3& fwd, const Math::Vector3& right,
	               float outSign,
	               std::vector<KdMeshVertex>& verts,
	               std::vector<KdMeshFace>& faces);

	// 鏡1枚。縁・鏡面・庇
	void AddFace(const Math::Vector3& center, const Math::Vector3& normal,
	             std::vector<KdMeshVertex>& verts,
	             std::vector<KdMeshFace>& faces);

	std::shared_ptr<KdMesh> m_spMesh;
	std::vector<KdMaterial> m_materials;

	bool m_visible = true;
	int  m_count   = 0;
};
