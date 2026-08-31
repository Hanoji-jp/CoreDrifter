#pragma once

#include "../Const/PadConst.h"

//==========================================================
// HjGamePad
//   XInput対応ゲームパッド(Xbox系)の入力ラッパー。
//   自作の再利用クラスなので Hj 接頭辞(基盤エンジンのKdと区別)。
//   毎フレーム先頭で Update() を呼び、以降はゲッターで参照する。
//
//   使う番号は固定しない。繋がっているものを探して使う(詳細は PadConst.h)。
//
//   マッピング(ドリフト操作向け):
//     左スティックX  … ステア(アナログ)
//     右トリガー(RT) … アクセル
//     左トリガー(LT) … ブレーキ/後退
//     A / RB ボタン  … サイドブレーキ
//==========================================================
class HjGamePad
{
public:
	void Update();   // 毎フレーム先頭で呼ぶ(XInput状態を取得)

	bool IsConnected() const { return m_connected; }

	// 実際に使っている番号。繋がっていなければ PadConst::InvalidSlot。
	// 反応しないときに、そもそも見つかっているのかを確かめる用
	int  UsingSlot() const { return m_slot; }

	// 調べ用の表示(F2のパネル)。
	//
	// 反応しないときに、どこで止まっているのかを目で見る。
	//   ・1台も見つからない      → 外の何かが横取りしている
	//   ・見つかるが値が動かない → 割り当ての側の問題
	// 推測で直す前に、これを見て切り分ける
	void DrawImGui();

	// 左スティック (-1..1, デッドゾーン適用済み)
	float LeftStickX() const { return m_lx; }
	float LeftStickY() const { return m_ly; }
	// 右スティック (-1..1)
	float RightStickX() const { return m_rx; }
	float RightStickY() const { return m_ry; }
	// トリガー (0..1)
	float RightTrigger() const { return m_rt; }
	float LeftTrigger()  const { return m_lt; }

	// XINPUT_GAMEPAD_* のフラグで押下中判定(ホールド)
	bool IsButtonDown(unsigned short flag) const { return (m_buttons & flag) != 0; }
	// 押した瞬間だけtrue(立ち上がりエッジ。シフト操作用)
	bool IsButtonPressed(unsigned short flag) const
	{
		return (m_buttons & flag) != 0 && (m_prevButtons & flag) == 0;
	}

private:
	unsigned short m_buttons     = 0;
	unsigned short m_prevButtons = 0;   // 前フレームのボタン(エッジ検出用)
	float          m_lx = 0.0f, m_ly = 0.0f;
	float          m_rx = 0.0f, m_ry = 0.0f;
	float          m_rt = 0.0f, m_lt = 0.0f;
	bool           m_connected = false;

	// 使っている番号と、探し直しまでの残りフレーム。
	// 繋がっていない番号への問い合わせは遅いので、毎フレーム全部は見ない
	int            m_slot        = PadConst::InvalidSlot;
	int            m_rescanCount = 0;
};
