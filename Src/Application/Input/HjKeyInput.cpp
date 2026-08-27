#include "HjKeyInput.h"

namespace
{
	// 1つの意味に複数のキーを割り当てる。
	// 決定は ENTER と SPACE のどちらでも通るようにしておく。
	struct KeyBind
	{
		int vk[2];   // 使わない側は 0
	};

	const KeyBind kBinds[static_cast<int>(HjKeyInput::Key::Count)] =
	{
		{ { VK_RETURN, VK_SPACE } },   // Decide
		{ { VK_ESCAPE, 0 } },          // Cancel
		{ { VK_UP,    'W' } },         // Up
		{ { VK_DOWN,  'S' } },         // Down
		{ { VK_LEFT,  'A' } },         // Left
		{ { VK_RIGHT, 'D' } },         // Right
	};
}

//----------------------------------------------------------
// ウィンドウが受け取った文字(UTF-16)を溜める。
//
// 仮想キーコードから組み立てる方式では、IMEで変換して確定した文字を
// 拾えない。日本語や中国語はここを通してしか受け取れない。
//----------------------------------------------------------
void HjKeyInput::PushChar(wchar_t wc)
{
	// 制御文字は文字として扱わない(BackSpaceやENTERはキー側で見る)
	if (wc < 0x20) { return; }

	wchar_t buf[3] = {};
	int len = 0;

	// サロゲートペアは2コードで1文字。上位を覚えて、下位が来たら合わせる
	if (wc >= 0xD800 && wc <= 0xDBFF)
	{
		m_highSurrogate = wc;
		return;
	}
	if (wc >= 0xDC00 && wc <= 0xDFFF)
	{
		if (m_highSurrogate == 0) { return; }
		buf[0] = m_highSurrogate;
		buf[1] = wc;
		len = 2;
		m_highSurrogate = 0;
	}
	else
	{
		m_highSurrogate = 0;
		buf[0] = wc;
		len = 1;
	}

	// 描画も保存もUTF-8で扱うので、ここで直しておく
	const int need = WideCharToMultiByte(CP_UTF8, 0, buf, len, nullptr, 0, nullptr, nullptr);
	if (need <= 0) { return; }

	std::string utf8(static_cast<size_t>(need), '\0');
	WideCharToMultiByte(CP_UTF8, 0, buf, len, &utf8[0], need, nullptr, nullptr);
	m_typed += utf8;
}

void HjKeyInput::Update()
{
	// ※ここで m_typed をクリアしてはいけない。
	//   メインループは「メッセージ処理 → Update → シーンの更新」の順なので、
	//   このフレームに届いた WM_CHAR をここで消すと、UI が読む前に無くなる。
	//   クリアはフレームの最後(EndFrame)で行う。

	// 前フレームの状態を退避してから今フレームを取る。
	// この2枚だけで全クラスのエッジ判定が成り立つ＝
	// 「同じキーを2箇所で拾って二重に反応する」ことが起きない。
	for (int i = 0; i < kVkCount; ++i)
	{
		m_prev[i] = m_now[i];
		m_now[i]  = (GetAsyncKeyState(i) & 0x8000) != 0;
	}
}

//----------------------------------------------------------
// フレームの最後に呼ぶ。
// この1フレームに届いた文字を捨てる。
// 残すと、1回打った文字が次のフレームでもう一度入る。
//----------------------------------------------------------
void HjKeyInput::EndFrame()
{
	m_typed.clear();
}

void HjKeyInput::ConsumeAll()
{
	// 今押されているものを「前から押していた」ことにする。
	// 次のフレームで立ち上がりエッジが立たなくなる＝
	// 画面遷移直後に同じキーで誤決定するのを防げる。
	for (int i = 0; i < kVkCount; ++i) { m_prev[i] = m_now[i]; }
}

bool HjKeyInput::Pressed(int vk) const
{
	if (vk < 0 || vk >= kVkCount) { return false; }

	// IMEで変換中のENTERは「変換の確定」、ESCは「変換の取り消し」。
	// 画面の決定・キャンセルとして扱うと、変換を確定した瞬間に
	// 画面まで閉じてしまう。
	if (m_imeComposing && (vk == VK_RETURN || vk == VK_ESCAPE)) { return false; }

	return m_now[vk] && !m_prev[vk];
}

bool HjKeyInput::Down(int vk) const
{
	if (vk < 0 || vk >= kVkCount) { return false; }
	return m_now[vk];
}

bool HjKeyInput::Pressed(Key k) const
{
	const KeyBind& b = kBinds[static_cast<int>(k)];
	for (int vk : b.vk)
	{
		if (vk != 0 && Pressed(vk)) { return true; }
	}
	return false;
}

bool HjKeyInput::Down(Key k) const
{
	const KeyBind& b = kBinds[static_cast<int>(k)];
	for (int vk : b.vk)
	{
		if (vk != 0 && Down(vk)) { return true; }
	}
	return false;
}
