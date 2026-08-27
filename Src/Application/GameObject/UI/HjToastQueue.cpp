#include "HjToastQueue.h"
#include "ToastConst.h"

void HjToastQueue::Push(const char* kicker, const char* text, const char* value, bool hot)
{
	if (!kicker || !text) { return; }

	// 溜まりすぎたら古い方から捨てる。
	// 画面に収まらない量を抱えても、見えないまま順番待ちになるだけ。
	while (m_items.size() >= static_cast<size_t>(ToastConst::MaxStack))
	{
		m_items.erase(m_items.begin());
	}

	Item it;
	strncpy_s(it.kicker, sizeof(it.kicker), kicker, _TRUNCATE);
	strncpy_s(it.text,   sizeof(it.text),   text,   _TRUNCATE);
	if (value) { strncpy_s(it.value, sizeof(it.value), value, _TRUNCATE); }
	it.hot = hot;
	m_items.push_back(it);
}
