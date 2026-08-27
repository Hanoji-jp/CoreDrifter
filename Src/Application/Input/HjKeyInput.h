#pragma once

//==========================================================
// HjKeyInput
//   キーボード入力の一元管理。自作なので Hj 接頭辞。
//
//   ■ なぜ集約するか
//   これまで各UIクラスが GetAsyncKeyState と「前フレーム押していたか」の
//   配列を個別に持っていた。同じコードが何箇所にも散り、
//   ・クラスごとに前フレーム状態を持つので、画面を跨ぐと押しっぱなしが
//     新規入力として拾われる(ENTERで決定→次画面でも即決定)
//   ・意味のあるキー(決定/戻る/上下)がバラバラの場所にベタ書きされる
//   という問題が出ていた。
//
//   毎フレーム先頭で Update() を1回だけ呼び、以降は全クラスがここを見る。
//   前フレーム状態が1箇所しかないので、押しっぱなしは必ず1回しか拾われない。
//
//   ■ 使い方
//     HjKeyInput::Instance().Update();          … 毎フレーム先頭で1回
//     if (HjKeyInput::Instance().Pressed(Key::Decide)) { ... }
//==========================================================
class HjKeyInput
{
public:
	// 画面操作で使う意味づけされたキー。
	// 生の仮想キーコードを各所へ書くと、割り当てを変えるときに
	// 全部を追いかけることになる。
	enum class Key
	{
		Decide,     // 決定(ENTER / SPACE)
		Cancel,     // 戻る・閉じる(ESC)
		Up, Down, Left, Right,
		Count
	};

	static HjKeyInput& Instance()
	{
		static HjKeyInput inst;
		return inst;
	}

	// 毎フレーム先頭で1回だけ呼ぶ
	void Update();
	// 毎フレームの最後に1回だけ呼ぶ(この1フレームに届いた文字を捨てる)
	void EndFrame();

	// 押した瞬間だけ true(立ち上がりエッジ)。連打防止はこれで足りる
	bool Pressed(Key k) const;
	// 押されている間ずっと true
	bool Down(Key k) const;

	// 仮想キーコードを直接見る版(デバッグキーなど、意味づけしないもの用)
	bool Pressed(int vk) const;
	bool Down(int vk) const;

	// 画面が切り替わった直後など、今押されているキーを
	// 「押した瞬間」として拾わせたくないときに呼ぶ。
	// これを入れないと、決定キーで画面遷移した次のフレームに
	// 遷移先が同じ決定キーを新規入力として拾ってしまう。
	void ConsumeAll();

	//===== 文字入力(IME経由) =====
	// 仮想キーコードから文字を組み立てるやり方だと、日本語や中国語のように
	// IMEで変換して確定する文字を拾えない。
	// ウィンドウが受け取った WM_CHAR をそのまま溜めて、UTF-8で取り出す。

	// ウィンドウ側から呼ぶ。UTF-16の1コードを積む
	void PushChar(wchar_t wc);

	// IMEで変換中かどうか。ウィンドウ側が知らせる。
	// 変換中のENTERは「変換の確定」、ESCは「変換の取り消し」であって、
	// 画面の決定・キャンセルではない。区別しないと、変換を確定した瞬間に
	// 画面まで閉じてしまう。
	void SetImeComposing(bool composing) { m_imeComposing = composing; }
	bool IsImeComposing() const { return m_imeComposing; }
	// この1フレームに確定した文字(UTF-8)。毎フレーム先頭のUpdateで空になる
	const std::string& TypedText() const { return m_typed; }
	void ClearTyped() { m_typed.clear(); }

private:
	HjKeyInput() = default;

	static constexpr int kVkCount = 256;

	bool m_now[kVkCount]  = {};
	bool m_prev[kVkCount] = {};

	// この1フレームに確定した文字(UTF-8)
	std::string  m_typed;
	// サロゲートペアの上位。次の下位と合わせて1文字にする
	wchar_t      m_highSurrogate = 0;

	// IMEで変換中か
	bool         m_imeComposing = false;

	HjKeyInput(const HjKeyInput&) = delete;
	void operator=(const HjKeyInput&) = delete;
};
