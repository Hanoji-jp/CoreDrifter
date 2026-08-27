#pragma once

#include "HjNetTransport.h"

//==========================================================
// HjSteamTransport
//   Steam経由で送る通信の口。
//
//   ■ 直接IPとの違い
//   直接IPは相手のグローバルIPが分かっていて、かつルーターが
//   その通信を通してくれる場合しか繋がらない。家庭の回線では
//   ふつう塞がっているので、ポート開放という設定が要る。
//   Steam経由なら Valve のサーバーが間を取り持つので、
//   何も設定しなくてもインターネット越しに繋がる。
//   相手のIPも互いに見えない。
//
//   ■ 住所がIPではない
//   宛先は相手のアカウント番号(SteamID)になる。
//   HjNetAddress が userId を持っているのはこのため。
//
//   ■ 使うAPI
//   Steamの通信には「繋ぎっぱなしの線を張る」やり方と、
//   「相手を指定して投げるだけ」のやり方がある。
//   後者(ISteamNetworkingMessages)を使う。
//   送る・受け取るという形が直接IPのときと同じなので、
//   上のセッション処理を一切変えずに差し替えられる。
//
//   ■ SDKが無くてもビルドは通る
//   Steamworks SDK は各自でダウンロードして置く必要がある。
//   置かれていない環境でビルドが壊れないよう、SDKが有るときだけ
//   中身が有効になる。無いときは Open() が false を返すだけで、
//   直接IPの側は普通に動く。
//==========================================================
class HjSteamTransport : public HjNetTransport
{
public:
	HjSteamTransport() = default;
	~HjSteamTransport() override { Close(); }

	// Steamが使える状態か(SDKが入っていて、Steamクライアントが起動している)。
	//
	// ここで初期化まで済ませる。Steamの各機能は初期化後でないと
	// 取り出せないので、「使えるか」を聞かれた時点で用意しておかないと、
	// 必ず「使えない」と答えることになる。
	static bool IsAvailable();

	// 使えないときの理由。画面へそのまま出す。
	// 「使えません」だけだと、SDKが無いのか、クライアントが落ちているのか、
	// 番号の指定を間違えているのか分からず、直しようがない
	static const char* GetInitError();

	// 自分のアカウント番号。相手に伝えて繋いでもらうために使う。
	// 文字列で返すのは、そのまま画面へ出して読み上げてもらうため
	static bool GetLocalUserId(char* buf, int bufSize);

	// Steamからの通知を受け取る。毎フレーム呼ぶ。
	// これを呼ばないと、相手から繋ぎに来ても気づけない
	static void RunCallbacks();

	// port は使わない(Steamが経路を決めるので、こちらでポートを持たない)
	bool Open(unsigned short port) override;
	void Close() override;
	bool IsOpen() const override { return m_open; }

	bool Send(const void* data, int size, const HjNetAddress& to) override;
	int  Receive(void* buf, int maxSize, HjNetAddress& from) override;

	// アカウント番号の文字列("76561198000000000")から宛先を作る。
	// port は使わない
	bool Resolve(const char* address, unsigned short port, HjNetAddress& out) override;
	void ToString(const HjNetAddress& addr, char* buf, int bufSize) const override;

private:
	bool m_open = false;

	HjSteamTransport(const HjSteamTransport&) = delete;
	void operator=(const HjSteamTransport&) = delete;
};
