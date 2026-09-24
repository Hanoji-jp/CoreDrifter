#pragma once

struct PointLight;
struct SpotLight;

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ゲーム内の空間環境をコントロールするパラメータ群
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
struct KdAmbientParameter
{
	// 環境光
	Math::Vector4	m_ambientLightColor;

	// 平行光
	Math::Vector3	m_directionalLightDir;
	Math::Vector3	m_directionalLightColor;

	// 距離フォグ
	Math::Vector3	m_distanceFogColor;
	float			m_distanceFogDensity = 0.0f;		// フォグ減衰率

	// 高さフォグ
	Math::Vector3	m_heightFogColor;
	float			m_heightFogTopValue = 0.0f;			// フォグを開始する上限の高さ
	float			m_heightFogBottomValue = 0.0f;		// フォグ色に染まる下限の高さ
	float			m_heightFogBeginDistance = 0.0f;	// フォグの開始する距離
};

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 光やフォグなどの空間環境をコントロールするパラメータを制御
// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== =====
// 光：環境光・平行光・点光源の情報を変化させる（シェーダーへの情報の転送を行う）
// フォグ：距離フォグ・高さフォグの情報を変化させる
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class KdAmbientController
{
public:
	KdAmbientController() {}
	~KdAmbientController() {}

	// シェーダーマネージャで設定したシェーダーの初期値を取得してくる
	void Init();

	void Update();

	void Draw();

	void AddPointLight(const Math::Vector3& Color, float Radius, const Math::Vector3& Pos, bool IsBright = true);
	void AddPointLight(const PointLight& pointLight);

	// スポット光を置く。点光と同じく、毎フレーム置き直す。
	//
	// 向きは「どこを狙うか」で渡す。方向で渡すと、
	// 置き場所を動かすたびに向きも計算し直すことになる。
	//
	// 角は度。outer が円錐の外側、inner はそこまで減らさない内側
	void AddSpotLight(const Math::Vector3& color, const Math::Vector3& pos,
		  const Math::Vector3& target,
		  float outerDeg, float innerDeg, float range);

	// 平行光の影生成用の射影行列設定：エリアを指定 x:幅 y:奥行
	void SetDirLightShadowArea(const Math::Vector2& lightingArea, float dirLightHeight);

	// 平行光の方向と色を設定
	void SetDirLight(const Math::Vector3& dir, const Math::Vector3& col);

	// 環境光の色を設定
	void SetAmbientLight(const Math::Vector4& col);

	// 環境の映り込み。上・横・下の3色。
	//
	// キューブマップの代わりで、反射の向きで混ぜて使う。
	// 屋外なら 上=空 / 横=地平 / 下=地面、
	// 車庫なら 上=照明 / 横=壁 / 下=床
	void SetEnvColors(const Math::Vector3& up,
	                  const Math::Vector3& side,
	                  const Math::Vector3& down);

	// フォグの有効と無効を切り替える
	void SetFogEnable(bool distance, bool height);

	// 距離フォグの設定
	void SetDistanceFog(const Math::Vector3& col, float density = 0.001f);

	// 高さフォグの設定
	void SetheightFog(const Math::Vector3& col, float topValue, float bottomValue, float distance);

private:
	// 環境の映り込み(上・横・下)
	Math::Vector4 m_envUp   = { 0.30f, 0.32f, 0.38f, 1.0f };
	Math::Vector4 m_envSide = { 0.18f, 0.19f, 0.22f, 1.0f };
	Math::Vector4 m_envDown = { 0.08f, 0.08f, 0.09f, 1.0f };


	void WriteLightParams(); 
	void WriteFogParams();

	KdAmbientParameter m_parameter;

	// 点光源
	std::list<PointLight> m_pointLights;

	// スポット光。点光と同じく毎フレーム作り直す
	std::list<SpotLight> m_spotLights;

	// 平行光の影生成用の射影行列
	Math::Matrix m_shadowProj;
	// 平行光源の高さ(実際には存在しない影生成用の仮の位置)
	float		m_dirLightHeight = 0.0f;

	// 変更があるかを判定するフラグ
	bool m_dirtyLightAmb = true;
	bool m_dirtyLightDir = true;
	bool m_dirtyFogDist = true;
	bool m_dirtyFogHeight = true;
};
