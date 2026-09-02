#pragma once

#include "Silvia.h"
#include "../../Network/HjNetProtocol.h"

//==========================================================
// RemoteCar
//   他のプレイヤーの車。
//
//   ■ 物理を回さない
//   本人のPCが出した答え(位置と向き)を受け取って、それを描くだけ。
//   こちらでも物理を回すと、同じ操作をしていない以上ずれていくうえ、
//   届いた位置で毎フレーム引き戻すことになって車が震える。
//
//   ■ わざと少し過去を描く
//   届いた最新の位置をそのまま描くと、パケットが1つ遅れただけで
//   相手の車が止まり、次が来た瞬間に飛ぶ。
//   InterpDelay ぶん遅らせて描けば、手元には常に「次の位置」があるので、
//   2点の間を繋ぐだけで滑らかになる。
//   遅れと滑らかさの引き換えで、0.12秒は見て分かるほどの遅れではない。
//
//   ■ タイヤの回転は速度から作る
//   毎秒20回では、その間にタイヤが1回転以上することがある。
//   送られた角度を繋ぐと回り方がおかしくなるので、こちらで回す。
//
//   使い方(シーン側):
//     PushState(届いたパケット)   受け取るたび
//     あとは通常のオブジェクトとして Update / Draw される
//==========================================================
class RemoteCar : public Silvia
{
public:
	// プレイヤー番号(名簿と対応)。どの車が誰かを見分けるのに使う
	explicit RemoteCar(int playerId) : m_playerId(playerId)
	{
		// 自分の差し替えを読み込まない。
		//
		// 保存ファイルは車種ごと(CarMod_Silvia.txt)なので、
		// そのまま読むと相手の車まで自分のモデルになる。
		// しかも合わせ込みは自分の車にしか当てていないので、
		// 相手だけ標準の寸法で別のモデルが出ることになる。
		//
		// ※相手のモデルを受け取る仕組みが出来たら、ここで当てる
		m_useModChoice = false;
	}

	void Init()   override;
	void Update() override;
	// 車の上に出す名前札。2Dで描くのでスプライトのパスに乗せる
	void DrawSprite() override;

	int GetPlayerId() const { return m_playerId; }

	// 表示する名前。名簿から貰った値を入れる
	void SetPlayerName(const std::string& name) { m_playerName = name; }
	// 持ち主の調整パネルの色。アウトラインと煙に反映される
	void SetLook(const HjCarLook& look)
	{
		ApplyLookColors(look.outline, look.smokeA, look.smokeB,
		                look.accent, look.neonA, look.neonB,
		                look.smokeHi, look.smokeGradDist);
	}

	// この車を観戦しているか。名前札に印を出す。
	// カメラが動いただけだと「自分がそこへ移動した」ようにも見えるので、
	// 見られている側の札で区別する
	void SetSpectated(bool on) { m_spectated = on; }

	// 届いた状態を積む。順番の入れ替わりはセッション側で弾いてある
	void PushState(const HjNetStatePacket& state);

	// まだ一度も状態が届いていない間は描かない。
	// 原点に車が置かれたままになるのを避ける
	bool CheckInScreen(const DirectX::BoundingFrustum&) const override { return m_hasState; }

private:
	// 受け取った状態と、それが手元へ届いた時刻。
	//
	// 送り主の時刻ではなく「自分が受け取った時刻」で並べる。
	// PC同士の時計は合っていないので、送り主の時刻をそのまま使うと
	// 時計合わせの仕組みが要る。受信時刻なら回線のばらつきぶんは
	// 揺れるが、InterpDelay がそれを吸収してくれる。
	struct Sample
	{
		float            time = 0.0f;
		HjNetStatePacket state;
	};

	// 描くべき時刻の状態を作る。false=まだ描けるものが無い。
	//
	// outRot は「坂の傾き × バンク × ヨー」を合成した姿勢。
	// 角度を1つずつ繋ぐと、3軸が同時に動いたときに本来と違う経路を
	// 通って車体が揺れるので、合成してから球面で繋ぐ。
	bool SampleAt(float renderTime, HjNetStatePacket& out, Math::Quaternion& outRot) const;

	// 1つの状態から「坂の傾き × バンク × ヨー」の合成姿勢を作る
	static Math::Quaternion MakeRotation(const HjNetStatePacket& st);

	// 見た目のタイヤの回転を、今の速度から進める
	void SpinWheels(float dt, float speed, bool handbrake);

	// ワールド座標を、UIが使うデザイン座標へ直す。
	// false=カメラの後ろにある(画面に出ない)
	static bool WorldToDesign(const Math::Vector3& world, float& outX, float& outY);

	// 角度の補間。359度→1度のような繋ぎ目で逆回りしないよう、
	// 近いほうの回り方を選ぶ
	static float LerpAngle(float a, float b, float t);

	int         m_playerId = -1;
	bool        m_hasState = false;
	std::string m_playerName;
	bool        m_spectated = false;

	// タイヤの転がり角。基底のものは物理が回す前提で外から触れないので、
	// ここで持って ApplyVisualState へ渡す
	float m_spinFront = 0.0f;
	float m_spinRear  = 0.0f;

	// 自分の時計。届いた時刻を刻むのと、描く時刻を決めるのに使う
	float m_time = 0.0f;

	// 送り主の時計と自分の時計のずれ。
	//
	// 届いた時刻で並べると、回線のばらつきがそのまま
	// 点の間隔になって、区間ごとに速さが変わって見える。
	// 連番から送信時刻を組み直し、このずれを足して自分の時間軸へ移す
	float m_clockOffset = 0.0f;
	bool  m_clockSet    = false;

	std::vector<Sample> m_history;
};
