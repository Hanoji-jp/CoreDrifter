#pragma once

#include "../../Physics/HjRigidBody.h"

class HjHeightField;
class HjRoad;

//==========================================================
// HjCarRigid
//   剛体の車。自作なので Hj 接頭辞。
//
//   ■ 担当は「車体の姿勢と接地」
//   車体から真下へレイを飛ばして接地点を求め、そこへバネと
//   減衰の力をかける。力を4点それぞれへかけるので、
//   ブレーキで前が沈み、曲がると外輪に荷重が乗る。
//   片輪が浮くし、転倒もする。
//
//   車輪まで剛体にすると拘束を解く仕組みが要る。
//   走りの絵として要るのは「接地点に力がかかること」だけ。
//
//   ■ タイヤと駆動は旧モデルの式をそのまま使う
//   ドリフトの手ざわりは平面モデル(CarBase)側で作り込んであり、
//   そちらのほうが乗りやすい。だから力の出し方は変えず、
//   荷重の出どころだけ差し替える。
//
//   旧モデルは荷重を式で前後左右へ振っていた(加速度から計算)。
//   こちらはサスのばねが出した実際の荷重を使うので、
//   縁石・轍・片輪の浮きがそのままグリップに出る。
//
//   ■ エンジンは持たない
//   エンジン・クラッチ・変速は CarBase が解く。
//   こちらは「後輪へ入る駆動(m/s^2)」を受け取るだけ。
//   両方に持たせると、調整したときに片方だけ変わる
//==========================================================
class HjCarRigid
{
public:
	//===== 1フレームの入力 =====
	struct Input
	{
		// -1〜1。負はブレーキ(後退ギアに入っていればアクセル)
		float throttle = 0.0f;

		// 実舵角(rad)。オートカウンターを含んだ最終値
		float steer = 0.0f;

		bool handbrake = false;

		//===== 駆動系の結果 =====
		// 後輪合計の駆動(m/s^2)。旧モデルの m_driveAccel
		float driveAccel = 0.0f;

		bool reverse      = false;   // 後退ギアに入っているか
		bool accelPressed = false;   // 駆動方向へ踏んでいるか
		bool clutchOut    = false;   // クラッチが切れているか
	};

	//===== 車ごとの調整 =====
	// 旧モデルの調整値をそのまま受け取る。
	//
	// 名前を変えると、調整パネルのどれがどこに効くのか
	// 分からなくなる。単位も旧モデルに合わせてある
	struct Setup
	{
		float track = 0.78f;   // 左右の半分(m)
		float base  = 1.62f;   // 前後の半分(m)

		//----- タイヤ -----
		float muFront = 1.0f;
		float muRear  = 1.0f;
		float tireB   = 8.0f;        // 簡易Pacejkaの立ち上がり
		float tireC   = 1.6f;        // 同じく形
		float tireLoadSens = 0.0f;   // 荷重感度
		float tireRelaxLen = 0.5f;   // リラクゼーション長(m)
		float slipEps      = 2.0f;   // 低速でスリップ角が暴れないための下駄

		float camber     = 0.0f;
		float camberGrip = 0.0f;

		float handbrakeGripMul = 0.5f;   // サイド中の後輪グリップ倍率

		//----- アライメント -----
		float toeFront  = 0.0f;
		float toeRear   = 0.0f;
		float ackermann = 0.0f;

		//----- 駆動輪 -----
		float longStiff    = 20.0f;   // 滑り速度から縦力を出す係数
		float wheelInertia = 1.0f;
		float driveRelax   = 5.0f;    // 惰行中に路面速へ戻る速さ
		float lsdLock      = 5.0f;    // デフの効き

		//----- 抵抗 -----
		float drag      = 0.02f;
		float scrubDrag = 1.0f;
		bool  scrubDragEnabled = true;

		//----- 力 -----
		float brakePower = 20.0f;
		float maxSpeed   = 80.0f;
		float yawDamp    = 0.5f;

		//----- 空力 -----
		float downforceCoef     = 0.0f;
		float downforceRearBias = 0.5f;

		//----- サス(剛体だけが使う) -----
		float springF = 1.0f;
		float springR = 1.0f;
		float arbF    = 1.0f;
		float arbR    = 1.0f;
	};

	//===== 接地の問い合わせ先 =====
	// 高さマップと道。どちらも借りるだけ
	void SetGround(const HjHeightField* field, const HjRoad* road)
	{
		m_pField = field;
		m_pRoad  = road;
	}

	void SetSetup(const Setup& s) { m_setup = s; Init(); }

	// 諸元を入れて初期化する
	void Init();

	// 置き直す。姿勢は水平・向きだけ指定
	void Place(const Math::Vector3& pos, float yaw);

	// 進める。dt が大きいときは中で分割する
	void Step(const Input& in, float dt);

	//===== 状態を読む =====
	const HjRigidBody& Body() const { return m_body; }

	Math::Vector3    Pos() const { return m_body.Pos(); }
	Math::Quaternion Rot() const { return m_body.Rot(); }
	Math::Vector3    Vel() const { return m_body.Vel(); }

	// 車体の向き(ヨー)。音と見た目が要る
	float Yaw() const;

	// 進む向きに対する速度と横滑り
	float ForwardSpeed() const;
	float SlipAngle() const;

	// 何輪が接地しているか。0なら空中
	int GroundedCount() const { return m_grounded; }

	// タイヤの滑り(0〜1)。煙と音に使う
	float SlipFront() const { return m_slipFront; }
	float SlipRear()  const { return m_slipRear; }

	// 見た目のサスの沈み(m)。0=伸びきり
	float Compression(int wheel) const;

	//===== 駆動輪の回転 =====
	// 接地面での速さ(m/s)。旧モデルの m_driveSpeed と同じもの。
	//
	// エンジン側(CarBase)と往復する。クラッチは向こうが解き、
	// 路面からの反力はこちらが解くので、片方だけでは閉じない
	float DriveSpeed() const { return m_driveSpeed; }
	float DriveDiff()  const { return m_driveDiff; }

	void SetDrive(float speed, float diff) { m_driveSpeed = speed; m_driveDiff = diff; }

	//===== ヨーへの口 =====
	// アシスト(スピン防止・振り返し・車体アライン)は CarBase が持つ。
	// 平面モデルではヨーを直に触っていたので、剛体でも同じ口を開ける
	float YawRate() const { return m_body.AngVel().y; }

	void AddYawRate(float dw);
	void DampYaw(float k);      // k は 0〜1 の割合
	void RotateYaw(float d);    // 車体の向きだけ回す

	// 転倒しているか
	bool IsFlipped() const;

	// 転倒したまま一定時間たったか。場面が拾って戻す
	bool ConsumeNeedReset();

	//===== 外から力を入れる =====
	// 衝突の撃力。壁や他の車から
	void AddImpulseAt(const Math::Vector3& j, const Math::Vector3& at)
	{
		m_body.AddImpulseAt(j, at);
	}

private:
	// 車輪1つぶんの状態
	struct Wheel
	{
		// 車体ローカルでの取り付け位置(サスの上端)
		Math::Vector3 mount;

		bool  front = false;
		bool  left  = false;

		// 今回の接地
		bool          hit = false;
		Math::Vector3 hitPos;
		Math::Vector3 hitNormal = Math::Vector3::Up;

		// サスの縮み(m)。0=伸びきり
		float compress = 0.0f;
		float prevCompress = 0.0f;

		// 縮み代を使い切った先へ入った量(m)。バンプラバーの潰れ
		float bump = 0.0f;
		float prevBump = 0.0f;

		// 垂直荷重(N)
		float load = 0.0f;

		// 滑り(0〜1)
		float slip = 0.0f;

		// 横力(N)。リラクゼーション長で遅らせるので持ち越す
		float fy = 0.0f;
	};

	// 1刻みぶん
	void SubStep(const Input& in, float dt);

	// 接地点と荷重を出す。サスのばねと減衰、スタビまで
	void StepSuspension(float dt);

	// タイヤの力をかけ、駆動輪の回転を進める
	void StepTires(const Input& in, float dt);

	// 車体にかかる抵抗と空力
	void StepBodyForces(const Input& in, float dt);

	// 接地点を探す。見つからなければ hit=false。
	//
	// dir はサスの向き(車体の下方向)。真下ではない。
	// 車体が傾けば、飛ばす向きも傾く
	bool Probe(const Math::Vector3& from, const Math::Vector3& dir, float len,
	           Math::Vector3& outPos, Math::Vector3& outNormal) const;

	// 地面の高さと向きを引く
	bool SampleGround(float x, float z, float& outY, Math::Vector3& outN) const;

	// めり込みを戻す
	void PushOut();

	HjRigidBody m_body;

	Wheel m_wheel[4];

	Setup m_setup;

	const HjHeightField* m_pField = nullptr;
	const HjRoad*        m_pRoad  = nullptr;

	//===== 駆動輪の回転 =====
	// 接地面での速さ(m/s)と、左右の差の半分
	float m_driveSpeed = 0.0f;
	float m_driveDiff  = 0.0f;

	int   m_grounded  = 0;
	float m_slipFront = 0.0f;
	float m_slipRear  = 0.0f;

	// 転倒している時間(秒)
	float m_flipTime = 0.0f;
	bool  m_needReset = false;
};
