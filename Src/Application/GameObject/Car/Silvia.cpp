#include "Silvia.h"

Silvia::Silvia()
{
	Setup(*this);
}

//----------------------------------------------------------
// 車種ごとの設定
//
// 相手の車は後から車種が決まるので、作り方から切り離しておく
//----------------------------------------------------------
void Silvia::Setup(CarBase& car)
{
	// モデル
	car.m_bodyPath   = "Asset/Data/silvia_body.gltf";
	car.m_wheelPath  = "Asset/Data/Car_Wheel.gltf";
	car.m_tuningName = "Silvia Tuning";
	car.m_saveKey    = "Silvia";

	// 車体の前後が逆なので180度補正
	car.m_bodyYaw = 3.14159265f;

	// ドリフトスモークの色(Unbound風。根元=鮮やかな緑 → 先端=深い緑へグラデ)
	// 明暗のコントラストはトゥーン陰影が作るので、色自体は同系色でまとめる。
	// 先端も彩度を保つ(暗くくすませると古い粒が溜まる上部が濁って見える)
	car.m_smokeColor  = Math::Vector3(0.55f, 1.00f, 0.30f);   // 明るい黄緑
	car.m_smokeColorB = Math::Vector3(0.18f, 0.78f, 0.42f);   // 少し深い緑(彩度は保つ)

	// タイヤ配置(モデルに合わせた初期値。パネルで微調整)
	car.m_track  = 0.72f;
	car.m_base   = 1.27f;
	car.m_wheelH = 0.24f;
	car.m_camber = 0.10f;

	// 性能ステータス(Silvia / FR・ドリフト向け)
	car.m_enginePower = 12.0f;   // 駆動加速(m/s^2相当)。後輪グリップ上限付近＝踏み過ぎで空転
	car.m_brakePower  = 18.0f;   // ブレーキ/後退
	car.m_maxSpeed    = 65.0f;   // 最高速(≈234km/h)
	car.m_drag        = 0.07f;   // 抵抗を下げて高速まで伸びる

	// CarX風タイヤ(食うドリフト。氷にならないよう全体グリップ高め)
	car.m_muFront   = 1.50f;   // 前グリップ高め(頭が入る)
	car.m_muRear    = 1.35f;   // 後もしっかり食う＝角度を保つ制御しやすいドリフト
	car.m_tireB     = 13.0f;   // 剛性高め=タイヤが素早く食う(氷感を消す)
	car.m_tireC     = 1.5f;
	car.m_izz       = 1.7f;
	car.m_cgHeight  = 0.55f;

	// 駆動輪の空転(摩擦円)
	car.m_longStiff    = 24.0f;
	car.m_wheelInertia = 1.8f;   // 空転しにくく＝アクセルがちゃんとグリップして進む
}
