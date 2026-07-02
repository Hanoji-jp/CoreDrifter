#pragma once

// 前方宣言（規約：ヘッダーでは他ヘッダーをincludeしない）
class CarBase;

//==========================================================
// ChaseCamera
//   車の後方・少し上から追従するカメラ。
//==========================================================
class ChaseCamera : public KdGameObject
{
public:
	void Init()    override;
	void PreDraw() override;

	// 追従対象をセット
	void SetTarget(const std::shared_ptr<CarBase>& _car) { m_wpCar = _car; }

private:
	std::shared_ptr<KdCamera> m_spCamera = nullptr;
	std::weak_ptr<CarBase>    m_wpCar;

	// オービット状態（右ドラッグで回転・ホイールでズーム）
	float m_orbitYaw   = 0.0f;   // 右ドラッグによる追加オフセット
	float m_orbitPitch = 0.0f;
	float m_distance   = 0.0f;
	// ドリフトカメラ：進行方向へ追従する基準ヨー
	float m_followYaw  = 0.0f;
	bool  m_followInit = false;
	Math::Vector3 m_lookAt = Math::Vector3::Zero;  // 注視点(車)を滑らかに追う
	POINT m_prevMouse = { 0, 0 };
	bool  m_dragging  = false;
	bool  m_initialized = false;
};
