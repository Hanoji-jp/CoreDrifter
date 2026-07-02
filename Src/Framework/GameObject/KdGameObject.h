#pragma once

// ゲーム上に存在するすべてのオブジェクトの基底となるクラス
class KdGameObject : public std::enable_shared_from_this<KdGameObject>
{
public:

	// どのような描画を行うのかを設定するTypeID：Bitフラグで複数指定可能
	enum
	{
		eDrawTypeLit = 1 << 0,
		eDrawTypeUnLit = 1 << 1,
		eDrawTypeBright = 1 << 2,
		eDrawTypeUI = 1 << 3,
		eDrawTypeDepthOfShadow = 1 << 4,
	};

	KdGameObject() {}
	virtual ~KdGameObject() { Release(); }

	// 生成される全てに共通するパラメータに対する初期化のみ
	virtual void Init() {}

	virtual void PreUpdate() {}
	virtual void Update() {}
	// スレッドプールから並列実行される更新（他オブジェクトへのアクセス禁止）
	// デフォルトは何もしない。並列化できる処理のみオーバーライドする
	virtual void ParallelUpdate() {}
	virtual void PostUpdate() {}

	// それぞれの状況で描画する関数
	virtual void GenerateDepthMapFromLight();
	virtual void PreDraw() {}
	virtual void DrawLit() {}
	virtual void DrawOutline() {}   // 原神式アウトライン（背面押し出し）パス用
	virtual void DrawUnLit() {}
	virtual void DrawEffect() {}
	virtual void DrawBright() {}
	virtual void DrawSprite() {}
	virtual void DrawDebug();

	virtual void SetAsset(const std::string&) {}

	virtual void SetPos(const Math::Vector3& pos) { m_mWorld.Translation(pos); }
	virtual Math::Vector3 GetPos() const { return m_mWorld.Translation(); }

	// 拡大率を変更する関数
	void SetScale(float scalar);
	virtual void SetScale(const Math::Vector3& scale);
	virtual Math::Vector3 GetScale() const;

	const Math::Matrix& GetMatrix() const { return m_mWorld; }

	virtual bool IsExpired() const { return m_isExpired; }

	// コリジョン形状を公開（押し出し判定などで外部から参照するために使用）
	virtual const KdCollider* GetCollider() const { return m_pCollider.get(); }

	// オブジェクトを消滅させる（次フレームで除去される）
	void Expire() { m_isExpired = true; }

	virtual bool IsVisible()	const { return false; }
	virtual bool IsRideable()	const { return false; }

	// 視錐台範囲内に入っているかどうか（デフォルト: ワールド位置を中心に m_cullingRadius の球体で判定）
	virtual bool CheckInScreen(const DirectX::BoundingFrustum& frustum) const
	{
		const DirectX::BoundingSphere sphere(m_mWorld.Translation(), m_cullingRadius);
		return frustum.Intersects(sphere);
	}

	// カメラからの距離を計算
	virtual void CalcDistSqrFromCamera(const Math::Vector3& camPos);

	float GetDistSqrFromCamera() const { return m_distSqrFromCamera; }

	UINT GetDrawType() const { return m_drawType; }

	bool Intersects(const KdCollider::SphereInfo& targetShape, std::list<KdCollider::CollisionResult>* pResults);
	bool Intersects(const KdCollider::BoxInfo& targetBox, std::list<KdCollider::CollisionResult>* pResults);
	bool Intersects(const KdCollider::RayInfo& targetShape, std::list<KdCollider::CollisionResult>* pResults);

protected:

	void Release() {}

	// 描画タイプ・何の描画を行うのかを決める / 最適な描画リスト作成用
	UINT m_drawType = 0;

	// フラスタムカリング用バウンディング球体の半径（各クラスの Init 内で適宜設定）
	float m_cullingRadius = 5.0f;

	// カメラからの距離
	float m_distSqrFromCamera = 0;

	// 存在消滅フラグ
	bool m_isExpired = false;

	// 3D空間に存在する機能
	Math::Matrix	m_mWorld;

	// 当たり判定クラス
	std::unique_ptr<KdCollider> m_pCollider = nullptr;

	// デバッグ情報クラス
	std::unique_ptr<KdDebugWireFrame> m_pDebugWire = nullptr;
};
