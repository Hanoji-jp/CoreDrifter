#pragma once

class HjHeightField;
class HjRoad;

//==========================================================
// HjFoliage
//   木と草。自作なので Hj 接頭辞。
//
//   ■ ここは草だけ
//   木と低木はモデルを手で置く(HjProps)。
//   草は1株ずつ置く意味がないので、道の周りへ自動で撒く。
//
//   ■ 焼き込みで描く
//   この枠組みには per-instance の描画が無い。
//   まとまりごとに、変換済みの頂点として1枚のメッシュへ詰める。
//
//   ■ 草は焼く
//   板を十字に組んだだけのものなので、変換済みの頂点として
//   まとまりへ詰めてしまうのが一番安い
//==========================================================
class HjFoliage : public KdGameObject
{
public:
	void Init() override;

	// 草を撒き直す。地形を彫ったら呼ぶ
	void BuildGrass(const HjHeightField& field, const HjRoad* road);

	void DrawLit() override;
	void GenerateDepthMapFromLight() override;

	// まとまりごとに判定するので、オブジェクト単位の判定からは外す
	bool CheckInScreen(const DirectX::BoundingFrustum&) const override { return true; }

	//===== 調べる用 =====
	int GetGrassCount() const { return m_grassCount; }
	int GetChunkCount() const;
	int GetDrawnCount() const { return m_drawn; }

private:
	// 焼き込み先。1つのまとまりぶん
	struct Chunk
	{
		std::vector<KdMeshVertex> verts;
		std::vector<KdMeshFace>   faces;

		std::shared_ptr<KdMesh> spMesh;

		// 画面に映るかを見る球。中身は動かないので1回求めるだけ
		Math::Vector3 center;
		float         radius = 0.0f;

		Math::Vector3 lo, hi;
		bool          bounds  = false;
		bool          visible = true;
	};

	using ChunkList = std::vector<Chunk>;
	using ChunkMap  = std::unordered_map<long long, int>;

	// 置ける場所か。斜面・道の持ち分・地形の面の有無で決める
	bool CanPlant(const HjHeightField& field, const HjRoad* road,
	              float wx, float wz, float upMin, float ownMax, float ownMin,
	              float& outY, Math::Vector3& outNormal) const;

	void AddGrass(Chunk& c, const Math::Vector3& pos, float yaw,
	              float scale, unsigned int seed) const;

	static Chunk& ChunkAt(ChunkList& list, ChunkMap& map, const Math::Vector3& pos);

	static void FinishChunks(ChunkList& list);

	void Cull();

	void DrawList(const ChunkList& list, bool useVisible);

	ChunkList m_grassChunks;
	ChunkMap  m_grassLookup;

	std::vector<KdMaterial> m_materials;

	int m_grassCount = 0;
	int m_drawn      = 0;
};
