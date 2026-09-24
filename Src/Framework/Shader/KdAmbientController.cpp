#include "KdAmbientController.h"

#include <algorithm>
#include <cmath>

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// シェーダーマネージャで設定したシェーダーの初期値を取得してくる
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAmbientController::Init()
{
	// 光源のパラメータ
	{
		const KdShaderManager::cbLight& cbLight = KdShaderManager::Instance().GetLightCB();

		m_parameter.m_ambientLightColor = cbLight.AmbientLight;

		m_parameter.m_directionalLightDir = cbLight.DirLight_Dir;
		m_parameter.m_directionalLightDir.Normalize();

		m_parameter.m_directionalLightColor = cbLight.DirLight_Color;
	}

	// フォグのパラメータ
	{
		const KdShaderManager::cbFog& cbFog = KdShaderManager::Instance().GetFogCB();

		m_parameter.m_distanceFogColor = cbFog.DistanceFogColor;
		m_parameter.m_distanceFogDensity = cbFog.DistanceFogDensity;
		
		m_parameter.m_heightFogColor = cbFog.HeightFogColor;
		m_parameter.m_heightFogTopValue= cbFog.HeightFogTopValue;
		m_parameter.m_heightFogBottomValue = cbFog.HeightFogBottomValue;
		m_parameter.m_heightFogBeginDistance = cbFog.HeightFogBeginDistance;
	}

	// 影用深度バッファ生成情報の初期化
	SetDirLightShadowArea({ 25.0f, 25.0f }, 30.0f);

	// 光のパラメータの反映
	// ----- ----- ----- ----- ----- ----- ----- ----- ----- -----
	WriteLightParams();

	// フォグのパラメータの反映
	// ----- ----- ----- ----- ----- ----- ----- ----- ----- -----
	WriteFogParams();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 点光源は毎フレーム登録する形なので0クリアする
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAmbientController::Update()
{
	m_pointLights.clear();
	m_spotLights.clear();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 描画の直前の更新が終わったタイミングで実施
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAmbientController::Draw()
{
	// 光のパラメータの反映
	// ----- ----- ----- ----- ----- ----- ----- ----- ----- -----
	WriteLightParams();

	// フォグのパラメータの反映
	// ----- ----- ----- ----- ----- ----- ----- ----- ----- -----
	WriteFogParams();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ポイントライトの追加
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAmbientController::AddPointLight(const Math::Vector3& Color, float Radius, const Math::Vector3& Pos, bool IsBright)
{
	m_pointLights.push_back(PointLight(Color, Radius, Pos, IsBright));
}

void KdAmbientController::AddPointLight(const PointLight& pointLight)
{
	m_pointLights.push_back(pointLight);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// スポットライトの追加
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAmbientController::AddSpotLight(const Math::Vector3& color,
	const Math::Vector3& pos, const Math::Vector3& target,
	float outerDeg, float innerDeg, float range)
{
	Math::Vector3 dir = target - pos;

	// 置き場所と狙う所が同じ。向きが出せないので置かない
	if (dir.LengthSquared() < 0.000001f) { return; }

	dir.Normalize();

	// 内側が外側より広いと、境目の割り算が負になって縁が反転する
	const float outer = std::clamp(outerDeg, 0.1f, 89.0f);
	const float inner = std::clamp(innerDeg, 0.0f, outer);

	constexpr float kDeg = 3.14159265f / 180.0f;

	m_spotLights.push_back(SpotLight(color, range, pos, dir,
		 std::cos(outer * kDeg),
		 std::cos(inner * kDeg)));
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 平行光影の生成エリアの設定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAmbientController::SetDirLightShadowArea(const Math::Vector2& lightingArea, float dirLightHeight)
{
	m_shadowProj = DirectX::XMMatrixOrthographicLH(lightingArea.x, lightingArea.x, 0, dirLightHeight * 2.0f);

	m_dirLightHeight = dirLightHeight;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 平行光の設定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAmbientController::SetDirLight(const Math::Vector3& dir, const Math::Vector3& col)
{
	m_parameter.m_directionalLightDir = dir;
	m_parameter.m_directionalLightDir.Normalize();

	m_parameter.m_directionalLightColor = col;

	m_dirtyLightDir = true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 環境光の設定
// アルファ値の変更でデフォルトの暗さを指定できる。0にした場合は点光源の周囲以外は真っ暗
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAmbientController::SetAmbientLight(const Math::Vector4& col)
{
	m_parameter.m_ambientLightColor = col;

	m_dirtyLightAmb = true;
}

//----------------------------------------------------------
// 環境の映り込み
//
// キューブマップの代わりに、まわりの景色を3色で持つ。
// 車体へ映るのは「上に何があるか」「横に何があるか」で
// ほぼ決まるので、これだけでも金属らしさが出る
//----------------------------------------------------------
void KdAmbientController::SetEnvColors(const Math::Vector3& up,
                                       const Math::Vector3& side,
                                       const Math::Vector3& down)
{
	m_envUp   = Math::Vector4(up.x,   up.y,   up.z,   1.0f);
	m_envSide = Math::Vector4(side.x, side.y, side.z, 1.0f);
	m_envDown = Math::Vector4(down.x, down.y, down.z, 1.0f);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// フォグの有効/無効をそれぞれ切り替える
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAmbientController::SetFogEnable(bool distance, bool height)
{
	KdShaderManager::Instance().WriteCBFogEnable(distance, height);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 距離フォグの設定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAmbientController::SetDistanceFog(const Math::Vector3& col, float density)
{
	m_parameter.m_distanceFogColor = col;

	m_parameter.m_distanceFogDensity = density;

	m_dirtyFogDist = true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 高さフォグの設定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAmbientController::SetheightFog(const Math::Vector3& col, float topValue, float bottomValue, float distance)
{
	m_parameter.m_heightFogColor = col;

	m_parameter.m_heightFogTopValue = topValue;

	m_parameter.m_heightFogBottomValue = bottomValue;
	
	m_parameter.m_heightFogBeginDistance = distance;

	m_dirtyFogHeight = true;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 光関連パラメータの書き込み
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAmbientController::WriteLightParams()
{
	KdShaderManager::Instance().WriteCBEnvColors(m_envUp, m_envSide, m_envDown);

	// 環境光
	if (m_dirtyLightAmb)
	{
		KdShaderManager::Instance().WriteCBAmbientLight(m_parameter.m_ambientLightColor);

		m_dirtyLightAmb = false;
	}

	// 平行光
	if (m_dirtyLightDir)
	{
		KdShaderManager::Instance().WriteCBDirectionalLight(
			m_parameter.m_directionalLightDir, m_parameter.m_directionalLightColor);

		m_dirtyLightDir = false;
	}

	// 点光源
	if (m_pointLights.size())
	{
		KdShaderManager::Instance().WriteCBPointLight(m_pointLights);
	}

	// スポット光。
	//
	// 空でも書く。書かないと使用数が前のまま残って、
	// 車庫を出た後も同じ所に光が当たり続ける
	KdShaderManager::Instance().WriteCBSpotLight(m_spotLights);

	// 影描画エリアの更新
	KdShaderManager::Instance().WriteCBShadowArea(m_shadowProj, m_dirLightHeight);
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// フォグ関連パラメータの書き込み
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAmbientController::WriteFogParams()
{
	// 距離フォグ
	if (m_dirtyFogDist)
	{
		KdShaderManager::Instance().WriteCBDistanceFog(m_parameter.m_distanceFogColor,
			m_parameter.m_distanceFogDensity);

		m_dirtyFogDist = false;
	}

	// 高さフォグ
	if (m_dirtyFogHeight)
	{
		KdShaderManager::Instance().WriteCBHeightFog(m_parameter.m_heightFogColor,
			m_parameter.m_heightFogTopValue, m_parameter.m_heightFogBottomValue,
			m_parameter.m_heightFogBeginDistance);

		m_dirtyFogHeight = false;
	}
}
