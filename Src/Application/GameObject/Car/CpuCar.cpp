#include "CpuCar.h"

void CpuCar::Init()
{
	Silvia::Init();

	// 差し替えは読み込まない。
	// 保存ファイルは車種ごとなので、読むとCPUの車まで
	// プレイヤーの差し替えになってしまう
	m_useModChoice = false;

	// 見分けが付くよう、色を変えておく。
	// 同じ色だと追走の映像でどちらが自分か分からない
	m_smokeColor  = Math::Vector3(0.30f, 0.65f, 1.00f);
	m_smokeColorB = Math::Vector3(0.15f, 0.30f, 0.85f);
	m_outlineColor = Math::Vector3(0.10f, 0.35f, 0.90f);
}

//----------------------------------------------------------
// 人ではなく、追いかける処理が作った値を返す
//----------------------------------------------------------
CarBase::DriveInput CpuCar::ReadInput()
{
	return m_input;
}

//----------------------------------------------------------
void CpuCar::Update()
{
	const float dt = KdFPSController::GetDt();

	// 手本が無いうちは何もしない。
	// 何か走らせると、目標が来た瞬間に飛ぶ
	m_input = DriveInput();

	if (m_pTrail && !m_pTrail->Empty())
	{
		HjChaseDriver::Output o;
		m_driver.Update(dt, *this, *m_pTrail, o);

		m_input.throttle  = o.throttle;
		m_input.steer     = o.steer;
		m_input.handbrake = o.handbrake;
		m_input.shiftUp   = o.shiftUp;
		m_input.shiftDown = o.shiftDown;
	}

	// 物理はそのまま回す。
	// 運転する人が差し替わっただけで、車としてはプレイヤーと同じ
	Silvia::Update();
}
