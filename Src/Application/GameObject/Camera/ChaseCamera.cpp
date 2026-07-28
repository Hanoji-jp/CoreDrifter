#include "ChaseCamera.h"
#include "../Car/CarBase.h"
#include "../../Const/CarConst.h"

void ChaseCamera::Init()
{
	m_spCamera = std::make_shared<KdCamera>();
	m_spCamera->SetProjectionMatrix(CarConst::CamFov);

	m_orbitYaw   = 0.0f;   // 右ドラッグのオフセット(基準は進行方向)
	m_orbitPitch = CarConst::CamInitPitch;
	m_distance   = CarConst::CamDistance;
}

void ChaseCamera::PreDraw()
{
	auto car = m_wpCar.lock();
	if (!car || !m_spCamera) { return; }

	const float dt = KdFPSController::GetDt();

	//===== マウス右ドラッグで車の周りを回転 =====
	const bool rDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
	POINT cur{}; GetCursorPos(&cur);
	if (rDown && !m_dragging) { m_prevMouse = cur; m_dragging = true; }
	if (rDown)
	{
		const float dx = static_cast<float>(cur.x - m_prevMouse.x);
		const float dy = static_cast<float>(cur.y - m_prevMouse.y);
		m_orbitYaw   += dx * CarConst::CamOrbitSensitivity;
		m_orbitPitch += dy * CarConst::CamOrbitSensitivity;
		m_orbitPitch  = std::clamp(m_orbitPitch, CarConst::CamPitchMin, CarConst::CamPitchMax);
		m_prevMouse   = cur;
	}
	else
	{
		m_dragging = false;
	}

	//===== ホイールでズーム =====
	const float wheel = ImGui::GetIO().MouseWheel;
	if (wheel != 0.0f)
	{
		m_distance -= wheel * CarConst::CamZoomSpeed;
		m_distance  = std::clamp(m_distance, CarConst::CamMinDistance, CarConst::CamMaxDistance);
	}

	//===== ドリフトカメラ：進行方向の後ろへ基準ヨーを追従させる =====
	// 車体の向き(heading)と進行方向(velocity)をブレンドした向きの「後ろ」にカメラを置く。
	// ドリフト中は進行方向を向くので、車体が斜めにスライドして見える。
	const float headingYaw = car->GetYaw();
	const Math::Vector3 vel = car->GetVel();
	const float speed = vel.Length();

	float baseYaw = headingYaw;
	float slipRad = 0.0f;                                       // 横滑り角(動的カメラ用)
	if (speed > CarConst::CamMinTravelSpeed)
	{
		const float travelYaw = atan2f(vel.x, vel.z);          // 進行方向のワールドヨー
		float dTravel = travelYaw - headingYaw;                 // 最短角へ正規化
		while (dTravel >  3.14159265f) { dTravel -= 6.2831853f; }
		while (dTravel < -3.14159265f) { dTravel += 6.2831853f; }
		baseYaw = headingYaw + dTravel * CarConst::CamDriftBias;
		slipRad = dTravel;
	}

	//===== 動的カメラ：速度でFOVが開き、ドリフトで少し寄る =====
	const float speedN = std::clamp(speed / CarConst::MaxSpeed, 0.0f, 1.0f);
	const float slipDeg = fabsf(slipRad) * 57.29578f;
	const float driftN = std::clamp(slipDeg / CarConst::CamDriftSlipDeg, 0.0f, 1.0f) * speedN;
	const float fovTarget  = CarConst::CamFov + speedN * CarConst::CamFovSpeedGain
	                                          + driftN * CarConst::CamFovDriftGain;
	const float distTarget = m_distance - driftN * CarConst::CamDriftPull;   // ドリフトで寄る
	// ドリフト方向へカメラを傾ける(視線軸まわりのロール)。速度が乗っているときほど強く。
	const float rollTarget = std::clamp(slipRad * CarConst::CamDriftRoll * speedN,
	                                    -CarConst::CamMaxRoll, CarConst::CamMaxRoll);
	if (!m_dynInit)
	{
		m_dynFov = fovTarget; m_dynDist = distTarget; m_dynRoll = rollTarget;
		m_dynInit = true;
	}
	else
	{
		const float k = std::min(1.0f, CarConst::CamDynSmooth * dt);
		m_dynFov  += (fovTarget  - m_dynFov)  * k;
		m_dynDist += (distTarget - m_dynDist) * k;
		m_dynRoll += (rollTarget - m_dynRoll) * k;
	}
	const float targetFollow = baseYaw + 3.14159265f;          // 「後ろ」から見る

	if (!m_followInit) { m_followYaw = targetFollow; m_followInit = true; }
	else
	{
		float d = targetFollow - m_followYaw;                  // 最短角で追従
		while (d >  3.14159265f) { d -= 6.2831853f; }
		while (d < -3.14159265f) { d += 6.2831853f; }
		m_followYaw += d * std::min(CarConst::CamYawFollow * dt, 1.0f);
	}

	//===== 注視点(車)を滑らかに追う =====
	// 注視点を進行方向へ前出しする。車を画面中央に置くと、ドリフト中(車が横向き)に
	// 行き先が画面外へ逃げて前が見えない。車の向きではなく進行方向基準にするのが要点。
	const Math::Vector3 lookFwd(sinf(baseYaw), 0.0f, cosf(baseYaw));
	const Math::Vector3 target = car->GetPos()
	                           + Math::Vector3(0.0f, CarConst::CamLookAtOffsetY, 0.0f)
	                           + lookFwd * (CarConst::CamLookAhead * speedN);
	if (!m_initialized) { m_lookAt = target; m_initialized = true; }
	else                { m_lookAt = Math::Vector3::Lerp(m_lookAt, target, std::min(CarConst::CamFollow * dt, 1.0f)); }

	//===== カメラ位置を計算(基準ヨー＝進行方向の後ろ + 右ドラッグのオフセット) =====
	const float camYaw = m_followYaw + m_orbitYaw;
	const float cp = cosf(m_orbitPitch);
	const Math::Vector3 dirToCam(
		sinf(camYaw) * cp,
		sinf(m_orbitPitch),
		cosf(camYaw) * cp);
	const Math::Vector3 camPos = m_lookAt + dirToCam * m_dynDist;

	//===== カメラのワールド行列(左手系: X=右, Y=上, Z=前, 平行移動=位置) =====
	Math::Vector3 camFwd = m_lookAt - camPos;
	if (camFwd.LengthSquared() < 1e-8f) { camFwd = Math::Vector3(0.0f, 0.0f, 1.0f); }
	camFwd.Normalize();
	Math::Vector3 camRight = Math::Vector3::Up.Cross(camFwd);
	if (camRight.LengthSquared() < 1e-8f) { camRight = Math::Vector3(1.0f, 0.0f, 0.0f); }
	camRight.Normalize();
	Math::Vector3 camUp = camFwd.Cross(camRight);

	// ドリフト方向へロール：右ベクトルと上ベクトルを視線軸まわりに回す＝地平線が傾く
	if (fabsf(m_dynRoll) > 1e-5f)
	{
		const float cs = cosf(m_dynRoll), sn = sinf(m_dynRoll);
		const Math::Vector3 r = camRight * cs + camUp * sn;
		const Math::Vector3 u = camUp * cs - camRight * sn;
		camRight = r;
		camUp    = u;
	}

	Math::Matrix camWorld = Math::Matrix::Identity;
	camWorld._11 = camRight.x; camWorld._12 = camRight.y; camWorld._13 = camRight.z;
	camWorld._21 = camUp.x;    camWorld._22 = camUp.y;    camWorld._23 = camUp.z;
	camWorld._31 = camFwd.x;   camWorld._32 = camFwd.y;   camWorld._33 = camFwd.z;
	camWorld._41 = camPos.x;   camWorld._42 = camPos.y;   camWorld._43 = camPos.z;

	// 速度・ドリフトで開いたFOVを射影に反映
	m_spCamera->SetProjectionMatrix(m_dynFov);
	m_spCamera->SetCameraMatrix(camWorld);
	m_spCamera->SetToShader();
}
