#pragma once

#include "../../Const/CarChoiceConst.h"

class CarBase;

//==========================================================
// HjCarPortrait
//   車庫で見せる車の絵。
//
//   ■ なぜレンダーターゲットへ描くか
//   スプライトは3Dより後に描かれる。車をそのまま3Dで置くと、
//   車庫の背景(紙・ドット・緑の台)を先に描けないので、
//   車が必ず背景の下に潜る。
//
//   一度別の絵に描いてから、UIの好きな順番で貼る。
//
//   ■ 背景は透明で抜く
//   台の形は UI 側が決める。ここで下地を塗ると、
//   台を大きくするたびにこちらの色も直すことになる
//==========================================================
class HjCarPortrait : public KdGameObject
{
public:
	void Init()    override;
	void Update()  override;
	void PreDraw() override;

	// 見せる車を決める。同じ車なら読み直さない
	void SetCar(CarChoiceConst::Kind kind);

	// 描き上がった絵。まだ無ければ nullptr
	const KdTexture* GetTexture() const;

private:
	// 車を絵の中に納める行列を作る
	void SetupCamera();

	KdRenderTargetPack m_rt;

	std::shared_ptr<CarBase> m_car;
	CarChoiceConst::Kind     m_kind = CarChoiceConst::Kind::Count;   // 未設定

	KdCamera m_cam;

	// 見せる向き。止まった絵だと模型に見えるのでゆっくり回す
	float m_spin = 0.0f;
};
