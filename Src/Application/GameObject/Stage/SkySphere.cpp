#include "SkySphere.h"
#include "../../Const/SkyConst.h"

void SkySphere::Init()
{
	m_drawType = eDrawTypeUnLit;   // 陰影なし＝星空をそのまま表示
	m_model.SetModelData("Asset/Data/galaxybox.gltf");
}

void SkySphere::DrawUnLit()
{
	if (!m_model.IsEnable()) { return; }

	// カメラ位置を中心に置く＝視点が動いても星空は動かない(無限遠に見える)
	const Math::Vector3 camPos = KdShaderManager::Instance().GetCameraCB().CamPos;
	const Math::Matrix world =
		Math::Matrix::CreateScale(SkyConst::Scale) *
		Math::Matrix::CreateTranslation(camPos);

	// 内側から見るのでカリングをオフ(裏面が消えて見えなくなるのを防ぐ)
	KdShaderManager::Instance().ChangeRasterizerState(KdRasterizerState::CullNone);
	KdShaderManager::Instance().m_StandardShader.DrawModel(m_model, world);
	KdShaderManager::Instance().UndoRasterizerState();
}
