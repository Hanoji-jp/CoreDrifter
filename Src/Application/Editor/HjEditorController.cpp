#include "HjEditorController.h"

//==========================================================
// 軸ヘルパー（ファイルローカル）
//==========================================================
namespace
{
	// 軸番号(0,1,2) → 単位方向ベクトル
	Math::Vector3 AxisDirOf(int _a)
	{
		if (_a == 0) { return Math::Vector3(1.0f, 0.0f, 0.0f); }
		if (_a == 1) { return Math::Vector3(0.0f, 1.0f, 0.0f); }
		return Math::Vector3(0.0f, 0.0f, 1.0f);
	}

	// 軸 _a に垂直な2つの単位ベクトル（回転リング描画用）
	void RingBasisOf(int _a, Math::Vector3& _v1, Math::Vector3& _v2)
	{
		if (_a == 0) { _v1 = Math::Vector3(0, 1, 0); _v2 = Math::Vector3(0, 0, 1); }      // X軸まわり → YZ平面
		else if (_a == 1) { _v1 = Math::Vector3(0, 0, 1); _v2 = Math::Vector3(1, 0, 0); } // Y軸まわり → ZX平面
		else { _v1 = Math::Vector3(1, 0, 0); _v2 = Math::Vector3(0, 1, 0); }              // Z軸まわり → XY平面
	}

	// レイ(_ro,_rd)に対して、点_Aを通り方向_uの直線上で最も近い点のパラメータsを返す
	float AxisClosestParam(const Math::Vector3& _ro, const Math::Vector3& _rd,
						   const Math::Vector3& _A, const Math::Vector3& _u)
	{
		const Math::Vector3 w0 = _A - _ro;
		const float b = _u.Dot(_rd);   // a=1(_u単位), c=1(_rd単位)
		const float d = _u.Dot(w0);
		const float e = _rd.Dot(w0);
		float denom = 1.0f - b * b;
		if (fabsf(denom) < 1e-6f) { denom = (denom < 0.0f) ? -1e-6f : 1e-6f; }
		return (b * e - d) / denom;
	}

	// 軸番号からVector3成分を取得／設定
	float GetComp(const Math::Vector3& _v, int _a) { return (_a == 0) ? _v.x : (_a == 1) ? _v.y : _v.z; }
	void  SetComp(Math::Vector3& _v, int _a, float _val) { if (_a == 0) { _v.x = _val; } else if (_a == 1) { _v.y = _val; } else { _v.z = _val; } }
}

void HjEditorController::CycleMode()
{
	if (m_mode == HjGizmoMode::Translate) { m_mode = HjGizmoMode::Scale; }
	else if (m_mode == HjGizmoMode::Scale) { m_mode = HjGizmoMode::Rotate; }
	else { m_mode = HjGizmoMode::Translate; }
}

// ゲーム画像内の正規化座標(_u,_v)からワールド空間のピッキングレイを作る
bool HjEditorController::ScreenRayFromGameUV(float _u, float _v,
	Math::Vector3& _outOrigin, Math::Vector3& _outDir) const
{
	if (_u < 0.0f || _u > 1.0f || _v < 0.0f || _v > 1.0f) { return false; }

	const Math::Matrix view = KdShaderManager::Instance().GetCameraCB().mView;
	const Math::Matrix proj = KdShaderManager::Instance().GetCameraCB().mProj;
	const Math::Matrix invVP = (view * proj).Invert();

	const float ndcX = _u * 2.0f - 1.0f;
	const float ndcY = 1.0f - _v * 2.0f;   // スクリーンYは下向き、NDCは上向き
	const Math::Vector3 pNear = Math::Vector3::Transform(Math::Vector3(ndcX, ndcY, 0.0f), invVP);
	const Math::Vector3 pFar  = Math::Vector3::Transform(Math::Vector3(ndcX, ndcY, 1.0f), invVP);

	_outOrigin = pNear;
	_outDir    = pFar - pNear;
	if (_outDir.LengthSquared() < 1e-8f) { return false; }
	_outDir.Normalize();
	return true;
}

// 軸ハンドルのピック（0:X 1:Y 2:Z / 当たらなければ -1）
int HjEditorController::PickGizmoAxis(const Math::Vector3& _ro, const Math::Vector3& _rd,
	const Math::Vector3& _selPos) const
{
	const float L = EditorPickConst::GizmoLength;
	const float R = EditorPickConst::GizmoPickRadius;
	int   bestAxis = -1;
	float bestDist = R;
	for (int a = 0; a < 3; ++a)
	{
		const Math::Vector3 u = AxisDirOf(a);
		float s = AxisClosestParam(_ro, _rd, _selPos, u);
		s = std::clamp(s, 0.0f, L);                 // ハンドルは [selPos, selPos+u*L]
		const Math::Vector3 pAxis = _selPos + u * s;
		float tRay = (pAxis - _ro).Dot(_rd);
		if (tRay < 0.0f) { tRay = 0.0f; }
		const Math::Vector3 pRay = _ro + _rd * tRay;
		const float dist = (pAxis - pRay).Length();
		if (dist < bestDist) { bestDist = dist; bestAxis = a; }
	}
	return bestAxis;
}

void HjEditorController::Update()
{
	// ── 中ボタンの「クリック」でギズモモードを切替（ドラッグはカメラのパンなので除外）──
	{
		const bool mmb = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
		if (mmb && !m_mmbPrev) { GetCursorPos(&m_mmbDownPos); m_mmbMoved = false; }
		if (mmb)
		{
			POINT cur{}; GetCursorPos(&cur);
			if (std::abs(cur.x - m_mmbDownPos.x) + std::abs(cur.y - m_mmbDownPos.y) > EditorPickConst::MiddleClickMovePx)
			{
				m_mmbMoved = true;
			}
		}
		if (!mmb && m_mmbPrev && !m_mmbMoved) { CycleMode(); }   // 動かさず離した＝クリック
		m_mmbPrev = mmb;
	}

	// ── アンドゥ／リドゥ（Ctrl+Z / Ctrl+Y）──
	{
		const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
		const bool zKey = (GetAsyncKeyState('Z') & 0x8000) != 0;
		const bool yKey = (GetAsyncKeyState('Y') & 0x8000) != 0;
		const bool zNow = ctrl && zKey;
		const bool yNow = ctrl && yKey;
		if (zNow && !m_ctrlZPrev) { Undo(); }
		if (yNow && !m_ctrlYPrev) { Redo(); }
		m_ctrlZPrev = zNow;
		m_ctrlYPrev = yNow;
	}

	float u = 0.0f, v = 0.0f;
	KdDebugGUI::Instance().GetGameUV(u, v);
	const bool hovered = KdDebugGUI::Instance().IsGameHovered();
	const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

	Math::Vector3 ro, rd;
	const bool rayOk = hovered && ScreenRayFromGameUV(u, v, ro, rd);

	const bool selValid = HasSelection();

	// クリック開始：まず選択中オブジェクトの軸ハンドルを判定 → 軸ドラッグ開始。
	// 軸でなければオブジェクトを選択（ドラッグはしない）。何も無ければ選択解除。
	if (lmb && !m_lmbPrev)
	{
		m_dragAxis   = -1;
		m_moveActive = false;

		if (rayOk)
		{
			int ax = -1;
			if (selValid)
			{
				ax = PickGizmoAxis(ro, rd, m_entries[m_selEntry].pos);
			}

			if (ax != -1)
			{
				const HjEditEntry& e = m_entries[m_selEntry];
				m_dragAxis       = ax;
				m_dragStartPos   = e.pos;
				m_dragStartScale = e.scale;
				m_dragStartRot   = e.rot;
				const Math::Vector3 udir = AxisDirOf(ax);
				m_dragStartS     = AxisClosestParam(ro, rd, m_dragStartPos, udir);
				m_moveActive     = true;
			}
			else
			{
				// オブジェクトをピック（選択のみ）
				int best = -1; float bestT = FLT_MAX;
				const float R = EditorPickConst::PickRadius;
				for (int i = 0; i < static_cast<int>(m_entries.size()); ++i)
				{
					const Math::Vector3 oc = ro - m_entries[i].pos;
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
				m_selEntry = best;   // 何も当たらなければ -1（選択解除）
			}
		}
	}

	// 軸ドラッグ中：モードに応じて 移動／拡縮／回転
	if (m_dragAxis != -1 && lmb && rayOk && selValid)
	{
		const Math::Vector3 udir = AxisDirOf(m_dragAxis);
		const float s = AxisClosestParam(ro, rd, m_dragStartPos, udir);
		const float delta = s - m_dragStartS;
		const HjEditEntry& e = m_entries[m_selEntry];

		if (m_mode == HjGizmoMode::Translate)
		{
			Math::Vector3 np = m_dragStartPos + udir * delta;
			// グリッドスナップ：動かした軸の座標だけグリッドに吸着
			if (m_snapEnabled && m_snapSize > 1e-4f)
			{
				auto snap = [g = m_snapSize](float val) { return std::roundf(val / g) * g; };
				if      (m_dragAxis == 0) { np.x = snap(np.x); }
				else if (m_dragAxis == 1) { np.y = snap(np.y); }
				else                      { np.z = snap(np.z); }
			}
			if (e.setPos) { e.setPos(np); }
		}
		else if (m_mode == HjGizmoMode::Scale)
		{
			Math::Vector3 ns = m_dragStartScale;
			float comp = GetComp(m_dragStartScale, m_dragAxis) + delta * EditorPickConst::GizmoScaleSpeed;
			comp = std::max(comp, EditorPickConst::GizmoScaleMin);
			SetComp(ns, m_dragAxis, comp);
			if (e.setScale) { e.setScale(ns); }
		}
		else // Rotate
		{
			Math::Vector3 nr = m_dragStartRot;
			float comp = GetComp(m_dragStartRot, m_dragAxis) + delta * EditorPickConst::GizmoRotSpeed;
			SetComp(nr, m_dragAxis, comp);
			if (e.setRot) { e.setRot(nr); }
		}
	}

	// ボタンを離したら：変化が発生していればアンドゥに記録
	if (!lmb)
	{
		if (m_moveActive && selValid)
		{
			const HjEditEntry& e = m_entries[m_selEntry];
			if (m_mode == HjGizmoMode::Translate && e.setPos)
			{
				const Math::Vector3 oldV = m_dragStartPos, newV = e.pos;
				if ((newV - oldV).LengthSquared() > EditorPickConst::MoveEpsilon)
				{
					const std::function<void(const Math::Vector3&)> set = e.setPos;
					PushCommand([set, oldV]() { if (set) { set(oldV); } },
								[set, newV]() { if (set) { set(newV); } });
				}
			}
			else if (m_mode == HjGizmoMode::Scale && e.setScale)
			{
				const Math::Vector3 oldV = m_dragStartScale, newV = e.scale;
				if ((newV - oldV).LengthSquared() > EditorPickConst::MoveEpsilon)
				{
					const std::function<void(const Math::Vector3&)> set = e.setScale;
					PushCommand([set, oldV]() { if (set) { set(oldV); } },
								[set, newV]() { if (set) { set(newV); } });
				}
			}
			else if (m_mode == HjGizmoMode::Rotate && e.setRot)
			{
				const Math::Vector3 oldV = m_dragStartRot, newV = e.rot;
				if ((newV - oldV).LengthSquared() > EditorPickConst::MoveEpsilon)
				{
					const std::function<void(const Math::Vector3&)> set = e.setRot;
					PushCommand([set, oldV]() { if (set) { set(oldV); } },
								[set, newV]() { if (set) { set(newV); } });
				}
			}
		}
		m_moveActive = false;
		m_dragAxis   = -1;
	}
	m_lmbPrev = lmb;
}

void HjEditorController::Undo()
{
	if (m_undoStack.empty()) { return; }
	EditCommand c = std::move(m_undoStack.back());
	m_undoStack.pop_back();
	if (c.undo) { c.undo(); }
	m_redoStack.push_back(std::move(c));
}

void HjEditorController::Redo()
{
	if (m_redoStack.empty()) { return; }
	EditCommand c = std::move(m_redoStack.back());
	m_redoStack.pop_back();
	if (c.redo) { c.redo(); }
	m_undoStack.push_back(std::move(c));
}

void HjEditorController::PushCommand(const std::function<void()>& _undo, const std::function<void()>& _redo)
{
	EditCommand c;
	c.undo = _undo;
	c.redo = _redo;
	m_undoStack.push_back(std::move(c));
	m_redoStack.clear();
}

// 選択中オブジェクトに選択枠＋モード別ギズモを描画（常に手前）
void HjEditorController::DrawMarker()
{
	if (!HasSelection()) { return; }

	const Math::Vector3 c = m_entries[m_selEntry].pos;
	std::vector<KdPolygon::Vertex> verts;

	auto addLine = [&](const Math::Vector3& a, const Math::Vector3& b, unsigned int col)
	{
		KdPolygon::Vertex v0{}, v1{};
		v0.pos = a; v0.color = col;
		v1.pos = b; v1.color = col;
		verts.push_back(v0); verts.push_back(v1);
	};
	auto addBox = [&](const Math::Vector3& ctr, float h, unsigned int col)
	{
		const Math::Vector3 cn[8] = {
			{ ctr.x - h, ctr.y - h, ctr.z - h }, { ctr.x + h, ctr.y - h, ctr.z - h },
			{ ctr.x + h, ctr.y + h, ctr.z - h }, { ctr.x - h, ctr.y + h, ctr.z - h },
			{ ctr.x - h, ctr.y - h, ctr.z + h }, { ctr.x + h, ctr.y - h, ctr.z + h },
			{ ctr.x + h, ctr.y + h, ctr.z + h }, { ctr.x - h, ctr.y + h, ctr.z + h },
		};
		const int eg[12][2] = { {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7} };
		for (int e = 0; e < 12; ++e) { addLine(cn[eg[e][0]], cn[eg[e][1]], col); }
	};

	// 選択ボックス（黄）
	addBox(c, EditorPickConst::MarkerHalf, 0xFF00FFFF);

	const float L = EditorPickConst::GizmoLength;
	const float tip = EditorPickConst::GizmoTipHalf;
	const unsigned int axisCol[3] = { 0xFF0000FF, 0xFF00FF00, 0xFFFF0000 }; // X=R, Y=G, Z=B

	if (m_mode == HjGizmoMode::Rotate)
	{
		// 各軸まわりの回転リング
		const int segs = EditorPickConst::GizmoRingSegments;
		for (int a = 0; a < 3; ++a)
		{
			Math::Vector3 v1, v2; RingBasisOf(a, v1, v2);
			const bool active = (a == m_dragAxis);
			const unsigned int col = active ? 0xFFFFFFFF : axisCol[a];
			Math::Vector3 prev = c + v1 * L;
			for (int k = 1; k <= segs; ++k)
			{
				const float th = DirectX::XM_2PI * (static_cast<float>(k) / segs);
				const Math::Vector3 cur = c + (v1 * cosf(th) + v2 * sinf(th)) * L;
				addLine(prev, cur, col);
				prev = cur;
			}
		}
	}
	else
	{
		// 移動／拡縮：軸ハンドル（線＋先端ハンドル）
		for (int a = 0; a < 3; ++a)
		{
			const Math::Vector3 uu = AxisDirOf(a);
			const Math::Vector3 end = c + uu * L;
			const bool active = (a == m_dragAxis);
			const unsigned int col = active ? 0xFFFFFFFF : axisCol[a];
			addLine(c, end, col);
			// Scaleモードは先端ハンドルを少し大きく（拡縮らしさ）
			const float h = (m_mode == HjGizmoMode::Scale) ? (tip * 1.8f) : tip;
			addBox(end, h, col);
		}
	}

	KdShaderManager::Instance().m_StandardShader.DrawVertices(
		verts, Math::Matrix::Identity, Math::Color(1, 1, 1, 1),
		KdDepthStencilState::ZDisable,   // 常に手前に表示
		D3D_PRIMITIVE_TOPOLOGY_LINELIST);
}

void HjEditorController::DrawImGui()
{
	// 独立したウィンドウとして表示（Begin/Endで囲まないと既定のDebugウィンドウに
	// 紛れて前面に出せず操作できなくなる）
	ImGui::Begin(U8("オブジェクト編集"));

	ImGui::TextColored({ 1.0f, 0.9f, 0.4f, 1.0f }, "Object Editing");

	// 現在のモード表示＋切替（中ボタンのクリックでも切替）
	const char* modeName =
		(m_mode == HjGizmoMode::Translate) ? U8("移動 (Translate)") :
		(m_mode == HjGizmoMode::Scale)     ? U8("拡大 (Scale)")     :
										   U8("回転 (Rotate)");
	ImGui::Text(U8("ギズモモード: %s   （マウス中ボタンのクリックで切替）"), modeName);
	if (ImGui::Button(U8("移動")))     { m_mode = HjGizmoMode::Translate; } ImGui::SameLine();
	if (ImGui::Button(U8("拡大")))     { m_mode = HjGizmoMode::Scale; }     ImGui::SameLine();
	if (ImGui::Button(U8("回転")))     { m_mode = HjGizmoMode::Rotate; }

	ImGui::TextWrapped(U8("オブジェクトを左クリックで選択。X/Y/Z軸ハンドルをドラッグで操作。Ctrl+Zで元に戻す / Ctrl+Yでやり直し。"));

	// グリッドスナップ（移動モードのみ有効）
	ImGui::Checkbox(U8("グリッドスナップ(移動)"), &m_snapEnabled);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.0f);
	ImGui::DragFloat(U8("グリッド幅"), &m_snapSize, 0.1f, 0.1f, 100.0f, "%.2f");

	// 選択中の表示＋コピー
	if (HasSelection())
	{
		const HjEditEntry& sel = m_entries[m_selEntry];
		ImGui::Text(U8("選択中: %s"), sel.label.c_str());
		ImGui::Text(U8("位置: %.1f, %.1f, %.1f"), sel.pos.x, sel.pos.y, sel.pos.z);
		ImGui::Text(U8("回転: %.1f, %.1f, %.1f"), sel.rot.x, sel.rot.y, sel.rot.z);
		ImGui::Text(U8("拡縮: %.2f, %.2f, %.2f"), sel.scale.x, sel.scale.y, sel.scale.z);
		if (ImGui::Button(U8("選択をコピー")))
		{
			if (sel.dup)
			{
				std::function<void()> dupFn = sel.dup;
				dupFn();
				if (m_removeLast)
				{
					std::function<void()> rm = m_removeLast;
					PushCommand([rm]() { rm(); }, [dupFn]() { dupFn(); });
				}
			}
		}
	}
	else
	{
		ImGui::TextDisabled(U8("選択中: (なし)"));
	}

	ImGui::Text(U8("元に戻す: %d  やり直し: %d"),
		static_cast<int>(m_undoStack.size()), static_cast<int>(m_redoStack.size()));
	if (ImGui::Button(U8("元に戻す (Ctrl+Z)"))) { Undo(); } ImGui::SameLine();
	if (ImGui::Button(U8("やり直し (Ctrl+Y)"))) { Redo(); }

	ImGui::End();
}
