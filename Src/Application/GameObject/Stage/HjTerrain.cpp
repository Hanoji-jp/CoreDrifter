#include "HjTerrain.h"
#include "HjRoad.h"

#include "../../Util/HjProfiler.h"
#include "../../Const/SplatConst.h"

namespace TC = TerrainConst;

//----------------------------------------------------------
void HjTerrain::Init()
{
	m_drawType = eDrawTypeLit;

	// 実データがあれば使う。無ければ仮の地形。
	//
	// 仮でも動くようにしてあるのは、標高データを用意する前に
	// 「車が地面に乗るか」を確かめられるようにするため。
	// データ待ちで手が止まらない
	// 彫った形があれば、まずそれ。次に取り込んだ標高、無ければ仮の地形。
	//
	// 彫った形を先に見ないと、筆で作った峠が起動のたびに消える。
	// 実際、書き出しても読み戻していなかった
	if (!m_field.LoadEdited(TC::EditPath))
	{
		if (!m_field.LoadFromFile(TC::HeightPath))
		{
			m_field.BuildTestTerrain();
		}
	}

	// 材質は1枚。塗り分け(草・岩)は後の工程
	m_materials.resize(1);
	m_materials[0].m_name = "terrain";

	// 素材を貼るまでの仮の色。
	// 既定のままだと真っ白で、起伏があるのか分からない
	m_materials[0].m_baseColorRate =
		Math::Vector4(TC::GroundR, TC::GroundG, TC::GroundB, 1.0f);

	// ※メッシュはここで組まない。
	//   道が地形を寄せたあとに、場面から BuildChunks を呼ぶ。
	//   先に組むと、寄せる前の形で頂点と法線を作ってしまう
}

//----------------------------------------------------------
// 格子からメッシュを組む
//----------------------------------------------------------
void HjTerrain::BuildChunks()
{
	m_chunks.clear();
	if (!m_field.IsValid()) { return; }

	// マス目の数を、まとまりの大きさで割る。
	// 割り切れなくてよい(端のまとまりが小さくなるだけ)
	const int cells = TC::ChunkCells;
	const int nx = (m_field.GetSizeX() - 1 + cells - 1) / cells;
	const int nz = (m_field.GetSizeZ() - 1 + cells - 1) / cells;

	m_chunks.reserve(static_cast<size_t>(nx) * nz);

	for (int cz = 0; cz < nz; ++cz)
	{
		for (int cx = 0; cx < nx; ++cx)
		{
			Chunk c;
			c.cellX = cx * cells;
			c.cellZ = cz * cells;
			if (BuildOneChunk(c.cellX, c.cellZ, c))
			{
				m_chunks.push_back(std::move(c));
			}
		}
	}
}

//----------------------------------------------------------
// この範囲に掛かるまとまりだけ組み直す
//
// 道を動かすたびに全部を組み直すと、数千個ぶんの頂点バッファを
// 毎フレーム作ることになり、操作が固まる。
// 削れたのは道の周りだけなので、そこだけ作り直せばよい
//----------------------------------------------------------
void HjTerrain::RebuildInArea(const Math::Vector3& mn, const Math::Vector3& mx)
{
	if (!m_field.IsValid()) { return; }

	const float cs = m_field.GetCellSize();
	const int cells = TC::ChunkCells;

	// 法線は隣のマス目を見るので、範囲の1マス外まで影響する。
	// 余裕を持たせないと、継ぎ目に線が出る
	const float pad = cs * 2.0f;

	for (auto& c : m_chunks)
	{
		const float x0 = (c.cellX * cs) - m_field.GetWorldW() * 0.5f;
		const float z0 = (c.cellZ * cs) - m_field.GetWorldD() * 0.5f;
		const float x1 = x0 + cells * cs;
		const float z1 = z0 + cells * cs;

		// 掛かっていなければ触らない
		if (x1 < mn.x - pad || x0 > mx.x + pad) { continue; }
		if (z1 < mn.z - pad || z0 > mx.z + pad) { continue; }

		Chunk n;
		n.cellX = c.cellX;
		n.cellZ = c.cellZ;
		if (BuildOneChunk(c.cellX, c.cellZ, n)) { c = std::move(n); }
	}
}

//----------------------------------------------------------
// まとまり1つ分のメッシュ
//
// 頂点の高さは HjHeightField から取る。
// 当たり判定も同じ所から取るので、見た目とずれない
//----------------------------------------------------------
bool HjTerrain::BuildOneChunk(int cellX, int cellZ, Chunk& out)
{
	const int cells = TC::ChunkCells;
	const int lastX = std::min(cellX + cells, m_field.GetSizeX() - 1);
	const int lastZ = std::min(cellZ + cells, m_field.GetSizeZ() - 1);

	const int vx = lastX - cellX + 1;   // 頂点の数(マス目+1)
	const int vz = lastZ - cellZ + 1;
	if (vx < 2 || vz < 2) { return false; }

	const float cs   = m_field.GetCellSize();
	const float offX = m_field.GetWorldW() * 0.5f;
	const float offZ = m_field.GetWorldD() * 0.5f;

	std::vector<KdMeshVertex> verts;
	verts.reserve(static_cast<size_t>(vx) * vz);

	// 塗り分けの重みを出すのに、道の持ち分が要る。
	// 頂点ごとに弱参照を解くと重いので、ここで1回だけ解く
	const auto splatRoad = m_wpRoad.lock();
	const int  splatNx   = m_field.GetSizeX();

	float minY =  1e18f;
	float maxY = -1e18f;

	for (int z = 0; z < vz; ++z)
	{
		for (int x = 0; x < vx; ++x)
		{
			const int gx = cellX + x;
			const int gz = cellZ + z;

			// 格子の中心をワールドの原点に置く
			const float wx = gx * cs - offX;
			const float wz = gz * cs - offZ;
			const float wy = m_field.HeightAtCell(gx, gz);

			minY = std::min(minY, wy);
			maxY = std::max(maxY, wy);

			KdMeshVertex v;
			v.Pos = Math::Vector3(wx, wy, wz);

			// 法線は格子から求める。
			// 面から作ると、まとまりの継ぎ目で向きが食い違って線が出る
			v.Normal = m_field.NormalAt(wx, wz);

			// UVはマス目1つで1周。素材を貼るときの基準にする
			v.UV = Math::Vector2(static_cast<float>(gx), static_cast<float>(gz));

			v.Tangent = Math::Vector3(1.0f, 0.0f, 0.0f);

			// 頂点色は「色」ではなく塗り分けの重み。
			// 急な所は岩、道の近くは土、残りが草。
			//
			// 傾きは法線から取る。当たり判定と同じ関数で出しているので、
			// 見えている岩肌と、実際に滑る斜面がずれない
			const float own = splatRoad
				? splatRoad->OwnWeightAt(gz * splatNx + gx)
				: 0.0f;

			v.Color = SplatConst::Weights(v.Normal.y, own);
			verts.push_back(v);
		}
	}

	std::vector<KdMeshFace> faces;
	faces.reserve(static_cast<size_t>(vx - 1) * (vz - 1) * 2);

	// 裾に完全に覆われた四角は張らない。
	//
	// 重ねて張ると、同じ高さで深度が争ってちらつく。
	// テクスチャを貼ると、模様が入れ替わって見えるのでもっと目立つ。
	//
	// 削り(切り土・盛り土)は今までどおり。高さマップも触らない。
	// 消すのは描く三角だけなので、当たり判定もブラシも変わらない
	const auto road = m_wpRoad.lock();
	const int  nx   = m_field.GetSizeX();

	auto covered = [&](int gx, int gz) -> bool
	{
		if (!road) { return false; }
		return road->IsCoveredCell(gz * nx + gx);
	};

	for (int z = 0; z < vz - 1; ++z)
	{
		for (int x = 0; x < vx - 1; ++x)
		{
			// 4隅とも覆われているときだけ飛ばす。
			// 縁に掛かる四角を消すと、裾が落ちた所で空が見える
			const int gx = cellX + x;
			const int gz = cellZ + z;

			if (covered(gx, gz) && covered(gx + 1, gz)
			 && covered(gx, gz + 1) && covered(gx + 1, gz + 1))
			{
				continue;
			}

			const UINT i0 = static_cast<UINT>(z * vx + x);
			const UINT i1 = i0 + 1;
			const UINT i2 = i0 + vx;
			const UINT i3 = i2 + 1;

			// 表が上を向く順番。逆にすると裏面になって消える
			faces.push_back({ { i0, i2, i1 } });
			faces.push_back({ { i1, i2, i3 } });
		}
	}

	// 全部覆われていたら、そのまとまりは出さない
	if (faces.empty()) { out.spMesh = nullptr; return true; }

	std::vector<KdMeshSubset> subsets(1);
	subsets[0].MaterialNo = 0;
	subsets[0].FaceStart  = 0;
	subsets[0].FaceCount  = static_cast<UINT>(faces.size());

	out.spMesh = std::make_shared<KdMesh>();
	if (!out.spMesh->Create(verts, faces, subsets, false))
	{
		out.spMesh = nullptr;
		return false;
	}

	// 映るかを見るための球。中身は動かないので1回だけ求める
	const float cxW = (cellX + (vx - 1) * 0.5f) * cs - offX;
	const float czW = (cellZ + (vz - 1) * 0.5f) * cs - offZ;
	out.center = Math::Vector3(cxW, (minY + maxY) * 0.5f, czW);

	const float halfW = (vx - 1) * cs * 0.5f;
	const float halfD = (vz - 1) * cs * 0.5f;
	const float halfH = (maxY - minY) * 0.5f;
	out.radius = sqrtf(halfW * halfW + halfD * halfD + halfH * halfH);

	return true;
}

//----------------------------------------------------------
// 映っているまとまりだけ残す
//
// ※描画の中で呼ぶ。PreDraw ではカメラがまだ設定されていない。
//   地形はカメラより先に足しているので、そこで判定すると
//   前のフレームの向きで切ることになり、見ている方向が消える
//----------------------------------------------------------
void HjTerrain::Cull()
{
	HjScopedTimer _t(U8(" └ 地形のカリング"));

	// 切っていないときは全部描く
	m_drawn = static_cast<int>(m_chunks.size());
	if (!TC::UseCulling)
	{
		for (auto& c : m_chunks) { c.visible = true; }
		return;
	}

	const auto& cam = KdShaderManager::Instance().GetCameraCB();

	// カメラに映る範囲を作る。
	// 車のエフェクトで使っている考え方と同じ
	DirectX::BoundingFrustum frustum;
	DirectX::BoundingFrustum::CreateFromMatrix(frustum, cam.mProj);

	Math::Matrix invView = cam.mView;
	invView = invView.Invert();
	frustum.Transform(frustum, invView);

	m_drawn = 0;
	for (auto& c : m_chunks)
	{
		c.visible = frustum.Intersects(
			DirectX::BoundingSphere(c.center, c.radius));

		if (c.visible) { ++m_drawn; }
	}
}

//----------------------------------------------------------
void HjTerrain::DrawLit()
{
	HjScopedTimer _t(U8(" └ 地形の描画"));

	// 画面に映るまとまりだけ描く。
	// ここで呼ぶのは、この時点ならカメラが確実に決まっているから
	Cull();

	auto& shader = KdShaderManager::Instance().m_StandardShader;

	// DrawMesh は定数バッファに触らないので、まとまりの数だけ
	// 送り直す必要はない。1回送って、描き終わったら戻す
	ApplySplat(shader);

	for (const auto& c : m_chunks)
	{
		if (!c.visible || !c.spMesh) { continue; }

		shader.DrawMesh(c.spMesh.get(), Math::Matrix::Identity,
		                m_materials, kWhiteColor, Math::Vector3::Zero);
	}

	// 戻さないと、次に描かれる車が頂点色を重みとして読んで緑になる
	shader.PopCBObject();
}

//----------------------------------------------------------
// 塗り分けの設定をシェーダーへ送る
//----------------------------------------------------------
void HjTerrain::ApplySplat(KdStandardShader& shader)
{
	namespace SC = SplatConst;

	shader.SetSplatEnable(true);
	shader.SetSplatLook(SC::Grain, SC::GrainFreq, SC::MacroFreq);

	shader.SetSplatLayer(0, { SC::RockR,  SC::RockG,  SC::RockB  }, SC::RockRough);
	shader.SetSplatLayer(1, { SC::DirtR,  SC::DirtG,  SC::DirtB  }, SC::DirtRough);
	shader.SetSplatLayer(2, { SC::GrassR, SC::GrassG, SC::GrassB }, SC::GrassRough);
	shader.SetSplatLayer(3, { SC::RoadR,  SC::RoadG,  SC::RoadB  }, SC::RoadRough);

	// DrawMesh は定数バッファを送らないので、自分で送る
	shader.PushCBObject();
}

//----------------------------------------------------------
void HjTerrain::GenerateDepthMapFromLight()
{
	auto& shader = KdShaderManager::Instance().m_StandardShader;

	// 影の生成でも入れる。
	// 入れないと頂点色の A(=0) を透明度として読まれ、
	// 地面が影を落とさなくなる
	ApplySplat(shader);

	// 影は視点ではなく光から描くので、カメラの範囲では切れない。
	// ここは全部描く(距離で切るのは後の工程)
	for (const auto& c : m_chunks)
	{
		if (!c.spMesh) { continue; }

		shader.DrawMesh(c.spMesh.get(), Math::Matrix::Identity,
		                m_materials, kWhiteColor, Math::Vector3::Zero);
	}

	shader.PopCBObject();
}

//----------------------------------------------------------
// 車を置ける場所
//
// 仮の地形では、真ん中に谷を通してある。その手前へ置く
//----------------------------------------------------------
Math::Vector3 HjTerrain::GetSpawnPos() const
{
	if (!m_field.IsValid()) { return Math::Vector3::Zero; }

	// 谷はX=0を通っている。Zは手前寄り
	const float x = 0.0f;
	const float z = -m_field.GetWorldD() * 0.35f;

	return Math::Vector3(x, m_field.HeightAt(x, z), z);
}
