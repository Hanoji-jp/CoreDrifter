#include "HjSteamTransport.h"

//==========================================================
// Steamworks SDK が置かれているときだけ中身を有効にする。
//
// SDK は各自でダウンロードして置く必要がある(ログインが要るため
// リポジトリには入れられない)。置かれていない環境でビルドが
// 壊れないよう、有無で切り替える。
//
// 置き場所:  Library/steamworks_sdk/public/steam/steam_api.h
// 有効化  :  プロジェクトのプリプロセッサ定義に HJ_USE_STEAM を足す
//==========================================================
#if defined(HJ_USE_STEAM)

#include "steam/steam_api.h"
#include "steam/isteamnetworkingmessages.h"

namespace
{
	// 通信の系統。用途ごとに分けられるが、車の状態しか流さないので1本でよい
	constexpr int kChannel = 0;

	//------------------------------------------------------
	// 繋ぎに来た相手を受け入れる係。
	//
	// Steamの「相手を指定して投げるだけ」の通信は、初回だけ
	// 受け取り側の許可が要る。許可しないと最初の1通で止まる。
	// 誰でも受け入れているのは、部屋に入れるかどうかを
	// セッション側(名簿)で判断しているため。
	//------------------------------------------------------
	class SessionAccepter
	{
	public:
		STEAM_CALLBACK(SessionAccepter, OnSessionRequest,
		               SteamNetworkingMessagesSessionRequest_t);
	};

	void SessionAccepter::OnSessionRequest(SteamNetworkingMessagesSessionRequest_t* p)
	{
		if (!p) { return; }
		SteamNetworkingMessages()->AcceptSessionWithUser(p->m_identityRemote);
	}

	// 生存期間をこの翻訳単位に閉じる。
	// 通知を受け取るには生きたインスタンスが要るだけで、外から触る用は無い
	std::unique_ptr<SessionAccepter> g_accepter;

	// 初期化の結果を覚えておく。
	// 成功したら二度目以降は何もしない。失敗は覚えない
	// (Steamをあとから起動する場合があるので、次に聞かれたらやり直す)。
	bool g_ready = false;
	char g_error[256] = "";

	//------------------------------------------------------
	// Steamを使える状態にする。何度呼んでもよい。
	//------------------------------------------------------
	bool EnsureInit()
	{
		if (g_ready) { return true; }

		// 失敗の理由まで受け取れる方を使う。
		// 単に真偽だけ返す方だと、何を直せばよいのか分からない
		SteamErrMsg msg = {};
		const ESteamAPIInitResult r = SteamAPI_InitEx(&msg);

		if (r != k_ESteamAPIInitResult_OK)
		{
			switch (r)
			{
			case k_ESteamAPIInitResult_NoSteamClient:
				snprintf(g_error, sizeof(g_error), "Steamが起動していません");
				break;
			case k_ESteamAPIInitResult_VersionMismatch:
				snprintf(g_error, sizeof(g_error), "Steamクライアントが古いようです");
				break;
			default:
				snprintf(g_error, sizeof(g_error), "%s", msg);
				break;
			}
			return false;
		}

		// 通信の機能が取り出せるか。
		// 初期化に成功していても、ここが取れなければ送受信はできない
		if (!SteamNetworkingMessages())
		{
			snprintf(g_error, sizeof(g_error),
			         "Steamの通信機能を取得できませんでした");
			return false;
		}

		// 繋ぎに来た相手を受け入れられるようにしておく
		if (!g_accepter) { g_accepter = std::make_unique<SessionAccepter>(); }

		g_error[0] = '\0';
		g_ready = true;
		return true;
	}

	// HjNetAddress の userId から、Steamが使う相手の表現へ直す
	SteamNetworkingIdentity MakeIdentity(unsigned long long userId)
	{
		SteamNetworkingIdentity id;
		id.Clear();
		id.SetSteamID64(userId);
		return id;
	}
}

bool HjSteamTransport::IsAvailable()
{
	return EnsureInit();
}

const char* HjSteamTransport::GetInitError()
{
	return g_error;
}

bool HjSteamTransport::GetLocalUserId(char* buf, int bufSize)
{
	if (!buf || bufSize <= 0) { return false; }
	if (!SteamUser()) { return false; }

	const unsigned long long id = SteamUser()->GetSteamID().ConvertToUint64();
	snprintf(buf, bufSize, "%llu", id);
	return true;
}

void HjSteamTransport::RunCallbacks()
{
	if (!g_ready) { return; }
	SteamAPI_RunCallbacks();
}

bool HjSteamTransport::Open(unsigned short port)
{
	(void)port;   // Steamが経路を決めるので、こちらではポートを持たない

	if (!EnsureInit()) { return false; }

	m_open = true;
	return true;
}

void HjSteamTransport::Close()
{
	if (!m_open) { return; }
	m_open = false;

	// 相手との経路を畳む。
	// 残したままにすると、次に開いたとき古い経路が生きていて
	// 前回の相手から届き続けることがある
	// (誰と繋いでいたかはセッション側が持っているので、ここでは何もしない)
}

bool HjSteamTransport::Send(const void* data, int size, const HjNetAddress& to)
{
	if (!m_open || !data || size <= 0 || to.userId == 0) { return false; }
	if (!SteamNetworkingMessages()) { return false; }

	const SteamNetworkingIdentity id = MakeIdentity(to.userId);

	// 位置の同期は「次が来れば古いものは要らない」ので、届く保証は要らない。
	// 保証を付けると、届かなかった1つを送り直している間、
	// 後ろが待たされて遅延が伸びる
	const EResult r = SteamNetworkingMessages()->SendMessageToUser(
		id, data, static_cast<uint32>(size),
		k_nSteamNetworkingSend_Unreliable, kChannel);

	return (r == k_EResultOK);
}

int HjSteamTransport::Receive(void* buf, int maxSize, HjNetAddress& from)
{
	if (!m_open || !buf || maxSize <= 0) { return 0; }
	if (!SteamNetworkingMessages()) { return 0; }

	SteamNetworkingMessage_t* msg = nullptr;
	const int got = SteamNetworkingMessages()->ReceiveMessagesOnChannel(kChannel, &msg, 1);
	if (got <= 0 || !msg) { return 0; }

	const int size = static_cast<int>(msg->m_cbSize);
	const int copy = (size < maxSize) ? size : maxSize;
	memcpy(buf, msg->m_pData, copy);

	from = HjNetAddress();
	from.userId = msg->m_identityPeer.GetSteamID64();

	// 受け取ったものは自分で解放する。放っておくと積み上がる
	msg->Release();
	return copy;
}

bool HjSteamTransport::Resolve(const char* address, unsigned short port, HjNetAddress& out)
{
	(void)port;
	if (!address || !*address) { return false; }

	// Steamの住所はアカウント番号そのもの。数字の文字列として受け取る
	char* end = nullptr;
	const unsigned long long id = strtoull(address, &end, 10);
	if (id == 0) { return false; }

	out = HjNetAddress();
	out.userId = id;
	return true;
}

void HjSteamTransport::ToString(const HjNetAddress& addr, char* buf, int bufSize) const
{
	if (!buf || bufSize <= 0) { return; }
	snprintf(buf, bufSize, "%llu", addr.userId);
}

#else   //===== SDKが無いとき =====

// SDKが置かれていない環境でもビルドが通るようにする。
// ここが呼ばれるのは、UIから「Steamで繋ぐ」を選んだときだけ。
// Open() が false を返すので、繋がらない理由として画面に出る。

bool HjSteamTransport::IsAvailable() { return false; }

const char* HjSteamTransport::GetInitError()
{
	return "Steamworks SDK が組み込まれていません";
}

bool HjSteamTransport::GetLocalUserId(char* buf, int bufSize)
{
	if (buf && bufSize > 0) { buf[0] = '\0'; }
	return false;
}

void HjSteamTransport::RunCallbacks() {}

bool HjSteamTransport::Open(unsigned short port)
{
	(void)port;
	return false;
}

void HjSteamTransport::Close() { m_open = false; }

bool HjSteamTransport::Send(const void*, int, const HjNetAddress&) { return false; }
int  HjSteamTransport::Receive(void*, int, HjNetAddress&) { return 0; }

bool HjSteamTransport::Resolve(const char*, unsigned short, HjNetAddress&) { return false; }

void HjSteamTransport::ToString(const HjNetAddress&, char* buf, int bufSize) const
{
	if (buf && bufSize > 0) { buf[0] = '\0'; }
}

#endif
