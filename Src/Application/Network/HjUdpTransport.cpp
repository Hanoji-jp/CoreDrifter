#include "HjUdpTransport.h"

namespace
{
	// winsockは使う前に初期化が要る。開いたソケットの数を数えて、
	// 最初の1つで初期化し、最後の1つで後始末する。
	// シーンを行き来するたびに初期化し直すのを避けるため。
	int  s_openCount = 0;
	bool s_wsaReady  = false;

	bool EnsureWinsock()
	{
		if (s_wsaReady) { return true; }

		WSADATA wsa{};
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { return false; }
		s_wsaReady = true;
		return true;
	}

	void ReleaseWinsockIfIdle()
	{
		if (s_openCount > 0 || !s_wsaReady) { return; }
		WSACleanup();
		s_wsaReady = false;
	}
}

//----------------------------------------------------------
// 受信用の口を開く。
// port=0 なら空いているポートをOSに任せる(参加側はどこでもよい)。
//----------------------------------------------------------
bool HjUdpTransport::Open(unsigned short port)
{
	Close();
	if (!EnsureWinsock()) { return false; }

	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET)
	{
		ReleaseWinsockIfIdle();
		return false;
	}

	sockaddr_in addr{};
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port        = htons(port);
	if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
	{
		closesocket(s);
		ReleaseWinsockIfIdle();
		return false;
	}

	// 「読めるものが無ければ待たない」設定にする。
	// 待つ設定のままだと、パケットが来ていないフレームでゲームごと止まる。
	u_long nonBlocking = 1;
	ioctlsocket(s, FIONBIO, &nonBlocking);

	// 実際に割り当てられたポートを控える(0を渡した場合に必要)
	sockaddr_in bound{};
	int boundLen = sizeof(bound);
	if (getsockname(s, reinterpret_cast<sockaddr*>(&bound), &boundLen) == 0)
	{
		m_port = ntohs(bound.sin_port);
	}
	else
	{
		m_port = port;
	}

	m_sock = static_cast<SockHandle>(s);
	++s_openCount;
	return true;
}

void HjUdpTransport::Close()
{
	if (m_sock == InvalidSock) { return; }

	closesocket(static_cast<SOCKET>(m_sock));
	m_sock = InvalidSock;
	m_port = 0;

	if (s_openCount > 0) { --s_openCount; }
	ReleaseWinsockIfIdle();
}

//----------------------------------------------------------
// 送る。届く保証は無い(UDP)。
// 送れなかった場合も、次の送信で最新の位置が届くので追いかけない。
//----------------------------------------------------------
bool HjUdpTransport::Send(const void* data, int size, const HjNetAddress& to)
{
	if (m_sock == InvalidSock || !data || size <= 0 || !to.IsValid()) { return false; }

	sockaddr_in addr{};
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = to.ip;
	addr.sin_port        = to.port;

	const int sent = sendto(static_cast<SOCKET>(m_sock),
	                        static_cast<const char*>(data), size, 0,
	                        reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
	return sent == size;
}

//----------------------------------------------------------
// 受け取る。戻り値=受信バイト数(0=今は何も来ていない)。
// 呼ぶ側は「0が返るまで」繰り返して、溜まっているぶんを全部取り出す。
//----------------------------------------------------------
int HjUdpTransport::Receive(void* buf, int maxSize, HjNetAddress& from)
{
	from = HjNetAddress{};
	if (m_sock == InvalidSock || !buf || maxSize <= 0) { return 0; }

	sockaddr_in addr{};
	int addrLen = sizeof(addr);
	const int got = recvfrom(static_cast<SOCKET>(m_sock),
	                         static_cast<char*>(buf), maxSize, 0,
	                         reinterpret_cast<sockaddr*>(&addr), &addrLen);
	if (got <= 0) { return 0; }   // 何も来ていない(非ブロッキングなので普通のこと)

	from.ip   = addr.sin_addr.s_addr;
	from.port = addr.sin_port;
	return got;
}

//----------------------------------------------------------
// "192.168.0.5" のような文字列から宛先を作る。
//----------------------------------------------------------
bool HjUdpTransport::Resolve(const char* address, unsigned short port, HjNetAddress& out)
{
	out = HjNetAddress{};
	if (!address || address[0] == '\0' || port == 0) { return false; }
	if (!EnsureWinsock()) { return false; }

	in_addr v4{};
	if (inet_pton(AF_INET, address, &v4) != 1) { return false; }

	out.ip   = v4.s_addr;
	out.port = htons(port);
	return true;
}

void HjUdpTransport::ToString(const HjNetAddress& addr, char* buf, int bufSize) const
{
	if (!buf || bufSize <= 0) { return; }
	buf[0] = '\0';
	if (!addr.IsValid()) { return; }

	in_addr v4{};
	v4.s_addr = addr.ip;

	char ip[INET_ADDRSTRLEN] = {};
	if (!inet_ntop(AF_INET, &v4, ip, sizeof(ip))) { return; }

	snprintf(buf, static_cast<size_t>(bufSize), "%s:%u", ip, ntohs(addr.port));
}

//----------------------------------------------------------
// 自分のIPv4アドレス(表示用)。
// ホストがこれを画面に出し、参加者が手で打ち込む。
// 複数のアダプタがある場合は最初に見つかったものを返す。
//----------------------------------------------------------
bool HjUdpTransport::GetLocalAddress(char* buf, int bufSize)
{
	if (!buf || bufSize <= 0) { return false; }
	buf[0] = '\0';
	if (!EnsureWinsock()) { return false; }

	char host[256] = {};
	if (gethostname(host, sizeof(host)) != 0) { return false; }

	addrinfo hints{};
	hints.ai_family   = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	addrinfo* list = nullptr;
	if (getaddrinfo(host, nullptr, &hints, &list) != 0 || !list) { return false; }

	bool ok = false;
	for (addrinfo* p = list; p; p = p->ai_next)
	{
		if (p->ai_family != AF_INET || !p->ai_addr) { continue; }

		const sockaddr_in* a = reinterpret_cast<const sockaddr_in*>(p->ai_addr);
		if (inet_ntop(AF_INET, &a->sin_addr, buf, static_cast<size_t>(bufSize)))
		{
			ok = true;
			break;
		}
	}

	freeaddrinfo(list);
	return ok;
}
