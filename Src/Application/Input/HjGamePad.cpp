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
	m_connected = false;

	// 覚えている番号をまず見る。
	// 繋がっていない番号への問い合わせは遅いので、毎フレーム全部は見に行かない
	if (m_slot != PadConst::InvalidSlot)
	{
		m_connected = (XInputGetState(m_slot, &state) == ERROR_SUCCESS);

		// 抜かれた。番号を捨てて、次から探し直す
		if (!m_connected) { m_slot = PadConst::InvalidSlot; }
	}

	// 番号が分からないので探す。
	//
	// Steam経由の起動では、Steam入力が物理コントローラーを隠して
	// 仮想のパッドを別の番号へ作り直す。0で決め打ちすると見つからない。
	//
	// 毎フレーム探すと、繋がっていない番号への問い合わせで重くなるため、
	// 間を空ける。差し込んですぐ効かなくても、1秒ほどで拾える
	if (!m_connected)
	{
		if (--m_rescanCount <= 0)
		{
			m_rescanCount = PadConst::RescanInterval;

			for (int i = 0; i < PadConst::MaxSlots; ++i)
			{
				if (XInputGetState(i, &state) != ERROR_SUCCESS) { continue; }

				m_slot      = i;
				m_connected = true;
				break;
			}
		}
	}

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

void HjGamePad::DrawImGui()
{
	ImGui::Text("slot : %d", m_slot);
	ImGui::SameLine();
	if (m_connected) { ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "CONNECTED"); }
	else             { ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "NOT FOUND"); }

	ImGui::Separator();

	// 4つ全部の生の状況。
	//
	// 覚えている番号だけでなく全部出さないと、「番号がずれているだけ」なのか
	// 「1台も見えていない」のかが区別できない。
	//
	// 全部空なら、こちらの問題ではなく、外の何かがコントローラーを
	// 横取りしている(入力を置き換える常駐ソフトなど)。
	// そこまで分かれば、コードを触らずに済む
	ImGui::TextUnformatted("XInput slots");
	for (int i = 0; i < PadConst::MaxSlots; ++i)
	{
		XINPUT_STATE st = {};
		const bool ok = (XInputGetState(i, &st) == ERROR_SUCCESS);

		ImGui::Text("  %d : %s", i, ok ? "o" : "-");
		if (!ok) { continue; }

		// 生値をそのまま出す。デッドゾーンを通した後だと、
		// 「値が来ていない」のか「消している」のかが分からなくなる
		ImGui::SameLine();
		ImGui::Text("LX %6d  LT %3d  RT %3d  btn %04X",
		            st.Gamepad.sThumbLX, st.Gamepad.bLeftTrigger,
		            st.Gamepad.bRightTrigger, st.Gamepad.wButtons);
	}
}
