#include "HjCarPortrait.h"

#include "../Car/CarBase.h"
#include "../Car/HjCarChoice.h"
#include "../../Const/GarageConst.h"

namespace GC = GarageConst;

namespace
{
	constexpr float Deg = 3.14159265f / 180.0f;
}

void HjCarPortrait::Init()
{
	// 深度も要る。深度なしだと後から描いた面が手前に出て、
	// 車の内側(座席や反対側のドア)が外から見える
	m_rt.CreateRenderTarget(GC::PortraitW, GC::PortraitH, true);

	const float aspect = static_cast<float>(GC::PortraitW)
	                   / static_cast<float>(GC::PortraitH);

	m_cam.SetProjectionMatrix(GC::PortraitFovDeg, 100.0f, 0.05f, aspect);
}

void HjCarPortrait::SetCar(CarChoiceConst::Kind kind)
{
	if (m_car && m_kind == kind) { return; }

	m_kind = kind;

	// 音もタイヤ痕の枠も要らないので Init() は呼ばない。
	// 設定を当てて、モデルだけ読む
	m_car = std::make_shared<CarBase>();
	HjCarChoice::ApplySpec(*m_car, kind);
	m_car->LoadPreviewModels();
}

void HjCarPortrait::Update()
{
	m_spin += KdFPSController::GetDt() * GC::PortraitSpinDeg * Deg;
}

const KdTexture* HjCarPortrait::GetTexture() const
{
	return m_rt.m_RTTexture ? m_rt.m_RTTexture.get() : nullptr;
}

//----------------------------------------------------------
void HjCarPortrait::SetupCamera()
{
	const float p = GC::PortraitPitch * Deg;

	// 車の実寸から、いつも同じ大きさに映る距離を出す。
	//
	// 車を回すので、向きによって幅が変わる。
	// 包む球で測っておけば、どの向きでも枠からはみ出さない
	float lookY = GC::PortraitDistFallback * 0.05f;
	float dist  = GC::PortraitDistFallback;

	Math::Vector3 center;
	float radius = 0.0f;
	if (m_car && m_car->GetBodyBounds(center, radius) && radius > 0.0001f)
	{
		lookY = center.y;

		// 画面の高さの PortraitFill を車で埋める。
		// 縦のほうが狭いので、縦で合わせれば横は必ず収まる
		const float halfFov = GC::PortraitFovDeg * 0.5f * Deg;
		dist = radius / (std::tan(halfFov) * GC::PortraitFill);
	}

	// 車の少し上、手前(-Z)から見下ろす位置。
	// 前へ dist だけ進むとちょうど注視点に着く
	const Math::Vector3 eye = {
		0.0f,
		lookY + dist * std::sin(p),
		      - dist * std::cos(p),
	};

	// Matrix::CreateLookAt は使わない。
	// DirectXTK のあれは右手系だが、この枠組みの射影行列は
	// XMMatrixPerspectiveFovLH で左手系。混ぜると被写体がカメラの
	// 後ろへ回って、何も映らない絵ができる。
	//
	// 他のカメラと同じく 回転×平行移動 で組む
	const Math::Matrix cam =
		Math::Matrix::CreateRotationX(p) *
		Math::Matrix::CreateTranslation(eye);

	m_cam.SetCameraMatrix(cam);

	// 遠くまで見る必要はないが、大きい車では距離もそれだけ伸びる。
	// 奥行きの範囲を距離に合わせておかないと、車の後ろ半分が切れる
	const float aspect = static_cast<float>(GC::PortraitW)
	                   / static_cast<float>(GC::PortraitH);

	m_cam.SetProjectionMatrix(GC::PortraitFovDeg, dist * 4.0f, dist * 0.05f, aspect);
	m_cam.SetToShader();
}

//----------------------------------------------------------
void HjCarPortrait::PreDraw()
{
	if (!m_car || !m_rt.m_RTTexture) { return; }

	// 元のカメラを覚えておく。
	// カメラの定数バッファは画面で1つしかないので、
	// 書き換えたまま抜けると本編がこのカメラで描かれる
	const auto saved = KdShaderManager::Instance().GetCameraCB();

	{
		KdRenderTargetChanger rtc;
		rtc.ChangeRenderTarget(m_rt);

		// 背景は透明で抜く。台の色はUI側が塗る
		m_rt.ClearTexture({ 0.0f, 0.0f, 0.0f, 0.0f });

		SetupCamera();

		auto& amb = KdShaderManager::Instance().WorkAmbientController();
		amb.SetDirLight(
			Math::Vector3(GC::PortraitLightDir[0], GC::PortraitLightDir[1], GC::PortraitLightDir[2]),
			Math::Vector3(GC::PortraitLightCol[0], GC::PortraitLightCol[1], GC::PortraitLightCol[2]));
		amb.SetAmbientLight(
			Math::Vector4(GC::PortraitAmbient[0], GC::PortraitAmbient[1], GC::PortraitAmbient[2], 1.0f));

		auto& shader = KdShaderManager::Instance().m_StandardShader;
		shader.BeginLit();
		{
			m_car->DrawPortrait(Math::Matrix::CreateRotationY(m_spin));
		}
		shader.EndLit();

		rtc.UndoRenderTarget();
	}

	// カメラを戻す。CamPos は行列から取り直されるので、
	// 覚えたビュー行列を逆にして渡す
	KdShaderManager::Instance().WriteCBCamera(saved.mView.Invert(), saved.mProj);
}
