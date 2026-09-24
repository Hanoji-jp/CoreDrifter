#include "HjRigidBody.h"

//----------------------------------------------------------
// 諸元を決める
//
// 慣性モーメントは直方体で近似する。
//
//   Ix = m(h^2 + d^2) / 12   のような式。
//
// 車は箱ではないが、寸法から素直に出せるうえ、
// 実測の慣性が手に入らない以上これ以上の精度に意味がない。
// 足りなければ、外から倍率で調整するほうが早い
//----------------------------------------------------------
void HjRigidBody::SetBox(float mass, const Math::Vector3& halfExtents)
{
	m_mass    = std::max(mass, 0.001f);
	m_invMass = 1.0f / m_mass;

	// 全長・全幅・全高
	const float w = std::max(halfExtents.x * 2.0f, 0.01f);
	const float h = std::max(halfExtents.y * 2.0f, 0.01f);
	const float d = std::max(halfExtents.z * 2.0f, 0.01f);

	const float k = m_mass / 12.0f;

	const float ix = k * (h * h + d * d);   // ピッチ(左右軸まわり)
	const float iy = k * (w * w + d * d);   // ヨー  (上下軸まわり)
	const float iz = k * (w * w + h * h);   // ロール(前後軸まわり)

	m_invInertiaLocal = Math::Vector3(1.0f / ix, 1.0f / iy, 1.0f / iz);
}

//----------------------------------------------------------
void HjRigidBody::Normalize()
{
	// 積分を重ねると長さが 1 からずれて、姿勢に歪みが出る
	m_rot.Normalize();
}

//----------------------------------------------------------
Math::Vector3 HjRigidBody::ToWorldDir(const Math::Vector3& local) const
{
	return Math::Vector3::Transform(local, m_rot);
}

Math::Vector3 HjRigidBody::ToLocalDir(const Math::Vector3& world) const
{
	Math::Quaternion inv;
	m_rot.Inverse(inv);

	return Math::Vector3::Transform(world, inv);
}

Math::Vector3 HjRigidBody::ToWorldPos(const Math::Vector3& local) const
{
	return m_pos + ToWorldDir(local);
}

//----------------------------------------------------------
// その点の速度
//
// 車体が回っていれば、前輪と後輪では地面に対する速度が違う。
// タイヤの滑りはここから出る
//----------------------------------------------------------
Math::Vector3 HjRigidBody::PointVel(const Math::Vector3& worldPos) const
{
	const Math::Vector3 r = worldPos - m_pos;

	return m_vel + m_angVel.Cross(r);
}

//----------------------------------------------------------
// 力を点にかける
//
// 重心からずれた所にかけると、並進だけでなく回転も生む。
// タイヤの力が車体を回すのはこれ
//----------------------------------------------------------
void HjRigidBody::AddForceAt(const Math::Vector3& f, const Math::Vector3& worldPos)
{
	m_force += f;

	const Math::Vector3 r = worldPos - m_pos;
	m_torque += r.Cross(f);
}

//----------------------------------------------------------
// 撃力を点にかける
//
// 力と違って、その場で速度を変える。衝突はこちら
//----------------------------------------------------------
void HjRigidBody::AddImpulseAt(const Math::Vector3& j, const Math::Vector3& worldPos)
{
	m_vel += j * m_invMass;

	const Math::Vector3 r = worldPos - m_pos;
	const Math::Vector3 angJ = r.Cross(j);

	m_angVel += Math::Vector3::Transform(angJ, InvInertiaWorld());
}

//----------------------------------------------------------
// いまの姿勢での慣性テンソルの逆
//
//   I⁻¹(world) = R · I⁻¹(local) · Rᵀ
//
// ローカルでは対角なので、回して戻すだけで済む
//----------------------------------------------------------
Math::Matrix HjRigidBody::InvInertiaWorld() const
{
	const Math::Matrix r = Math::Matrix::CreateFromQuaternion(m_rot);

	Math::Matrix diag = Math::Matrix::Identity;
	diag._11 = m_invInertiaLocal.x;
	diag._22 = m_invInertiaLocal.y;
	diag._33 = m_invInertiaLocal.z;

	return r.Transpose() * diag * r;
}

//----------------------------------------------------------
// 1フレーム進める
//
// ■ 半陰的オイラー
// 速度を先に更新してから位置へ足す。
// 位置を先に動かす形(陽的)だと、バネを入れたときに
// 振幅が増えていって発散する。サスはバネなので、ここは譲れない
//----------------------------------------------------------
void HjRigidBody::Integrate(float dt)
{
	if (dt <= 0.0f) { ClearAccum(); return; }

	//===== 並進 =====
	m_vel += m_force * (m_invMass * dt);

	//===== 回転 =====
	m_angVel += Math::Vector3::Transform(m_torque, InvInertiaWorld()) * dt;

	// 暴走への保険。
	//
	// 数値が壊れると角速度が発散して、その場で高速回転する。
	// 車が1秒に2回転を超えることは無いので、そこで頭を打つ
	{
		const float w2 = m_angVel.Length();
		const float lim = 12.0f;   // rad/s

		if (w2 > lim) { m_angVel *= lim / w2; }
	}

	//===== 位置 =====
	m_pos += m_vel * dt;

	//===== 姿勢 =====
	// 角速度をクォータニオンの変化率へ直す。
	//
	//   dq/dt = 0.5 · ω ⊗ q
	//
	// ω は実部 0 のクォータニオンとして扱う。
	//
	// ■ 積の順に注意
	// DirectXMath の XMQuaternionMultiply(Q1, Q2) は、数学の表記では
	// Q2 ⊗ Q1 を返す(「Q1 の後に Q2」という順で定義されている)。
	// SimpleMath の operator* はこれをそのまま呼ぶので、
	// a * b は数学的には b ⊗ a になる。
	//
	// つまり ω ⊗ q が欲しければ m_rot * w と書く。
	// 逆に書くと、角速度がワールドではなく車体の座標系で効く。
	// ヨーだけなら差が出ないので、車体が傾いた瞬間に軸が食い違い、
	// 回転が自分を強める向きに入ってその場で回り出す
	const Math::Quaternion w(m_angVel.x, m_angVel.y, m_angVel.z, 0.0f);

	Math::Quaternion dq = m_rot * w;
	dq.x *= 0.5f * dt;
	dq.y *= 0.5f * dt;
	dq.z *= 0.5f * dt;
	dq.w *= 0.5f * dt;

	m_rot.x += dq.x;
	m_rot.y += dq.y;
	m_rot.z += dq.z;
	m_rot.w += dq.w;

	Normalize();

	ClearAccum();
}
