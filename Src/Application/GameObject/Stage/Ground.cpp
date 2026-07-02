#include "Ground.h"

void Ground::Init()
{
	m_drawType = eDrawTypeLit;
	m_model.SetModelData("Asset/Data/Ground.gltf");
	Math::Matrix mScale = Math::Matrix::CreateScale(100.0f, 1.0f, 100.0f);
	m_mWorld = mScale;
}

void Ground::DrawLit()
{
	// 地面モデルをそのまま描画（モデル自身の座標）
	KdShaderManager::Instance().m_StandardShader.DrawModel(m_model, m_mWorld);
}
