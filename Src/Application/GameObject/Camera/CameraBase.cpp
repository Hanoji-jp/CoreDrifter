#include "CameraBase.h"
#include "../../main.h"

void CameraBase::Init()
{
	if (!m_spCamera)
	{
		m_spCamera = std::make_shared<KdCamera>();
	}
	// ↓画面中央座標
	m_FixMousePos.x = 640;
	m_FixMousePos.y = 360;
}

void CameraBase::PreDraw()
{
	if (!m_spCamera) { return; }

	m_spCamera->SetCameraMatrix(m_mWorld);
	m_spCamera->SetToShader();
}

void CameraBase::SetTarget(const std::shared_ptr<KdGameObject>& target)
{
	if (!target) { return; }

	m_wpTarget = target;
}

void CameraBase::UpdateRotateByMouse()
{
	// マウスでカメラを回転させる処理

	// マウスの移動量を取得する
	// Raw Input(WM_INPUT)で取得した「OSのマウスアクセラレーションに依存しない生の移動量」を使う
	// → 速く動かしても加速せず、移動量に正しく比例して回転する
	POINT _mouseMove = Application::Instance().GetMouseRawMove();

	// カーソルをウィンドウのクライアント中央に固定し、画面外へ出ないようにする
	// (Raw Inputの移動量はハードウェアの相対移動なので、SetCursorPosの影響は受けない)
	HWND hWnd = Application::Instance().GetWindowHandle();
	RECT clientRect{};
	GetClientRect(hWnd, &clientRect);
	POINT center{ (clientRect.right - clientRect.left) / 2,
				  (clientRect.bottom - clientRect.top) / 2 };
	ClientToScreen(hWnd, &center);	// クライアント座標 → スクリーン座標
	SetCursorPos(center.x, center.y);

	// 実際にカメラを回転させる処理(0.15はただの感度の補正値)
	m_DegAng.x += _mouseMove.y * 0.15f;
	m_DegAng.y += _mouseMove.x * 0.15f;

	// 回転制御
	m_DegAng.x = std::clamp(m_DegAng.x, -45.f, 45.f);
}