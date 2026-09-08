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
// 道を辿って、急すぎるコーナーを拾う
//
// 曲がりの半径が道の外端より小さいと、内側のふちの半径が
// 負になる。面が裏返って、道が作れない。
//
// 実測では、制御点63個のうち5個がここに当たっていた。
// 数字を見せないと、どの点を直せばよいのか分からない
//----------------------------------------------------------
void HjRoadMap::ScanRoad(const HjRoad& road)
{
	m_scan.clear();
	m_broken = 0;
	m_tight  = 0;

	const HjRoadSpline& sp = road.Spline();
	if (!sp.IsValid()) { return; }

	const float total = sp.TotalLength();

	for (float s = 0.0f; s <= total; s += RM::ScanStep)
	{
		// 平均した曲率ではなく、その場の値を使う。
		// 平均だと、ヘアピンの入口で直線と混ざって緩く出る
		const float k = sp.LocalCurvatureAt(s);
		const Math::Vector3 p = sp.PositionAt(s);

		ScanPoint sc;
		sc.x = p.x;
		sc.z = p.z;
		sc.radius = (k > 1e-5f) ? (1.0f / k) : 1e6f;
		m_scan.push_back(sc);

		if      (sc.radius < RM::RadiusBroken) { ++m_broken; }
		else if (sc.radius < RM::RadiusTight)  { ++m_tight; }
	}
}

//----------------------------------------------------------
// 拡大と移動
//
// 地形1499mを520pxに収めると1ピクセルが約2.9m。
// クリックの震えがそのまま20〜40mの折れになるので、
// 拡大できないと滑らかな線は引けない
//----------------------------------------------------------
void HjRoadMap::UpdateView(const ImVec2& origin, float size, bool hovered)
{
	if (!hovered) { return; }

	ImGuiIO& io = ImGui::GetIO();

	//===== ホイールで拡大 =====
	if (io.MouseWheel != 0.0f)
	{
		// マウスの下の地点を動かさずに拡大する。
		// 中心を軸にすると、狙った所がすぐ画面外へ逃げる
		const float mx = (io.MousePos.x - origin.x) / size;
		const float my = (io.MousePos.y - origin.y) / size;

		const float beforeU = FromViewU(mx);
		const float beforeV = FromViewV(my);

		const float scale = (io.MouseWheel > 0.0f) ? RM::ZoomStep : (1.0f / RM::ZoomStep);
		m_zoom = std::clamp(m_zoom * scale, RM::ZoomMin, RM::ZoomMax);

		m_viewU += beforeU - FromViewU(mx);
		m_viewV += beforeV - FromViewV(my);
	}

	//===== 中ボタンで掴んで動かす =====
	// 左は点を動かすのに使っているので、別のボタンに分ける
	if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
	{
		const ImVec2 d = ImGui::GetIO().MouseDelta;
		m_viewU -= d.x / (size * m_zoom);
		m_viewV -= d.y / (size * m_zoom);
	}

	// 見る範囲が地図の外へ出ないようにする。
	// 出ると、絵の端が引き伸ばされた所に線を引くことになる
	const float half = 0.5f / m_zoom;
	m_viewU = std::clamp(m_viewU, half, 1.0f - half);
	m_viewV = std::clamp(m_viewV, half, 1.0f - half);
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
	if (m_scanDirty) { ScanRoad(road); m_scanDirty = false; }

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
		m_scanDirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Button(U8("書き出す"))) { road.SavePath(); }
	ImGui::SameLine();
	if (ImGui::Button(U8("全体を出す")))
	{
		m_zoom = RM::ZoomMin;
		m_viewU = 0.5f;
		m_viewV = 0.5f;
	}

	//===== 急すぎるコーナーの数 =====
	// 直したかどうかは、見た目より数のほうが確実に分かる
	if (m_broken > 0)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f),
			U8("裏返る箇所 %d  ← 半径が %.1fm 未満。道が作れない"),
			m_broken, RM::RadiusBroken);
	}
	if (m_tight > 0)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
			U8("きつすぎ %d  ← 内側を絞っても足りない"), m_tight);
	}
	if (m_broken == 0 && m_tight == 0)
	{
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), U8("急すぎるコーナーは無い"));
	}

	//===== 地図 =====
	const float S = RM::ViewSize;
	const ImVec2 origin = ImGui::GetCursorScreenPos();

	// 見ている範囲だけを切り出して貼る
	const float half = 0.5f / m_zoom;
	const ImVec2 uv0(m_viewU - half, m_viewV - half);
	const ImVec2 uv1(m_viewU + half, m_viewV + half);

	if (m_spTex && m_spTex->WorkSRView())
	{
		ImGui::Image((ImTextureID)(intptr_t)m_spTex->WorkSRView(), ImVec2(S, S), uv0, uv1);
	}
	else
	{
		// 絵が作れなくても線は引けるようにする
		ImGui::Dummy(ImVec2(S, S));
	}

	const bool hovered = ImGui::IsItemHovered();
	auto* dl = ImGui::GetWindowDrawList();

	UpdateView(origin, S, hovered);

	// はみ出した線がパネルの外へ描かれないように切る
	dl->PushClipRect(origin, ImVec2(origin.x + S, origin.y + S), true);

	// 地図の中でのマウス位置(0〜1)。
	// ここが肝で、3Dのような変換を通さないのでずれようがない
	const ImVec2 mp = ImGui::GetIO().MousePos;
	const float mu = FromViewU((mp.x - origin.x) / S);
	const float mv = FromViewV((mp.y - origin.y) / S);

	const int n = road.PointCount();

	// 地図の中の位置 → 画面の位置
	auto toScreen = [&](const Math::Vector3& w)
	{
		float u, v;
		WorldToMap(w, field, u, v);
		return ImVec2(origin.x + ToViewU(u) * S, origin.y + ToViewV(v) * S);
	};

	//===== 道を描く =====
	// 実際の幅を縮尺どおりに描くと細すぎて見えないので、
	// 太い帯を薄く重ねて幅の目安にする
	for (int i = 0; i + 1 < n; ++i)
	{
		const ImVec2 a = toScreen(road.GetPoint(i));
		const ImVec2 b = toScreen(road.GetPoint(i + 1));

		dl->AddLine(a, b, RM::ColBand, RM::RoadBandWidth);
		dl->AddLine(a, b, RM::ColLine, RM::LineWidth);
	}

	//===== 急すぎるコーナーを重ねる =====
	// どこを直せばよいかが分からないと、直しようがない
	for (size_t i = 0; i + 1 < m_scan.size(); ++i)
	{
		const float r = std::min(m_scan[i].radius, m_scan[i + 1].radius);
		if (r >= RM::RadiusWarn) { continue; }

		const unsigned int col =
			(r < RM::RadiusBroken) ? RM::ColBroken :
			(r < RM::RadiusTight)  ? RM::ColTight  : RM::ColWarn;

		const ImVec2 a = toScreen(Math::Vector3(m_scan[i].x,     0.0f, m_scan[i].z));
		const ImVec2 b = toScreen(Math::Vector3(m_scan[i + 1].x, 0.0f, m_scan[i + 1].z));

		dl->AddLine(a, b, col, RM::WarnWidth);
	}

	//===== 点を描く =====
	for (int i = 0; i < n; ++i)
	{
		const ImVec2 c = toScreen(road.GetPoint(i));
		const bool sel = (i == m_selected);

		dl->AddCircleFilled(c, sel ? RM::PointRadiusHot : RM::PointRadius,
		                    sel ? RM::ColPointHot : RM::ColPoint);
	}

	//===== 車 =====
	// どこを走っているかが分かる
	dl->AddCircle(toScreen(m_carPos), RM::PointRadiusHot, RM::ColCar, 0, 2.0f);

	dl->PopClipRect();

	// 縮尺。拡大すると、いま何メートルを見ているのか分からなくなる
	ImGui::TextDisabled(U8("%.0f倍   1ピクセル = %.2fm   ホイールで拡大 / 中ボタンで移動"),
		m_zoom, field.GetWorldW() / (S * m_zoom));

	//===== 入力 =====
	if (!hovered) { return; }

	const bool lmb    = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	const bool lmbHit = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

	// 一番近い点を探す。画面のピクセルで測る。
	//
	// ※near は Windows のマクロなので使えない。
	//   使うと、意味不明な構文エラーになって原因が追いにくい
	int   hitIndex = -1;
	float hitDist  = RM::GrabRadius;

	for (int i = 0; i < n; ++i)
	{
		const ImVec2 c = toScreen(road.GetPoint(i));

		const float dx = c.x - mp.x;
		const float dy = c.y - mp.y;
		const float d = sqrtf(dx * dx + dy * dy);

		if (d < hitDist) { hitDist = d; hitIndex = i; }
	}

	if (lmbHit)
	{
		if (m_appending)
		{
			// 末尾へ足していく。押すたびに道が伸びる。
			//
			// 高さは、その場の地形の高さを規定値にする。
			// 道の高さは制御点が持つので、置いた時点で決まる
			Math::Vector3 p = MapToWorld(mu, mv, field);

			const float g = field.HeightAt(p.x, p.z);
			if (g > TerrainConst::OutsideHeight) { p.y = g; }

			road.AppendPoint(p);
			m_selected = road.PointCount() - 1;
			m_scanDirty = true;
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
		m_scanDirty = true;
	}

	if (!lmb) { m_dragging = -1; }
}
