#pragma once

//==========================================================
// HjSpawnPoint
//   走り出す場所。自作なので Hj 接頭辞。
//
//   ■ なぜ Stage から切り離すか
//   もとは Stage が持っていた。ところが地形のステージでは
//   Stage そのものを作らないので、Stage が null になり、
//   スポーンを設定するボタンが全部 if (stage) で素通りしていた。
//
//   「設定したのに変わらない」の正体はこれ。
//   ステージの作り方に関係なく在るものとして、独立して持つ。
//
//   ■ 保存する先
//   道の制御点(road_path.txt)と同じ、作る側のデータ。
//   遊ぶ側の設定(save.dat)ではないので、テキストで隣に置く。
//
//   ■ 決めていない間は道の始点に任せる
//   道は制御点を動かすたびに始点も動くので、
//   何も決めていないうちはそれに乗っておくのが正しい
//==========================================================
class HjSpawnPoint
{
public:
	static HjSpawnPoint& Instance()
	{
		static HjSpawnPoint inst;
		return inst;
	}

	// 自分で決めた場所を持っているか
	bool Has() const { return m_has; }

	const Math::Vector3& Pos() const { return m_pos; }
	float                Yaw() const { return m_yaw; }

	void Set(const Math::Vector3& pos, float yaw)
	{
		m_pos = pos;
		m_yaw = yaw;
		m_has = true;
	}

	// 決めたものを捨てて、道の始点へ任せる
	void Clear() { m_has = false; }

	void Load();
	void Save() const;

private:
	Math::Vector3 m_pos = Math::Vector3::Zero;
	float         m_yaw = 0.0f;
	bool          m_has = false;
};
