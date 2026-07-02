#include "HjEditorHost.h"

void HjEditorHost::Init()
{
	// エディタ用フリーカメラ
	m_spCam = std::make_shared<HjEditorCamera>();
	m_spCam->SetPos(Math::Vector3(0.0f, 8.0f, -20.0f));

	// デモ配置オブジェクト(位置/回転/拡縮を持つ箱)
	// VRで見やすいよう、目線の高さ(約1.5m)で周囲を囲むように配置
	m_objs.clear();
	m_objs.push_back(DemoObj{ Math::Vector3( 0.0f, 1.5f,  3.0f) });
	m_objs.push_back(DemoObj{ Math::Vector3( 3.0f, 1.5f,  0.0f) });
	m_objs.push_back(DemoObj{ Math::Vector3(-3.0f, 1.5f,  0.0f) });
	m_objs.push_back(DemoObj{ Math::Vector3( 0.0f, 1.5f, -3.0f) });

	// ゲームビューポート(画面外配置/ドッキング対応のImGuiウィンドウ)を有効化。
	// この "Game" ウィンドウ上でマウスピッキングする。
	KdDebugGUI::Instance().SetGameViewport(true);

	// エディタ操作パネル(モード/スナップ/コピー/Undo-Redo)を ImGui へ表示
	KdDebugGUI::Instance().SetGuiCallback([this]() { m_editor.DrawImGui(); });

	// コピー/生成の Undo 用：最後に追加した配置を取り除く
	m_editor.SetRemoveLastHandler([this]() { if (!m_objs.empty()) { m_objs.pop_back(); } });
}

void HjEditorHost::Update()
{
	// フリーカメラ操作
	if (m_spCam) { m_spCam->Update(); }

	// 配置オブジェクトをエディタへ登録 → 選択/軸ギズモ操作/Undo-Redo
	RegisterEntries();
	m_editor.Update();
}

void HjEditorHost::PreDraw()
{
	// エディタカメラをシェーダーへ反映(描画前)
	if (m_spCam) { m_spCam->SetToShader(); }
}

void HjEditorHost::DrawEffect()
{
	// 配置オブジェクト(箱)と、選択中の軸ギズモ/選択枠を描画。
	// DrawEffect はシーンRT(ポストプロセス用RT)へ描かれるパスなので、
	// SetGameViewport(true) で ImGui の「Game」ウィンドウにそのまま映る。
	DrawBoxes();
	m_editor.DrawMarker();
}

void HjEditorHost::RegisterEntries()
{
	m_editor.Begin();
	for (int i = 0; i < static_cast<int>(m_objs.size()); ++i)
	{
		HjEditEntry e;
		e.pos   = m_objs[i].pos;
		e.rot   = m_objs[i].rot;
		e.scale = m_objs[i].scale;
		e.label = "Box " + std::to_string(i);
		// 位置/回転/拡縮を設定(ドラッグ操作 / Undo-Redo から呼ばれる)
		e.setPos   = [this, i](const Math::Vector3& p) { if (i < static_cast<int>(m_objs.size())) { m_objs[i].pos   = p; } };
		e.setRot   = [this, i](const Math::Vector3& r) { if (i < static_cast<int>(m_objs.size())) { m_objs[i].rot   = r; } };
		e.setScale = [this, i](const Math::Vector3& s) { if (i < static_cast<int>(m_objs.size())) { m_objs[i].scale = s; } };
		// 複製(「選択をコピー」から呼ばれる。少しずらして末尾に追加)
		e.dup = [this, i]()
		{
			if (i < static_cast<int>(m_objs.size()))
			{
				DemoObj o = m_objs[i];
				o.pos += Math::Vector3(EditorPickConst::CopyOffset, 0.0f, 0.0f);
				m_objs.push_back(o);
			}
		};
		m_editor.AddEntry(e);
	}
}

void HjEditorHost::DrawBoxes()
{
	if (m_objs.empty()) { return; }

	std::vector<KdPolygon::Vertex> verts;
	const float h = 0.6f;   // 箱のハーフサイズ(ローカル)

	const Math::Vector3 local[8] = {
		{ -h, -h, -h }, {  h, -h, -h }, {  h,  h, -h }, { -h,  h, -h },
		{ -h, -h,  h }, {  h, -h,  h }, {  h,  h,  h }, { -h,  h,  h },
	};
	const int eg[12][2] = { {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7} };

	auto addLine = [&](const Math::Vector3& a, const Math::Vector3& b, unsigned int col)
	{
		KdPolygon::Vertex v0{}, v1{};
		v0.pos = a; v0.color = col;
		v1.pos = b; v1.color = col;
		verts.push_back(v0); verts.push_back(v1);
	};

	for (const auto& o : m_objs)
	{
		// ワールド行列(拡縮 → 回転 → 平行移動)で箱を変形
		const Math::Matrix world =
			Math::Matrix::CreateScale(o.scale) *
			Math::Matrix::CreateFromYawPitchRoll(
				DirectX::XMConvertToRadians(o.rot.y),
				DirectX::XMConvertToRadians(o.rot.x),
				DirectX::XMConvertToRadians(o.rot.z)) *
			Math::Matrix::CreateTranslation(o.pos);

		Math::Vector3 w[8];
		for (int k = 0; k < 8; ++k) { w[k] = Math::Vector3::Transform(local[k], world); }
		for (int e = 0; e < 12; ++e) { addLine(w[eg[e][0]], w[eg[e][1]], 0xFFFFFFFF); } // 白(視認性重視)
	}

	KdShaderManager::Instance().m_StandardShader.DrawVertices(
		verts, Math::Matrix::Identity, Math::Color(1, 1, 1, 1),
		KdDepthStencilState::ZDisable, D3D_PRIMITIVE_TOPOLOGY_LINELIST);
}
