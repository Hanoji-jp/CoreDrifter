#pragma once

#include "../Const/NetConst.h"

//==========================================================
// HjNetAddress
//   通信相手を表す最小限の住所。
//   winsockの型をそのまま持ち回すと、通信の実装が上位へ漏れる。
//
//   ■ 繋ぎ方によって「住所」の形が違う
//   直接IPなら IPアドレスとポート。Steam経由なら相手のアカウント番号で、
//   IPは出てこない(Valveのサーバーが間を取り持つため、こちらからは見えない)。
//   どちらも入る形にしておかないと、上の層が繋ぎ方を知ってしまう。
//
//   使っていないほうは0のまま。どちらで繋いでいるかは
//   通信の口(HjNetTransport)だけが知っていればよい。
//==========================================================
struct HjNetAddress
{
	//--- 直接IPで繋ぐとき ---
	unsigned int   ip   = 0;   // IPv4(ネットワークバイト順)
	unsigned short port = 0;

	//--- Steam経由で繋ぐとき ---
	// 相手のアカウントを表す64bitの番号(SteamID)
	unsigned long long userId = 0;

	// どちらかが埋まっていれば宛先として使える
	bool IsValid() const { return port != 0 || userId != 0; }

	bool operator==(const HjNetAddress& o) const
	{
		return ip == o.ip && port == o.port && userId == o.userId;
	}
	bool operator!=(const HjNetAddress& o) const { return !(*this == o); }
};

//==========================================================
// HjNetTransport
//   「パケットを送る・受け取る」だけを担当する口。
//
//   今は直接IPで繋ぐ(HjUdpTransport)が、あとでマッチングサーバー経由へ
//   差し替えたい。ここを1枚挟んでおけば、上のセッション処理や
//   ルーム画面は手を入れずに済む。
//
//   どの実装も「届かないことがある・順番が入れ替わる」前提。
//   到達を保証しないかわりに遅延が小さいのがUDPで、
//   位置の同期のように「次が来れば古いものは要らない」用途に向く。
//==========================================================
class HjNetTransport
{
public:
	virtual ~HjNetTransport() = default;

	// 受信用の口を開く。port=0 なら空いているポートを任せる(参加側)
	virtual bool Open(unsigned short port) = 0;
	virtual void Close() = 0;
	virtual bool IsOpen() const = 0;

	// 送る。届く保証は無い
	virtual bool Send(const void* data, int size, const HjNetAddress& to) = 0;
	// 受け取る。戻り値=受信バイト数(0=今は何も来ていない)
	virtual int  Receive(void* buf, int maxSize, HjNetAddress& from) = 0;

	// 文字列から宛先を作る。
	// 直接IPなら "192.168.0.5"、Steam経由なら相手のアカウント番号の文字列。
	// どちらを渡すかは、どの実装を使っているかで決まる
	virtual bool Resolve(const char* address, unsigned short port, HjNetAddress& out) = 0;
	// 住所を "192.168.0.5:50721" の形へ。画面表示用
	virtual void ToString(const HjNetAddress& addr, char* buf, int bufSize) const = 0;
};
