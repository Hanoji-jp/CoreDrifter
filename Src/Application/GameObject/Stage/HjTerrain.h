#pragma once

#include "HjHeightField.h"

//==========================================================
// HjTerrain
//   高さマップから作る地形。自作なので Hj 接頭辞。
//
//   ■ 頂点はCPUで組む
//   頂点シェーダーで持ち上げる作りもあるが、採らない。
//   GPUの補間とC++の補間は細部が一致せず、
//   「見えている地面」と「当たる地面」がずれる。
//   場所によって車が少し埋まる・浮く、という追いにくいバグになる。
//
//   ここでは頂点も当たり判定も HjHeightField から作る。
//   同じ関数を通るので、ずれようがない。
//
//   ■ まとまりに分ける理由
//   1枚のメッシュにすると、画面外でも全部描くことになる。
//   区切っておけば、映っているぶんだけ描ける。
//   ステージのノード単位カリングと同じ考え方。
//==========================================================
class HjTerrain : public KdGameObject
{
public:
	void Init() override;

	// 地形の格子からメッシュを組む。
	//
	// ※道が地形を書き換えたあとに呼ぶこと。
	//   先に組むと、寄せる前の地形で頂点と法線を作ってしまう
	void BuildChunks();

	// この範囲に掛かるまとまりだけ組み直す。
	//
	// 道を動かすたびに全部を組み直すと、数千個ぶんの
	// 頂点バッファを毎フレーム作ることになり、操作が固まる。
	// 削れたのは道の周りだけなので、そこだけ作り直せばよい
	void RebuildInArea(const Math::Vector3& mn, const Math::Vector3& mx);
	void DrawLit() override;
	void GenerateDepthMapFromLight() override;

	// 場面側のオブジェクト単位のカリングから外す。
	//
	// 既定は「原点を中心にした球」で判定するが、地形は数キロ四方あり、
	// 中心から遠い所に立っていると丸ごと画面外と判断されて消える。
	// まとまりごとの判定はこちらで持っているので、ここは常に通す
	bool CheckInScreen(const DirectX::BoundingFrustum&) const override { return true; }

	//===== 高さの問い合わせ =====
	// 当たり判定はここを通す。
	// 光線を飛ばさないので、サスペンションのように
	// 1フレームに何十回聞かれても耐える
	const HjHeightField& Field() const { return m_field; }

	// 書き換えられる形で渡す。
	// 道を通すために、その周りの地形を寄せる必要がある
	HjHeightField& WorkField() { return m_field; }

	// 車を置ける場所。仮の地形では谷の中
	Math::Vector3 GetSpawnPos() const;

	// 調べ用。描いているまとまりの数
	int GetDrawnChunks() const { return m_drawn; }
	int GetChunkCount()  const { return static_cast<int>(m_chunks.size()); }

private:
	// 1つのまとまり。格子の一部を切り出したメッシュ
	struct Chunk
	{
		std::shared_ptr<KdMesh> spMesh;

		// 画面に映るかを見るための球。
		// 中身は動かないので、作るときに1回求めるだけでよい
		Math::Vector3 center;
		float         radius = 0.0f;

		bool visible = true;

		// 格子のどこを切り出したか。
		// 範囲を指定して組み直すときに、どれが掛かるかを見る
		int cellX = 0;
		int cellZ = 0;
	};

	// 格子からメッシュを組む
	// 映っているまとまりだけ残す。
	// ※描画の中で呼ぶこと。PreDraw ではカメラがまだ決まっていない
	void Cull();

	bool BuildOneChunk(int cellX, int cellZ, Chunk& out);

	HjHeightField      m_field;
	std::vector<Chunk> m_chunks;

	// 地形の材質。塗り分けは後の工程なので、いまは1枚
	std::vector<KdMaterial> m_materials;

	int m_drawn = 0;
};
