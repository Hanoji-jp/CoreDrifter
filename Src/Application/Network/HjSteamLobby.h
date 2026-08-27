#pragma once

//==========================================================
// HjSteamLobby
//   Steamのロビー(部屋)。部屋を作る・探す・入る、を担当する。
//
//   ■ 通信そのものは持たない
//   ロビーは「誰がどの部屋にいるか」を管理するだけで、
//   車の位置はここを通らない。部屋に入ってホストの番号が分かったら、
//   あとは HjNetSession が直接やりとりする。
//   分けておくと、ロビーをやめて直接IPに戻しても通信側は変わらない。
//
//   ■ これがあると番号を渡さなくて済む
//   直接繋ぐには17桁のアカウント番号を相手へ伝える必要があるが、
//   部屋の一覧から選べるならその手間が消える。
//
//   ■ 招待ダイアログについて
//   フレンドへの招待画面はSteamのオーバーレイが出す。
//   オーバーレイは「Steamが起動したプロセス」にしか出ないので、
//   開発中(Visual Studioから実行)は開かない。
//   呼び出し自体は用意してあるので、自分のappidを取れば動く。
//
//   ■ SDKが無くてもビルドは通る
//   HjSteamTransport と同じで、SDKが無いときは何もしない実装になる。
//==========================================================

// Steamの型をヘッダーへ持ち込まないための実体(cpp側で定義する)
class HjSteamLobbyImpl;

class HjSteamLobby
{
public:
	static HjSteamLobby& Instance()
	{
		static HjSteamLobby inst;
		return inst;
	}

	// 今なにをしている最中か。画面の出し分けに使う
	enum class State
	{
		Idle,       // 何もしていない
		Creating,   // 部屋を作っている最中
		Searching,  // 一覧を取り直している最中
		Joining,    // 部屋に入ろうとしている最中
		InLobby,    // 部屋にいる
	};

	// 一覧に並ぶ部屋1つぶん
	struct Entry
	{
		unsigned long long id = 0;
		std::string        name;
		int                members    = 0;
		int                maxMembers = 0;
	};

	//===== 操作 =====
	// 部屋を作る。作れたら自分が部屋主になる
	bool Create(const char* lobbyName, int maxMembers);
	// 一覧を取り直す。結果は少し遅れて届く
	bool RequestList();
	// 部屋に入る
	bool Join(unsigned long long lobbyId);
	// 部屋から出る
	void Leave();

	// フレンドへの招待画面を開く。
	//
	// これはSteamのオーバーレイが出す画面で、
	// 「Steamが起動したプロセス」にしか出ない。
	// Visual Studioやexe直叩きでは何も起きないので、
	// 開発中は下の InviteFriend を使うこと。
	void OpenInviteOverlay();

	// オーバーレイが使える状態か。押しても何も起きない理由の切り分けに使う
	bool IsOverlayAvailable() const;

	// フレンドを直接招待する。
	//
	// オーバーレイを通さずSteamへ直接頼むので、開発中でも動く。
	// 相手には通常のSteam通知が届き、承諾すると相手のゲームへ
	// 部屋番号が飛ぶ(そちらは GameLobbyJoinRequested_t で受けている)。
	//
	// ※相手がこのゲームを起動していないと、承諾しても入れない。
	//   借りているアプリ番号ではSteamがこのゲームを起動できないため。
	bool InviteFriend(unsigned long long userId);

	// フレンド1人ぶん
	struct Friend
	{
		unsigned long long id = 0;
		std::string        name;
		bool               online = false;
		bool               inGame = false;   // このゲームを起動中か
	};
	// フレンド一覧を取り直す
	void RefreshFriends();
	const std::vector<Friend>& GetFriends() const;

	//===== 状態 =====
	State GetState() const;
	bool  IsInLobby() const;
	// 今いる部屋の番号
	unsigned long long GetLobbyId() const;
	// 部屋主のアカウント番号。通信の接続先になる
	unsigned long long GetOwnerId() const;
	bool  IsOwner() const;
	int   GetMemberCount() const;
	// 直近のエラー文(空なら問題なし)
	const char* GetLastError() const;

	// 見つかった部屋の一覧
	const std::vector<Entry>& GetList() const;

	// 部屋に入った直後に一度だけ true を返す。
	// シーン側が「入れたので通信を始める」判断をするのに使う
	bool ConsumeJoinedFlag();

	//===== 毎フレーム =====
	// Steamからの通知を処理する
	void Update();

	// 状態パネル
	void DrawImGui();

private:
	HjSteamLobby();
	~HjSteamLobby();
	HjSteamLobby(const HjSteamLobby&) = delete;
	void operator=(const HjSteamLobby&) = delete;

	// Steamの型を隠すための実体。SDKが無いときは中身が空になる
	std::unique_ptr<HjSteamLobbyImpl> m_impl;

	// フレンド一覧。毎フレーム作り直すと重いので保持する
	std::vector<Friend> m_friends;

	// 部屋に付ける名前。
	// 作成を頼んだ時点ではまだ部屋が存在せず、名前を入れる先が無い。
	// 部屋ができるまでここで持っておき、できた時点で流し込む
	std::string m_pendingName;
};
