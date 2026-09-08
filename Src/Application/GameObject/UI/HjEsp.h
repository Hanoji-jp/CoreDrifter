#pragma once

#include "../../Const/CheatConst.h"

class CarBase;

//==========================================================
// HjEsp
//   相手の位置を、地形の向こうからでも見えるように出す。
//   自作なので Hj 接頭辞。
//
//   ■ なぜ物として置くか
//   デバッグ線は KdGameObject が持つ仕組みで出されるので、
//   物でないとそもそも出せない。
//
//   ■ なぜ深度を切るか
//   そのまま描くと地形に隠れる。隠れるなら見る意味がない。
//   自分で描く番のあいだだけ深度を切って、すぐ戻す。
//
//   ■ 誰を出すか
//   場面が「出す相手」を毎フレーム渡す。
//   ここが相手一覧の持ち方を知ると、通信の作りに縛られる。
//==========================================================
class HjEsp : public KdGameObject
{
public:
	// 出す相手を入れ直す。毎フレーム、場面から呼ぶ
	void Clear() { m_targets.clear(); }
	void Add(const std::weak_ptr<CarBase>& car) { m_targets.push_back(car); }

	// 見ている位置。色と距離の出し方に使う
	void SetEye(const Math::Vector3& eye) { m_eye = eye; }

	void DrawDebug() override;

	// 止めている間も動くと名乗る。
	// でないと、メニューを開いている間だけ消える
	bool UpdatesWhileFrozen() const override { return true; }

	// 場面側のカリングから外す。原点に居ないので既定の球では消える
	bool CheckInScreen(const DirectX::BoundingFrustum&) const override { return true; }

private:
	// 1台ぶんを積む
	void PushOne(const Math::Vector3& pos, float dist);

	std::vector<std::weak_ptr<CarBase>> m_targets;

	Math::Vector3 m_eye;
};
