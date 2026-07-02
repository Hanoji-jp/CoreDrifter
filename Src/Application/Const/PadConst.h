#pragma once

// ゲームパッド(XInput / Xbox系コントローラー)の定数
namespace PadConst
{
	// 使用するコントローラー番号(0=1P)
	constexpr int PlayerIndex = 0;

	// スティック/トリガーの生値の最大
	constexpr float StickRawMax   = 32767.0f;
	constexpr float TriggerRawMax = 255.0f;

	// 遊び(デッドゾーン)。これ未満は0扱い、超えた分を0〜1へ再マップ
	constexpr float StickDeadzone   = 0.20f;
	constexpr float TriggerDeadzone = 0.08f;

	// ボタン割り当て(値は Xinput.h の XINPUT_GAMEPAD_* フラグ)
	//   A=0x1000, B=0x2000, X=0x4000, Y=0x8000
	//   LB(LEFT_SHOULDER)=0x0100, RB(RIGHT_SHOULDER)=0x0200
	constexpr unsigned short HandbrakeButtons = 0x1000; // A ：サイドブレーキ
	constexpr unsigned short ClutchButton      = 0x4000; // X ：クラッチ(押している間=切)
	constexpr unsigned short ShiftUpButton      = 0x0200; // RB：シフトアップ
	constexpr unsigned short ShiftDownButton    = 0x0100; // LB：シフトダウン
}
