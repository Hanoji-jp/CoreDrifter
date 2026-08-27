#pragma once

#include "HjUI.h"
#include "RoomConst.h"

//==========================================================
// RoomUI
//   マルチプレイのルーム画面。
//
//   ■ 画面は本番の作り、中身は当面「直接IP」
//   将来マッチングサーバーを入れたときに画面を作り直さなくて済むよう、
//   見た目と操作の流れは本番のまま作る。今は部屋一覧の代わりに
//   ホストのアドレスを直接打ち込む。
//
//   ■ 流れ
//     Entry  … 「立てる(HOST)」か「入る(JOIN)」を選ぶ
//     Host   … 自分のアドレスを出して参加者を待つ
//     Join   … 相手のアドレスを打ち込んで繋ぐ
//==========================================================
class RoomUI : public KdGameObject
{
public:
	// 画面の段階。今どこにいるかで出す物と操作が変わる
	enum class Phase
	{
		Entry,   // 立てる/入る を選ぶ
		Host,    // 参加者を待っている
		Join,    // アドレスを入力している
	};

	void Update()     override;
	void DrawSprite() override;

	// ゲーム開始が押されたか(1回だけ真)
	bool ConsumeStart() { const bool a = m_start; m_start = false; return a; }
	// 戻るが押されたか(1回だけ真)。Entryまで戻り切っていたら画面ごと戻す
	bool ConsumeBack()  { const bool a = m_back;  m_back  = false; return a; }

	Phase GetPhase() const { return m_phase; }

private:
	void UpdateEntry();
	void UpdateHost();
	void UpdateJoin();
	// 文字キーを拾ってアドレス欄へ入れる(数字と.のみ)
	void UpdateAddressInput();

	// 段階が変わっても動かない部分(帯・バッジ・見出し・縦レール)
	void DrawChrome();
	void DrawEntry();
	void DrawHost();
	void DrawJoin();
	// 参加者のカードを1枚。空きスロットは枠だけにして差を見せる
	void DrawSlot(int index, bool filled, bool host, bool ready);
	void DrawSlots();
	void DrawReadyBar();
	void DrawFooter();

	Phase m_phase = Phase::Entry;
	int   m_sel   = 0;        // Entryでの選択(0=HOST 1=JOIN)

	// 入力中のアドレス。終端ぶん+1
	char  m_address[RoomConst::AddressMax + 1] = {};
	int   m_addressLen = 0;

	// 自分のアドレス(ホスト時に相手へ伝えるために出す)
	char  m_localAddress[RoomConst::AddressMax + 1] = {};
	bool  m_localResolved = false;

	// 自分が準備完了にしたか。通信が入れば相手にも送る値になる
	bool  m_ready = false;

	bool  m_start = false;
	bool  m_back  = false;

};
