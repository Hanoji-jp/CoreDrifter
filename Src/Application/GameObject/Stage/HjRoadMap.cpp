#include "HjRoadMap.h"

#include "HjRoad.h"
#include "HjHeightField.h"

namespace RM = RoadMapConst;

//----------------------------------------------------------
// 地形から陰影の絵を作る
//
// 標高をそのまま明るさにすると、尾根も谷も似た灰色になって
// 道を引ける所が分からない。
// 斜めから光を当てた陰影を混ぜると、地形の形が読める
//----------------------------------------------------------
void HjRoadMap::Build(const HjHeightField& field)
{
	m_spTex = nullptr;
	if (!field.IsValid()) { return; }

	const int N = RM::TexSize;

	// 標高の幅。これで明るさへ均す
	float lo = 1e18f, hi = -1e18f;
	for (int z = 0; z < field.GetSizeZ(); ++z)
	{
		for (int x = 0; x < field.GetSizeX(); ++x)
		{
			const float h = field.HeightAtCell(x, z);
			lo = std::min(lo, h);
			hi = std::max(hi, h);
		}
	}
	const float span = std::max(hi - lo, 1.0f);

	// 光の向きを揃える
	float lx = RM::LightX, ly = RM::LightY, lz = RM::LightZ;
	const float ln = sqrtf(lx * lx + ly * ly + lz * lz);
	lx /= ln; ly /= ln; lz /= ln;

	// 地形の格子を間引いて拾う。
	// 1476x1476 をそのまま使うと、絵を作るだけで待たされる
	const float sx = static_cast<float>(field.GetSizeX() - 1) / (N - 1);
	const float sz = static_cast<float>(field.GetSizeZ() - 1) / (N - 1);

	// 隣を見る距離(実寸)。陰影の傾きを出すのに要る
	const float d2 = field.GetCellSize() * std::max(sx, 1.0f);

	std::vector<unsigned int> px(static_cast<size_t>(N) * N);

	for (int y = 0; y < N; ++y)
	{
		for (int x = 0; x < N; ++x)
		{
			const int gx = static_cast<int>(x * sx);
			const int gz = static_cast<int>(y * sz);

			const int step = std::max(static_cast<int>(sx), 1);

			const float hl = field.HeightAtCell(gx - step, gz);
			const float hr = field.HeightAtCell(gx + step, gz);
			const float hb = field.HeightAtCell(gx, gz - step);
			const float hf = field.HeightAtCell(gx, gz + step);

			// 隣との差から斜面の向きを出す
			float nx = hl - hr;
			float ny = 2.0f * d2;
			float nz = hb - hf;
			const float nl = std::max(sqrtf(nx * nx + ny * ny + nz * nz), 1e-6f);
			nx /= nl; ny /= nl; nz /= nl;

			float shade = nx * lx + ny * ly + nz * lz;
			shade = std::clamp(shade * 0.5f + 0.5f, 0.0f, 1.0f);

			// 標高そのもの。高い所を明るく
			const float t = (field.HeightAtCell(gx, gz) - lo) / span;

			// 陰影を主にして、標高を薄く混ぜる。
			// 標高だけだと形が読めず、陰影だけだと高低が分からない
			float v = t * (1.0f - RM::ShadeMix) + shade * RM::ShadeMix;
			v = std::clamp(v, 0.0f, 1.0f);

			const unsigned int c = static_cast<unsigned int>(v * 255.0f);

			// ABGR。少し緑に寄せて、線の色と分ける
			px[static_cast<size_t>(y) * N + x] =
				0xFF000000u | (c << 16) | (static_cast<unsigned int>(c * 1.05f) << 8) | c;
		}
	}

	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = px.data();
	data.SysMemPitch = static_cast<UINT>(N * sizeof(unsigned int));

	m_spTex = std::make_shared<KdTexture>();
	if (!m_spTex->Create(N, N, DXGI_FORMAT_R8G8B8A8_UNORM, 1, &data))
	{
		m_spTex = nullptr;
	}
}

//----------------------------------------------------------
// 地図の中の位置(0〜1) ↔ ワールド
//
// 地形は中心が原点。地図の左上が北西
//----------------------------------------------------------
Math::Vector3 HjRoadMap::MapToWorld(float u, float v, const HjHeightField& field) const
{
	const float w = field.GetWorldW();
	const float d = field.GetWorldD();

	// 地図の下が南。ワールドのZは北が正なので、Vを反転する
	return Math::Vector3(u * w - w * 0.5f, 0.0f, (1.0f - v) * d - d * 0.5f);
}

void HjRoadMap::WorldToMap(const Math::Vector3& p, const HjHeightField& field,
                           float& outU, float& outV) const
{
	const float w = std::max(field.GetWorldW(), 1.0f);
	const float d = std::max(field.GetWorldD(), 1.0f);

	outU = (p.x + w * 0.5f) / w;
	outV = 1.0f - (p.z + d * 0.5f) / d;
}

//----------------------------------------------------------
// 地図を出して、線を引く
//----------------------------------------------------------
void HjRoadMap::DrawGui(HjRoad& road, const HjHeightField& field)
{
	if (!field.IsValid())
	{
		ImGui::TextDisabled(U8("地形が無い"));
		return;
	}

	if (!m_spTex) { Build(field); }

	//===== 操作 =====
	ImGui::Checkbox(U8("線を継ぎ足す"), &m_appending);
	ImGui::SameLine();
	ImGui::TextDisabled(m_appending
		? U8("地図を押すと末尾へ足していく")
		: U8("点を掴んで動かす"));

	if (ImGui::Button(U8("選んだ点を消す")) && m_selected >= 0)
	{
		road.ErasePoint(m_selected);
		m_selected = -1;
	}
	ImGui::SameLine();
	if (ImGui::Button(U8("書き出す"))) { road.SavePath(); }

	//===== 地図 =====
	const float S = RM::ViewSize;
	const ImVec2 origin = ImGui::GetCursorScreenPos();

	if (m_spTex && m_spTex->WorkSRView())
	{
		ImGui::Image((ImTextureID)(intptr_t)m_spTex->WorkSRView(), ImVec2(S, S));
	}
	else
	{
		// 絵が作れなくても線は引けるようにする
		ImGui::Dummy(ImVec2(S, S));
	}

	const bool hovered = ImGui::IsItemHovered();
	auto* dl = ImGui::GetWindowDrawList();

	// 地図の中でのマウス位置(0〜1)。
	// ここが肝で、3Dのような変換を通さないのでずれようがない
	const ImVec2 mp = ImGui::GetIO().MousePos;
	const float mu = (mp.x - origin.x) / S;
	const float mv = (mp.y - origin.y) / S;

	const int n = road.PointCount();

	//===== 道を描く =====
	// 実際の幅を縮尺どおりに描くと細すぎて見えないので、
	// 太い帯を薄く重ねて幅の目安にする
	for (int i = 0; i + 1 < n; ++i)
	{
		float u0, v0, u1, v1;
		WorldToMap(road.GetPoint(i), field, u0, v0);
		WorldToMap(road.GetPoint(i + 1), field, u1, v1);

		const ImVec2 a(origin.x + u0 * S, origin.y + v0 * S);
		const ImVec2 b(origin.x + u1 * S, origin.y + v1 * S);

		dl->AddLine(a, b, RM::ColBand, RM::RoadBandWidth);
		dl->AddLine(a, b, RM::ColLine, RM::LineWidth);
	}

	//===== 点を描く =====
	for (int i = 0; i < n; ++i)
	{
		float u, v;
		WorldToMap(road.GetPoint(i), field, u, v);

		const ImVec2 c(origin.x + u * S, origin.y + v * S);
		const bool sel = (i == m_selected);

		dl->AddCircleFilled(c, sel ? RM::PointRadiusHot : RM::PointRadius,
		                    sel ? RM::ColPointHot : RM::ColPoint);
	}

	//===== 車 =====
	// どこを走っているかが分かる
	{
		float u, v;
		WorldToMap(m_carPos, field, u, v);
		if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)
		{
			const ImVec2 c(origin.x + u * S, origin.y + v * S);
			dl->AddCircle(c, RM::PointRadiusHot, RM::ColCar, 0, 2.0f);
		}
	}

	//===== 入力 =====
	if (!hovered) { return; }

	const bool lmb  = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	const bool lmbHit = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

	// 一番近い点を探す。画面のピクセルで測る。
	//
	// ※near は Windows のマクロなので使えない。
	//   使うと、意味不明な構文エラーになって原因が追いにくい
	int   hitIndex = -1;
	float hitDist  = RM::GrabRadius;

	for (int i = 0; i < n; ++i)
	{
		float u, v;
		WorldToMap(road.GetPoint(i), field, u, v);

		const float dx = (origin.x + u * S) - mp.x;
		const float dy = (origin.y + v * S) - mp.y;
		const float d = sqrtf(dx * dx + dy * dy);

		if (d < hitDist) { hitDist = d; hitIndex = i; }
	}

	if (lmbHit)
	{
		if (m_appending)
		{
			// 末尾へ足していく。押すたびに道が伸びる
			road.AppendPoint(MapToWorld(mu, mv, field));
			m_selected = road.PointCount() - 1;
		}
		else if (hitIndex >= 0)
		{
			m_selected = hitIndex;
			m_dragging = hitIndex;
		}
		else
		{
			m_selected = -1;
		}
	}

	// 引きずる。地図の上なのでマウスの位置がそのまま座標になる
	if (lmb && m_dragging >= 0 && m_dragging < road.PointCount())
	{
		const Math::Vector3 w = MapToWorld(mu, mv, field);
		const Math::Vector3 cur = road.GetPoint(m_dragging);

		// 高さ(持ち上げ)は触らない。平面の位置だけ決める
		road.MovePoint(m_dragging, Math::Vector3(w.x, cur.y, w.z));
	}

	if (!lmb) { m_dragging = -1; }
}
