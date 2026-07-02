#include "HjGamePad.h"

#include <Xinput.h>
#pragma comment(lib, "Xinput.lib")

namespace
{
	// 生値(-1..1相当)へデッドゾーンを適用し、遊びを除いた分を0〜1へ再マップする
	float ApplyDeadzone(float value, float deadzone)
	{
		const float mag  = fabsf(value);
		if (mag < deadzone) { return 0.0f; }
		const float sign  = (value < 0.0f) ? -1.0f : 1.0f;
		const float rescaled = (mag - deadzone) / (1.0f - deadzone);
		return sign * std::min(rescaled, 1.0f);
	}
}

void HjGamePad::Update()
{
	// 前フレームのボタンを退避(エッジ検出用)
	m_prevButtons = m_buttons;

	XINPUT_STATE state = {};
	const DWORD res = XInputGetState(PadConst::PlayerIndex, &state);

	m_connected = (res == ERROR_SUCCESS);
	if (!m_connected)
	{
		// 未接続時は入力なしに倒す
		m_buttons = 0;
		m_lx = m_ly = m_rx = m_ry = m_rt = m_lt = 0.0f;
		return;
	}

	const XINPUT_GAMEPAD& pad = state.Gamepad;
	m_buttons = pad.wButtons;

	// スティックは -32768..32767 → -1..1 にしてデッドゾーン適用
	m_lx = ApplyDeadzone(pad.sThumbLX / PadConst::StickRawMax, PadConst::StickDeadzone);
	m_ly = ApplyDeadzone(pad.sThumbLY / PadConst::StickRawMax, PadConst::StickDeadzone);
	m_rx = ApplyDeadzone(pad.sThumbRX / PadConst::StickRawMax, PadConst::StickDeadzone);
	m_ry = ApplyDeadzone(pad.sThumbRY / PadConst::StickRawMax, PadConst::StickDeadzone);

	// トリガーは 0..255 → 0..1 にしてデッドゾーン適用
	m_rt = ApplyDeadzone(pad.bRightTrigger / PadConst::TriggerRawMax, PadConst::TriggerDeadzone);
	m_lt = ApplyDeadzone(pad.bLeftTrigger  / PadConst::TriggerRawMax, PadConst::TriggerDeadzone);
}
