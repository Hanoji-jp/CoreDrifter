#include "HjSteamLobby.h"
#include "HjSteamTransport.h"
#include "../Const/NetConst.h"

#if defined(HJ_USE_STEAM)

#include "steam/steam_api.h"

namespace
{
	// 部屋に付ける目印。
	//
	// 開発中は Valve のサンプル用アプリ番号を借りているので、
	// 一覧をそのまま取ると世界中の無関係な部屋が出てくる。
	// 自分の部屋にだけこの印を付け、検索でも同じ印を条件にすることで、
	// このゲームの部屋だけが並ぶ。
	constexpr const char* kTagKey   = "hj_game";
	constexpr const char* kTagValue = "drift_touge";
	// 部屋の名前を入れておく場所
	constexpr const char* kNameKey  = "hj_name";
}

//==========================================================
// Steamの通知を受け取る実体。
//
// Steamは「頼んだ結果があとで返ってくる」形なので、
// 返事を受け取る窓口を持ち続ける必要がある。
// その窓口ごとここへ隔離して、ヘッダーへは出さない。
//==========================================================
class HjSteamLobbyImpl
{
public:
	HjSteamLobby::State state = HjSteamLobby::State::Idle;

	CSteamID lobby;          // 今いる部屋
	CSteamID owner;          // 部屋主
	bool     joinedFlag = false;   // 入った直後に1回だけ立てる
	char     error[192] = "";

	std::vector<HjSteamLobby::Entry> list;

	//===== 頼んだ結果の受け取り口 =====
	CCallResult<HjSteamLobbyImpl, LobbyCreated_t>   createResult;
	CCallResult<HjSteamLobbyImpl, LobbyMatchList_t> listResult;
	CCallResult<HjSteamLobbyImpl, LobbyEnter_t>     enterResult;

	void OnCreated(LobbyCreated_t* p, bool ioFailure);
	void OnList(LobbyMatchList_t* p, bool ioFailure);
	void OnEntered(LobbyEnter_t* p, bool ioFailure);

	//===== 向こうから勝手に来る通知 =====
	// フレンドの招待を承諾したとき。相手のSteamが部屋番号を渡してくる
	STEAM_CALLBACK(HjSteamLobbyImpl, OnJoinRequested, GameLobbyJoinRequested_t);
	// 部屋の出入りがあったとき。部屋主が抜けると別の人へ移るので見張る
	STEAM_CALLBACK(HjSteamLobbyImpl, OnChatUpdate, LobbyChatUpdate_t);

	// 部屋に入れたときの共通処理
	void EnterLobby(CSteamID id);
};

void HjSteamLobbyImpl::EnterLobby(CSteamID id)
{
	lobby = id;
	owner = SteamMatchmaking()->GetLobbyOwner(id);
	state = HjSteamLobby::State::InLobby;
	joinedFlag = true;
	error[0] = '\0';
}

void HjSteamLobbyImpl::OnCreated(LobbyCreated_t* p, bool ioFailure)
{
	if (ioFailure || !p || p->m_eResult != k_EResultOK)
	{
		snprintf(error, sizeof(error), "部屋を作れませんでした");
		state = HjSteamLobby::State::Idle;
		return;
	}

	const CSteamID id(p->m_ulSteamIDLobby);

	// 自分の部屋だと分かる印を付ける。
	// これが無いと、検索で他のゲームの部屋まで拾ってしまう
	SteamMatchmaking()->SetLobbyData(id, kTagKey, kTagValue);

	EnterLobby(id);
}

void HjSteamLobbyImpl::OnList(LobbyMatchList_t* p, bool ioFailure)
{
	list.clear();

	if (ioFailure || !p)
	{
		snprintf(error, sizeof(error), "一覧を取れませんでした");
		state = HjSteamLobby::State::Idle;
		return;
	}

	const int n = static_cast<int>(p->m_nLobbiesMatching);
	for (int i = 0; i < n; ++i)
	{
		const CSteamID id = SteamMatchmaking()->GetLobbyByIndex(i);

		HjSteamLobby::Entry e;
		e.id         = id.ConvertToUint64();
		e.name       = SteamMatchmaking()->GetLobbyData(id, kNameKey);
		e.members    = SteamMatchmaking()->GetNumLobbyMembers(id);
		e.maxMembers = SteamMatchmaking()->GetLobbyMemberLimit(id);
		if (e.name.empty()) { e.name = "(名前なし)"; }

		list.push_back(e);
	}

	state = HjSteamLobby::State::Idle;
	error[0] = '\0';
}

void HjSteamLobbyImpl::OnEntered(LobbyEnter_t* p, bool ioFailure)
{
	if (ioFailure || !p ||
	    p->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess)
	{
		snprintf(error, sizeof(error), "部屋に入れませんでした");
		state = HjSteamLobby::State::Idle;
		return;
	}

	EnterLobby(CSteamID(p->m_ulSteamIDLobby));
}

void HjSteamLobbyImpl::OnJoinRequested(GameLobbyJoinRequested_t* p)
{
	// フレンドの招待を承諾した。相手が入っている部屋番号が渡ってくるので、
	// そのまま入りに行く
	if (!p) { return; }

	state = HjSteamLobby::State::Joining;
	SteamAPICall_t call = SteamMatchmaking()->JoinLobby(p->m_steamIDLobby);
	enterResult.Set(call, this, &HjSteamLobbyImpl::OnEntered);
}

void HjSteamLobbyImpl::OnChatUpdate(LobbyChatUpdate_t* p)
{
	if (!p || !lobby.IsValid()) { return; }

	// 部屋主が抜けるとSteamが別の人へ移す。
	// 通信の接続先が変わるので、覚え直しておく
	owner = SteamMatchmaking()->GetLobbyOwner(lobby);
}

//==========================================================
// 表の顔
//==========================================================

HjSteamLobby::HjSteamLobby()  = default;
HjSteamLobby::~HjSteamLobby() = default;

bool HjSteamLobby::Create(const char* lobbyName, int maxMembers)
{
	// Steamが使える状態か(初期化もここで済む)
	if (!HjSteamTransport::IsAvailable()) { return false; }
	if (!m_impl) { m_impl = std::make_unique<HjSteamLobbyImpl>(); }
	if (!SteamMatchmaking()) { return false; }

	Leave();   // 二重に入らない

	const int limit = std::clamp(maxMembers, 2, NetConst::MaxPlayers);

	// 誰でも入れる部屋にする。
	// フレンド限定にすると、一覧から入ってもらう遊び方ができない
	SteamAPICall_t call = SteamMatchmaking()->CreateLobby(k_ELobbyTypePublic, limit);

	m_impl->state = State::Creating;
	m_impl->createResult.Set(call, m_impl.get(), &HjSteamLobbyImpl::OnCreated);

	// 名前は部屋ができてから入れる必要があるので覚えておく…のではなく、
	// 作成後の通知で入れる。ここでは呼び出し側の文字列を保持しない
	// (通知まで生きている保証が無い)
	if (lobbyName && *lobbyName)
	{
		// 作成完了後に設定するため、いったん実体側へ預ける
		m_impl->list.clear();
		snprintf(m_impl->error, sizeof(m_impl->error), "%s", "");
		m_pendingName = lobbyName;
	}
	return true;
}

bool HjSteamLobby::RequestList()
{
	if (!HjSteamTransport::IsAvailable()) { return false; }
	if (!m_impl) { m_impl = std::make_unique<HjSteamLobbyImpl>(); }
	if (!SteamMatchmaking()) { return false; }

	// このゲームの部屋だけを条件にする。
	// 開発中はサンプル用のアプリ番号を借りているので、
	// 条件を付けないと無関係な部屋が大量に出てくる
	SteamMatchmaking()->AddRequestLobbyListStringFilter(
		kTagKey, kTagValue, k_ELobbyComparisonEqual);
	SteamMatchmaking()->AddRequestLobbyListDistanceFilter(
		k_ELobbyDistanceFilterWorldwide);

	SteamAPICall_t call = SteamMatchmaking()->RequestLobbyList();

	m_impl->state = State::Searching;
	m_impl->listResult.Set(call, m_impl.get(), &HjSteamLobbyImpl::OnList);
	return true;
}

bool HjSteamLobby::Join(unsigned long long lobbyId)
{
	if (!HjSteamTransport::IsAvailable()) { return false; }
	if (!m_impl) { m_impl = std::make_unique<HjSteamLobbyImpl>(); }
	if (!SteamMatchmaking() || lobbyId == 0) { return false; }

	Leave();

	SteamAPICall_t call = SteamMatchmaking()->JoinLobby(CSteamID(lobbyId));
	m_impl->state = State::Joining;
	m_impl->enterResult.Set(call, m_impl.get(), &HjSteamLobbyImpl::OnEntered);
	return true;
}

void HjSteamLobby::Leave()
{
	if (!m_impl || !SteamMatchmaking()) { return; }
	if (!m_impl->lobby.IsValid()) { return; }

	SteamMatchmaking()->LeaveLobby(m_impl->lobby);
	m_impl->lobby = CSteamID();
	m_impl->owner = CSteamID();
	m_impl->state = State::Idle;
	m_impl->joinedFlag = false;
}

void HjSteamLobby::OpenInviteOverlay()
{
	if (!m_impl || !SteamFriends() || !m_impl->lobby.IsValid()) { return; }
	SteamFriends()->ActivateGameOverlayInviteDialog(m_impl->lobby);
}

bool HjSteamLobby::IsOverlayAvailable() const
{
	// オーバーレイはSteamが起動したプロセスにしか入らない。
	// 入っていなければ、招待画面を呼んでも何も起きない
	return SteamUtils() && SteamUtils()->IsOverlayEnabled();
}

//----------------------------------------------------------
// フレンドを直接招待する。
// オーバーレイを介さないので、開発中でも動く。
//----------------------------------------------------------
bool HjSteamLobby::InviteFriend(unsigned long long userId)
{
	if (!m_impl || !SteamMatchmaking()) { return false; }
	if (!m_impl->lobby.IsValid() || userId == 0) { return false; }

	return SteamMatchmaking()->InviteUserToLobby(m_impl->lobby, CSteamID(userId));
}

//----------------------------------------------------------
// フレンド一覧を取り直す。
// 毎フレーム作ると重いので、押されたときだけ更新する。
//----------------------------------------------------------
void HjSteamLobby::RefreshFriends()
{
	m_friends.clear();
	if (!HjSteamTransport::IsAvailable() || !SteamFriends()) { return; }

	// 自分のアプリ番号。相手が同じものを起動しているかの判定に使う
	const uint32 myApp = SteamUtils() ? SteamUtils()->GetAppID() : 0;

	// k_EFriendFlagImmediate ＝ 通常のフレンド
	const int n = SteamFriends()->GetFriendCount(k_EFriendFlagImmediate);
	for (int i = 0; i < n; ++i)
	{
		const CSteamID id = SteamFriends()->GetFriendByIndex(i, k_EFriendFlagImmediate);

		Friend f;
		f.id     = id.ConvertToUint64();
		f.name   = SteamFriends()->GetFriendPersonaName(id);
		f.online = (SteamFriends()->GetFriendPersonaState(id) != k_EPersonaStateOffline);

		// 同じゲームを起動しているかを見る。
		// 起動していない相手は招待を承諾しても入れないので、区別して出す
		FriendGameInfo_t info = {};
		if (SteamFriends()->GetFriendGamePlayed(id, &info))
		{
			f.inGame = (info.m_gameID.AppID() == myApp);
		}

		m_friends.push_back(f);
	}

	// 起動中→オンライン→それ以外の順に並べる。
	// 招待して意味があるのは起動中の相手だけなので、上に来ると探しやすい
	std::sort(m_friends.begin(), m_friends.end(),
	          [](const Friend& a, const Friend& b)
	          {
	              if (a.inGame != b.inGame) { return a.inGame; }
	              if (a.online != b.online) { return a.online; }
	              return a.name < b.name;
	          });
}

const std::vector<HjSteamLobby::Friend>& HjSteamLobby::GetFriends() const
{
	return m_friends;
}

HjSteamLobby::State HjSteamLobby::GetState() const
{
	return m_impl ? m_impl->state : State::Idle;
}

bool HjSteamLobby::IsInLobby() const
{
	return m_impl && m_impl->lobby.IsValid();
}

unsigned long long HjSteamLobby::GetLobbyId() const
{
	return m_impl ? m_impl->lobby.ConvertToUint64() : 0;
}

unsigned long long HjSteamLobby::GetOwnerId() const
{
	return m_impl ? m_impl->owner.ConvertToUint64() : 0;
}

bool HjSteamLobby::IsOwner() const
{
	if (!m_impl || !SteamUser()) { return false; }
	return m_impl->owner == SteamUser()->GetSteamID();
}

int HjSteamLobby::GetMemberCount() const
{
	if (!m_impl || !SteamMatchmaking() || !m_impl->lobby.IsValid()) { return 0; }
	return SteamMatchmaking()->GetNumLobbyMembers(m_impl->lobby);
}

const char* HjSteamLobby::GetLastError() const
{
	return m_impl ? m_impl->error : "";
}

const std::vector<HjSteamLobby::Entry>& HjSteamLobby::GetList() const
{
	static const std::vector<Entry> empty;
	return m_impl ? m_impl->list : empty;
}

bool HjSteamLobby::ConsumeJoinedFlag()
{
	if (!m_impl || !m_impl->joinedFlag) { return false; }
	m_impl->joinedFlag = false;
	return true;
}

void HjSteamLobby::Update()
{
	if (!m_impl) { return; }

	// 通知を回す。HjSteamTransport 側でも回しているが、
	// 通信を始める前(部屋を探している間)はそちらが動いていない
	HjSteamTransport::RunCallbacks();

	// 部屋ができた直後に名前を入れる。
	// 作成を頼んだ時点ではまだ部屋が無いので、ここまで待つ必要がある
	if (!m_pendingName.empty() && m_impl->lobby.IsValid() && SteamMatchmaking())
	{
		SteamMatchmaking()->SetLobbyData(m_impl->lobby, kNameKey, m_pendingName.c_str());
		m_pendingName.clear();
	}
}

#else   //===== SDKが無いとき =====

// SDKが置かれていない環境でもビルドが通るようにする。
// すべて「できなかった」を返すだけ。

class HjSteamLobbyImpl {};

HjSteamLobby::HjSteamLobby()  = default;
HjSteamLobby::~HjSteamLobby() = default;

bool HjSteamLobby::Create(const char*, int) { return false; }
bool HjSteamLobby::RequestList()            { return false; }
bool HjSteamLobby::Join(unsigned long long) { return false; }
void HjSteamLobby::Leave() {}
void HjSteamLobby::OpenInviteOverlay() {}
bool HjSteamLobby::IsOverlayAvailable() const { return false; }
bool HjSteamLobby::InviteFriend(unsigned long long) { return false; }
void HjSteamLobby::RefreshFriends() { m_friends.clear(); }

const std::vector<HjSteamLobby::Friend>& HjSteamLobby::GetFriends() const
{
	return m_friends;
}

HjSteamLobby::State HjSteamLobby::GetState() const { return State::Idle; }
bool HjSteamLobby::IsInLobby() const { return false; }
unsigned long long HjSteamLobby::GetLobbyId() const { return 0; }
unsigned long long HjSteamLobby::GetOwnerId() const { return 0; }
bool HjSteamLobby::IsOwner() const { return false; }
int  HjSteamLobby::GetMemberCount() const { return 0; }
const char* HjSteamLobby::GetLastError() const { return "Steamworks SDK が組み込まれていません"; }

const std::vector<HjSteamLobby::Entry>& HjSteamLobby::GetList() const
{
	static const std::vector<Entry> empty;
	return empty;
}

bool HjSteamLobby::ConsumeJoinedFlag() { return false; }
void HjSteamLobby::Update() {}

#endif

//==========================================================
// 状態パネル(SDKの有無に関係なく出す)
//==========================================================
void HjSteamLobby::DrawImGui()
{
	if (!ImGui::CollapsingHeader(U8("ロビー(Steam)"))) { return; }

	const char* stateText = U8("待機");
	switch (GetState())
	{
	case State::Creating:  stateText = U8("部屋を作成中"); break;
	case State::Searching: stateText = U8("一覧を取得中"); break;
	case State::Joining:   stateText = U8("入室中");       break;
	case State::InLobby:   stateText = U8("入室済み");     break;
	default: break;
	}
	ImGui::Text(U8("状態: %s"), stateText);

	if (GetLastError()[0])
	{
		ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "%s", GetLastError());
	}

	// 部屋の名前。画面の下書き用なのでここだけで持てば足りる
	static char s_roomName[64] = "TOUGE ROOM";

	if (!IsInLobby())
	{
		ImGui::InputText(U8("部屋の名前"), s_roomName, sizeof(s_roomName));
		if (ImGui::Button(U8("部屋を作る"))) { Create(s_roomName, NetConst::MaxPlayers); }
		ImGui::SameLine();
		if (ImGui::Button(U8("一覧を更新"))) { RequestList(); }

		ImGui::Separator();
		ImGui::Text(U8("見つかった部屋"));

		const auto& list = GetList();
		if (list.empty())
		{
			ImGui::TextDisabled(U8("(なし)"));
		}
		for (const Entry& e : list)
		{
			ImGui::PushID(static_cast<int>(e.id));
			if (ImGui::Button(U8("入る"))) { Join(e.id); }
			ImGui::SameLine();
			ImGui::Text(U8("%s  %d/%d"), e.name.c_str(), e.members, e.maxMembers);
			ImGui::PopID();
		}
	}
	else
	{
		ImGui::Text(U8("部屋番号: %llu"), GetLobbyId());
		ImGui::Text(U8("部屋主: %llu %s"), GetOwnerId(),
		            IsOwner() ? U8("(自分)") : "");
		ImGui::Text(U8("人数: %d"), GetMemberCount());

		if (ImGui::Button(U8("部屋を出る"))) { Leave(); }

		ImGui::Separator();
		ImGui::Text(U8("フレンドを招待"));

		// オーバーレイ経由。Steamが起動したプロセスでないと出ない
		if (IsOverlayAvailable())
		{
			if (ImGui::Button(U8("Steamの招待画面を開く"))) { OpenInviteOverlay(); }
		}
		else
		{
			ImGui::TextDisabled(
				U8("Steamの招待画面は使えません(Steamから起動したときだけ出ます)"));
		}

		// こちらはオーバーレイを介さないので開発中でも動く
		if (ImGui::Button(U8("フレンド一覧を更新"))) { RefreshFriends(); }

		const auto& friends = GetFriends();
		if (friends.empty())
		{
			ImGui::TextDisabled(U8("(更新を押してください)"));
		}

		// 送った結果をここへ出す。
		// 押しても画面が変わらないと、送れたのか分からない
		static char s_inviteMsg[128] = "";

		for (const Friend& f : friends)
		{
			ImGui::PushID(static_cast<int>(f.id));

			// 誰にでも送れるようにしておく。
			// 相手がこのゲームを起動していないと承諾しても入れないが、
			// 押せないようにすると、送れるかどうかの確認すらできない。
			// 起動しているかは下の表示で区別する。
			if (ImGui::Button(U8("招待")))
			{
				const bool sent = InviteFriend(f.id);
				if (!sent)
				{
					snprintf(s_inviteMsg, sizeof(s_inviteMsg),
					         U8("送れませんでした: %s"), f.name.c_str());
				}
				else if (f.inGame)
				{
					snprintf(s_inviteMsg, sizeof(s_inviteMsg),
					         U8("招待を送りました: %s"), f.name.c_str());
				}
				else
				{
					snprintf(s_inviteMsg, sizeof(s_inviteMsg),
					         U8("送りました: %s (このゲームを起動していないので、承諾しても入れません)"),
					         f.name.c_str());
				}
			}

			ImGui::SameLine();
			if (f.inGame)      { ImGui::TextColored(ImVec4(0.4f, 1, 0.5f, 1), U8("%s  ゲーム中"), f.name.c_str()); }
			else if (f.online) { ImGui::Text(U8("%s  オンライン"), f.name.c_str()); }
			else               { ImGui::TextDisabled(U8("%s  オフライン"), f.name.c_str()); }

			ImGui::PopID();
		}

		if (s_inviteMsg[0]) { ImGui::TextWrapped("%s", s_inviteMsg); }
	}
}
