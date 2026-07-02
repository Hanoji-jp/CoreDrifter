#include "../../../Pch.h"
#include "HjEditorCamera.h"

HjEditorCamera::HjEditorCamera()
{
    GetCursorPos(&m_prevMousePos);
}

void HjEditorCamera::Update()
{
    RotateByMouse();
    PanByMouse();
    ZoomByWheel();
    MoveByKey();

    // Yaw -> Pitch の順で回転行列を合成してカメラ行列を構築
    const Math::Matrix mRot =
        Math::Matrix::CreateRotationX(m_pitch) *
        Math::Matrix::CreateRotationY(m_yaw);

    const Math::Matrix mTrans = Math::Matrix::CreateTranslation(m_pos);

    SetCameraMatrix(mRot * mTrans);
}

void HjEditorCamera::RotateByMouse()
{
    // 右ボタンが押されている間だけ回転
    if (!(GetAsyncKeyState(VK_RBUTTON) & 0x8000)) { return; }

    POINT curPos{};
    GetCursorPos(&curPos);

    const float dx = static_cast<float>(curPos.x - m_prevMousePos.x);
    const float dy = static_cast<float>(curPos.y - m_prevMousePos.y);

    m_yaw   += dx * MapConst::EditorCameraRotSpeed;
    m_pitch += dy * MapConst::EditorCameraRotSpeed;

    // Pitchを±89度に制限
    constexpr float PitchLimit = DirectX::XMConvertToRadians(89.0f);
    m_pitch = std::clamp(m_pitch, -PitchLimit, PitchLimit);

    m_prevMousePos = curPos;
}

void HjEditorCamera::PanByMouse()
{
    // 中ボタンが押されている間だけパン
    if (!(GetAsyncKeyState(VK_MBUTTON) & 0x8000)) { return; }

    POINT curPos{};
    GetCursorPos(&curPos);

    const float dx = static_cast<float>(curPos.x - m_prevMousePos.x);
    const float dy = static_cast<float>(curPos.y - m_prevMousePos.y);

    // カメラのローカル右・上方向に平行移動
    const Math::Matrix mRot =
        Math::Matrix::CreateRotationX(m_pitch) *
        Math::Matrix::CreateRotationY(m_yaw);

    const Math::Vector3 right = Math::Vector3::TransformNormal(Math::Vector3::Right, mRot);
    const Math::Vector3 up    = Math::Vector3::TransformNormal(Math::Vector3::Up,    mRot);

    m_pos -= right * dx * MapConst::EditorCameraMoveSpeed * 0.1f;
    m_pos += up    * dy * MapConst::EditorCameraMoveSpeed * 0.1f;

    m_prevMousePos = curPos;
}

void HjEditorCamera::ZoomByWheel()
{
    // ホイールはRawInput / GetAsyncKeyState では取れないので
    // ImGuiのIO経由で取得する
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel == 0.0f) { return; }

    const Math::Matrix mRot =
        Math::Matrix::CreateRotationX(m_pitch) *
        Math::Matrix::CreateRotationY(m_yaw);

    const Math::Vector3 forward = Math::Vector3::TransformNormal(Math::Vector3::Forward, mRot);

    m_pos += forward * wheel * MapConst::EditorCameraMoveSpeed * 2.0f;
}

void HjEditorCamera::MoveByKey()
{
    // ImGuiウィンドウ上でキーを押した場合は無視
    if (ImGui::GetIO().WantCaptureKeyboard) { return; }

    const Math::Matrix mRot =
        Math::Matrix::CreateRotationX(m_pitch) *
        Math::Matrix::CreateRotationY(m_yaw);

    const Math::Vector3 forward = Math::Vector3::TransformNormal(Math::Vector3::Forward, mRot);
    const Math::Vector3 right   = Math::Vector3::TransformNormal(Math::Vector3::Right,   mRot);

    if (GetAsyncKeyState('W') & 0x8000) { m_pos += forward * MapConst::EditorCameraMoveSpeed; }
    if (GetAsyncKeyState('S') & 0x8000) { m_pos -= forward * MapConst::EditorCameraMoveSpeed; }
    if (GetAsyncKeyState('A') & 0x8000) { m_pos -= right   * MapConst::EditorCameraMoveSpeed; }
    if (GetAsyncKeyState('D') & 0x8000) { m_pos += right   * MapConst::EditorCameraMoveSpeed; }
    if (GetAsyncKeyState('Q') & 0x8000) { m_pos.y -= MapConst::EditorCameraMoveSpeed; }
    if (GetAsyncKeyState('E') & 0x8000) { m_pos.y += MapConst::EditorCameraMoveSpeed; }
}
