#pragma once

#include "../../Const/StageConst.h"

//==========================================================
// Stage
//   コースマップ本体。gltfモデルを読み込み、当たり判定形状(モデル
//   コリジョン)を TypeGround(乗れる) + TypeBump(壁) として登録する。
//   車はこのオブジェクトへ下方レイ/球判定を飛ばして接地・壁押し戻しをする。
//
//   ※当たり判定は「当てられる側(=地形)」がコリジョン形状を持つ。
//     当てる側(=車)が Intersects() を実行する。
//==========================================================
class Stage : public KdGameObject
{
public:
	void Init()      override;
	void DrawLit()   override;
	void DrawDebug() override;

	// 位置合わせ用の調整パネル(ImGui)
	void DrawTuningImGui();

private:
	// m_offset / m_scale / m_yaw から m_mWorld を作り直す
	void RebuildMatrix();

	KdModelWork   m_model;

	// 配置(gltfの実寸・原点に合わせてImGuiで調整)
	Math::Vector3 m_offset = Math::Vector3(StageConst::OffsetX, StageConst::OffsetY, StageConst::OffsetZ);
	float         m_scale  = StageConst::ModelScale;
	float         m_yaw    = StageConst::YawOffset;
};
