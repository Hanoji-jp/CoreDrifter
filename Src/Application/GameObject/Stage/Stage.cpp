#include "Stage.h"

void Stage::Init()
{
	m_drawType = eDrawTypeLit;

	// マップモデル読み込み
	m_model.SetModelData(StageConst::ModelPath);

	// 広大なマップなのでカリングで消えないよう半径を大きく
	m_cullingRadius = StageConst::CullingRadius;

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
	KdShaderManager::Instance().m_StandardShader.DrawModel(m_model, m_mWorld);
}

void Stage::DrawDebug()
{
	// 当たり判定形状のワイヤ表示など(必要になれば追加)
}

void Stage::DrawTuningImGui()
{
	ImGui::Begin(U8("ステージ(マップ配置)"));

	bool changed = false;
	changed |= ImGui::DragFloat(U8("スケール"), &m_scale, StageConst::ScaleStep,
	                            StageConst::ScaleMin, StageConst::ScaleMax);
	changed |= ImGui::DragFloat3(U8("オフセット(XYZ)"), &m_offset.x, StageConst::OffsetStep);
	changed |= ImGui::DragFloat(U8("向き(Y回転 rad)"), &m_yaw, StageConst::YawStep);

	if (changed) { RebuildMatrix(); }   // 動かすと当たり判定(m_mWorld)も一緒に動く

	ImGui::End();
}
