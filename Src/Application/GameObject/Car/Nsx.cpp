#include "Nsx.h"

Nsx::Nsx()
{
	Setup(*this);
}

//----------------------------------------------------------
// 車種ごとの設定
//
// 相手の車は後から車種が決まるので、作り方から切り離しておく
//----------------------------------------------------------
void Nsx::Setup(CarBase& car)
{
	// モデル。
	// 配布されているものは4輪が車体に埋まっていて、そのままだと
	// タイヤが回らない。車体側から4輪を取り除いたものと、
	// 車輪1つを原点へ置いたものに分けてある
	car.m_bodyPath   = "Asset/Data/nsx/nsx_body.gltf";
	car.m_wheelPath  = "Asset/Data/nsx/nsx_wheel.gltf";
	car.m_tuningName = "NSX Tuning";
	car.m_saveKey    = "Nsx";

	// 読み込みのときにZが反転するので、前後が逆になる。
	// シルビアと同じ理由で180度補正
	car.m_bodyYaw = 3.14159265f;

	// モデルの実寸から出した値。
	//
	// 元のモデルは 全長3.098 / 半トレッド0.556 / 半ホイールベース0.867。
	// シルビアの半ホイールベース1.49 に合わせると倍率は約1.72。
	// この倍率だと半トレッドも車輪の半径も、シルビアとほぼ同じになる。
	//
	// ※並べたときに片方だけ大きい、という状態を避けるため、
	//   見た目の好みではなく寸法で合わせている
	car.m_bodyScale  = 1.72f;
	car.m_wheelScale = 1.72f;

	car.m_track  = 0.955f;
	car.m_base   = 1.490f;
	car.m_wheelH = 0.374f;
	car.m_camber = 0.07f;

	// 前後で車輪の位置が対称でない(前1.439 / 後1.542)。
	// 中間を base にして、その差をここで戻す
	car.m_frontOffZ = -0.051f;
	car.m_rearOffZ  = -0.052f;

	// ドリフトスモークの色。シルビアの緑に対して、こちらは赤寄り。
	// 色で車種を見分けられるほうが、走っていて分かりやすい
	car.m_smokeColor  = Math::Vector3(1.00f, 0.55f, 0.30f);   // 明るい橙
	car.m_smokeColorB = Math::Vector3(0.78f, 0.20f, 0.24f);   // 深い赤(彩度は保つ)

	// 性能ステータス(NSX / MR・自然吸気)
	car.m_enginePower = 11.0f;   // シルビアよりわずかに穏やか
	car.m_brakePower  = 19.0f;   // 車重が軽く、止まりはよい
	car.m_maxSpeed    = 68.0f;   // 最高速は伸びる(≈245km/h)
	car.m_drag        = 0.065f;

	// ミッドシップの味付け。
	//
	// 重心が後ろ寄りなので後輪に荷重が乗る。
	// 後ろが出にくく、出たあとは戻しにくい。
	// 前を強くしすぎると、ただ曲がる車になってFRとの差が消える
	car.m_muFront   = 1.42f;
	car.m_muRear    = 1.45f;   // 前より後を強く＝出にくい
	car.m_tireB     = 13.5f;
	car.m_tireC     = 1.5f;

	// 短いホイールベースと低い重心。
	// 回頭は速いが、いったん回り出すと止めにくい
	car.m_izz       = 1.5f;
	car.m_cgHeight  = 0.48f;

	// 駆動輪の空転
	car.m_longStiff    = 25.0f;
	car.m_wheelInertia = 1.7f;
}
