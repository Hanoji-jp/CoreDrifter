#include "HjRetainWall.h"

#include "HjRoad.h"
#include "HjHeightField.h"

#include "../../Util/HjProfiler.h"

namespace RW = RetainWallConst;

namespace
{
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

	// 刻みごとの色ムラ。整数ハッシュなので毎回同じ
	float VaryAt(int step)
	{
		unsigned int h = static_cast<unsigned int>(step) * 2654435761u;
		h = (h ^ (h >> 13)) * 1274126177u;
		h ^= (h >> 16);

		return ((h & 0xFFFFu) / 65535.0f - 0.5f) * RW::Vary;
	}

	// 断面の点の数。刻みごとに変えない(変えると帯として繋げない)
	// 根元 + 目地(段ごとに2点) + 天端まわり5点
	constexpr int NodeCount = 1 + RW::MaxRows * 2 + 5;
}

//----------------------------------------------------------
// 断面を組む
//
// 下から上へ。目地の所で2点を凹ませ、天端に笠石を付ける。
//
// ■ 点の数は固定
// 壁の高さは刻みごとに違うが、点の数を変えると隣の刻みと
// 繋げなくなる。低い所では、上の段を天端の高さへ潰して畳む。
// 潰れた四角は面積が無いだけで、絵には出ない
//----------------------------------------------------------
int HjRetainWall::BuildProfile(float h, Node* out)
{
	const float foot = -RW::FootSink;

	// 高さに応じた張り出し。上へ行くほど山側へ倒す
	auto batter = [&](float y) -> float
	{
		return RW::Batter * (y - foot);
	};

	int n = 0;

	// 根元
	out[n++] = { batter(foot), foot };

	// 目地。段の切れ目ごとに、少し引っ込んだ2点を挟む
	for (int r = 0; r < RW::MaxRows; ++r)
	{
		float y = foot + RW::PanelH * (r + 1);

		// 天端を越える段は、天端へ畳む
		if (y > h) { y = h; }

		out[n++] = { batter(y) + RW::JointDepth, y - RW::JointHalf };
		out[n++] = { batter(y) + RW::JointDepth, y + RW::JointHalf };
	}

	// 壁の天端
	out[n++] = { batter(h), h };

	// 笠石。道側へ張り出す。
	// これが無いと、ただの壁で終わって構造物に見えない
	out[n++] = { batter(h) - RW::CopingOut, h };
	out[n++] = { batter(h) - RW::CopingOut, h + RW::CopingH };
	out[n++] = { batter(h),                 h + RW::CopingH };

	// 天端の床。山側へ水平に伸ばして断面を閉じる。
	// 無いと一番上で面が終わり、上から覗くと壁の中が見える
	out[n++] = { batter(h) + RW::TopReach,  h + RW::CopingH };

	return n;
}

//----------------------------------------------------------
// 地形の起伏から高さを当てはめる
//
// 手で置く前の下敷き。
// 切り通しになっている所(道の縁の外の地形が路面より上)へ壁を入れる
//----------------------------------------------------------
void HjRetainWall::AutoFill(HjRoad& road, const HjHeightField* field)
{
	if (!field || !field->IsValid()) { return; }

	const int pts = road.PointCount();

	for (int i = 0; i < pts; ++i)
	{
		for (int side = 0; side < 2; ++side)
		{
			const float sign = (side == 1) ? 1.0f : -1.0f;

			// 制御点の道のりに一番近い刻みを探す。
			// 断面の向きは刻みでしか持っていない
			const int step = road.StepOfPoint(i);

			Math::Vector3 center, right;
			float miter = 1.0f;
			road.CrossAt(step, center, right, miter);

			const float edgeY = road.HeightAtOffset(step, RW::Offset * sign);

			const float off = (RW::Offset + RW::LookOut) * sign * miter;
			const float wx = center.x + right.x * off;
			const float wz = center.z + right.z * off;

			if (!field->Contains(wx, wz))
			{
				road.SetWallAt(i, side, 0.0f);
				continue;
			}

			const float rise = field->HeightAt(wx, wz) - edgeY;

			if (rise < RW::RiseThreshold)
			{
				road.SetWallAt(i, side, 0.0f);
				continue;
			}

			road.SetWallAt(i, side,
			               std::clamp(rise, RW::MinHeight, RW::MaxHeight));
		}
	}
}

//----------------------------------------------------------
// 片側ぶんを積む
//----------------------------------------------------------
void HjRetainWall::BuildSide(const HjRoad& road, int side,
                             std::vector<KdMeshVertex>& verts,
                             std::vector<KdMeshFace>& faces)
{
	const int n = road.StationCount();
	if (n < 2) { return; }

	const float sign = (side == 1) ? 1.0f : -1.0f;

	// 壁が立つ所。高さ 0 は壁なし
	std::vector<float> hs(n, 0.0f);
	for (int i = 0; i < n; ++i)
	{
		hs[i] = road.WallAtStep(i, side);
		// ほぼ 0 まで引っ張る。
		// 途中で切ると、そこに厚みの無い縁が切り口として出る
		if (hs[i] < RW::RunEpsilon) { hs[i] = 0.0f; }
	}

	Node prof[NodeCount];

	for (int i = 0; i < n; )
	{
		if (hs[i] <= 0.0f) { ++i; continue; }

		int j = i;
		while (j < n && hs[j] > 0.0f) { ++j; }

		// 1つの走りぶん。刻みをまたいで帯にする
		const int rows = j - i;
		if (rows < 2) { i = j; continue; }

		const UINT base = static_cast<UINT>(verts.size());

		for (int k = i; k < j; ++k)
		{
			BuildProfile(hs[k], prof);

			Math::Vector3 center, right;
			float miter = 1.0f;
			road.CrossAt(k, center, right, miter);

			const float roadY = road.HeightAtOffset(k, RW::Offset * sign);
			const float s     = road.StationAt(k);

			// 接線は進む向き。
			//
			// 断面から出る法線は必ず(横, 上)の面の中にあるので、
			// 進む向きは常にそれと直交する。
			//
			// 上向きを接線にしていたので、天端のような水平な面では
			// 法線と接線が平行になり、従接線が長さ0になって
			// 接空間の基底が潰れ、法線が壊れて真っ黒になっていた
			const Math::Vector3 tan(-right.z, 0.0f, right.x);

			const float vary = VaryAt(k);

			const unsigned int colWall = PackColor(RW::WallR + vary,
			                                       RW::WallG + vary,
			                                       RW::WallB + vary);
			const unsigned int colCope = PackColor(RW::CopeR + vary,
			                                       RW::CopeG + vary,
			                                       RW::CopeB + vary);

			// 断面の「線分」ごとに頂点を持つ。
			//
			// 点を共有すると、目地の角で法線が均されて溝が消える。
			// フラットに塗り分かれてこそ、コンクリートの継ぎ目に見える
			for (int sg = 0; sg + 1 < NodeCount; ++sg)
			{
				const Node& a = prof[sg];
				const Node& b = prof[sg + 1];

				// 断面の向きから法線を作る。
				// 面は道の側を向く
				float tx = b.out - a.out;
				float ty = b.y   - a.y;

				const float len = sqrtf(tx * tx + ty * ty);
				if (len > 1e-6f) { tx /= len; ty /= len; }

				const float nOut = -ty;
				const float nY   =  tx;

				const Math::Vector3 nrm = right * (sign * nOut)
				                        + Math::Vector3::Up * nY;

				// 笠石と天端は最後の4線分
				const bool cope = (sg >= NodeCount - 5);
				const unsigned int col = cope ? colCope : colWall;

				for (int e = 0; e < 2; ++e)
				{
					const Node& p = (e == 0) ? a : b;

					const float off = (RW::Offset + p.out) * sign * miter;

					KdMeshVertex v;
					v.Pos = Math::Vector3(center.x + right.x * off,
					                      roadY + p.y,
					                      center.z + right.z * off);
					v.Normal  = nrm;
					v.Tangent = tan;
					v.Color   = col;
					v.UV      = Math::Vector2(p.y * 0.5f, s * 0.5f);
					verts.push_back(v);
				}
			}

			m_length += (k > i) ? (road.StationAt(k) - road.StationAt(k - 1)) : 0.0f;
		}

		//===== 面を張る =====
		const int perRow = (NodeCount - 1) * 2;

		for (int r = 0; r + 1 < rows; ++r)
		{
			for (int sg = 0; sg + 1 < NodeCount; ++sg)
			{
				const UINT i0 = base + static_cast<UINT>(r * perRow + sg * 2);
				const UINT i1 = i0 + 1;
				const UINT i2 = base + static_cast<UINT>((r + 1) * perRow + sg * 2);
				const UINT i3 = i2 + 1;

				// 左右で巻きが逆になる。
				// 揃えないと片側の壁だけ裏返る
				if (side == 1)
				{
					faces.push_back({ { i0, i2, i1 } });
					faces.push_back({ { i1, i2, i3 } });
				}
				else
				{
					faces.push_back({ { i0, i1, i2 } });
					faces.push_back({ { i1, i3, i2 } });
				}
			}
		}

		//===== 走りの両端に蓋をする =====
		//
		// 壁は断面を掃引した開いた帯なので、そのままだと両端が
		// 切りっぱなしで、厚みの無い縁が切り口として見える。
		//
		// 断面は「根元 → 壁面 → 笠石 → 天端の床」と続く折れ線で、
		// 最後の点から根元へ戻せば閉じた多角形になる。
		// そこを塞げば、壁が土の中から出てきて土の中へ消える形になる。
		//
		// 高さが 0 まで落ちきって終わる走りでは、蓋は面積が無くなって
		// 勝手に消える(潰れた三角は絵に出ない)
		for (int e = 0; e < 2; ++e)
		{
			const int  r   = (e == 0) ? 0 : (rows - 1);
			const int  k   = i + r;
			const bool head = (e == 0);

			BuildProfile(hs[k], prof);

			Math::Vector3 center, right;
			float miter = 1.0f;
			road.CrossAt(k, center, right, miter);

			const float roadY = road.HeightAtOffset(k, RW::Offset * sign);

			// 蓋の向き。手前の端は上流、奥の端は下流を向く
			const Math::Vector3 fwd(-right.z, 0.0f, right.x);
			const Math::Vector3 nrm = head ? -fwd : fwd;

			const UINT cb = static_cast<UINT>(verts.size());

			for (int p2 = 0; p2 < NodeCount; ++p2)
			{
				const float off = (RW::Offset + prof[p2].out) * sign * miter;

				KdMeshVertex v;
				v.Pos = Math::Vector3(center.x + right.x * off,
				                      roadY + prof[p2].y,
				                      center.z + right.z * off);
				v.Normal  = nrm;

				// 蓋の法線は進む向きなので、接線は上でよい(直交する)
				v.Tangent = Math::Vector3::Up;
				v.Color   = PackColor(RW::WallR, RW::WallG, RW::WallB);
				v.UV      = Math::Vector2(prof[p2].out, prof[p2].y);
				verts.push_back(v);
			}

			// 根元から扇に割る。
			// 笠石のくぼみで少し重なるが、蓋なので中は見えない
			for (int p2 = 1; p2 + 1 < NodeCount; ++p2)
			{
				const UINT a0 = cb;
				const UINT a1 = cb + static_cast<UINT>(p2);
				const UINT a2 = cb + static_cast<UINT>(p2 + 1);

				// 裏面は張らない。
				//
				// この壁は CullNone で描いているので、1枚張れば両側から見える。
				// 重ねて張ると同じ位置に4枚が並び、どれが手前か決まらないうえ、
				// 裏向きの面はシェーダーが法線を反転するので、
				// 上を向いているはずの天端が下向きと解釈されて真っ黒になる
				faces.push_back({ { a0, a1, a2 } });
			}
		}

		i = j;
	}
}

//----------------------------------------------------------
void HjRetainWall::Build(const HjRoad& road)
{
	HjScopedTimer _t(U8("擁壁を組む"));

	m_drawType = eDrawTypeLit;
	m_spMesh.reset();
	m_length = 0.0f;

	if (m_materials.empty())
	{
		m_materials.resize(1);
		m_materials[0].m_name = "retainwall";
		m_materials[0].m_baseColorRate = Math::Vector4::One;
	}

	std::vector<KdMeshVertex> verts;
	std::vector<KdMeshFace>   faces;

	BuildSide(road, 0, verts, faces);
	BuildSide(road, 1, verts, faces);

	if (faces.empty()) { return; }

	std::vector<KdMeshSubset> subsets(1);
	subsets[0].MaterialNo = 0;
	subsets[0].FaceStart  = 0;
	subsets[0].FaceCount  = static_cast<UINT>(faces.size());

	m_spMesh = std::make_shared<KdMesh>();
	if (!m_spMesh->Create(verts, faces, subsets, false)) { m_spMesh = nullptr; }
}

//----------------------------------------------------------
void HjRetainWall::DrawLit()
{
	if (!m_visible || !m_spMesh) { return; }

	HjScopedTimer _t(U8(" 擁壁の描画"));

	// 両面を描く。
	//
	// ヘアピンではミターで断面の向きが強く傾くので、
	// 一部の四角だけ巻きが入れ替わることがある。
	// 片面だと、そこだけ穴が開いたように抜ける
	KdShaderManager::Instance().ChangeRasterizerState(KdRasterizerState::CullNone);

	KdShaderManager::Instance().m_StandardShader.DrawMesh(
		m_spMesh.get(), Math::Matrix::Identity,
		m_materials, kWhiteColor, Math::Vector3::Zero);

	KdShaderManager::Instance().UndoRasterizerState();
}

//----------------------------------------------------------
void HjRetainWall::GenerateDepthMapFromLight()
{
	if (!m_visible || !m_spMesh) { return; }

	KdShaderManager::Instance().m_StandardShader.DrawMesh(
		m_spMesh.get(), Math::Matrix::Identity,
		m_materials, kWhiteColor, Math::Vector3::Zero);
}
