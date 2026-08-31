#pragma once

// ゲームパッド(XInput / Xbox系コントローラー)の定数
namespace PadConst
{
	//===== どのコントローラーを使うか =====
	// 番号を固定しない。
	//
	// 0番に来るとは限らない。他の機器が先に並んだり、入力を置き換える
	// 常駐ソフトが仮想のパッドを作り直したりすると、1以降へずれる。
	// 決め打ちすると、環境によって無反応になる。
	//
	// 繋がっているものを探して使う。
	constexpr int MaxSlots    = 4;    // XInputが扱える台数
	constexpr int InvalidSlot = -1;   // まだ見つかっていない

	// 探し直す間隔(フレーム)。
	//
	// 繋がっていない番号への問い合わせは遅いので、毎フレーム4つ全部を
	// 見に行くと、それだけでフレーム落ちの原因になる。
	// 一度見つけた番号を覚えておき、切れたときだけ、間を空けて探し直す。
	//
	// ※ここは秒ではなくフレーム数でよい。
	//   探し直す間隔が0.5秒か1秒かで困ることはなく、
	//   そのためだけに時間を取ってくる依存を増やす価値がない。
	constexpr int RescanInterval = 60;

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
