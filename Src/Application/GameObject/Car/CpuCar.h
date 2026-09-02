#pragma once

#include "Silvia.h"
#include "HjChaseDriver.h"

//==========================================================
// CpuCar
//   追走で相手をする車。自分で運転する。
//
//   ■ 物理は本物を回す
//   通信で来た車(RemoteCar)は物理を止めて、届いた答えを置くだけだった。
//   こちらは止めない。ぶつかれば飛ぶし、スピンもする。
//   運転する人(HjChaseDriver)が差し替わっただけで、車としては同じ。
//
//   ■ 手本は外から渡す
//     後追い … プレイヤーの生きた跡
//     先行   … 録画した跡
//   どちらも同じ形なので、この車はどちらが来たかを知らない。
//==========================================================
class CpuCar : public Silvia
{
public:
	void Init()   override;
	void Update() override;

	// 追いかける相手の跡。持ち主は外なので借りるだけ
	void SetTrail(const HjCarTrail* trail) { m_pTrail = trail; }

	// 調整のために中を見る
	const HjChaseDriver::Debug& GetDriverDebug() const { return m_driver.GetDebug(); }

	// 走り直すとき
	void ResetDriver() { m_driver.Reset(); }

protected:
	// 人ではなく、追いかける処理が作った値を返す
	DriveInput ReadInput() override;

private:
	HjChaseDriver     m_driver;

	// 借りているだけ。持ち主は場面の側。
	// 生ポインタだが所有しないことを型で示している
	const HjCarTrail* m_pTrail = nullptr;

	// 出来上がった操作。ReadInput が呼ばれたときに返す
	DriveInput m_input;
};
