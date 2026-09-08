#include "HjEsp.h"

#include "HjCheats.h"
#include "../Car/CarBase.h"

namespace CH = CheatConst;

//----------------------------------------------------------
// 1台ぶんを積む
//
// 箱と、上へ伸びる柱。
// 遠くでは箱が点になるので、柱が無いと見つけられない
//----------------------------------------------------------
void HjEsp::PushOne(const Math::Vector3& pos, float dist)
{
	// 近いほど赤、遠いほど青。
	// 同じ色だと、どれが近いのか分からない
	const float t = std::clamp(
		(dist - CH::NearDist) / std::max(CH::FarDist - CH::NearDist, 1.0f),
		0.0f, 1.0f);

	const Math::Color col(1.0f - t, 0.35f, 0.25f + t * 0.75f, 1.0f);

	const Math::Vector3 c(pos.x, pos.y + CH::BoxLift, pos.z);

	// 箱。8つの角を12本の線で結ぶ
	const float hx = CH::BoxHalfW;
	const float hy = CH::BoxHalfH;
	const float hz = CH::BoxHalfD;

	Math::Vector3 v[8];
	for (int i = 0; i < 8; ++i)
	{
		v[i] = Math::Vector3(
			c.x + ((i & 1) ? hx : -hx),
			c.y + ((i & 2) ? hy : -hy),
			c.z + ((i & 4) ? hz : -hz));
	}

	// 同じ軸の対だけを結ぶ。ビットが1つだけ違う組が辺になる
	for (int i = 0; i < 8; ++i)
	{
		for (int bit = 1; bit < 8; bit <<= 1)
		{
			const int j = i | bit;
			if (j == i) { continue; }

			m_pDebugWire->AddDebugLine(v[i], v[j], col);
		}
	}

	// 自分から相手へ線を引く。
	// 箱だけだと、遠くで点になったときに見つけられない
	if (CH::DrawTracer)
	{
		const Math::Vector3 from(
			m_eye.x, m_eye.y + CH::TracerLift, m_eye.z);

		m_pDebugWire->AddDebugLine(from, c, col);
	}
}

//----------------------------------------------------------
// 出す
//
// 深度を切ってから描く。
// そのままだと地形に隠れるので、隠れるなら見る意味がない
//----------------------------------------------------------
void HjEsp::DrawDebug()
{
	if (!HjCheats::Instance().IsEsp()) { return; }
	if (m_targets.empty()) { return; }

	if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }

	for (const auto& wp : m_targets)
	{
		const auto car = wp.lock();
		if (!car) { continue; }

		const Math::Vector3 pos = car->GetPos();
		const float dist = (pos - m_eye).Length();

		// 遠すぎるものは出さない。全部出すと画面が線だらけになる
		if (dist > CH::MaxRange) { continue; }

		PushOne(pos, dist);
	}

	// 自分の番のあいだだけ深度を切る。
	// 戻さないと、このあとに描かれるものが全部前へ出る
	KdShaderManager::Instance().ChangeDepthStencilState(KdDepthStencilState::ZDisable);

	KdGameObject::DrawDebug();

	KdShaderManager::Instance().UndoDepthStencilState();
}
