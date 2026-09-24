#include "HjFoliage.h"

#include "HjHeightField.h"
#include "HjRoad.h"

#include "../../Const/FoliageConst.h"
#include "../../Const/TerrainConst.h"
#include "../../Util/HjProfiler.h"

namespace FC = FoliageConst;

namespace
{
	constexpr float Pi2 = 6.28318530718f;

	//======================================================
	// 座標から作る整数ハッシュ
	//
	// 乱数を使わない理由は、姿を毎回同じにするため。
	// 置くたびに乱数を引くと、同じ場所へ置き直しただけで
	// 木の向きと大きさが変わる。草も起動のたびに生え変わる
	//======================================================
	unsigned int Hash(int x, int z, unsigned int salt)
	{
		unsigned int h = static_cast<unsigned int>(x) * 374761393u
		               + static_cast<unsigned int>(z) * 668265263u
		               + salt * 2246822519u;

		h = (h ^ (h >> 13)) * 1274126177u;
		return h ^ (h >> 16);
	}

	float Rand01(unsigned int h)
	{
		return (h & 0xFFFFFFu) / static_cast<float>(0xFFFFFF);
	}

	float RandRange(unsigned int h, float lo, float hi)
	{
		return lo + (hi - lo) * Rand01(h);
	}

	// ワールド座標をハッシュの種にする。
	// cm 単位まで丸めて整数にする
	// ABGR へ詰める
	unsigned int PackColor(float r, float g, float b)
	{
		auto to8 = [](float v) -> unsigned int
		{
			const float c = (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v);
			return static_cast<unsigned int>(c * 255.0f + 0.5f);
		};

		return 0xFF000000u | (to8(b) << 16) | (to8(g) << 8) | to8(r);
	}

	//======================================================
	// 三角を1枚足す
	//
	// 法線は面から出して、3頂点とも同じ向きにする。
	// 平らに塗り分かれるので、この作品のフラットな絵と揃う。
	// 頂点を共有して滑らかにすると、低ポリゴンの木は
	// 溶けた飴のように見える
	//======================================================
	void AddTri(std::vector<KdMeshVertex>& verts, std::vector<KdMeshFace>& faces,
	            const Math::Vector3& a, const Math::Vector3& b, const Math::Vector3& c,
	            unsigned int col)
	{
		Math::Vector3 n = (b - a).Cross(c - a);
		if (n.LengthSquared() < 1e-12f) { return; }
		n.Normalize();

		const unsigned int base = static_cast<unsigned int>(verts.size());

		const Math::Vector3 p[3] = { a, b, c };
		for (int i = 0; i < 3; ++i)
		{
			KdMeshVertex v;
			v.Pos     = p[i];
			v.Normal  = n;
			v.Tangent = Math::Vector3(1.0f, 0.0f, 0.0f);
			v.UV      = Math::Vector2(0.0f, 0.0f);
			v.Color   = col;
			verts.push_back(v);
		}

		KdMeshFace f;
		f.Idx[0] = base;
		f.Idx[1] = base + 1;
		f.Idx[2] = base + 2;
		faces.push_back(f);
	}
}

//----------------------------------------------------------
void HjFoliage::Init()
{
	m_drawType = eDrawTypeLit;

	// 材質は1枚。色は頂点で持つ。
	// 木と草で分けると、まとまりを1枚のメッシュにできない
	m_materials.resize(1);
	m_materials[0].m_name = "foliage";
	m_materials[0].m_baseColorRate = Math::Vector4::One;
}

//----------------------------------------------------------
// 置ける場所か
//----------------------------------------------------------
bool HjFoliage::CanPlant(const HjHeightField& field, const HjRoad* road,
                         float wx, float wz, float upMin, float ownMax, float ownMin,
                         float& outY, Math::Vector3& outNormal) const
{
	if (!field.Contains(wx, wz)) { return false; }

	field.SampleAt(wx, wz, outY, outNormal);

	// 地形の外(データが無い所)には生やさない
	if (outY <= TerrainConst::OutsideHeight) { return false; }

	// 急な斜面には根が張れない。
	// 面の巻き方で法線が下を向くことがあるので大きさだけ見る
	const float up = (outNormal.y < 0.0f) ? -outNormal.y : outNormal.y;
	if (up < upMin) { return false; }

	if (!road) { return ownMin <= 0.0f; }

	float cx = 0.0f, cz = 0.0f;
	field.WorldToCell(wx, wz, cx, cz);

	const int nx = field.GetSizeX();
	const int gx = static_cast<int>(cx + 0.5f);
	const int gz = static_cast<int>(cz + 0.5f);

	if (gx < 0 || gz < 0 || gx >= nx || gz >= field.GetSizeZ()) { return false; }

	const int cell = gz * nx + gx;

	// 地形の面が無い所には生やさない。
	//
	// 道の裾に覆われたマス目は、地形の四角そのものを張っていない。
	// 高さマップは当たり判定用にそこも答えを返すので、
	// 高さだけを見て置くと、面の無い空中に木が立つ
	if (road->IsCoveredCell(cell)) { return false; }

	const float own = road->OwnWeightAt(cell);
	if (own > ownMax) { return false; }
	if (own < ownMin) { return false; }

	return true;
}

//----------------------------------------------------------
HjFoliage::Chunk& HjFoliage::ChunkAt(ChunkList& list, ChunkMap& map,
                                     const Math::Vector3& pos)
{
	const int cx = static_cast<int>(floorf(pos.x / FC::ChunkSize));
	const int cz = static_cast<int>(floorf(pos.z / FC::ChunkSize));

	const long long key = (static_cast<long long>(cz) << 32)
	                    ^ static_cast<long long>(static_cast<unsigned int>(cx));

	auto it = map.find(key);
	if (it != map.end()) { return list[it->second]; }

	map[key] = static_cast<int>(list.size());
	list.emplace_back();
	return list.back();
}

//----------------------------------------------------------
// 草を1株
//
// 板を十字に組む。1枚だと真横から見たときに消える
//----------------------------------------------------------
void HjFoliage::AddGrass(Chunk& c, const Math::Vector3& pos, float yaw,
                         float scale, unsigned int seed) const
{
	const float vary = (Rand01(seed * 6151u) - 0.5f) * FC::GrassVary;
	const unsigned int col = PackColor(
		FC::GrassR + vary * 0.5f,
		FC::GrassG + vary,
		FC::GrassB + vary * 0.4f);

	const float w = FC::GrassW * 0.5f * scale;
	const float h = FC::GrassH * scale;

	// 根本を少し埋める。ちょうどに置くと、斜面で角が浮いて隙間が見える
	const float baseY = pos.y - FC::GrassSink;

	for (int b = 0; b < FC::GrassBlades; ++b)
	{
		const float a = yaw + Pi2 * 0.5f * b / FC::GrassBlades;
		const float dx = cosf(a) * w;
		const float dz = sinf(a) * w;

		const Math::Vector3 l0(pos.x - dx, baseY,     pos.z - dz);
		const Math::Vector3 r0(pos.x + dx, baseY,     pos.z + dz);
		const Math::Vector3 l1(pos.x - dx, baseY + h, pos.z - dz);
		const Math::Vector3 r1(pos.x + dx, baseY + h, pos.z + dz);

		AddTri(c.verts, c.faces, l0, l1, r0, col);
		AddTri(c.verts, c.faces, r0, l1, r1, col);
	}
}

//----------------------------------------------------------
// 草を撒く
//
// 道の周りだけ。世界中に撒くと数十万株になるうえ、
// 走っていて目に入るのは道の周りだけ
//----------------------------------------------------------
void HjFoliage::BuildGrass(const HjHeightField& field, const HjRoad* road)
{
	HjScopedTimer _t(U8("草を撒く"));

	m_grassChunks.clear();
	m_grassLookup.clear();
	m_grassCount = 0;

	if (!field.IsValid() || !road) { return; }

	const float halfW = field.GetWorldW() * 0.5f;
	const float halfD = field.GetWorldD() * 0.5f;

	const int nx = static_cast<int>(field.GetWorldW() / FC::GrassSpacing);
	const int nz = static_cast<int>(field.GetWorldD() / FC::GrassSpacing);

	for (int iz = 0; iz < nz; ++iz)
	{
		for (int ix = 0; ix < nx; ++ix)
		{
			const unsigned int h0 = Hash(ix, iz, 11u);
			if (Rand01(h0) > FC::GrassDensity) { continue; }

			const unsigned int hx = Hash(ix, iz, 12u);
			const unsigned int hz = Hash(ix, iz, 13u);

			const float jx = (Rand01(hx) - 0.5f) * FC::GrassSpacing * FC::GrassJitter * 2.0f;
			const float jz = (Rand01(hz) - 0.5f) * FC::GrassSpacing * FC::GrassJitter * 2.0f;

			const float wx = ix * FC::GrassSpacing - halfW + jx;
			const float wz = iz * FC::GrassSpacing - halfD + jz;

			float y = 0.0f;
			Math::Vector3 n;
			if (!CanPlant(field, road, wx, wz,
			              FC::GrassUpMin, FC::GrassRoadMax, FC::GrassRoadMin, y, n))
			{
				continue;
			}

			const unsigned int hs = Hash(ix, iz, 14u);
			const unsigned int hy = Hash(ix, iz, 15u);

			const Math::Vector3 at(wx, y, wz);

			AddGrass(ChunkAt(m_grassChunks, m_grassLookup, at), at,
			         Rand01(hy) * Pi2,
			         RandRange(hs, FC::GrassScaleMin, FC::GrassScaleMax),
			         h0);

			++m_grassCount;
		}
	}

	FinishChunks(m_grassChunks);
}

//----------------------------------------------------------
// 溜めた面をメッシュにする
//----------------------------------------------------------
void HjFoliage::FinishChunks(ChunkList& list)
{
	std::vector<KdMeshSubset> subsets(1);

	for (auto& c : list)
	{
		if (c.faces.empty()) { continue; }

		// 画面に映るかを見る球。中身は動かないので1回で足りる
		for (const auto& v : c.verts)
		{
			if (!c.bounds) { c.lo = v.Pos; c.hi = v.Pos; c.bounds = true; }
			else
			{
				c.lo = Math::Vector3::Min(c.lo, v.Pos);
				c.hi = Math::Vector3::Max(c.hi, v.Pos);
			}
		}

		c.center = (c.lo + c.hi) * 0.5f;
		c.radius = (c.hi - c.lo).Length() * 0.5f;

		subsets[0].MaterialNo = 0;
		subsets[0].FaceStart  = 0;
		subsets[0].FaceCount  = static_cast<UINT>(c.faces.size());

		c.spMesh = std::make_shared<KdMesh>();
		if (!c.spMesh->Create(c.verts, c.faces, subsets, false)) { c.spMesh = nullptr; }

		// メッシュにした後は元の配列を捨てる。
		// 数十万頂点ぶんを持ち続ける理由がない
		c.verts.clear(); c.verts.shrink_to_fit();
		c.faces.clear(); c.faces.shrink_to_fit();
	}
}

//----------------------------------------------------------
int HjFoliage::GetChunkCount() const
{
	return static_cast<int>(m_grassChunks.size());
}

//----------------------------------------------------------
void HjFoliage::Cull()
{
	const auto& cam = KdShaderManager::Instance().GetCameraCB();

	DirectX::BoundingFrustum frustum;
	DirectX::BoundingFrustum::CreateFromMatrix(frustum, cam.mProj);

	Math::Matrix invView = cam.mView;
	invView = invView.Invert();
	frustum.Transform(frustum, invView);

	m_drawn = 0;

	auto cull = [&](ChunkList& list)
	{
		for (auto& c : list)
		{
			if (!c.spMesh) { continue; }

			c.visible = frustum.Intersects(DirectX::BoundingSphere(c.center, c.radius));
			if (c.visible) { ++m_drawn; }
		}
	};

	cull(m_grassChunks);
}

//----------------------------------------------------------
void HjFoliage::DrawList(const ChunkList& list, bool useVisible)
{
	auto& shader = KdShaderManager::Instance().m_StandardShader;

	for (const auto& c : list)
	{
		if (!c.spMesh) { continue; }
		if (useVisible && !c.visible) { continue; }

		shader.DrawMesh(c.spMesh.get(), Math::Matrix::Identity,
		                m_materials, kWhiteColor, Math::Vector3::Zero);
	}
}

//----------------------------------------------------------
void HjFoliage::DrawLit()
{
	HjScopedTimer _t(U8(" 植生の描画"));

	Cull();

	// 草の板は裏からも見える必要がある。
	// 木も、低ポリゴンだと隙間から裏面が覗く
	KdShaderManager::Instance().ChangeRasterizerState(KdRasterizerState::CullNone);

	DrawList(m_grassChunks, true);

	KdShaderManager::Instance().UndoRasterizerState();
}

//----------------------------------------------------------
void HjFoliage::GenerateDepthMapFromLight()
{
	// 影は光から描くので、カメラの範囲では切らない
	DrawList(m_grassChunks, false);
}
