#pragma once

//==========================================================
// HjToastQueue
//   走行中の通知(チェーン確定など)を溜めておく場所。
//
//   出来事が起きる側(採点)と、それを見せる側(画面)は別の関心事。
//   採点が直接UIを触ると、画面の作りを変えるたびに採点へ手が入る。
//   間に待ち行列を置いて、片方は積むだけ、片方は取り出すだけにする。
//==========================================================
class HjToastQueue
{
public:
	// 通知1件。文字は短命なのでその場でコピーして持つ
	struct Item
	{
		char  kicker[16] = {};   // 種別(CHAIN / BEST など)
		char  text[40]   = {};   // 本文
		char  value[24]  = {};   // 右端に出す数値
		bool  hot        = false; // 良い出来事か(アシッドで出す)
		float age        = 0.0f;  // 出てからの秒数
	};

	static HjToastQueue& Instance()
	{
		static HjToastQueue inst;
		return inst;
	}

	void Push(const char* kicker, const char* text, const char* value, bool hot);

	std::vector<Item>& Items() { return m_items; }
	void Clear() { m_items.clear(); }

private:
	HjToastQueue() = default;

	std::vector<Item> m_items;

	HjToastQueue(const HjToastQueue&) = delete;
	void operator=(const HjToastQueue&) = delete;
};
