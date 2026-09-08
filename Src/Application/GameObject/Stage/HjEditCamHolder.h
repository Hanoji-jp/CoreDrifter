#pragma once

#include "../Camera/HjEditorCamera.h"
#include "HjTerrainBrush.h"

class HjRoad;
class HjRoadEditor;
class HjTerrainBrush;

//==========================================================
// HjEditCamHolder
//   編集中の描画をまとめて受け持つ。自作なので Hj 接頭辞。
//
//   ■ なぜ物として置くか
//   カメラも、デバッグ線も、描画の番でないと効かない。
//
//   カメラは PreDraw で決まるので、走行中のカメラより後に
//   自分のものを渡す必要がある。
//   デバッグ線は KdGameObject が持つ仕組みで出されるので、
//   物でないとそもそも出せない。
//
//   場面から直接呼ぼうとすると、物の描画が終わったあとに
//   割り込む場所が要る。物として一番後ろに足すほうが素直。
//==========================================================
class HjEditCamHolder : public KdGameObject
{
public:
	void PreDraw() override
	{
		if (!m_enabled) { return; }

		// 走行中のカメラを上書きする。
		// 編集中は車を追う必要がない
		if (m_spCam) { m_spCam->SetToShader(); }

		// 制御点の印を積む。
		// 実際に出すのは基底(DrawDebug)なので、ここでは積むだけ
		if (m_pRoad && m_pEditor)
		{
			if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }
			m_pEditor->PushMarker(*m_pRoad, *m_pDebugWire);
		}

		// 筆の輪。どこをどれだけ触るのかが見えないと狙って彫れない
		if (m_pBrush)
		{
			if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }
			m_pBrush->PushRing(*m_pDebugWire);
		}
	}

	void SetCamera(const std::shared_ptr<HjEditorCamera>& cam) { m_spCam = cam; }
	void SetEnabled(bool on) { m_enabled = on; }

	// 印を出す対象。持ち主は場面なので借りるだけ
	void SetRoad(const HjRoad* road, const HjRoadEditor* editor)
	{
		m_pRoad   = road;
		m_pEditor = editor;
	}

	// 筆の輪を出す対象。持ち主は場面なので借りるだけ
	void SetBrush(const HjTerrainBrush* brush) { m_pBrush = brush; }

	// 止めている間も動くと名乗る。
	// でないと、編集中に画面が止まったままになる
	bool UpdatesWhileFrozen() const override { return true; }

	// 場面側のカリングから外す。
	// 原点に居ないので、既定の球では消える
	bool CheckInScreen(const DirectX::BoundingFrustum&) const override { return true; }

private:
	std::shared_ptr<HjEditorCamera> m_spCam;

	const HjRoad*       m_pRoad   = nullptr;
	const HjRoadEditor*   m_pEditor = nullptr;
	const HjTerrainBrush* m_pBrush  = nullptr;

	bool m_enabled = false;
};
