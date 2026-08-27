#pragma once

#include "HjNetProtocol.h"
#include "HjNetTransport.h"

//==========================================================
// HjNetSession
//   P2Pの対戦セッション。誰が参加していて、誰へ何を送るかを持つ。
//
//   ■ ホストは「名簿係」でしかない
//   参加要求を受けて番号を振り、全員へ名簿を配る。それだけ。
//   車の状態は名簿を見て参加者同士が直接送り合う。
//   ホストを中継すると往復ぶん遅れるうえ、人数が増えるほど
//   ホストだけ帯域を食う。P2Pにしているのはそのため。
//
//   ■ 通信の口は差し替えられる
//   HjNetTransport を持つだけにしてあるので、あとで
//   マッチングサーバー経由の実装へ差し替えても、ここから上は変わらない。
//
//   ■ 状態は貯めて渡す
//   受け取った車の状態はキューへ積み、シーン側が取り出して
//   それぞれの車へ配る。ここで直接シーンを触ると、
//   通信とゲームの都合が混ざって追えなくなる。
//
//   使い方(シーン側):
//     StartHost(名前) / StartJoin("192.168.0.5", 名前)
//     毎フレーム:
//       SetLocalState(...)          自分の車の状態を渡す
//       Update(dt)                  送受信
//       while (PopState(out)) {...} 届いた状態を配る
//==========================================================
class HjNetSession
{
public:
	// シーンをまたいで生き続ける。ルーム画面で繋いで、走行画面でも使うため
	static HjNetSession& Instance()
	{
		static HjNetSession inst;
		return inst;
	}

	// どうやって相手と繋ぐか。
	//   DirectIp … 相手のIPを直接指定する。同じLANならすぐ繋がるが、
	//              インターネット越しはルーターの設定(ポート開放)が要る
	//   Steam    … Valveのサーバーが間を取り持つ。設定なしで繋がり、
	//              互いのIPも見えない。Steamクライアントの起動が要る
	enum class Link
	{
		DirectIp,
		Steam,
	};

	// 今どういう状態か。画面の出し分けに使う
	enum class Mode
	{
		Offline,    // 繋いでいない
		Hosting,    // ホストとして待ち受け中
		Joining,    // 参加要求を送っている最中(まだ受理されていない)
		Connected,  // 参加者として繋がっている
	};

	//===== 開始・終了 =====
	// 繋ぎ方を選ぶ。繋いでいる最中は変えられない(先に Leave すること)
	void SetLink(Link link);
	Link GetLink() const { return m_link; }

	// ホストとして待ち受ける
	// myColor = 見分け用の色(アウトラインと煙に使われる)
	bool StartHost(const char* myName, const Math::Vector3& myColor);
	// 相手のIPへ参加要求を送る
	bool StartJoin(const char* address, const char* myName, const Math::Vector3& myColor);
	// 退出を伝えてから閉じる
	void Leave();

	Mode GetMode()      const { return m_mode; }
	bool IsActive()     const { return m_mode != Mode::Offline; }
	int  GetMyId()      const { return m_myId; }
	// 自分を含む参加人数
	int  GetPlayerCount() const;
	// 表示用。ホストが参加者へ伝えるアドレス("192.168.0.5:50721")
	const char* GetHostAddressText() const { return m_hostAddrText; }
	// 直近のエラー文(接続に失敗したときの表示用)。空なら問題なし
	const char* GetLastError() const { return m_lastError; }

	//===== 毎フレーム =====
	// 自分の車の状態を渡す。次の送信タイミングでそのまま送られる
	void SetLocalState(const HjCarSyncState& state);

	// 送受信。参加要求の再送とタイムアウトの面倒もここで見る
	void Update(float dt);

	// 届いた車の状態を1つ取り出す。false=もう無い
	bool PopState(HjNetStatePacket& out);

	// 参加者の情報(自分を除く)。車を並べるのに使う
	struct PeerView
	{
		int         id   = -1;
		std::string name;
		bool        alive = false;   // タイムアウトしていないか
	};
	// 生きている相手の一覧を作って返す
	std::vector<PeerView> BuildPeerList() const;

	// 番号から名前を引く。一覧を作らずに済むので、毎フレーム呼んでよい
	const char* GetPeerName(int id) const;
	// 番号から見分け用の色を引く
	Math::Vector3 GetPeerColor(int id) const;

	// 相手が抜けた/タイムアウトした番号を取り出す。false=もう無い
	bool PopRemovedId(int& outId);

	// 状態パネル(Hierarchy/Inspector へ出す)
	void DrawImGui();

private:
	HjNetSession() = default;
	~HjNetSession() { Leave(); }
	HjNetSession(const HjNetSession&) = delete;
	void operator=(const HjNetSession&) = delete;

	//===== 参加者1人ぶん =====
	struct Peer
	{
		HjNetAddress addr;
		std::string  name;
		int          id       = -1;
		bool         used     = false;
		Math::Vector3 color   = Math::Vector3(1.0f, 1.0f, 1.0f);   // 見分け用
		float        silence  = 0.0f;   // 最後に何か届いてからの秒数
		unsigned int lastSeq  = 0;      // 受け取った中で一番新しい連番
	};

	//===== 受信の振り分け =====
	void ReceiveAll(float dt);
	void HandleJoin(const HjNetJoinPacket& pkt, const HjNetAddress& from);
	void HandleRoster(const HjNetRosterPacket& pkt);
	void HandleState(const HjNetStatePacket& pkt, const HjNetAddress& from);
	void HandleLeave(const HjNetLeavePacket& pkt);

	//===== 送信 =====
	void SendState();                                  // 自分の状態を全員へ
	void SendRoster();                                 // ホストが名簿を全員へ
	void SendJoinRequest();                            // 参加要求(届くまで繰り返す)
	void SendToAllPeers(const void* data, int size);   // 自分以外の全員へ

	//===== 名簿の操作 =====
	int  FindPeerByAddr(const HjNetAddress& addr) const;
	int  FindPeerById(int id) const;
	int  AcquireFreeSlot();          // 空き枠を取って番号を振る。-1=満員
	void DropPeer(int index);        // 抜けた相手を消して、番号を回収キューへ
	void ClearPeers();
	void UpdateTimeouts(float dt);

	// 選んだ繋ぎ方に応じた通信の口を作る
	std::unique_ptr<HjNetTransport> MakeTransport() const;

	// 通信の口。直接IP版とSteam版のどちらかが入る
	std::unique_ptr<HjNetTransport> m_transport;
	Link m_link = Link::DirectIp;

	Mode m_mode = Mode::Offline;
	int  m_myId = -1;
	std::string   m_myName;
	Math::Vector3 m_myColor = Math::Vector3(1.0f, 1.0f, 1.0f);

	// 参加者(自分は含めない)
	std::vector<Peer> m_peers;
	// 参加側が覚えておくホストの住所
	HjNetAddress m_hostAddr;

	// 送信の間隔をためる。毎フレーム送ると帯域を食うので回数を絞る
	float m_sendTimer = 0.0f;
	// 参加要求の再送タイマー
	float m_joinTimer = 0.0f;
	// 送る連番。受け取り側が古いものを捨てる判断に使う
	unsigned int m_seq = 0;

	// 次に送る自分の状態。SetLocalStateで書き換えられる
	HjNetStatePacket m_localState;
	bool m_hasLocalState = false;

	// 届いた状態の置き場。シーンが取り出すまで貯めておく
	std::vector<HjNetStatePacket> m_inbox;
	// 抜けた相手の番号。シーンが車を消すために取り出す
	std::vector<int> m_removed;

	char m_hostAddrText[64] = {};
	char m_lastError[128]   = {};
};
