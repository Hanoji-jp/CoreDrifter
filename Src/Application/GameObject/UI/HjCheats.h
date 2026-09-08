#pragma once

//==========================================================
// HjCheats
//   走りながら切り替える小細工の入れ物。自作なので Hj 接頭辞。
//
//   ■ なぜ独立して持つか
//   切り替えるのはMODメニュー、効くのは場面や描画。
//   間に持ち主を作ると、どちらかが消えたときに繋がらなくなる。
//   選択(HjCarChoice / HjStageChoice)と同じ作法にしてある。
//
//   ■ 保存しない
//   車種やステージと違って、切ったまま次も走りたいものではない。
//   起動のたびに切れているほうが事故がない。
//==========================================================
class HjCheats
{
public:
	static HjCheats& Instance()
	{
		static HjCheats inst;
		return inst;
	}

	// 車から離れて自由に動く。マップの見回りに使う
	bool IsFreeFly() const { return m_freeFly; }
	void SetFreeFly(bool on) { m_freeFly = on; }

	// 相手の位置を地形の向こうからでも見えるようにする
	bool IsEsp() const { return m_esp; }
	void SetEsp(bool on) { m_esp = on; }

private:
	bool m_freeFly = false;
	bool m_esp     = false;
};
