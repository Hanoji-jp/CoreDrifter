#include "TitleShowcase.h"

namespace SC = ShowcaseConst;

void TitleShowcase::Init()
{
	// 走行画面と同じ読み込み。モデルも調整パネルの色もそのまま入る
	Silvia::Init();

	// 物理を止める。タイトルで走り出す必要はない
	SetHalted(true);

	// 音は鳴らさない。タイトルではBGMが流れているので、
	// そこへアイドリングが重なると何を聞いているのか分からなくなる
	StopAudio();

	// 展示用のカメラ。
	// ※画角は「度」で渡す。ラジアンに直すと極端な望遠になり何も映らない
	m_spCamera = std::make_shared<KdCamera>();
	m_spCamera->SetProjectionMatrix(SC::CamFovDeg, SC::CamFar, SC::CamNear);
}

void TitleShowcase::Update()
{
	const float dt = KdFPSController::GetDt();
	if (dt <= 0.0f) { return; }

	// 回転台に載せたように回す。
	// カメラを回すより、背景が動かないぶん落ち着いて見える
	m_turn += SC::TurnSpeed * dt;

	constexpr float TwoPi = 6.28318530f;
	if (m_turn > TwoPi) { m_turn -= TwoPi; }

	// 位置と向きだけ入れる。物理は止めてあるので、これが答えになる。
	// 通信で来た車と同じ入口を使う(物理を回さずに見た目だけ決める道)
	ApplyVisualState(Math::Vector3::Zero, m_turn, Math::Vector3::Zero,
	                 0.0f, 0.0f, 0.0f);

	// 少し傾けて置く。真横から見ると平面的になる
	ApplyVisualTilt(DirectX::XMConvertToRadians(SC::TiltDeg), 0.0f, 0.0f, 0.0f);
}

void TitleShowcase::PreDraw()
{
	// ※基底の PreDraw は呼ばない。
	//   あちらはタイヤ痕を焼き付けマップへ書き込む処理で、
	//   レンダーターゲットを差し替える。走ってもいないタイトルで
	//   その切り替えを走らせる理由がない。

	if (!m_spCamera) { return; }

	// 斜め前・少し上から見る。真横だと平面的になる
	Math::Vector3 eye(SC::CamDist * 0.55f, SC::CamHeight, -SC::CamDist);
	Math::Vector3 at(0.0f, SC::CamLookY, 0.0f);

	// カメラのワールド行列を軸から直接組む(左手系: X=右, Y=上, Z=前)。
	// CreateWorld は前方の扱いが違うので、渡すとカメラが反対を向く。
	// ChaseCamera と同じ組み方に揃えておく。
	Math::Vector3 fwd = at - eye;
	fwd.Normalize();
	Math::Vector3 right = Math::Vector3::Up.Cross(fwd);
	right.Normalize();
	const Math::Vector3 up = fwd.Cross(right);

	// 画面上の置き場所をずらす。
	//
	// カメラを逆方向へ平行移動すれば、車が画面上で動く。
	// 車そのものを動かすと、回転台の中心までずれて回り方が変わる。
	//
	// ずれの量はデザイン座標で指定してあるので、
	// 「画面のこの位置」からワールドの距離へ直す。
	// 画面の高さいっぱいが、この距離では何メートルに見えるかを出して、
	// 1ピクセルあたりの長さを求める。
	{
		const float halfH = SC::CamDist
		                  * tanf(DirectX::XMConvertToRadians(SC::CamFovDeg) * 0.5f);
		const float perPx = (halfH * 2.0f) / UIConst::DesignH;

		const Math::Vector3 shift = right * (-SC::ScreenOffsetX * perPx)
		                          + up    * (-SC::ScreenOffsetY * perPx);
		eye += shift;
		at  += shift;
	}

	// ずらしたので、向きを作り直す
	fwd = at - eye;
	fwd.Normalize();
	right = Math::Vector3::Up.Cross(fwd);
	right.Normalize();
	const Math::Vector3 up2 = fwd.Cross(right);

	Math::Matrix camWorld = Math::Matrix::Identity;
	camWorld._11 = right.x; camWorld._12 = right.y; camWorld._13 = right.z;
	camWorld._21 = up2.x;   camWorld._22 = up2.y;   camWorld._23 = up2.z;
	camWorld._31 = fwd.x;   camWorld._32 = fwd.y;   camWorld._33 = fwd.z;
	camWorld._41 = eye.x;   camWorld._42 = eye.y;   camWorld._43 = eye.z;

	m_spCamera->SetCameraMatrix(camWorld);
	m_spCamera->SetToShader();
}
