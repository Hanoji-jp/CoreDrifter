#include "Silvia.h"

Silvia::Silvia()
{
	// モデル
	m_bodyPath   = "Asset/Data/silvia_body.gltf";
	m_wheelPath  = "Asset/Data/Car_Wheel.gltf";
	m_tuningName = "Silvia Tuning";
	m_saveKey    = "Silvia";

	// 車体の前後が逆なので180度補正
	m_bodyYaw = 3.14159265f;

	// タイヤ配置(モデルに合わせた初期値。パネルで微調整)
	m_track  = 0.72f;
	m_base   = 1.27f;
	m_wheelH = 0.24f;
	m_camber = 0.10f;

	// 性能ステータス(Silvia / FR・ドリフト向け)
	m_enginePower = 12.0f;   // 駆動加速(m/s^2相当)。後輪グリップ上限付近＝踏み過ぎで空転
	m_brakePower  = 18.0f;   // ブレーキ/後退
	m_maxSpeed    = 65.0f;   // 最高速(≈234km/h)
	m_drag        = 0.07f;   // 抵抗を下げて高速まで伸びる

	// CarX風タイヤ(食うドリフト。氷にならないよう全体グリップ高め)
	m_muFront   = 1.50f;   // 前グリップ高め(頭が入る)
	m_muRear    = 1.35f;   // 後もしっかり食う＝角度を保つ制御しやすいドリフト
	m_tireB     = 13.0f;   // 剛性高め=タイヤが素早く食う(氷感を消す)
	m_tireC     = 1.5f;
	m_izz       = 1.7f;
	m_cgHeight  = 0.55f;

	// 駆動輪の空転(摩擦円)
	m_longStiff    = 24.0f;
	m_wheelInertia = 1.8f;   // 空転しにくく＝アクセルがちゃんとグリップして進む
}
