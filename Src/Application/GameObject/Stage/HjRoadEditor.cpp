#include "HjRoadEditor.h"

#include "HjRoad.h"
#include "HjHeightField.h"

namespace RE = RoadEditConst;

namespace
{
	// 軸番号 → 向き
	Math::Vector3 AxisDirOf(int a)
	{
		if (a == 0) { return Math::Vector3(1.0f, 0.0f, 0.0f); }
		if (a == 1) { return Math::Vector3(0.0f, 1.0f, 0.0f); }
		return Math::Vector3(0.0f, 0.0f, 1.0f);
	}

	// 視線(ro,rd)に対し  、点Aを通り方向u(単位)の直線上で
	// 最も近い点のパラメータ s を返す。
	//
	// ※符号を間違えると軸の反対側を掴むことになり、
	//   線の上を狙っているのに掴めない、という形で出る。
	//   前の作品で実際に動いていた式をそのまま使う
	float AxisClosestParam(const Math::Vector3& ro, const Math::Vector3& rd,
	                       const Math::Vector3& A, const Math::Vector3& u)
	{
		const Math::Vector3 w0 = A - ro;

		// u も rd も単位なので、他の係数は1
		const float b = u.Dot(rd);
		const float d = u.Dot(w0);
		const float e = rd.Dot(w0);

		float denom = 1.0f - b * b;

		// 軸と視線がほぼ平行。どこを指しているか決まらない
		if (fabsf(denom) < 1e-6f) { denom = (denom < 0.0f) ? -1e-6f : 1e-6f; }

		return (b * e - d) / denom;
	}
}

//----------------------------------------------------------
// ゲーム画像内の位置からワールドの視線を作る
//----------------------------------------------------------
bool HjRoadEditor::ScreenRay(float u, float v,
                             Math::Vector3& outOrigin, Math::Vector3& outDir)
{
	if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) { return false; }

	const Math::Matrix view = KdShaderManager::Instance().GetCameraCB().mView;
	const Math::Matrix proj = KdShaderManager::Instance().GetCameraCB().mProj;
	const Math::Matrix invVP = (view * proj).Invert();

	// 画面のYは下向き、投影後のYは上向き
	const float ndcX = u * 2.0f - 1.0f;
	const float ndcY = 1.0f - v * 2.0f;

	const Math::Vector3 pNear =
		Math::Vector3::Transform(Math::Vector3(ndcX, ndcY, 0.0f), invVP);
	const Math::Vector3 pFar =
		Math::Vector3::Transform(Math::Vector3(ndcX, ndcY, 1.0f), invVP);

	outOrigin = pNear;
	outDir    = pFar - pNear;
	if (outDir.LengthSquared() < 1e-8f) { return false; }

	outDir.Normalize();
	return true;
}

//----------------------------------------------------------
// 軸ハンドルのピック
//----------------------------------------------------------
HjRoadEditor::Axis HjRoadEditor::PickAxis(const Math::Vector3& ro,
                                          const Math::Vector3& rd,
                                          const Math::Vector3& selPos)
{
	const float L = RE::GizmoLength;
	const float R = RE::GizmoPickRadius;

	int   bestAxis = -1;
	float bestDist = R;

	for (int a = 0; a < 3; ++a)
	{
		const Math::Vector3 u = AxisDirOf(a);

		// ハンドルは [selPos, selPos + u*L] の線分
		float s = AxisClosestParam(ro, rd, selPos, u);
		s = std::clamp(s, 0.0f, L);

		const Math::Vector3 pAxis = selPos + u * s;

		float tRay = (pAxis - ro).Dot(rd);
		if (tRay < 0.0f) { tRay = 0.0f; }

		const Math::Vector3 pRay = ro + rd * tRay;
		const float dist = (pAxis - pRay).Length();

		if (dist < bestDist) { bestDist = dist; bestAxis = a; }
	}

	if (bestAxis == 0) { return Axis::X; }
	if (bestAxis == 1) { return Axis::Y; }
	if (bestAxis == 2) { return Axis::Z; }
	return Axis::None;
}

//----------------------------------------------------------
// 視線を地形へ当てる
//
// 高さマップは面の集まりではないので、三角形との交差では求まらない。
// 少しずつ進めて、地面より下へ潜った所を探す
//----------------------------------------------------------
bool HjRoadEditor::RayToTerrain(const Math::Vector3& origin, const Math::Vector3& dir,
                                const HjHeightField& field, Math::Vector3& outHit)
{
	if (!field.IsValid()) { return false; }

	constexpr float Step   = 2.0f;
	constexpr float MaxLen = 3000.0f;
	constexpr int   Refine = 12;

	float prevT = 0.0f;
	bool  prevAbove = true;

	for (float t = 0.0f; t < MaxLen; t += Step)
	{
		const Math::Vector3 p = origin + dir * t;
		const float h = field.HeightAt(p.x, p.z);

		// 地形の外。まだ届いていないか、通り過ぎた
		if (h <= TerrainConst::OutsideHeight)
		{
			prevT = t;
			prevAbove = true;
			continue;
		}

		const bool above = (p.y > h);

		// 上から下へ跨いだ。ここに地面がある
		if (!above && prevAbove && t > 0.0f)
		{
			// 跨いだ区間を半分ずつ詰める。
			// 刻みのままだと、最大で刻みぶんずれる
			float lo = prevT;
			float hi = t;
			for (int i = 0; i < Refine; ++i)
			{
				const float mid = (lo + hi) * 0.5f;
				const Math::Vector3 q = origin + dir * mid;
				if (q.y > field.HeightAt(q.x, q.z)) { lo = mid; } else { hi = mid; }
			}
			outHit = origin + dir * hi;
			return true;
		}

		prevT = t;
		prevAbove = above;
	}
	return false;
}

//----------------------------------------------------------
// 掴む・動かす
//----------------------------------------------------------
void HjRoadEditor::Update(HjRoad& road, const HjHeightField& field)
{
	(void)field;

	float u = 0.0f, v = 0.0f;
	KdDebugGUI::Instance().GetGameUV(u, v);

	const bool hovered = KdDebugGUI::Instance().IsGameHovered();
	const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

	Math::Vector3 ro, rd;
	const bool rayOk = hovered && ScreenRay(u, v, ro, rd);

	const int n = road.PointCount();
	const bool selValid = (m_selected >= 0 && m_selected < n);

	// 押した瞬間。
	//
	// 軸を先に見る。点より後にすると、軸が点に重なっている所で
	// 掴めずに選び直しになる
	if (lmb && !m_lmbPrev)
	{
		m_dragAxis = Axis::None;

		if (rayOk)
		{
			Axis ax = Axis::None;
			if (selValid)
			{
				ax = PickAxis(ro, rd, road.GetPointDisplayPos(m_selected));
			}

			if (ax != Axis::None)
			{
				m_dragAxis  = ax;
				m_dragStart = road.GetPoint(m_selected);

				const int ai = (ax == Axis::X) ? 0 : (ax == Axis::Y) ? 1 : 2;
				m_dragStartS = AxisClosestParam(ro, rd,
					road.GetPointDisplayPos(m_selected), AxisDirOf(ai));
			}
			else
			{
				// 点をピック(選ぶだけ)。球との交差で見る。
				// 当たらなければ選択解除
				int   best = -1;
				float bestT = FLT_MAX;
				const float R = RE::PickRadius;

				for (int i = 0; i < n; ++i)
				{
					// 画面に出ている位置で判定する。
					// 制御点の高さは持ち上げなので、そのままだとずれる
					const Math::Vector3 oc = ro - road.GetPointDisplayPos(i);

					const float b = oc.Dot(rd);
					const float c = oc.LengthSquared() - R * R;
					const float disc = b * b - c;
					if (disc < 0.0f) { continue; }

					const float sq = sqrtf(disc);
					float t = -b - sq;
					if (t < 0.0f) { t = -b + sq; }
					if (t < 0.0f) { continue; }

					if (t < bestT) { bestT = t; best = i; }
				}
				m_selected = best;
			}
		}
	}

	// 軸を掴んでいる間。選んだ方向にだけ動かす
	if (m_dragAxis != Axis::None && lmb && rayOk && selValid)
	{
		const int ai = (m_dragAxis == Axis::X) ? 0 : (m_dragAxis == Axis::Y) ? 1 : 2;
		const Math::Vector3 udir = AxisDirOf(ai);

		const float s = AxisClosestParam(ro, rd,
			road.GetPointDisplayPos(m_selected), udir);

		Math::Vector3 np = m_dragStart + udir * (s - m_dragStartS);

		// 目盛りに吸わせる。動かした軸だけ
		if (m_snapEnabled && m_snapSize > 1e-4f)
		{
			auto snap = [g = m_snapSize](float val) { return roundf(val / g) * g; };
			if (m_dragAxis == Axis::X)      { np.x = snap(np.x); }
			else if (m_dragAxis == Axis::Y) { np.y = snap(np.y); }
			else                            { np.z = snap(np.z); }
		}

		road.MovePoint(m_selected, np);
	}

	// 離した
	if (!lmb) { m_dragAxis = Axis::None; }

	m_lmbPrev = lmb;

	// 選んでいる点が消えていることがある
	if (m_selected >= road.PointCount()) { m_selected = -1; }
}

//----------------------------------------------------------
// 一覧と数値
//
// 遠くの点や、重なっている点は掴みにくい。
// 数値で直接動かせる道も要る
//----------------------------------------------------------
void HjRoadEditor::DrawGui(HjRoad& road)
{
	const int n = road.PointCount();

	ImGui::Checkbox(U8("目盛りに吸わせる"), &m_snapEnabled);
	if (m_snapEnabled)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80.0f);
		ImGui::DragFloat("##snap", &m_snapSize, 0.1f, 0.1f, 50.0f, "%.1f m");
	}

	//===== 増やす・減らす =====
	if (ImGui::Button(U8("末尾に足す")))
	{
		road.InsertAfter(n - 1);
		m_selected = road.PointCount() - 1;
	}
	ImGui::SameLine();

	// 選んだ点の次へ足す。
	// 前後の中間に置くので、押しただけでは形が崩れない
	if (ImGui::Button(U8("次へ足す")) && m_selected >= 0)
	{
		road.InsertAfter(m_selected);
		m_selected = std::min(m_selected + 1, road.PointCount() - 1);
	}
	ImGui::SameLine();

	if (ImGui::Button(U8("消す")) && m_selected >= 0)
	{
		road.ErasePoint(m_selected);
		m_selected = -1;
	}

	//===== 一覧 =====
	ImGui::Text(U8("制御点 %d 個"), n);

	if (ImGui::BeginListBox("##roadpts",
	                        ImVec2(-FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * 6.0f)))
	{
		for (int i = 0; i < n; ++i)
		{
			const Math::Vector3 p = road.GetPoint(i);

			char label[96];
			snprintf(label, sizeof(label), "[%2d] x %7.1f  z %7.1f  h %+5.2f##%d",
			         i, p.x, p.z, p.y, i);

			if (ImGui::Selectable(label, m_selected == i)) { m_selected = i; }
		}
		ImGui::EndListBox();
	}

	//===== 選んだ点 =====
	if (m_selected < 0 || m_selected >= n)
	{
		ImGui::TextDisabled(U8("点をクリックするか、一覧から選ぶ"));
		return;
	}

	const Math::Vector3 p = road.GetPoint(m_selected);

	float xz[2] = { p.x, p.z };
	if (ImGui::DragFloat2(U8("位置 (東西 / 南北)"), xz, 0.5f))
	{
		road.MovePoint(m_selected, Math::Vector3(xz[0], p.y, xz[1]));
	}

	// 高さは「持ち上げ」。
	//
	// 道の高さは地形から拾って勾配の上限に収めるので、
	// 絶対の高さを入れても上書きされる。
	// 自動で決まった形に対する上下として足す
	float lift = p.y;
	if (ImGui::DragFloat(U8("持ち上げ"), &lift, 0.1f, -60.0f, 60.0f))
	{
		road.MovePoint(m_selected, Math::Vector3(p.x, lift, p.z));
	}
	ImGui::SetItemTooltip(U8("自動で決まった高さからの上下。0で自動のまま"));

	// 前後の点へ移る。
	// 一覧を目で追わずに、道に沿って見ていける
	if (ImGui::Button(U8("← 前")) && m_selected > 0) { --m_selected; }
	ImGui::SameLine();
	if (ImGui::Button(U8("次 →")) && m_selected + 1 < n) { ++m_selected; }
}

//----------------------------------------------------------
// 制御点の印と軸
//----------------------------------------------------------
void HjRoadEditor::PushMarker(const HjRoad& road, KdDebugWireFrame& dbg) const
{
	const int n = road.PointCount();

	for (int i = 0; i < n; ++i)
	{
		const Math::Vector3 p = road.GetPointDisplayPos(i);
		const bool sel = (i == m_selected);

		// 選んでいる点は色を変えて大きくする。
		// 同じ見た目だと、どれを動かしているのか分からない
		const Math::Color col = sel
			? Math::Color(1.0f, 0.55f, 0.15f, 1.0f)
			: Math::Color(0.35f, 0.65f, 1.0f, 1.0f);

		dbg.AddDebugSphere(p, sel ? RE::MarkRadiusHot : RE::MarkRadius, col);

		// 繋がりの線。どの順で並んでいるかが分かる
		if (RE::ShowLinks && i + 1 < n)
		{
			dbg.AddDebugLine(p, road.GetPointDisplayPos(i + 1),
			                 Math::Color(0.3f, 0.8f, 0.9f, 1.0f));
		}
	}

	//===== 軸ハンドル =====
	if (m_selected < 0 || m_selected >= n) { return; }

	const Math::Vector3 c = road.GetPointDisplayPos(m_selected);
	const float L = RE::GizmoLength;

	for (int a = 0; a < 3; ++a)
	{
		const Math::Vector3 u = AxisDirOf(a);

		Math::Color col;
		if (a == 0)      { col = Math::Color(1.0f, 0.25f, 0.25f, 1.0f); }
		else if (a == 1) { col = Math::Color(0.25f, 1.0f, 0.35f, 1.0f); }
		else             { col = Math::Color(0.35f, 0.45f, 1.0f, 1.0f); }

		// 掴んでいる軸は明るく。どれを動かしているか分かる
		const Axis mine = (a == 0) ? Axis::X : (a == 1) ? Axis::Y : Axis::Z;
		if (m_dragAxis == mine) { col = Math::Color(1.0f, 0.95f, 0.4f, 1.0f); }

		const Math::Vector3 tip = c + u * L;
		dbg.AddDebugLine(c, tip, col);
		dbg.AddDebugSphere(tip, RE::GizmoTipHalf, col);
	}
}
