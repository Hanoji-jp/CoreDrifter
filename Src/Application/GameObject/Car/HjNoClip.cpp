#include "HjNoClip.h"

#include "CarBase.h"
#include "../../Input/HjGamePad.h"

namespace CH = CheatConst;

//----------------------------------------------------------
// 画面のカメラの前・右を取り出す
//
// 車の向きで動かすと、後ろを向いた瞬間に操作が反転する。
// 見ている方向へ進むほうが迷わない
//----------------------------------------------------------
void HjNoClip::CameraBasis(Math::Vector3& outFwd, Math::Vector3& outRight)
{
	const Math::Matrix view = KdShaderManager::Instance().GetCameraCB().mView;

	// ビュー行列はカメラの逆なので、戻すとカメラの向きになる
	const Math::Matrix cam = view.Invert();

	outFwd   = Math::Vector3(cam._31, cam._32, cam._33);
	outRight = Math::Vector3(cam._11, cam._12, cam._13);

	if (outFwd.LengthSquared()   > 1e-8f) { outFwd.Normalize(); }
	if (outRight.LengthSquared() > 1e-8f) { outRight.Normalize(); }
}

//----------------------------------------------------------
// 車ごと飛ばす
//----------------------------------------------------------
void HjNoClip::Update(CarBase& car, bool enabled, float dt)
{
	//===== 入り切りの切り替わり =====
	if (enabled && !m_prev)
	{
		// 物理を止める。
		// 動かしている間に回すと、位置を毎フレーム引き戻されて震える
		m_haltedBefore = car.IsHalted();
		car.SetHalted(true);
	}
	else if (!enabled && m_prev)
	{
		// 入る前へ戻す。着地した所からそのまま走れる
		car.SetHalted(m_haltedBefore);
	}
	m_prev = enabled;

	if (!enabled) { return; }

	//===== 入力 =====
	// キーボードとパッドの両方。どちらでも動かせないと、
	// 見に行きたいときに持ち替えることになる
	const HjGamePad& pad = car.GetPad();

	float mx = pad.LeftStickX();
	float mz = pad.LeftStickY();
	float my = pad.RightTrigger() - pad.LeftTrigger();

	if (GetAsyncKeyState('D') & 0x8000) { mx += 1.0f; }
	if (GetAsyncKeyState('A') & 0x8000) { mx -= 1.0f; }
	if (GetAsyncKeyState('W') & 0x8000) { mz += 1.0f; }
	if (GetAsyncKeyState('S') & 0x8000) { mz -= 1.0f; }

	if (GetAsyncKeyState(VK_SPACE)   & 0x8000) { my += 1.0f; }
	if (GetAsyncKeyState(VK_CONTROL) & 0x8000) { my -= 1.0f; }

	mx = std::clamp(mx, -1.0f, 1.0f);
	mz = std::clamp(mz, -1.0f, 1.0f);
	my = std::clamp(my, -1.0f, 1.0f);

	// 速さ。押し込むと速い。
	// 広いマップを端まで見るので、普通の速さだけでは足りない
	const bool boost = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0)
	                || pad.IsButtonDown(CH::PadBoost);

	const float speed = boost ? CH::FlySpeedFast : CH::FlySpeed;

	//===== 動かす =====
	Math::Vector3 fwd, right;
	CameraBasis(fwd, right);

	const Math::Vector3 move =
		  right * mx
		+ fwd   * mz
		+ Math::Vector3::Up * my;

	if (move.LengthSquared() < 1e-8f) { return; }

	car.SetPos(car.GetPos() + move * (speed * dt));
}
