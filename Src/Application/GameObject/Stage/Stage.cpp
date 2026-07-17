#include "Stage.h"

void Stage::Init()
{
	m_drawType = eDrawTypeLit;

	// マップモデル読み込み
	m_model.SetModelData(StageConst::ModelPath);

	// 広大なマップなのでカリングで消えないよう半径を大きく
	m_cullingRadius = StageConst::CullingRadius;

	// 保存済みの配置(大きさ・座標・向き)とスポーンがあれば読み込む(既定値を上書き)
	LoadConfig();

	// 配置行列を作成
	RebuildMatrix();

	// 当たり判定形状を登録：モデルメッシュ全体を「地形(乗れる)＋壁」として使う。
	//   TypeGround … 車が下方レイで接地判定に使う
	//   TypeBump   … 車が球判定で壁の押し戻しに使う
	//   ※1枚メッシュ兼用だが、車側で「面法線がほぼ垂直＝壁」だけ押し戻すよう
	//     フィルタするので、走る路面(水平面)は壁扱いにならない。
	m_pCollider = std::make_unique<KdCollider>();
	m_pCollider->RegisterCollisionShape(
		"StageCollision",
		m_model.GetData(),
		KdCollider::TypeGround | KdCollider::TypeBump);
}

void Stage::RebuildMatrix()
{
	m_mWorld =
		Math::Matrix::CreateScale(m_scale) *
		Math::Matrix::CreateRotationY(m_yaw) *
		Math::Matrix::CreateTranslation(m_offset);
}

void Stage::DrawLit()
{
	KdShaderManager::Instance().ChangeRasterizerState(KdRasterizerState::CullNone);   // 裏面カリング有効
	KdShaderManager::Instance().m_StandardShader.DrawModel(m_model, m_mWorld);
	KdShaderManager::Instance().UndoRasterizerState();
}

void Stage::DrawDebug()
{
	// 当たり判定形状のワイヤ表示など(必要になれば追加)
}

void Stage::DrawTuningImGui()
{
	ImGui::Begin(U8("ステージ(マップ配置)"));

	bool changed = false;
	changed |= ImGui::DragFloat(U8("スケール(大きさ)"), &m_scale, StageConst::ScaleStep,
	                            StageConst::ScaleMin, StageConst::ScaleMax);
	changed |= ImGui::DragFloat3(U8("オフセット(座標XYZ)"), &m_offset.x, StageConst::OffsetStep);
	changed |= ImGui::DragFloat(U8("向き(Y回転 rad)"), &m_yaw, StageConst::YawStep);

	if (changed) { RebuildMatrix(); }   // 動かすと当たり判定(m_mWorld)も一緒に動く

	ImGui::Separator();
	ImGui::Text(U8("プレイヤースポーン"));
	ImGui::DragFloat3(U8("スポーン座標(XYZ)"), &m_spawnPos.x, StageConst::SpawnStep);
	ImGui::DragFloat(U8("スポーン向き(Y回転 rad)"), &m_spawnYaw, StageConst::YawStep);

	ImGui::Separator();
	// 大きさ・座標・向き＋スポーンをまとめてファイルへ保存/読込
	if (ImGui::Button(U8("保存(マップ配置＋スポーン)"))) { SaveConfig(); }
	ImGui::SameLine();
	if (ImGui::Button(U8("読込(ファイルから)")))         { LoadConfig(); RebuildMatrix(); }

	ImGui::End();
}

//----------------------------------------------------------
// 配置(大きさ・座標・向き)＋スポーンの保存 / 読込
//   CarTune_*.txt と同じ「key value」の簡易テキスト形式。
//----------------------------------------------------------
std::vector<std::pair<const char*, float*>> Stage::ConfigParamList()
{
	return {
		{ "scale", &m_scale },
		{ "offX", &m_offset.x }, { "offY", &m_offset.y }, { "offZ", &m_offset.z },
		{ "yaw",  &m_yaw },
		{ "spawnX", &m_spawnPos.x }, { "spawnY", &m_spawnPos.y }, { "spawnZ", &m_spawnPos.z },
		{ "spawnYaw", &m_spawnYaw },
	};
}

void Stage::SaveConfig()
{
	std::ofstream ofs(StageConst::ConfigPath);
	if (!ofs) { return; }
	for (const auto& p : ConfigParamList()) { ofs << p.first << " " << *p.second << "\n"; }
}

void Stage::LoadConfig()
{
	std::ifstream ifs(StageConst::ConfigPath);
	if (!ifs) { return; }
	auto params = ConfigParamList();
	std::string key;
	float val = 0.0f;
	while (ifs >> key >> val)
	{
		for (const auto& p : params) { if (key == p.first) { *p.second = val; break; } }
	}
}
