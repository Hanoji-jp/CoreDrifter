#pragma once

//==========================================================
// HjRigidBody
//   6自由度の剛体。自作なので Hj 接頭辞。
//
//   ■ 何を持つか
//   位置・姿勢(クォータニオン)・速度・角速度と、質量・慣性。
//   力と撃力を「どこにかけたか」込みで受け取り、並進と回転へ分ける。
//
//   ■ なぜ自前で持つか
//   既存の車は3自由度の平面モデルで、向きがスカラーのヨーしかない。
//   坂の傾きは4輪のレイから推定して見た目に反映しているだけで、
//   車体が本当に傾いているわけではない。
//
//   そのため、片輪が浮く・転倒する・縁石で跳ねる、が原理的に出せない。
//   姿勢をクォータニオンで持ち、力を接地点へかける形に変える。
//
//   ■ 物理エンジンは入れない
//   要るのは車1台ぶんの剛体と、それに力をかける口だけ。
//   汎用の解決器や拘束は使わないので、外から持ってくると
//   使わない部分のほうが大きくなる
//==========================================================
class HjRigidBody
{
public:
	//===== 諸元 =====
	// 慣性モーメントは直方体で近似する。
	// 車は箱ではないが、寸法から素直に出せて、
	// 実測値が手に入らない以上これ以上の精度に意味がない
	void SetBox(float mass, const Math::Vector3& halfExtents);

	float Mass()    const { return m_mass; }
	float InvMass() const { return m_invMass; }

	//===== 状態 =====
	const Math::Vector3&    Pos() const { return m_pos; }
	const Math::Quaternion& Rot() const { return m_rot; }
	const Math::Vector3&    Vel() const { return m_vel; }
	const Math::Vector3&    AngVel() const { return m_angVel; }

	void SetPos(const Math::Vector3& p) { m_pos = p; }
	void SetRot(const Math::Quaternion& q) { m_rot = q; Normalize(); }
	void SetVel(const Math::Vector3& v) { m_vel = v; }
	void SetAngVel(const Math::Vector3& w) { m_angVel = w; }

	// 止める。位置と姿勢は残す
	void Halt() { m_vel = Math::Vector3::Zero; m_angVel = Math::Vector3::Zero; }

	//===== 座標の行き来 =====
	Math::Vector3 ToWorldDir(const Math::Vector3& local) const;
	Math::Vector3 ToLocalDir(const Math::Vector3& world) const;
	Math::Vector3 ToWorldPos(const Math::Vector3& local) const;

	// その点の速度。回転ぶんを足す。
	// タイヤの滑りを出すのに要る(車体が回っていれば、
	// 前輪と後輪では地面に対する速度が違う)
	Math::Vector3 PointVel(const Math::Vector3& worldPos) const;

	//===== 力を溜める =====
	// 1フレームぶんを溜めて、Integrate でまとめて効かせる。
	// その場で速度を変えると、かけた順で結果が変わる
	void AddForce(const Math::Vector3& f) { m_force += f; }
	void AddForceAt(const Math::Vector3& f, const Math::Vector3& worldPos);

	void AddTorque(const Math::Vector3& t) { m_torque += t; }

	// 撃力。衝突はこちらで解く
	void AddImpulseAt(const Math::Vector3& j, const Math::Vector3& worldPos);

	//===== 進める =====
	void Integrate(float dt);

	// 溜めたものを捨てる。止めている間に溜め込まないため
	void ClearAccum() { m_force = Math::Vector3::Zero; m_torque = Math::Vector3::Zero; }

private:
	void Normalize();

	// いまの姿勢での慣性テンソルの逆。
	// ローカルの対角行列を姿勢で回して作る
	Math::Matrix InvInertiaWorld() const;

	float m_mass    = 1.0f;
	float m_invMass = 1.0f;

	// ローカル座標での慣性モーメントの逆(対角3成分)
	Math::Vector3 m_invInertiaLocal = Math::Vector3::One;

	Math::Vector3    m_pos;
	Math::Quaternion m_rot = Math::Quaternion::Identity;

	Math::Vector3 m_vel;
	Math::Vector3 m_angVel;

	Math::Vector3 m_force;
	Math::Vector3 m_torque;
};
