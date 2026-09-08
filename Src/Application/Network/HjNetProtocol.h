#pragma once

#include "../Const/NetConst.h"

//==========================================================
// HjNetProtocol
//   やりとりするパケットの形。
//
//   ■ なぜ構造体をそのまま送るか
//   位置の同期は毎秒20回×人数ぶん流れるので、1回あたりを小さく保ちたい。
//   文字列に直すと桁数ぶん膨らむうえ、送る側と受け取る側で
//   書式を合わせる手間が増える。数値のまま送るのが一番安い。
//
//   ■ 詰め物(パディング)を禁止する理由
//   構造体はふつう、CPUが読みやすいように隙間を空けて配置される。
//   その隙間の入り方は環境によって変わるので、そのまま送ると
//   受け取り側で項目がずれる。#pragma pack で隙間を禁止して、
//   バイトの並びを固定する。
//
//   ■ 順番が入れ替わる前提
//   UDPは届く順番が保証されない。古い位置があとから届いたときに
//   それを採用すると、相手の車が一瞬戻る。連番(seq)を持たせて、
//   受け取り側が「今持っているものより古ければ捨てる」判断をする。
//==========================================================

// パケットの種類。先頭1バイトに入れて、受け取り側が振り分ける
enum class HjNetMsg : unsigned char
{
	Join       = 1,   // 参加要求          参加側 → ホスト
	JoinAccept = 2,   // 受理と番号の通知  ホスト → 参加側
	Roster     = 3,   // 参加者一覧        ホスト → 全員
	State      = 4,   // 車の状態          全員 → 全員
	Leave      = 5,   // 退出              誰か → 全員
	Look       = 6,   // 見た目の変更      誰か → 全員
};

#pragma pack(push, 1)

//----------------------------------------------------------
// 参加要求。名前だけ伝える
//----------------------------------------------------------
struct HjNetJoinPacket
{
	unsigned char msg  = static_cast<unsigned char>(HjNetMsg::Join);
	// 車の見た目。持ち主が調整パネルで設定した色をそのまま運ぶ。
	// 通信側で色を決めると、せっかく詰めた配色が上書きされる。
	// 1成分1バイトで足りる(見分けに色の微妙な差は関係ない)
	unsigned char outlineR = 0, outlineG = 0, outlineB = 0;
	unsigned char smokeAR = 0, smokeAG = 0, smokeAB = 0;
	unsigned char smokeBR = 0, smokeBG = 0, smokeBB = 0;
	unsigned char accentR = 0, accentG = 0, accentB = 0;
	unsigned char neonAR = 0, neonAG = 0, neonAB = 0;
	unsigned char neonBR = 0, neonBG = 0, neonBB = 0;
	unsigned char smokeHiR = 0, smokeHiG = 0, smokeHiB = 0;
	// 煙のグラデが色Bになりきる距離。0〜255 を 0〜32m として使う
	unsigned char smokeGradDist = 0;
	// 乗っている車種(CarChoiceConst::Kind の番号)。
	//
	// 走行中に変わるものではないので、毎秒20回の状態パケットには入れず、
	// 参加のときに1回だけ送る。
	// 送らないと、相手がNSXでもこちら側ではシルビアで出る
	unsigned char carKind = 0;

	char          name[NetConst::MaxNameLen + 1] = {};
};

//----------------------------------------------------------
// 名簿の1人ぶん。
// ホストが全員へ配ることで、参加者同士が直接送り合えるようになる
// (ホストを経由すると往復ぶん遅れるうえ、ホストに負荷が集まる)。
//----------------------------------------------------------
struct HjNetPeerEntry
{
	// 相手の住所。繋ぎ方によって埋まる側が変わる(HjNetAddress と同じ考え方)
	unsigned long long userId = 0;   // Steam経由のとき
	unsigned int   ip   = 0;         // 直接IPのとき(IPv4・ネットワークバイト順)
	unsigned short port = 0;
	unsigned char  id   = 0;         // プレイヤー番号
	unsigned char  used = 0;         // 0=空き

	// 車の見た目。持ち主の調整パネルの色をそのまま運ぶ。
	// 走行中に変わるものではないので、毎秒20回の状態パケットには入れず、
	// 名簿と一緒に1回だけ配る
	unsigned char outlineR = 0, outlineG = 0, outlineB = 0;
	unsigned char smokeAR = 0, smokeAG = 0, smokeAB = 0;
	unsigned char smokeBR = 0, smokeBG = 0, smokeBB = 0;
	unsigned char accentR = 0, accentG = 0, accentB = 0;
	unsigned char neonAR = 0, neonAG = 0, neonAB = 0;
	unsigned char neonBR = 0, neonBG = 0, neonBB = 0;
	unsigned char smokeHiR = 0, smokeHiG = 0, smokeHiB = 0;
	// 煙のグラデが色Bになりきる距離。0〜255 を 0〜32m として使う
	unsigned char smokeGradDist = 0;
	// 乗っている車種(CarChoiceConst::Kind の番号)。
	// 隙間を埋めていたバイトをそのまま使うので、大きさは変わらない
	unsigned char  carKind = 0;

	char           name[NetConst::MaxNameLen + 1] = {};
};

//----------------------------------------------------------
// 受理の返事と、参加者一覧。中身が同じなので同じ形を使い、
// 先頭の種類で意味を分ける。
//   JoinAccept … あなたの番号は yourId です＋現在の一覧
//   Roster     … 一覧が変わったので配り直し
//----------------------------------------------------------
struct HjNetRosterPacket
{
	unsigned char  msg    = static_cast<unsigned char>(HjNetMsg::Roster);
	unsigned char  yourId = 0;
	unsigned char  count  = 0;
	unsigned char  pad    = 0;
	HjNetPeerEntry peers[NetConst::MaxPlayers];
};

//----------------------------------------------------------
// 車の見た目。
//
// 車の調整パネルで設定した色をそのまま持つ。
// 通信側で色を決めると、せっかく詰めた配色が上書きされる。
//----------------------------------------------------------
// 煙のグラデ距離を1バイトへ丸めるときの上限(m)。
// 送る側と受け取る側で同じ値を使わないと距離が食い違う
constexpr float HjSmokeGradDistMax = 32.0f;

struct HjCarLook
{
	Math::Vector3 outline;   // 車の輪郭
	Math::Vector3 smokeA;    // 煙(手前)
	Math::Vector3 smokeB;    // 煙(奥)
	Math::Vector3 accent;    // ドリフト中に車体へ乗るアクセント
	Math::Vector3 neonA;     // タイヤ周りの線画
	Math::Vector3 neonB;
	Math::Vector3 smokeHi;   // 煙の一番光が当たる面

	// 煙のグラデが色Bになりきる距離(m)。色ではないが見た目の一部
	float smokeGradDist = 6.0f;

	// 中身が変わったか。変わったときだけ送り直すために使う
	bool operator==(const HjCarLook& o) const
	{
		return outline == o.outline && smokeA == o.smokeA && smokeB == o.smokeB
		    && accent  == o.accent  && neonA  == o.neonA  && neonB  == o.neonB
		    && smokeHi == o.smokeHi && smokeGradDist == o.smokeGradDist;
	}
	bool operator!=(const HjCarLook& o) const { return !(*this == o); }
};

//----------------------------------------------------------
// 車の状態。これが毎秒20回流れる本体。
//
// 送るのは「見た目を組み立てるのに要るもの」だけ。
// エンジン回転数やギアは相手の画面に出ないので送らない。
// 速度を入れているのは、受け取り側が補間の間を埋めるのと、
// 横滑り角を自分で出せるようにするため。
//
// ■ タイヤの転がり角は送らない
// 毎秒20回では、その間にタイヤが1回転以上することがある。
// 送られてきた角度を繋ぐと、回転が足りない・逆回りに見えるといった
// おかしな回り方になる。速度と半径から受け取り側で回したほうが、
// パケットが落ちても滑らかで、しかも安い。
// ハンドブレーキで後輪が止まることだけは flags で伝える。
//----------------------------------------------------------
struct HjNetStatePacket
{
	unsigned char msg = static_cast<unsigned char>(HjNetMsg::State);
	unsigned char id  = 0;      // 送り主のプレイヤー番号
	unsigned char flags  = 0;   // bit0=ハンドブレーキ / bit1=接地

	// 煙とタイヤ痕の量を決める滑り量。
	// 受け取り側で計算し直すこともできるが、しきい値や係数を
	// 両方で持つことになり、片方だけ変えたときに絵が食い違う。
	// 送り主が使った値をそのまま貰えば、必ず同じ絵になる。
	// 見た目にしか使わないので1バイトへ丸めて構わない
	unsigned char slipRear  = 0;
	unsigned char slipFront = 0;

	unsigned int  seq = 0;      // 連番。古いものを捨てる判断に使う

	float px = 0.0f, py = 0.0f, pz = 0.0f;   // 位置
	float yaw = 0.0f;                        // 車体の向き
	float vx = 0.0f, vy = 0.0f, vz = 0.0f;   // 速度

	float steer = 0.0f;   // 前輪の切れ角(見た目)

	// 車体の傾き。
	//
	// ■ なぜ送るのか
	// 受け取り側で作ろうとすると2つとも作れない。
	// 地形の傾きは4輪から地面へレイを飛ばして求めるものだが、
	// 他人の車は物理を回さないのでレイを飛ばしていない。
	// サスの傾きは加速度から作るもので、通信で届いた位置を
	// 微分して出すことになるが、届き方のばらつきがそのまま増幅されて
	// 車体がガタガタ震える。
	//
	// 送り主の画面ではもう答えが出ているので、それを貰うのが一番安い。
	// 4つで16バイト増えるが、毎秒20回でも1人あたり320バイト/秒しかない。
	float terrainPitch = 0.0f;   // 坂(登り降り)
	float terrainRoll  = 0.0f;   // バンク(路面の左右傾き)
	float bodyPitch    = 0.0f;   // サス：前後の沈み込み
	float bodyRoll     = 0.0f;   // サス：旋回時の左右の傾き
};

//----------------------------------------------------------
// 車から通信へ渡す状態。
//
// 引数を1つずつ並べると10個近くになり、順番を間違えても
// コンパイルが通ってしまう(同じ型が並ぶため)。
// まとめて名前で渡す。
//----------------------------------------------------------
struct HjCarSyncState
{
	Math::Vector3 pos;
	Math::Vector3 vel;
	float yaw   = 0.0f;
	float steer = 0.0f;

	float terrainPitch = 0.0f;
	float terrainRoll  = 0.0f;
	float bodyPitch    = 0.0f;
	float bodyRoll     = 0.0f;

	float slipRear01  = 0.0f;
	float slipFront01 = 0.0f;

	bool  handbrake = false;
	bool  onGround  = true;
};

//----------------------------------------------------------
// 見た目の変更。
//
// 名簿は参加・退出のときしか配られないので、途中で色を変えても
// そのままでは相手へ届かない。変えた本人が全員へ直接伝える。
//----------------------------------------------------------
struct HjNetLookPacket
{
	unsigned char msg = static_cast<unsigned char>(HjNetMsg::Look);
	unsigned char id  = 0;
	unsigned char pad[2] = { 0, 0 };
	unsigned char outlineR = 0, outlineG = 0, outlineB = 0;
	unsigned char smokeAR = 0, smokeAG = 0, smokeAB = 0;
	unsigned char smokeBR = 0, smokeBG = 0, smokeBB = 0;
	unsigned char accentR = 0, accentG = 0, accentB = 0;
	unsigned char neonAR = 0, neonAG = 0, neonAB = 0;
	unsigned char neonBR = 0, neonBG = 0, neonBB = 0;
	unsigned char smokeHiR = 0, smokeHiG = 0, smokeHiB = 0;
	// 煙のグラデが色Bになりきる距離。0〜255 を 0〜32m として使う
	unsigned char smokeGradDist = 0;
};

//----------------------------------------------------------
// 退出の通知
//----------------------------------------------------------
struct HjNetLeavePacket
{
	unsigned char msg = static_cast<unsigned char>(HjNetMsg::Leave);
	unsigned char id  = 0;
	unsigned char pad[2] = { 0, 0 };
};

#pragma pack(pop)

// 送受信バッファはこの大きさがあれば全種類を収められる
static_assert(sizeof(HjNetRosterPacket) <= NetConst::MaxPacket,
              "名簿パケットが1パケットの上限を超えている");
static_assert(sizeof(HjNetStatePacket) <= NetConst::MaxPacket,
              "状態パケットが1パケットの上限を超えている");

//----------------------------------------------------------
// flags の中身。ビットを直接書くと意味が読めないので名前を付ける
//----------------------------------------------------------
namespace HjNetFlag
{
	constexpr unsigned char Handbrake = 1 << 0;
	// 接地しているか。空中では痕も煙も出ない
	constexpr unsigned char OnGround  = 1 << 1;
}
