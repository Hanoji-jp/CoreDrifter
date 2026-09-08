#include "HjNetSession.h"
#include "../GameObject/Car/HjCarChoice.h"
#include "HjUdpTransport.h"
#include "HjSteamTransport.h"

namespace
{
	// 色は 0〜1 で扱うが、送るときは1成分1バイトにする。
	// 見分けが目的なので、この粗さで十分
	unsigned char ToByte(float v)
	{
		return static_cast<unsigned char>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
	}
	Math::Vector3 FromBytes(unsigned char r, unsigned char g, unsigned char b)
	{
		return Math::Vector3(r / 255.0f, g / 255.0f, b / 255.0f);
	}
}

//==========================================================
// 開始・終了
//==========================================================

//----------------------------------------------------------
// 自分の車の見た目。シーンが毎フレーム入れる。
//
// 名簿は参加・退出のときしか配られないので、途中で色を変えても
// そのままでは相手へ届かない。変わったことに気づいて送り直す。
//----------------------------------------------------------
void HjNetSession::SetMyLook(const HjCarLook& look)
{
	if (m_myLook == look) { return; }

	m_myLook    = look;
	m_lookDirty = true;
}

void HjNetSession::SetLink(Link link)
{
	// 繋いでいる最中に切り替えると、開いている口と食い違う。
	// 先に Leave してもらう
	if (m_mode != Mode::Offline) { return; }
	m_link = link;
}

//----------------------------------------------------------
// 繋ぎ方に応じた通信の口を作る。
// ここだけが実装を知っていればよく、上の処理は口の形しか見ない。
//----------------------------------------------------------
std::unique_ptr<HjNetTransport> HjNetSession::MakeTransport() const
{
	if (m_link == Link::Steam) { return std::make_unique<HjSteamTransport>(); }
	return std::make_unique<HjUdpTransport>();
}

bool HjNetSession::StartHost(const char* myName)
{
	Leave();   // 二重に開かない

	auto tp = MakeTransport();
	if (!tp->Open(NetConst::DefaultPort))
	{
		if (m_link == Link::Steam)
		{
			snprintf(m_lastError, sizeof(m_lastError),
			         "Steamに繋げませんでした: %s", HjSteamTransport::GetInitError());
		}
		else
		{
			snprintf(m_lastError, sizeof(m_lastError),
			         "ポート %u を開けませんでした", NetConst::DefaultPort);
		}
		return false;
	}

	// 参加者へ伝える住所を作っておく。
	// 自分の住所は自分では分からないので、通信側から取り出す。
	if (m_link == Link::Steam)
	{
		// Steamでは相手のアカウント番号が住所になる
		char uid[64] = {};
		if (HjSteamTransport::GetLocalUserId(uid, sizeof(uid)))
		{
			snprintf(m_hostAddrText, sizeof(m_hostAddrText), "%s", uid);
		}
		else
		{
			snprintf(m_hostAddrText, sizeof(m_hostAddrText), "(不明)");
		}
	}
	else
	{
		char ip[64] = {};
		if (HjUdpTransport::GetLocalAddress(ip, sizeof(ip)))
		{
			snprintf(m_hostAddrText, sizeof(m_hostAddrText), "%s:%u", ip, NetConst::DefaultPort);
		}
		else
		{
			snprintf(m_hostAddrText, sizeof(m_hostAddrText), "(不明):%u", NetConst::DefaultPort);
		}
	}

	m_transport = std::move(tp);
	m_peers.assign(NetConst::MaxPlayers, Peer());
	for (int i = 0; i < NetConst::MaxPlayers; ++i) { m_peers[i].id = i; }

	m_mode = Mode::Hosting;
	m_myId = 0;              // ホストは常に0番
	m_myName  = myName ? myName : "";
	m_seq = 0;
	m_sendTimer = 0.0f;
	m_lastError[0] = '\0';
	return true;
}

bool HjNetSession::StartJoin(const char* address, const char* myName)
{
	Leave();

	auto tp = MakeTransport();
	// 参加側は待ち受ける必要が無いので、空いているポートを任せる。
	// ここで固定のポートを使うと、同じPCで2つ起動したときにぶつかる。
	// (Steamの場合はポートを使わないので、この値は無視される)
	if (!tp->Open(0))
	{
		if (m_link == Link::Steam)
		{
			snprintf(m_lastError, sizeof(m_lastError),
			         "Steamに繋げませんでした: %s", HjSteamTransport::GetInitError());
		}
		else
		{
			snprintf(m_lastError, sizeof(m_lastError), "受信用のポートを開けませんでした");
		}
		return false;
	}

	if (!tp->Resolve(address, NetConst::DefaultPort, m_hostAddr))
	{
		snprintf(m_lastError, sizeof(m_lastError), "接続先が正しくありません: %s",
		         address ? address : "");
		return false;
	}

	m_transport = std::move(tp);
	m_peers.assign(NetConst::MaxPlayers, Peer());
	for (int i = 0; i < NetConst::MaxPlayers; ++i) { m_peers[i].id = i; }

	m_mode = Mode::Joining;   // 受理されるまでは参加中ではない
	m_myId = -1;
	m_myName  = myName ? myName : "";
	m_seq = 0;
	m_sendTimer = 0.0f;
	m_joinTimer = NetConst::JoinRetry;   // すぐ1回送る
	m_transport->ToString(m_hostAddr, m_hostAddrText, sizeof(m_hostAddrText));
	m_lastError[0] = '\0';
	return true;
}

void HjNetSession::Leave()
{
	if (m_transport && m_mode != Mode::Offline && m_myId >= 0)
	{
		// 抜けることを伝える。届かなくてもタイムアウトで消えるので、
		// 1回送るだけにしておく(閉じる処理を待たせない)。
		HjNetLeavePacket pkt;
		pkt.id = static_cast<unsigned char>(m_myId);
		SendToAllPeers(&pkt, sizeof(pkt));
	}

	if (m_transport) { m_transport->Close(); }
	m_transport.reset();

	ClearPeers();
	m_inbox.clear();
	m_removed.clear();
	m_mode = Mode::Offline;
	m_myId = -1;
	m_hasLocalState = false;
	m_hostAddr = HjNetAddress();
}

//==========================================================
// 毎フレーム
//==========================================================

void HjNetSession::SetLocalState(const HjCarSyncState& st)
{
	m_localState.px = st.pos.x;  m_localState.py = st.pos.y;  m_localState.pz = st.pos.z;
	m_localState.vx = st.vel.x;  m_localState.vy = st.vel.y;  m_localState.vz = st.vel.z;
	m_localState.yaw   = st.yaw;
	m_localState.steer = st.steer;

	m_localState.terrainPitch = st.terrainPitch;
	m_localState.terrainRoll  = st.terrainRoll;
	m_localState.bodyPitch    = st.bodyPitch;
	m_localState.bodyRoll     = st.bodyRoll;

	m_localState.slipRear  = ToByte(st.slipRear01);
	m_localState.slipFront = ToByte(st.slipFront01);

	m_localState.flags = 0;
	if (st.handbrake) { m_localState.flags |= HjNetFlag::Handbrake; }
	if (st.onGround)  { m_localState.flags |= HjNetFlag::OnGround; }

	m_hasLocalState = true;
}

void HjNetSession::Update(float dt)
{
	if (!m_transport || m_mode == Mode::Offline) { return; }

	// Steamからの通知を受け取る。
	// これを呼ばないと、相手が繋ぎに来ても気づけず最初の1通で止まる
	if (m_link == Link::Steam) { HjSteamTransport::RunCallbacks(); }

	ReceiveAll(dt);
	UpdateTimeouts(dt);

	// 受理されるまで参加要求を繰り返す。
	// UDPは届かないことがあるので、1回送って待つ作りだと繋がらないことがある。
	if (m_mode == Mode::Joining)
	{
		m_joinTimer += dt;
		if (m_joinTimer >= NetConst::JoinRetry)
		{
			m_joinTimer = 0.0f;
			SendJoinRequest();
		}
		return;   // まだ番号を貰っていないので状態は送れない
	}

	// 状態の送信は回数を絞る。描画のたびに送ると帯域を食うだけで、
	// 受け取り側は補間で埋めるので滑らかさは変わらない。
	// 見た目が変わっていたら伝える。
	// 走行中に色を変えても相手へ届くようにするためのもの。
	// 変わった時だけなので、ほとんど流れない
	if (m_lookDirty)
	{
		SendLook();
		m_sentLook  = m_myLook;
		m_lookDirty = false;
	}

	m_sendTimer += dt;
	const float interval = 1.0f / std::max(NetConst::SendRate, 1.0f);
	if (m_sendTimer >= interval)
	{
		// 0に戻さず引く。
		//
		// 戻すと、超えたぶんの端数を毎回捨てることになり、
		// 実際の送信間隔がフレーム時間に引きずられる。
		// 受け取り側は連番から送信時刻を組み直すので、
		// ここが揺れると相手の車の速さが揺れて見える。
		m_sendTimer -= interval;

		// 大きく遅れたときに、溜まったぶんを一気に送らない。
		// 追いつこうとして連射しても、詰まった帯域が余計に詰まるだけ
		if (m_sendTimer > interval) { m_sendTimer = 0.0f; }

		SendState();
	}
}

bool HjNetSession::PopState(HjNetStatePacket& out)
{
	if (m_inbox.empty()) { return false; }
	out = m_inbox.front();
	m_inbox.erase(m_inbox.begin());
	return true;
}

bool HjNetSession::PopRemovedId(int& outId)
{
	if (m_removed.empty()) { return false; }
	outId = m_removed.front();
	m_removed.erase(m_removed.begin());
	return true;
}

//==========================================================
// 受信
//==========================================================

void HjNetSession::ReceiveAll(float dt)
{
	(void)dt;

	unsigned char buf[NetConst::MaxPacket] = {};
	HjNetAddress from;

	// 溜まっているぶんを取り出す。
	// 1フレームの上限を決めておかないと、大量に届いたときに
	// ここでフレームを使い切ってしまう。
	for (int i = 0; i < NetConst::RecvPerFrame; ++i)
	{
		const int size = m_transport->Receive(buf, sizeof(buf), from);
		if (size <= 0) { break; }

		const HjNetMsg kind = static_cast<HjNetMsg>(buf[0]);
		switch (kind)
		{
		case HjNetMsg::Join:
			// ホスト以外は参加要求を受け付けない
			if (m_mode == Mode::Hosting && size >= static_cast<int>(sizeof(HjNetJoinPacket)))
			{
				HjNetJoinPacket pkt;
				memcpy(&pkt, buf, sizeof(pkt));
				pkt.name[NetConst::MaxNameLen] = '\0';   // 相手が終端を入れ忘れても落ちないように
				HandleJoin(pkt, from);
			}
			break;

		case HjNetMsg::JoinAccept:
		case HjNetMsg::Roster:
			if (size >= static_cast<int>(sizeof(HjNetRosterPacket)))
			{
				HjNetRosterPacket pkt;
				memcpy(&pkt, buf, sizeof(pkt));
				HandleRoster(pkt);
			}
			break;

		case HjNetMsg::State:
			if (size >= static_cast<int>(sizeof(HjNetStatePacket)))
			{
				HjNetStatePacket pkt;
				memcpy(&pkt, buf, sizeof(pkt));
				HandleState(pkt, from);
			}
			break;

		case HjNetMsg::Look:
			if (size >= static_cast<int>(sizeof(HjNetLookPacket)))
			{
				HjNetLookPacket pkt;
				memcpy(&pkt, buf, sizeof(pkt));
				HandleLook(pkt);
			}
			break;

		case HjNetMsg::Leave:
			if (size >= static_cast<int>(sizeof(HjNetLeavePacket)))
			{
				HjNetLeavePacket pkt;
				memcpy(&pkt, buf, sizeof(pkt));
				HandleLeave(pkt);
			}
			break;

		default:
			break;   // 知らない種類は黙って捨てる
		}
	}
}

//----------------------------------------------------------
// 参加要求(ホストのみ)。番号を振って、全員へ名簿を配り直す。
//----------------------------------------------------------
void HjNetSession::HandleJoin(const HjNetJoinPacket& pkt, const HjNetAddress& from)
{
	int index = FindPeerByAddr(from);
	if (index < 0)
	{
		index = AcquireFreeSlot();
		if (index < 0) { return; }   // 満員。返事をしなければ相手は諦める
	}

	// 同じ相手からの再送なら、名前を上書きするだけ。
	// 受理の返事が届かなかった場合、相手は要求を繰り返すので、
	// そのたびに新しい番号を振ると枠を食い潰す。
	m_peers[index].addr    = from;
	m_peers[index].name    = pkt.name;
	m_peers[index].look.outline = FromBytes(pkt.outlineR, pkt.outlineG, pkt.outlineB);
	m_peers[index].look.smokeA = FromBytes(pkt.smokeAR, pkt.smokeAG, pkt.smokeAB);
	m_peers[index].look.smokeB = FromBytes(pkt.smokeBR, pkt.smokeBG, pkt.smokeBB);
	m_peers[index].look.accent = FromBytes(pkt.accentR, pkt.accentG, pkt.accentB);
	m_peers[index].look.neonA = FromBytes(pkt.neonAR, pkt.neonAG, pkt.neonAB);
	m_peers[index].look.neonB = FromBytes(pkt.neonBR, pkt.neonBG, pkt.neonBB);
	m_peers[index].look.smokeHi = FromBytes(pkt.smokeHiR, pkt.smokeHiG, pkt.smokeHiB);
	m_peers[index].look.smokeGradDist = (pkt.smokeGradDist / 255.0f) * HjSmokeGradDistMax;
	m_peers[index].carKind = pkt.carKind;
	m_peers[index].used    = true;
	m_peers[index].silence = 0.0f;

	SendRoster();
}

//----------------------------------------------------------
// 名簿を受け取った(参加側)。ここで初めて自分の番号が決まる。
//----------------------------------------------------------
void HjNetSession::HandleRoster(const HjNetRosterPacket& pkt)
{
	if (m_mode == Mode::Hosting) { return; }   // ホストは自分が配る側

	m_myId = pkt.yourId;
	m_mode = Mode::Connected;

	const int count = std::min<int>(pkt.count, NetConst::MaxPlayers);
	for (int i = 0; i < count; ++i)
	{
		const HjNetPeerEntry& e = pkt.peers[i];
		const int id = e.id;
		if (id < 0 || id >= NetConst::MaxPlayers) { continue; }
		if (id == m_myId) { continue; }   // 自分は相手ではない

		if (!e.used)
		{
			// 名簿から消えている＝抜けた
			if (m_peers[id].used) { DropPeer(id); }
			continue;
		}

		const bool isNew = !m_peers[id].used;
		m_peers[id].used  = true;
		m_peers[id].name  = e.name;
		m_peers[id].carKind = e.carKind;
		m_peers[id].look.outline = FromBytes(e.outlineR, e.outlineG, e.outlineB);
		m_peers[id].look.smokeA = FromBytes(e.smokeAR, e.smokeAG, e.smokeAB);
		m_peers[id].look.smokeB = FromBytes(e.smokeBR, e.smokeBG, e.smokeBB);
		m_peers[id].look.accent = FromBytes(e.accentR, e.accentG, e.accentB);
		m_peers[id].look.neonA = FromBytes(e.neonAR, e.neonAG, e.neonAB);
		m_peers[id].look.neonB = FromBytes(e.neonBR, e.neonBG, e.neonBB);
		m_peers[id].look.smokeHi = FromBytes(e.smokeHiR, e.smokeHiG, e.smokeHiB);
		m_peers[id].look.smokeGradDist = (e.smokeGradDist / 255.0f) * HjSmokeGradDistMax;

		if (id == 0)
		{
			// ホストの住所だけは名簿に入っていない。
			// ホストは自分がどのアドレスで見えているかを知らないため。
			// こちらは繋ぎに行った先を知っているので、それを使う。
			m_peers[id].addr = m_hostAddr;
		}
		else
		{
			// 繋ぎ方によって埋まっている側が違う。両方そのまま写せばよい
			HjNetAddress a;
			a.ip     = e.ip;
			a.port   = e.port;
			a.userId = e.userId;
			m_peers[id].addr = a;
		}

		if (isNew) { m_peers[id].silence = 0.0f; m_peers[id].lastSeq = 0; }
	}
}

//----------------------------------------------------------
// 車の状態。古いものは捨てて、新しいものだけ受け皿へ積む。
//----------------------------------------------------------
void HjNetSession::HandleState(const HjNetStatePacket& pkt, const HjNetAddress& from)
{
	const int id = pkt.id;
	if (id < 0 || id >= NetConst::MaxPlayers) { return; }
	if (id == m_myId) { return; }   // 自分の状態が返ってきても使わない

	Peer& p = m_peers[id];

	// 名簿より先に状態が届くことがある。
	// 送り主の住所は受信時に分かるので、そのまま登録して受け取りを始める。
	if (!p.used)
	{
		p.used = true;
		p.addr = from;
		p.lastSeq = 0;
	}

	// 連番が進んでいなければ、順番が入れ替わって古いものが来ている。
	// これを採用すると相手の車が一瞬戻るので捨てる。
	if (pkt.seq != 0 && pkt.seq <= p.lastSeq) { return; }
	p.lastSeq = pkt.seq;
	p.silence = 0.0f;

	// シーンが取り出さないまま溜まり続けないよう上限を設ける。
	// 位置の同期は「次が来れば古いものは要らない」ので、
	// あふれたら古いほうから捨てて構わない。
	if (static_cast<int>(m_inbox.size()) >= NetConst::InboxMax)
	{
		m_inbox.erase(m_inbox.begin());
	}
	m_inbox.push_back(pkt);
}

//----------------------------------------------------------
// 見た目が変わった通知。
//----------------------------------------------------------
void HjNetSession::HandleLook(const HjNetLookPacket& pkt)
{
	const int id = pkt.id;
	if (id < 0 || id >= NetConst::MaxPlayers) { return; }
	if (id == m_myId) { return; }

	m_peers[id].look.outline = FromBytes(pkt.outlineR, pkt.outlineG, pkt.outlineB);
	m_peers[id].look.smokeA = FromBytes(pkt.smokeAR, pkt.smokeAG, pkt.smokeAB);
	m_peers[id].look.smokeB = FromBytes(pkt.smokeBR, pkt.smokeBG, pkt.smokeBB);
	m_peers[id].look.accent = FromBytes(pkt.accentR, pkt.accentG, pkt.accentB);
	m_peers[id].look.neonA = FromBytes(pkt.neonAR, pkt.neonAG, pkt.neonAB);
	m_peers[id].look.neonB = FromBytes(pkt.neonBR, pkt.neonBG, pkt.neonBB);
	m_peers[id].look.smokeHi = FromBytes(pkt.smokeHiR, pkt.smokeHiG, pkt.smokeHiB);
	m_peers[id].look.smokeGradDist = (pkt.smokeGradDist / 255.0f) * HjSmokeGradDistMax;
}

//----------------------------------------------------------
// 見た目の変更を全員へ伝える。
//
// 名簿はホストしか配らないので、参加者が色を変えた場合は
// これが無いと誰にも届かない。P2Pなので全員へ直接送る。
//----------------------------------------------------------
void HjNetSession::SendLook()
{
	if (m_myId < 0) { return; }

	HjNetLookPacket pkt;
	pkt.id = static_cast<unsigned char>(m_myId);
	pkt.outlineR = ToByte(m_myLook.outline.x);
	pkt.outlineG = ToByte(m_myLook.outline.y);
	pkt.outlineB = ToByte(m_myLook.outline.z);
	pkt.smokeAR = ToByte(m_myLook.smokeA.x);
	pkt.smokeAG = ToByte(m_myLook.smokeA.y);
	pkt.smokeAB = ToByte(m_myLook.smokeA.z);
	pkt.smokeBR = ToByte(m_myLook.smokeB.x);
	pkt.smokeBG = ToByte(m_myLook.smokeB.y);
	pkt.smokeBB = ToByte(m_myLook.smokeB.z);
	pkt.accentR = ToByte(m_myLook.accent.x);
	pkt.accentG = ToByte(m_myLook.accent.y);
	pkt.accentB = ToByte(m_myLook.accent.z);
	pkt.neonAR = ToByte(m_myLook.neonA.x);
	pkt.neonAG = ToByte(m_myLook.neonA.y);
	pkt.neonAB = ToByte(m_myLook.neonA.z);
	pkt.neonBR = ToByte(m_myLook.neonB.x);
	pkt.neonBG = ToByte(m_myLook.neonB.y);
	pkt.neonBB = ToByte(m_myLook.neonB.z);
	pkt.smokeHiR = ToByte(m_myLook.smokeHi.x);
	pkt.smokeHiG = ToByte(m_myLook.smokeHi.y);
	pkt.smokeHiB = ToByte(m_myLook.smokeHi.z);
	pkt.smokeGradDist = ToByte(m_myLook.smokeGradDist / HjSmokeGradDistMax);

	SendToAllPeers(&pkt, sizeof(pkt));
}

void HjNetSession::HandleLeave(const HjNetLeavePacket& pkt)
{
	const int id = pkt.id;
	if (id < 0 || id >= NetConst::MaxPlayers) { return; }
	if (!m_peers[id].used) { return; }

	DropPeer(id);
	if (m_mode == Mode::Hosting) { SendRoster(); }
}

//==========================================================
// 送信
//==========================================================

void HjNetSession::SendJoinRequest()
{
	if (!m_transport || !m_hostAddr.IsValid()) { return; }

	HjNetJoinPacket pkt;
	pkt.outlineR = ToByte(m_myLook.outline.x);
	pkt.outlineG = ToByte(m_myLook.outline.y);
	pkt.outlineB = ToByte(m_myLook.outline.z);
	pkt.smokeAR = ToByte(m_myLook.smokeA.x);
	pkt.smokeAG = ToByte(m_myLook.smokeA.y);
	pkt.smokeAB = ToByte(m_myLook.smokeA.z);
	pkt.smokeBR = ToByte(m_myLook.smokeB.x);
	pkt.smokeBG = ToByte(m_myLook.smokeB.y);
	pkt.smokeBB = ToByte(m_myLook.smokeB.z);
	pkt.accentR = ToByte(m_myLook.accent.x);
	pkt.accentG = ToByte(m_myLook.accent.y);
	pkt.accentB = ToByte(m_myLook.accent.z);
	pkt.neonAR = ToByte(m_myLook.neonA.x);
	pkt.neonAG = ToByte(m_myLook.neonA.y);
	pkt.neonAB = ToByte(m_myLook.neonA.z);
	pkt.neonBR = ToByte(m_myLook.neonB.x);
	pkt.neonBG = ToByte(m_myLook.neonB.y);
	pkt.neonBB = ToByte(m_myLook.neonB.z);
	// 乗っている車種。送らないと、相手の画面では既定の車で出る
	pkt.carKind = static_cast<unsigned char>(HjCarChoice::Instance().Get());

	strncpy_s(pkt.name, sizeof(pkt.name), m_myName.c_str(), _TRUNCATE);
	m_transport->Send(&pkt, sizeof(pkt), m_hostAddr);
}

void HjNetSession::SendRoster()
{
	if (m_mode != Mode::Hosting) { return; }

	HjNetRosterPacket pkt;
	pkt.msg   = static_cast<unsigned char>(HjNetMsg::JoinAccept);
	pkt.count = static_cast<unsigned char>(NetConst::MaxPlayers);

	// 0番はホスト自身。住所は入れない(自分がどう見えているか分からないため)。
	// 受け取った側が、繋ぎに行った先の住所で埋める。
	pkt.peers[0].id   = 0;
	pkt.peers[0].used = 1;
	pkt.peers[0].carKind = static_cast<unsigned char>(HjCarChoice::Instance().Get());
	pkt.peers[0].outlineR = ToByte(m_myLook.outline.x);
	pkt.peers[0].outlineG = ToByte(m_myLook.outline.y);
	pkt.peers[0].outlineB = ToByte(m_myLook.outline.z);
	pkt.peers[0].smokeAR = ToByte(m_myLook.smokeA.x);
	pkt.peers[0].smokeAG = ToByte(m_myLook.smokeA.y);
	pkt.peers[0].smokeAB = ToByte(m_myLook.smokeA.z);
	pkt.peers[0].smokeBR = ToByte(m_myLook.smokeB.x);
	pkt.peers[0].smokeBG = ToByte(m_myLook.smokeB.y);
	pkt.peers[0].smokeBB = ToByte(m_myLook.smokeB.z);
	pkt.peers[0].accentR = ToByte(m_myLook.accent.x);
	pkt.peers[0].accentG = ToByte(m_myLook.accent.y);
	pkt.peers[0].accentB = ToByte(m_myLook.accent.z);
	pkt.peers[0].neonAR = ToByte(m_myLook.neonA.x);
	pkt.peers[0].neonAG = ToByte(m_myLook.neonA.y);
	pkt.peers[0].neonAB = ToByte(m_myLook.neonA.z);
	pkt.peers[0].neonBR = ToByte(m_myLook.neonB.x);
	pkt.peers[0].neonBG = ToByte(m_myLook.neonB.y);
	pkt.peers[0].neonBB = ToByte(m_myLook.neonB.z);
	pkt.peers[0].smokeHiR = ToByte(m_myLook.smokeHi.x);
	pkt.peers[0].smokeHiG = ToByte(m_myLook.smokeHi.y);
	pkt.peers[0].smokeHiB = ToByte(m_myLook.smokeHi.z);
	pkt.peers[0].smokeGradDist = ToByte(m_myLook.smokeGradDist / HjSmokeGradDistMax);
	strncpy_s(pkt.peers[0].name, sizeof(pkt.peers[0].name), m_myName.c_str(), _TRUNCATE);

	for (int i = 1; i < NetConst::MaxPlayers; ++i)
	{
		pkt.peers[i].id     = static_cast<unsigned char>(i);
		pkt.peers[i].used   = m_peers[i].used ? 1 : 0;
		pkt.peers[i].carKind = m_peers[i].carKind;
		pkt.peers[i].ip     = m_peers[i].addr.ip;
		pkt.peers[i].port   = m_peers[i].addr.port;
		pkt.peers[i].userId = m_peers[i].addr.userId;
		pkt.peers[i].outlineR = ToByte(m_peers[i].look.outline.x);
		pkt.peers[i].outlineG = ToByte(m_peers[i].look.outline.y);
		pkt.peers[i].outlineB = ToByte(m_peers[i].look.outline.z);
		pkt.peers[i].smokeAR = ToByte(m_peers[i].look.smokeA.x);
		pkt.peers[i].smokeAG = ToByte(m_peers[i].look.smokeA.y);
		pkt.peers[i].smokeAB = ToByte(m_peers[i].look.smokeA.z);
		pkt.peers[i].smokeBR = ToByte(m_peers[i].look.smokeB.x);
		pkt.peers[i].smokeBG = ToByte(m_peers[i].look.smokeB.y);
		pkt.peers[i].smokeBB = ToByte(m_peers[i].look.smokeB.z);
		pkt.peers[i].accentR = ToByte(m_peers[i].look.accent.x);
		pkt.peers[i].accentG = ToByte(m_peers[i].look.accent.y);
		pkt.peers[i].accentB = ToByte(m_peers[i].look.accent.z);
		pkt.peers[i].neonAR = ToByte(m_peers[i].look.neonA.x);
		pkt.peers[i].neonAG = ToByte(m_peers[i].look.neonA.y);
		pkt.peers[i].neonAB = ToByte(m_peers[i].look.neonA.z);
		pkt.peers[i].neonBR = ToByte(m_peers[i].look.neonB.x);
		pkt.peers[i].neonBG = ToByte(m_peers[i].look.neonB.y);
		pkt.peers[i].neonBB = ToByte(m_peers[i].look.neonB.z);
		pkt.peers[i].smokeHiR = ToByte(m_peers[i].look.smokeHi.x);
		pkt.peers[i].smokeHiG = ToByte(m_peers[i].look.smokeHi.y);
		pkt.peers[i].smokeHiB = ToByte(m_peers[i].look.smokeHi.z);
		pkt.peers[i].smokeGradDist = ToByte(m_peers[i].look.smokeGradDist / HjSmokeGradDistMax);
		strncpy_s(pkt.peers[i].name, sizeof(pkt.peers[i].name),
		          m_peers[i].name.c_str(), _TRUNCATE);
	}

	// 相手ごとに「あなたの番号」を書き換えて送る
	for (int i = 1; i < NetConst::MaxPlayers; ++i)
	{
		if (!m_peers[i].used) { continue; }
		pkt.yourId = static_cast<unsigned char>(i);
		m_transport->Send(&pkt, sizeof(pkt), m_peers[i].addr);
	}
}

void HjNetSession::SendState()
{
	if (!m_hasLocalState || m_myId < 0) { return; }

	m_localState.msg = static_cast<unsigned char>(HjNetMsg::State);
	m_localState.id  = static_cast<unsigned char>(m_myId);
	m_localState.seq = ++m_seq;

	SendToAllPeers(&m_localState, sizeof(m_localState));
}

void HjNetSession::SendToAllPeers(const void* data, int size)
{
	if (!m_transport) { return; }

	for (const Peer& p : m_peers)
	{
		if (!p.used || !p.addr.IsValid()) { continue; }
		m_transport->Send(data, size, p.addr);
	}
}

//==========================================================
// 名簿の操作
//==========================================================

int HjNetSession::FindPeerByAddr(const HjNetAddress& addr) const
{
	for (int i = 0; i < static_cast<int>(m_peers.size()); ++i)
	{
		if (m_peers[i].used && m_peers[i].addr == addr) { return i; }
	}
	return -1;
}

int HjNetSession::FindPeerById(int id) const
{
	if (id < 0 || id >= static_cast<int>(m_peers.size())) { return -1; }
	return m_peers[id].used ? id : -1;
}

int HjNetSession::AcquireFreeSlot()
{
	// 0番はホストなので1番から探す
	for (int i = 1; i < NetConst::MaxPlayers; ++i)
	{
		if (!m_peers[i].used) { return i; }
	}
	return -1;
}

void HjNetSession::DropPeer(int index)
{
	if (index < 0 || index >= static_cast<int>(m_peers.size())) { return; }

	m_peers[index].used = false;
	m_peers[index].name.clear();
	m_peers[index].addr = HjNetAddress();
	m_peers[index].silence = 0.0f;
	m_peers[index].lastSeq = 0;

	// シーンが車を消せるよう番号を残す
	m_removed.push_back(index);
}

void HjNetSession::ClearPeers()
{
	for (Peer& p : m_peers)
	{
		p.used = false;
		p.name.clear();
		p.addr = HjNetAddress();
		p.silence = 0.0f;
		p.lastSeq = 0;
	}
}

//----------------------------------------------------------
// 何も届かなくなった相手を切る。
// UDPは切断を教えてくれないので、沈黙している時間で判断するしかない。
//----------------------------------------------------------
void HjNetSession::UpdateTimeouts(float dt)
{
	bool changed = false;

	for (int i = 0; i < static_cast<int>(m_peers.size()); ++i)
	{
		if (!m_peers[i].used) { continue; }

		m_peers[i].silence += dt;
		if (m_peers[i].silence >= NetConst::Timeout)
		{
			DropPeer(i);
			changed = true;
		}
	}

	// 名簿が変わったらホストが配り直す
	if (changed && m_mode == Mode::Hosting) { SendRoster(); }
}

int HjNetSession::GetPlayerCount() const
{
	if (m_mode == Mode::Offline || m_myId < 0) { return 0; }

	int n = 1;   // 自分
	for (const Peer& p : m_peers) { if (p.used) { ++n; } }
	return n;
}

const char* HjNetSession::GetPeerName(int id) const
{
	if (id < 0 || id >= static_cast<int>(m_peers.size())) { return ""; }
	if (!m_peers[id].used) { return ""; }
	return m_peers[id].name.c_str();
}

HjCarLook HjNetSession::GetPeerLook(int id) const
{
	if (id < 0 || id >= static_cast<int>(m_peers.size())) { return HjCarLook(); }
	return m_peers[id].look;
}

//----------------------------------------------------------
// 番号から車種を引く
//
// 相手の車を作るときに使う。
// 決め打ちにすると、相手がNSXでもシルビアで出る
//----------------------------------------------------------
int HjNetSession::GetPeerCarKind(int id) const
{
	if (id < 0 || id >= static_cast<int>(m_peers.size())) { return 0; }
	return static_cast<int>(m_peers[id].carKind);
}

std::vector<HjNetSession::PeerView> HjNetSession::BuildPeerList() const
{
	std::vector<PeerView> out;
	for (const Peer& p : m_peers)
	{
		if (!p.used) { continue; }
		PeerView v;
		v.id    = p.id;
		v.name  = p.name;
		v.alive = (p.silence < NetConst::Timeout);
		out.push_back(v);
	}
	return out;
}

//==========================================================
// 状態パネル
//==========================================================

void HjNetSession::DrawImGui()
{
	if (!ImGui::CollapsingHeader(U8("通信(P2P)"))) { return; }

	const char* modeText = U8("未接続");
	switch (m_mode)
	{
	case Mode::Hosting:   modeText = U8("ホスト");     break;
	case Mode::Joining:   modeText = U8("参加要求中"); break;
	case Mode::Connected: modeText = U8("参加中");     break;
	default: break;
	}
	ImGui::Text(U8("状態: %s   自分の番号: %d   人数: %d"), modeText, m_myId, GetPlayerCount());

	if (m_hostAddrText[0]) { ImGui::Text(U8("アドレス: %s"), m_hostAddrText); }
	if (m_lastError[0])    { ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", m_lastError); }

	// 接続先の入力欄。画面の下書き用なので、ここだけで持てば足りる
	// ※この入力欄の変数に winsock の予約名は使えない。
	//   in_addr のためにマクロが定義されており、書いた名前が
	//   別の綴りへ置き換えられて、意味の分からない構文エラーになる。
	static char s_hostInput[64] = "127.0.0.1";
	static char s_nameInput[NetConst::MaxNameLen + 1] = "PLAYER";

	ImGui::InputText(U8("名前"), s_nameInput, sizeof(s_nameInput));

	// ※色はここでは設定しない。
	//   車の見た目は調整パネル(Silvia Tuning)で設定した色をそのまま配る。
	//   通信側で決めると、せっかく詰めた配色が上書きされる。

	if (m_mode == Mode::Offline)
	{
		// 繋ぎ方。Steamは設定なしでインターネット越しに繋がるが、
		// SDKとクライアントが要る
		int link = (m_link == Link::Steam) ? 1 : 0;
		const char* linkNames[] = { U8("直接IP"), U8("Steam経由") };
		if (ImGui::Combo(U8("繋ぎ方"), &link, linkNames, IM_ARRAYSIZE(linkNames)))
		{
			SetLink(link == 1 ? Link::Steam : Link::DirectIp);
		}
		// 使えない理由をそのまま出す。
		// 「使えません」だけでは、何を直せばよいのか分からない
		if (m_link == Link::Steam && !HjSteamTransport::IsAvailable())
		{
			ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1),
			                   U8("Steamが使えません: %s"), HjSteamTransport::GetInitError());
		}

		if (ImGui::Button(U8("ホストになる")))
		{
			StartHost(s_nameInput);
		}
		// 直接IPなら "192.168.0.5"、Steamなら相手のアカウント番号
		ImGui::InputText(m_link == Link::Steam ? U8("相手のID") : U8("接続先IP"),
		                 s_hostInput, sizeof(s_hostInput));
		ImGui::SameLine();
		if (ImGui::Button(U8("参加する")))
		{
			StartJoin(s_hostInput, s_nameInput);
		}
	}
	else
	{
		if (ImGui::Button(U8("切断"))) { Leave(); }
	}

	ImGui::Separator();
	ImGui::Text(U8("参加者"));
	for (const Peer& p : m_peers)
	{
		if (!p.used) { continue; }
		char addr[64] = {};
		if (m_transport) { m_transport->ToString(p.addr, addr, sizeof(addr)); }
		ImGui::BulletText(U8("[%d] %s  %s  無通信 %.1fs"),
		                  p.id, p.name.c_str(), addr, p.silence);
	}
}
