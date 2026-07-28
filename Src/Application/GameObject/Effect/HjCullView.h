#pragma once

#include "../../Const/CullingConst.h"

//==========================================================
// HjCullView
//   「この球はカメラに映るか」を答えるだけの小さな判定器。
//   シェーダーへ渡しているカメラ行列から視錐台を組み立てて持つ。
//
//   エフェクト側は粒・区間ごとにこれを引き、映らないものは
//   頂点を積む前に捨てる(CPUの変換コストと転送量がそのまま減る)。
//
//   使い方:
//     HjCullView view;
//     view.Update();                    // 描画の頭で1回
//     if (!view.IsVisible(pos, r)) { continue; }
//==========================================================
class HjCullView
{
public:
	// 現在のカメラから視錐台を作り直す(描画1回につき1度でよい)
	void Update()
	{
		const auto& cam = KdShaderManager::Instance().GetCameraCB();

		DirectX::BoundingFrustum local;
		DirectX::BoundingFrustum::CreateFromMatrix(local, cam.mProj);
		// ビュー行列の逆＝カメラのワールド行列。これで視錐台をワールドへ運ぶ
		local.Transform(m_frustum, cam.mView.Invert());

		m_camPos = cam.CamPos;
	}

	// 中心pos・半径radiusの球が映るか。maxDistは打ち切り距離(0以下で無制限)
	bool IsVisible(const Math::Vector3& pos, float radius, float maxDist) const
	{
		if (maxDist > 0.0f)
		{
			const Math::Vector3 d = pos - m_camPos;
			// 距離は二乗のまま比べる(平方根を取らないぶん軽い)
			if (d.LengthSquared() > maxDist * maxDist) { return false; }
		}
		return m_frustum.Intersects(DirectX::BoundingSphere(pos, radius));
	}

	const Math::Vector3& GetCamPos() const { return m_camPos; }

private:
	DirectX::BoundingFrustum m_frustum;
	Math::Vector3            m_camPos = Math::Vector3::Zero;
};
