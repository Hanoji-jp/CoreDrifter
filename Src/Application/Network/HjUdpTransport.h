#pragma once

#include "HjNetTransport.h"

//==========================================================
// HjUdpTransport
//   UDPで直接IPへ送る通信の口。
//
//   ソケットは「読めるものが無ければ待たない」設定(非ブロッキング)にする。
//   待つ設定のままだと、パケットが来ていないフレームでゲームごと止まる。
//   受信は毎フレーム「あるだけ取り出す」形で回す。
//
//   winsockの初期化は最初に開いた1つが行い、最後に閉じた1つが後始末する。
//==========================================================
class HjUdpTransport : public HjNetTransport
{
public:
	HjUdpTransport() = default;
	~HjUdpTransport() override { Close(); }

	bool Open(unsigned short port) override;
	void Close() override;
	bool IsOpen() const override { return m_sock != InvalidSock; }

	bool Send(const void* data, int size, const HjNetAddress& to) override;
	int  Receive(void* buf, int maxSize, HjNetAddress& from) override;

	bool Resolve(const char* address, unsigned short port, HjNetAddress& out) override;
	void ToString(const HjNetAddress& addr, char* buf, int bufSize) const override;

	// 実際に開けたポート。参加側は0を渡すので、開いた後でないと分からない
	unsigned short GetPort() const { return m_port; }

	// 自分のIPv4アドレス(表示用)。ホストが相手へ伝えるために使う
	static bool GetLocalAddress(char* buf, int bufSize);

private:
	// SOCKET は符号なし整数のハンドル。winsockの型をヘッダーへ持ち込まないよう
	// 同じ幅の整数で持ち、無効値も自前で定義する。
	using SockHandle = unsigned long long;
	static constexpr SockHandle InvalidSock = static_cast<SockHandle>(~0ull);

	SockHandle     m_sock = InvalidSock;
	unsigned short m_port = 0;

	HjUdpTransport(const HjUdpTransport&) = delete;
	void operator=(const HjUdpTransport&) = delete;
};
