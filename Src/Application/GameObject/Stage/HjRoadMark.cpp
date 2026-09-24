#include "HjRoadMark.h"

#include "HjRoad.h"

#include "../../Const/RoadMarkConst.h"
#include "../../Util/HjProfiler.h"

namespace RM = RoadMarkConst;

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

	// 道のりから、その地点が線の実部か
	bool DashOn(float s)
	{
		const float period = RM::DashOn + RM::DashOff;
		if (period <= 0.01f) { return true; }

		const float t = s - floorf(s / period) * period;
		return t < RM::DashOn;
	}
}

//----------------------------------------------------------
// 1本ぶんの帯を積む
//
// 断面は2点(線の左端と右端)。刻みごとに並べて押し出す。
//
// 高さは road.HeightAtOffset から取る。路面と同じ式なので、
// カントの付いた所でも線が浮いたり沈んだりしない
//----------------------------------------------------------
void HjRoadMark::BuildStripe(const HjRoad& road, float offset, float width,
                             bool dashed, unsigned int color,
                             std::vector<KdMeshVertex>& verts,
                             std::vector<KdMeshFace>& faces,
                             const std::vector<std::pair<float, float>>* skip)
{
	const int n = road.StationCount();
	if (n < 2) { return; }

	const float half = width * 0.5f;

	// その道のりで線を引くか。
	// ヘアピンはセンターラインを切るので、そこだけ空ける
	auto on = [&](float s) -> bool
	{
		if (dashed && !DashOn(s)) { return false; }

		if (skip)
		{
			for (const auto& r : *skip)
			{
				if (s >= r.first && s <= r.second) { return false; }
			}
		}
		return true;
	};

	// 実部が続いている区間ごとに、1つの帯にする
	for (int i = 0; i < n; )
	{
		if (!on(road.StationAt(i))) { ++i; continue; }

		int j = i;
		while (j < n && on(road.StationAt(j))) { ++j; }

		const int rows = j - i;
		if (rows < 2) { i = j; continue; }

		const UINT base = static_cast<UINT>(verts.size());

		for (int k = i; k < j; ++k)
		{
			Math::Vector3 center, right;
			float miter = 1.0f;
			road.CrossAt(k, center, right, miter);

			const float s = road.StationAt(k);

			// 線の左端と右端。
			// ミターは断面の張り出しなので、線の位置にも掛ける
			for (int e = 0; e < 2; ++e)
			{
				const float off = offset + ((e == 0) ? -half : half);

				// 高さは路面の式から。カントが付いていても線が乗る
				const float y = road.HeightAtOffset(k, off) + RM::Lift;

				const float wo = off * miter;

				KdMeshVertex v;
				v.Pos = Math::Vector3(center.x + right.x * wo,
				                      y,
				                      center.z + right.z * wo);

				// 法線は上。路面と同じ向きにしておかないと、
				// 線だけ違う明るさになって浮いて見える
				v.Normal  = Math::Vector3::Up;
				v.Tangent = right;
				v.Color   = color;
				v.UV      = Math::Vector2(static_cast<float>(e), s * 0.5f);

				verts.push_back(v);
			}

			m_length += (k > i) ? (road.StationAt(k) - road.StationAt(k - 1)) : 0.0f;
		}

		for (int r = 0; r + 1 < rows; ++r)
		{
			const UINT i0 = base + static_cast<UINT>(r * 2);
			const UINT i1 = i0 + 1;
			const UINT i2 = i0 + 2;
			const UINT i3 = i0 + 3;

			faces.push_back({ { i0, i2, i1 } });
			faces.push_back({ { i1, i2, i3 } });
		}

		i = j;
	}
}

//----------------------------------------------------------
// 片側の車線へ、横向きの帯を1本
//
// 道のりで位置を指す。刻みの番号ではなく道のりで受けるのは、
// 間隔を詰めていく都合で、刻みと合わない所へ置きたいため。
//
// ■ 片側だけに引く
// 減速マークは「これから曲がる人」へ向けたものなので、
// その人が走っている車線にしか引かれない。
// 中心線を挟んで左右対称に引くと、対向車線にも
// 出口側から見た減速マークが生えることになる
//----------------------------------------------------------
void HjRoadMark::AddLaneBar(const HjRoad& road, float s, float sign,
                            float inner, float outer, float len,
                            unsigned int color,
                            std::vector<KdMeshVertex>& verts,
                            std::vector<KdMeshFace>& faces)
{
	if (road.StationCount() < 2) { return; }

	const float half  = len * 0.5f;
	const float total = road.TotalLength();

	// 道の端では、帯の両端が道からはみ出す
	const float sA = std::clamp(s - half, 0.0f, total);
	const float sB = std::clamp(s + half, 0.0f, total);
	if (sB - sA < 1e-3f) { return; }

	//===== 両端それぞれで断面を引く =====
	//
	// ■ 一番近い刻みで代用してはいけない
	// 前は最寄りの刻みの断面を借りて、そこから進む向きへ真っ直ぐ
	// ずらしていた。刻みは最大3m間隔なので、ヘアピンでは
	// 1.5m ぶんを直線で外挿することになり、帯が道からはみ出して
	// 斜めに寝る。横向きの標示は道を横断するものなので、
	// 角度がずれた時点で成立しない。
	//
	// 端ごとに断面を引けば、帯そのものがカーブに沿って曲がる。
	// 勾配にも両端が別々に乗るので、坂で浮いたり沈んだりしない
	Math::Vector3 cen[2], rgt[2];
	float mit[2] = { 1.0f, 1.0f };

	road.FrameAtS(sA, cen[0], rgt[0], mit[0]);
	road.FrameAtS(sB, cen[1], rgt[1], mit[1]);

	const float ends[2] = { sA, sB };
	const float offs[2] = { outer * sign, inner * sign };

	const UINT base = static_cast<UINT>(verts.size());

	for (int e = 0; e < 2; ++e)          // 手前・奥
	{
		for (int w = 0; w < 2; ++w)      // 外・内
		{
			const float off = offs[w];

			// 高さも道のりで引く。カントにも勾配にも乗る
			const float y = road.HeightAtS(ends[e], off) + RM::Lift;

			const float wo = off * mit[e];

			KdMeshVertex v;
			v.Pos = Math::Vector3(cen[e].x + rgt[e].x * wo,
			                      y,
			                      cen[e].z + rgt[e].z * wo);
			v.Normal  = Math::Vector3::Up;
			v.Tangent = rgt[e];
			v.Color   = color;
			v.UV      = Math::Vector2(static_cast<float>(w),
			                          static_cast<float>(e));
			verts.push_back(v);
		}
	}

	// 裏面は張らない。
	//
	// 路面標示は CullNone で描いているので、1枚張れば両側から見える。
	// 重ねて張ると同じ位置に4枚が並び、どれが手前か決まらないうえ、
	// 裏向きの面はシェーダーが法線を反転するので、
	// 上を向いているはずの帯が下向きと解釈されて黒くなる
	faces.push_back({ { base,     base + 2, base + 1 } });
	faces.push_back({ { base + 1, base + 2, base + 3 } });
}

//----------------------------------------------------------
// カーブ1つぶん。外側にだけ並べる
//
// 入口の手前から並べ始めて、カーブを抜けるまで続ける。
// 間隔は入口へ近づくほど詰め、カーブの中では詰めたまま保つ。
//
// 詰めていくのは錯覚のため。等間隔に並べてもただの縞だが、
// 詰まっていくと同じ速度で走っていても帯が速く流れるので、
// 速度が上がったように感じる
//----------------------------------------------------------
void HjRoadMark::AddOuterBars(const HjRoad& road, float entryS, float exitS,
                              float sign, float inner, unsigned int color,
                              std::vector<KdMeshVertex>& verts,
                              std::vector<KdMeshFace>& faces)
{
	const float total = road.TotalLength();

	float at = std::max(entryS - RM::SlowLead, 0.0f);
	const float end = std::min(exitS, total);

	int guard = 0;

	while (at <= end && guard < RM::SlowMaxBars)
	{
		++guard;

		AddLaneBar(road, at, sign, inner, RM::SlowOuter,
		           RM::SlowBarLen, color, verts, faces);

		// 入口までの残り。カーブの中に入ったら 0
		const float left = std::max(entryS - at, 0.0f);

		const float t = std::clamp(1.0f - left / std::max(RM::SlowLead, 0.01f),
		                           0.0f, 1.0f);

		const float gap = RM::SlowGapFar
		                + (RM::SlowGapNear - RM::SlowGapFar) * t;

		at += std::max(gap, 0.3f);
	}
}

//----------------------------------------------------------
// 進入側の車線へ、その人の手前から並べる
//
// 強いカーブでだけ使う。両方向から危ないので、
// 両方の車線に出る。ただし同じ場所ではなく、
// それぞれの進入側なので、カーブを挟んで反対の端から始まる
//----------------------------------------------------------
void HjRoadMark::AddApproach(const HjRoad& road, float entryS,
                             float dir, float sign, unsigned int color,
                             std::vector<KdMeshVertex>& verts,
                             std::vector<KdMeshFace>& faces)
{
	const float total = road.TotalLength();

	float left = RM::SlowLead;
	int   guard = 0;

	while (left > 0.0f && guard < RM::SlowMaxBars)
	{
		++guard;

		const float at = entryS - dir * left;
		if (at < 0.0f || at > total) { break; }

		AddLaneBar(road, at, sign, RM::SlowInner, RM::SlowOuter,
		           RM::SlowBarLen, color, verts, faces);

		// 入口へ近いほど詰まる
		const float t = std::clamp(1.0f - left / std::max(RM::SlowLead, 0.01f),
		                           0.0f, 1.0f);

		const float gap = RM::SlowGapFar
		                + (RM::SlowGapNear - RM::SlowGapFar) * t;

		left -= std::max(gap, 0.3f);
	}
}

//----------------------------------------------------------
// カーブの区間と強さを拾う
//
// 道を1回なめるだけ。標示も、センターラインを切る区間も、
// ここで出した結果から決める
//----------------------------------------------------------
void HjRoadMark::CollectCurves(const HjRoad& road, std::vector<Curve>& out)
{
	out.clear();

	const int n = road.StationCount();
	if (n < 3) { return; }

	bool  inCurve = false;
	int   runStart = 0;
	float runPeak = 0.0f;

	float lastEntry = -1e9f;

	for (int i = 1; i < n; ++i)
	{
		float turn = 0.0f;

		if (i < n - 1)
		{
			// 向きの決め方は道が持っている。
			// 写して持つと、符号を取り違えたときに直す所が散らばる
			turn = road.TurnAt(i);
		}

		// 一番弱い段より下は、そもそもカーブとして拾わない
		const bool tight = (fabsf(turn) >= RM::TierMid);

		if (tight)
		{
			if (!inCurve)
			{
				inCurve  = true;
				runStart = i;
				runPeak  = turn;
			}
			else if (fabsf(turn) > fabsf(runPeak))
			{
				// 一番強かった所を採る。
				// 入口だけで決めると、S字の入りではまだ曲がりが弱く、
				// 段も向きも取り違える
				runPeak = turn;
			}
			continue;
		}

		if (!inCurve) { continue; }

		inCurve = false;

		Curve c;
		c.entryS = road.StationAt(runStart);
		c.exitS  = road.StationAt(i - 1);
		c.peak   = runPeak;

		if (c.entryS - lastEntry < RM::SlowMinGap) { continue; }
		lastEntry = c.entryS;

		const float mag = fabsf(runPeak);

		c.tier = (mag >= RM::TierHairpin) ? 3
		       : (mag >= RM::TierStrong)  ? 2
		       : 1;

		out.push_back(c);
	}
}

//----------------------------------------------------------
// センターラインを消す区間
//
// ■ 連続していることが条件
// ヘアピンが1つあるだけの道なら幅は足りているので、線は残る。
// 消えるのは九十九折りになっている区間。
//
// 近いヘアピン同士をひと続きにまとめて、本数が足りたものだけ消す
//----------------------------------------------------------
void HjRoadMark::CenterCutRanges(const std::vector<Curve>& curves,
                                 std::vector<std::pair<float, float>>& out)
{
	out.clear();

	for (size_t i = 0; i < curves.size(); )
	{
		if (curves[i].tier < 3) { ++i; continue; }

		// 近いヘアピンをまとめる
		size_t j = i;
		int    count = 1;

		while (j + 1 < curves.size()
		    && curves[j + 1].tier >= 3
		    && curves[j + 1].entryS - curves[j].exitS <= RM::HairpinLinkDist)
		{
			++j;
			++count;
		}

		if (count >= RM::HairpinRunMin)
		{
			out.emplace_back(curves[i].entryS - RM::NoMarkPad,
			                 curves[j].exitS  + RM::NoMarkPad);
		}

		i = j + 1;
	}
}

//----------------------------------------------------------
// カーブの減速マーク
//
// ■ 曲がりの強さで段が変わる
// いろは坂を見ると、緩いカーブには何も無く、きつくなるほど
// 外側だけ → 両車線 → センターラインごと消えて1車線、と
// 段が上がっていく。
//
// 一律に引くと、その段が消えて「どこも同じ危険度」に見える。
// 走る側からすると、標示は危険度の物差しでもある
//
// ■ 外側は曲がる向きで決まる
// 右へ曲がるカーブなら外側は左。左へ曲がるなら右。
// 膨らんで落ちる側への警告なので、内側には引かない
//----------------------------------------------------------
void HjRoadMark::BuildSlowBars(const HjRoad& road, unsigned int color,
                               std::vector<KdMeshVertex>& verts,
                               std::vector<KdMeshFace>& faces)
{
	std::vector<Curve> curves;
	CollectCurves(road, curves);

	const unsigned int yellow = PackColor(RM::YellowR, RM::YellowG, RM::YellowB);

	for (const auto& c : curves)
	{
		// 外側は曲がる向きの逆。
		// 断面の向き(right)は道のりが増える向きに対しての右
		const float outSign = (c.peak > 0.0f) ? -1.0f : 1.0f;

		// 強いカーブは追越禁止が掛かる区間なので、標示も黄になる
		const bool strong = (c.tier >= 2);

		const unsigned int col = (strong && RM::StrongUsesYellow) ? yellow : color;

		// ヘアピンは1車線扱い。センターラインが無いので、
		// 帯も道幅いっぱいまで引く
		const float inner = (c.tier >= 3) ? RM::SlowInnerWide : RM::SlowInner;

		AddOuterBars(road, c.entryS, c.exitS, outSign, inner, col, verts, faces);

		if (c.tier != 2) { continue; }

		//===== 強いカーブは両車線の進入側にも =====
		// 両方向から危ないので両方に出るが、同じ場所ではない。
		// それぞれの進入側なので、カーブを挟んで反対の端から始まる。
		//
		// 日本は左側通行。順方向に走る人の車線は左(負)、
		// 逆方向の人の車線は右(正)。逆方向の人にとっての入口は、
		// 道のりで言えばカーブの出口側
		AddApproach(road, c.entryS,  1.0f, -1.0f, yellow, verts, faces);
		AddApproach(road, c.exitS,  -1.0f,  1.0f, yellow, verts, faces);
	}
}

//----------------------------------------------------------
void HjRoadMark::Build(const HjRoad& road)
{
	HjScopedTimer _t(U8("路面標示を組む"));

	m_drawType = eDrawTypeLit;
	m_spMesh.reset();
	m_length = 0.0f;

	if (m_materials.empty())
	{
		m_materials.resize(1);
		m_materials[0].m_name = "roadmark";
		m_materials[0].m_baseColorRate = Math::Vector4::One;
	}

	std::vector<KdMeshVertex> verts;
	std::vector<KdMeshFace>   faces;

	const unsigned int white  = PackColor(RM::WhiteR,  RM::WhiteG,  RM::WhiteB);
	const unsigned int yellow = PackColor(RM::YellowR, RM::YellowG, RM::YellowB);

	// 外側線。路面の縁の少し内側に、左右へ1本ずつ。
	//
	// こちらはヘアピンでも消さない。路肩との境を示すもので、
	// 車線を分ける線とは役目が違う。
	// 幅が足りなくても、路肩の位置は示す必要がある
	if (RM::ShowEdge)
	{
		BuildStripe(road, -RM::EdgeOffset, RM::EdgeWidth, false, white, verts, faces);
		BuildStripe(road,  RM::EdgeOffset, RM::EdgeWidth, false, white, verts, faces);
	}

	// センターライン。
	// 峠は黄色の実線(追越禁止)。白の破線にすると、
	// 追い越してよい道に見えてしまう。
	//
	// ヘアピンが続く区間では消す。道幅が足りなくて1車線扱いになる
	if (RM::ShowCenter)
	{
		std::vector<Curve> curves;
		CollectCurves(road, curves);

		std::vector<std::pair<float, float>> cut;
		CenterCutRanges(curves, cut);

		BuildStripe(road, 0.0f, RM::CenterWidth, RM::CenterDashed,
		            yellow, verts, faces, cut.empty() ? nullptr : &cut);
	}

	// カーブ手前の減速マーク。
	// 間隔を詰めていくので、同じ速度でも帯が速く流れて見える
	if (RM::ShowSlowBars)
	{
		BuildSlowBars(road, white, verts, faces);
	}

	if (faces.empty()) { return; }

	std::vector<KdMeshSubset> subsets(1);
	subsets[0].MaterialNo = 0;
	subsets[0].FaceStart  = 0;
	subsets[0].FaceCount  = static_cast<UINT>(faces.size());

	m_spMesh = std::make_shared<KdMesh>();
	if (!m_spMesh->Create(verts, faces, subsets, false)) { m_spMesh = nullptr; }
}

//----------------------------------------------------------
void HjRoadMark::DrawLit()
{
	if (!m_visible || !m_spMesh) { return; }

	HjScopedTimer _t(U8(" 路面標示の描画"));

	// 両面で描く。
	// ヘアピンではミターで断面が強く傾くので、
	// 一部の四角だけ巻きが入れ替わることがある
	KdShaderManager::Instance().ChangeRasterizerState(KdRasterizerState::CullNone);

	KdShaderManager::Instance().m_StandardShader.DrawMesh(
		m_spMesh.get(), Math::Matrix::Identity,
		m_materials, kWhiteColor, Math::Vector3::Zero);

	KdShaderManager::Instance().UndoRasterizerState();
}

//----------------------------------------------------------
void HjRoadMark::GenerateDepthMapFromLight()
{
	// 影は落とさない。
	//
	// 厚みの無い帯なので、影を落とすと路面に自分の影が乗って
	// 線が二重になる
}
