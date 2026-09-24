#include "HjCarPortrait.h"

#include "../Car/CarBase.h"
#include "../Car/HjCarChoice.h"
#include "../../Const/GarageConst.h"
#include "UIConst.h"

#include <algorithm>

namespace GC = GarageConst;

namespace
{
	constexpr float Deg = 3.14159265f / 180.0f;
}

HjCarPortrait::~HjCarPortrait()
{
	// 下げたしきい値を戻す。
	// 定数バッファの控えを書き換えるだけなので、
	// 終了間際に呼ばれても描画は走らない
	KdShaderManager::Instance().m_postProcessShader.SetBrightThreshold(1.2f);
}

//----------------------------------------------------------
void HjCarPortrait::Init()
{
	// 画面へ直に描くので、絵の用意は要らない。
	// カメラは毎フレーム組み直す(車の大きさで距離が変わる)
}

void HjCarPortrait::SetCar(CarChoiceConst::Kind kind)
{
	// 今どの車へ向かっているか。
	// 演出の途中なら、まだ読み込んでいない行き先のほうが「今」になる
	const CarChoiceConst::Kind now =
		(m_swapT >= 0.0f && !m_swapped) ? m_pendingKind : m_kind;

	if (now == kind) { return; }

	// 初めの1台は演出しない。
	// 開いた瞬間からカメラを振ると、何が起きたのか分からない
	if (!m_car)
	{
		ApplyCar(kind);
		return;
	}

	m_pendingKind = kind;

	// まだ始まっていない、または差し替えが済んでいるなら、頭から。
	// 済む前に選び直したときは、行き先だけ差し替えて振りは続ける
	if (m_swapT < 0.0f || m_swapped)
	{
		m_swapT   = 0.0f;
		m_swapped = false;
	}
}

//----------------------------------------------------------
// 車を読み込んで差し替える
//
// 演出の真ん中から呼ばれる。読み込みで1フレーム落ちても、
// カメラが振れている最中なので目立たない
//----------------------------------------------------------
void HjCarPortrait::ApplyCar(CarChoiceConst::Kind kind)
{
	m_kind = kind;

	// 音もタイヤ痕の枠も要らないので Init() は呼ばない。
	// 設定を当てて、モデルだけ読む
	m_car = std::make_shared<CarBase>();
	HjCarChoice::ApplySpec(*m_car, kind);
	m_car->LoadPreviewModels();

	// 場所は車の大きさに合わせて作る。
	// 読み込んでからでないと寸法が取れない
	BuildStage();
}

namespace
{
	// 三角形を1枚積む。
	//
	// 法線を必ず受け取る。既定で上向きにしておくと、壁や
	// 台の側面まで床と同じ明るさになり、面の向きが読めない
	void PushTri(std::vector<KdPolygon::Vertex>& out,
	             const Math::Vector3& a, const Math::Vector3& b,
	             const Math::Vector3& c,
	             const Math::Vector3& n,
	             unsigned int ca = 0xFFFFFFFF,
	             unsigned int cb = 0xFFFFFFFF,
	             unsigned int cc = 0xFFFFFFFF)
	{
		KdPolygon::Vertex v;
		v.normal = n;

		// 接線は法線と平行でなければ何でもよい。
		// 平行にすると従法線が出せず、面が真っ黒になる
		v.tangent = (fabsf(n.y) > 0.9f)
		          ? Math::Vector3(1.0f, 0.0f, 0.0f)
		          : Math::Vector3(0.0f, 1.0f, 0.0f);

		v.pos = a; v.color = ca; out.push_back(v);
		v.pos = b; v.color = cb; out.push_back(v);
		v.pos = c; v.color = cc; out.push_back(v);
	}

	// 水平な長方形。y は固定なので法線は上向き
	void PushQuadY(std::vector<KdPolygon::Vertex>& out,
	               float x0, float z0, float x1, float z1, float y)
	{
		const Math::Vector3 a(x0, y, z0), b(x1, y, z0);
		const Math::Vector3 c(x1, y, z1), d(x0, y, z1);

		PushTri(out, a, b, c, Math::Vector3::Up);
		PushTri(out, a, c, d, Math::Vector3::Up);
	}

	//======================================================
	// 壁に貼る板。手前を向く
	//======================================================
	void PushQuadZ(std::vector<KdPolygon::Vertex>& out,
		   float x0, float y0, float x1, float y1, float z)
	{
		const Math::Vector3 n(0.0f, 0.0f, -1.0f);

		const Math::Vector3 a(x0, y0, z), b(x1, y0, z);
		const Math::Vector3 c(x1, y1, z), d(x0, y1, z);

		PushTri(out, a, b, c, n);
		PushTri(out, a, c, d, n);
	}

	//======================================================
	// 下を向く板。羽根の裏や、物の底に使う
	//======================================================
	void PushQuadDown(std::vector<KdPolygon::Vertex>& out,
		      float x0, float z0, float x1, float z1, float y)
	{
		const Math::Vector3 n(0.0f, -1.0f, 0.0f);

		const Math::Vector3 a(x0, y, z0), b(x1, y, z0);
		const Math::Vector3 c(x1, y, z1), d(x0, y, z1);

		PushTri(out, a, b, c, n);
		PushTri(out, a, c, d, n);
	}

	//======================================================
	// 箱。手前の面と上の面だけ作る。
	// カメラは手前の上にいるので、裏と底は見えない
	//======================================================
	void PushBox(std::vector<KdPolygon::Vertex>& out,
		 float cx, float cy, float cz,
		 float hx, float hy, float hz)
	{
		PushQuadY(out, cx - hx, cz - hz, cx + hx, cz + hz, cy + hy);
		PushQuadZ(out, cx - hx, cy - hy, cx + hx, cy + hy, cz - hz);
	}

	//======================================================
	// 枠。中を抜いた四角。4枚の板で作る
	//======================================================
	void PushFrameZ(std::vector<KdPolygon::Vertex>& out,
		    float cx, float cy, float w, float h, float t, float z)
	{
		const float x0 = cx - w * 0.5f - t, x1 = cx + w * 0.5f + t;
		const float y0 = cy - h * 0.5f - t, y1 = cy + h * 0.5f + t;

		PushQuadZ(out, x0, y0, x1, y0 + t, z);   // 下
		PushQuadZ(out, x0, y1 - t, x1, y1, z);   // 上
		PushQuadZ(out, x0, y0, x0 + t, y1, z);   // 左
		PushQuadZ(out, x1 - t, y0, x1, y1, z);   // 右
	}
}

//----------------------------------------------------------
// 車が立っている場所を作る
//
// ■ なぜ3Dで置くか
// 台(UIの緑)を貼るだけでは、車だけが3Dで浮く。
// 床を同じ空間に置けば、車はその上に立っていることになる。
// 回しても見下ろす角を変えても、遠近は勝手に正しくなる。
//
// 2Dで奥行きを描くと、角度を触るたびに描き直しになる
//----------------------------------------------------------
void HjCarPortrait::BuildStage()
{
	m_floor.clear();
	m_wall.clear();
	m_slat.clear();
	m_slatGap.clear();
	m_frame.clear();
	m_doorIn.clear();
	m_tube.clear();
	m_line.clear();
	m_stop.clear();
	m_disc.clear();
	m_ring.clear();

	Math::Vector3 center;
	float radius = 0.0f;

	if (!m_car || !m_car->GetBodyBounds(center, radius) || radius <= 0.0001f)
	{
		return;
	}

	m_stageRadius = radius;

	// 渡された寸法はメートル。
	// モデルの単位が違っても部屋の比が保つように、
	// 実車ならこれくらいという半径で割った比を全部へ掛ける
	m_scale = radius / GC::RefCarRadius;

	const float S = m_scale;

	const float hw = GC::RoomW * 0.5f * S;   // 左右の端
	const float hd = GC::RoomD * 0.5f * S;   // 手前と奥の端
	const float zw = GC::BackWallZ * S;      // 奥の壁

	//===== 床と奥の壁 =====
	// 地だけは部屋より広げる。
	// 寸法どおりだと、画面の左右の端に何も無い帯が出る
	const float mw = hw + GC::RoomMargin * S;

	PushQuadY(m_floor, -mw, -hd, mw, hd, 0.0f);
	PushQuadZ(m_wall, -mw, 0.0f, mw, GC::RoomH * S, zw);

	//===== シャッター =====
	// 羽根は本当に出っ張らせる。
	// 模様として描くと、上から光が当たったときに平らだと分かる
	{
		const float w  = GC::ShutterW * S;
		const float h  = GC::ShutterH * S;
		const float cy = GC::ShutterY * S;
		const float zs = GC::ShutterZ * S;

		const float x0 = -w * 0.5f, x1 = w * 0.5f;
		const float y0 = cy - h * 0.5f, y1 = cy + h * 0.5f;

		const float d = GC::ShutterDepth * S;

		// 奥の面。羽根と羽根の間から、これが見える
		PushQuadZ(m_slatGap, x0, y0, x1, y1, zs);

		const float pitch = GC::ShutterPitch * S;
		const int   rows  = static_cast<int>(h / pitch);

		for (int i = 0; i < rows; ++i)
		{
			const float by = y0 + pitch * i;
			const float ty = by + pitch * 0.72f;   // 残りが隙間

			// 羽根の面。壁より手前へ出す
			PushQuadZ(m_slat, x0, by, x1, ty, zs - d);

			// 羽根の下の小口。ここが見えると厚みが出る
			PushQuadDown(m_slat, x0, zs - d, x1, zs, by);
		}

		// 枠
		PushFrameZ(m_frame, 0.0f, cy, w, h, GC::ShutterFrame * S, zs - d * 1.5f);
	}

	//===== 通用口 =====
	// 枠だけ。中は引っ込めてあるので影になる
	{
		const float cx = GC::DoorX * S;
		const float cy = GC::DoorY * S;
		const float w  = GC::DoorW * S;
		const float h  = GC::DoorH * S;
		const float zd = GC::DoorZ * S;

		PushQuadZ(m_doorIn, cx - w * 0.5f, cy - h * 0.5f,
				  cx + w * 0.5f, cy + h * 0.5f,
				  zd + GC::DoorRecess * S);

		PushFrameZ(m_frame, cx, cy, w, h, GC::DoorFrame * S, zd);
	}

	//===== 蛍光灯 =====
	// 壁の高い所に5本。滲ませるので、抽出パスにも出す
	{
		const float hl = GC::TubeLen * 0.5f * S;
		const float ht = GC::TubeThick * 0.5f * S;

		const float ty = GC::TubeY * S;
		const float tz = GC::TubeZ * S;

		for (int i = 0; i < GC::TubeCount; ++i)
		{
			const float cx = GC::TubeX[i] * S;

			PushQuadZ(m_tube, cx - hl, ty - ht, cx + hl, ty + ht, tz);
		}
	}

	//===== 床の白線 =====
	// 駐車枠。奥へ向かって狭まって見えるので、奥行きはこれが一番効く
	{
		const float w = GC::LineW * 0.5f * S;
		const float y = 0.004f * S;   // 床のわずか上

		const float bx = GC::BayLineX * S;

		PushQuadY(m_line, -bx - w, -hd, -bx + w, hd, y);
		PushQuadY(m_line,  bx - w, -hd,  bx + w, hd, y);

		const float cz = GC::CrossLineZ * S;

		PushQuadY(m_line, -bx, cz - w, bx, cz + w, y);
	}

	//===== 車止め =====
	PushBox(m_stop, 0.0f, GC::StopY * S, GC::StopZ * S,
		GC::StopW * 0.5f * S, GC::StopH * 0.5f * S, GC::StopD * 0.5f * S);

	//===== 回す台 =====
	{
		const float R = GC::TurnOuterD * 0.5f * S;
		const float y = GC::TurnY * S;

		const float bw = GC::RingBandW * S;

		const float step = 6.2831853f / GC::TurnSeg;

		const Math::Vector3 mid(0.0f, y, 0.0f);

		for (int i = 0; i < GC::TurnSeg; ++i)
		{
			const float a0 = step * i;
			const float a1 = step * (i + 1);

			const float c0 = cosf(a0), s0 = sinf(a0);
			const float c1 = cosf(a1), s1 = sinf(a1);

			// 円盤
			PushTri(m_disc, mid,
				    Math::Vector3(c0 * R, y, s0 * R),
				    Math::Vector3(c1 * R, y, s1 * R),
				    Math::Vector3::Up);

			// 縁の輪。円盤のわずか上
			{
				const float r0 = R - bw;
				const float ry = y + 0.002f * S;

				const Math::Vector3 p0(c0 * r0, ry, s0 * r0);
				const Math::Vector3 p1(c1 * r0, ry, s1 * r0);
				const Math::Vector3 p2(c1 * R,  ry, s1 * R);
				const Math::Vector3 p3(c0 * R,  ry, s0 * R);

				PushTri(m_ring, p0, p1, p2, Math::Vector3::Up);
				PushTri(m_ring, p0, p2, p3, Math::Vector3::Up);
			}
		}
	}

	BuildWallText(radius);
}

//----------------------------------------------------------
// 壁に刷る文字
//
// フォントの絵を1文字ずつ板へ貼って、壁の手前に並べる。
//
// ■ UIの文字とは別物
// あちらは画面へ直に描くので、3Dの空間には置けない。
// こちらは板なので、カメラを回せば一緒に遠近が付く
//----------------------------------------------------------
void HjCarPortrait::BuildWallText(float radius)
{
	(void)radius;   // 寸法は m_scale から出す

	m_wallText.clear();

	auto sprite = KdFontManager::Instance().CreateFontTexture(
		UIConst::FontTitle, GC::WallTextStr, 3);

	if (!sprite || sprite->GetTexList().empty()) { return; }

	//===== 縮める倍率 =====
	// 焼いた高さを、決めた文字の高さへ合わせる
	const auto& first = sprite->GetTexList()[0];
	if (!first || !first->FontTex) { return; }

	const float texH = static_cast<float>(first->FontTex->GetInfo().Height);
	if (texH < 1.0f) { return; }

	const float S = m_scale;

	const float k = (GC::WallTextH * S) / texH;

	//===== 1行の幅 =====
	float lineW = 0.0f;

	for (auto& d : sprite->GetTexList())
	{
		if (d && d->FontTex)
		{
			lineW += static_cast<float>(d->FontTex->GetInfo().Width) * k;
		}
	}

	if (lineW <= 0.0f) { return; }

	//===== 並べる =====
	// 決めた位置を真ん中にする
	const float y = GC::WallTextY * S;
	const float z = GC::WallTextZ * S;

	float x = GC::WallTextX * S - lineW * 0.5f;

	for (auto& d : sprite->GetTexList())
	{
		if (!d || !d->FontTex) { continue; }

		const float w = static_cast<float>(d->FontTex->GetInfo().Width) * k;
		const float h = static_cast<float>(d->FontTex->GetInfo().Height) * k;

		auto poly = std::make_shared<KdSquarePolygon>(d->FontTex);
		poly->SetPivot(KdSquarePolygon::PivotType::Left_Bottom);
		poly->SetScale(Math::Vector2(w, h));

		WallGlyph g;
		g.poly = poly;
		g.pos  = Math::Vector3(x, y, z);

		m_wallText.push_back(g);

		x += w;
	}
}

//----------------------------------------------------------
// 壁の文字を描く
//
// 薄く出す。主役は車で、ここは壁に何か書いてあるという気配。
// はっきり出すと文字を読む画面になる
//----------------------------------------------------------
void HjCarPortrait::DrawWallText() const
{
	if (m_wallText.empty()) { return; }

	auto& shader = KdShaderManager::Instance().m_StandardShader;

	// 壁と同じ面・同じ光の当たり方なので、
	// 濃さをそのまま混ぜる割合として渡せる
	const Math::Color col(GC::PaperCol[0], GC::PaperCol[1], GC::PaperCol[2],
		              GC::WallTextAlpha);

	for (const auto& g : m_wallText)
	{
		if (!g.poly) { continue; }

		shader.DrawPolygon(*g.poly,
			   Math::Matrix::CreateTranslation(g.pos), col);
	}
}


//----------------------------------------------------------
// 車を置く行列
//
// 手で回した向きに回して、台の上へ持ち上げる。
// 台の高さを足さないと、車が台にめり込む
//----------------------------------------------------------
Math::Matrix HjCarPortrait::CarMatrix() const
{
	// 台は床へ1cm埋めてあるだけなので、ほぼ床面に立つ
	return Math::Matrix::CreateRotationY(m_spin)
		 * Math::Matrix::CreateTranslation(0.0f, GC::TurnY * m_scale, 0.0f);
}

//----------------------------------------------------------
// 場所を描く
//
// 光は当てない。台(UIの緑)と同じ色で出したいので、
// 明るさが変わると切り抜きの縁で色が食い違う
//----------------------------------------------------------
void HjCarPortrait::DrawStage()
{
	if (m_floor.empty()) { return; }

	auto& shader = KdShaderManager::Instance().m_StandardShader;

	// 床に貼るものは全部ほぼ同じ高さに重なる。
	// 深度で競うと面が取り合ってちらつくので、順番だけで決める
	const auto tri  = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	const auto zoff = KdDepthStencilState::ZDisable;

	const float k = GC::StageLit;

	auto col = [k](const float c[3], float a = 1.0f)
	{
		return Math::Color(c[0] * k, c[1] * k, c[2] * k, a);
	};

	// 差し替わった瞬間だけ輪を強くする。
	// カメラは固定なので、これが入れ替わりの合図になる
	const float fl = 1.0f + m_flash * (GC::SwapFlash - 1.0f);

	//===== 床 =====
	shader.DrawVertices(m_floor, Math::Matrix::Identity, col(GC::FloorCol), zoff, tri);

	// 白線。壁と同じ面ではないが、塗ってあるものなので濃さで混ぜる
	shader.DrawVertices(m_line, Math::Matrix::Identity,
		            Math::Color(GC::PaperCol[0], GC::PaperCol[1],
		                        GC::PaperCol[2], GC::LineAlpha), zoff, tri);

	//===== 回す台 =====
	// 円盤は床に埋まっているので回さない。回すのは車だけ
	{
		const Math::Color disc(GC::FloorCol[0] * k * GC::DiscMul,
		                       GC::FloorCol[1] * k * GC::DiscMul,
		                       GC::FloorCol[2] * k * GC::DiscMul, 1.0f);

		shader.DrawVertices(m_disc, Math::Matrix::Identity, disc, zoff, tri);

		const Math::Color ring(GC::AcidCol[0] * GC::RingLit * fl,
		                       GC::AcidCol[1] * GC::RingLit * fl,
		                       GC::AcidCol[2] * GC::RingLit * fl, 1.0f);

		shader.DrawVertices(m_ring, Math::Matrix::Identity, ring, zoff, tri);
	}

	// 車止め。つや消しなので持ち上げない
	shader.DrawVertices(m_stop, Math::Matrix::Identity, col(GC::AcidCol), zoff, tri);

	//===== 奥の壁 =====
	// 床より後。壁は床の向こうに立っている
	shader.DrawVertices(m_wall,    Math::Matrix::Identity, col(GC::WallCol),    zoff, tri);
	shader.DrawVertices(m_doorIn,  Math::Matrix::Identity, col(GC::SlatGapCol), zoff, tri);
	shader.DrawVertices(m_slatGap, Math::Matrix::Identity, col(GC::SlatGapCol), zoff, tri);
	shader.DrawVertices(m_slat,    Math::Matrix::Identity, col(GC::SlatCol),    zoff, tri);
	shader.DrawVertices(m_frame,   Math::Matrix::Identity, col(GC::FrameCol),   zoff, tri);

	// 塗ってある文字。枠より後に出して、上から刷ってある形にする
	DrawWallText();

	//===== 蛍光灯 =====
	{
		const Math::Color tube(GC::PaperCol[0] * GC::TubeLit,
		                       GC::PaperCol[1] * GC::TubeLit,
		                       GC::PaperCol[2] * GC::TubeLit, 1.0f);

		shader.DrawVertices(m_tube, Math::Matrix::Identity, tube, zoff, tri);
	}
}

//----------------------------------------------------------
// 車の向きを進める
//
// カメラは動かさない。車のほうを回して見せる向きを作る。
// 見下ろす角(PortraitPitch)と合わせて、これが「カメラの向き」になる
//----------------------------------------------------------
void HjCarPortrait::Update()
{
	// 見せたい向き。開いたときだけ入れる。
	// 毎フレーム入れ直すと、手で回しても次のフレームで戻る
	if (!m_yawInit)
	{
		m_spin = GC::CarStartYawDeg * Deg;
		m_yawInit = true;
	}

	const float dt = KdFPSController::GetDt();

	//===== 入れ替えの演出 =====
	if (m_swapT >= 0.0f)
	{
		m_swapT += dt / GC::SwapTime;

		// 真ん中で差し替える
		if (!m_swapped && m_swapT >= 0.5f)
		{
			ApplyCar(m_pendingKind);

			m_swapped = true;
			m_flash   = 1.0f;
		}

		if (m_swapT >= 1.0f) { m_swapT = -1.0f; }
	}

	// 差し替わった瞬間の明るさ。すぐ戻す
	if (m_flash > 0.0f)
	{
		m_flash = std::max(m_flash - dt / GC::SwapFlashTime, 0.0f);
	}

	//===== 回す =====
	// ひとりでにゆっくり回り続ける。
	// 手で回している間はそちらが勝ち、離すと勢いが残って
	// 0.4秒かけて元の速さへ戻る。
	//
	// 離した瞬間に自動の速さへ跳ぶと、そこで一度止まって見える
	if (m_yawInput != 0.0f && dt > 0.0f)
	{
		m_spin += m_yawInput;
		m_extraVel = m_yawInput / dt;
	}
	else
	{
		m_extraVel *= std::max(1.0f - dt / GC::ManualEaseSec, 0.0f);
		m_spin += m_extraVel * dt;
	}

	m_yawInput = 0.0f;

	m_spin += GC::SpinDegPerSec * Deg * dt;
}

//----------------------------------------------------------
void HjCarPortrait::SetupCamera()
{
	// 固定。揺らさない。
	//
	// 車庫は「据えた1台を見る」画面で、回すのは車のほう。
	// カメラが動くと、見たい角度で止められない
	const float S = (m_scale > 0.0001f) ? m_scale : 1.0f;

	const Math::Vector3 eye(0.0f, GC::CamY * S, GC::CamZ * S);
	const Math::Vector3 at (0.0f, GC::CamTargetY * S, 0.0f);

	const Math::Vector3 v = at - eye;

	// 見下ろす角。
	//
	// Matrix::CreateLookAt は使わない。あれは右手系だが、
	// この枠組みの射影行列は左手系。混ぜると被写体がカメラの
	// 後ろへ回って、何も映らない絵ができる
	const float pitch = std::atan2(-v.y, v.z);

	const Math::Matrix cam =
		Math::Matrix::CreateRotationX(pitch) *
		Math::Matrix::CreateTranslation(eye);

	m_cam.SetCameraMatrix(cam);

	// 奥行きの範囲は部屋に合わせる。
	// 足りないと、奥の壁の上半分が切れる
	const float dist = v.Length();

	m_cam.SetProjectionMatrix(GC::CamFovDeg, dist * 4.0f, dist * 0.02f,
			      GC::ScreenAspect);
	m_cam.SetToShader();
}

//----------------------------------------------------------
void HjCarPortrait::PreDraw()
{
	if (!m_car) { return; }

	// 場面のカメラをこちらにする。
	// 車庫は画面全体がこの空間なので、奪ってよい
	SetupCamera();

	// 輪が白へ飛ばないように、抽出のしきい値を上げる。
	// 飛ぶとUIの主色と別の色になり、色が揃わなくなる
	KdShaderManager::Instance().m_postProcessShader
		.SetBrightThreshold(GC::BloomThreshold);

	auto& amb = KdShaderManager::Instance().WorkAmbientController();

	//===== 主光 =====
	// 台の真上から1つだけ。影を落とすのはこれだけ
	amb.SetDirLight(
		Math::Vector3(GC::KeyLightDir[0], GC::KeyLightDir[1],
			      GC::KeyLightDir[2]),
		Math::Vector3(GC::KeyLightCol[0], GC::KeyLightCol[1],
			      GC::KeyLightCol[2]));

	amb.SetAmbientLight(
		Math::Vector4(GC::AmbientCol[0], GC::AmbientCol[1],
			      GC::AmbientCol[2], 1.0f));

	// 車体に映り込む環境。上=蛍光灯 / 横=壁 / 下=床
	amb.SetEnvColors(
		Math::Vector3(GC::EnvUp[0],   GC::EnvUp[1],   GC::EnvUp[2]),
		Math::Vector3(GC::EnvSide[0], GC::EnvSide[1], GC::EnvSide[2]),
		Math::Vector3(GC::EnvDown[0], GC::EnvDown[1], GC::EnvDown[2]));

	const float S = (m_scale > 0.0001f) ? m_scale : 1.0f;

	//===== 台の真上の灯り =====
	// 面で光らせる代わりに、広い円錐を1つ。
	// 毎フレーム置き直す。枠組みは Update で灯の一覧を空にする
	amb.AddSpotLight(
		Math::Vector3(GC::KeySpotCol[0], GC::KeySpotCol[1], GC::KeySpotCol[2]),
		Math::Vector3(0.0f, GC::KeyLightY * S, 0.0f),
		Math::Vector3(0.0f, 0.0f, 0.0f),
		GC::KeySpotOuter, GC::KeySpotInner, GC::KeySpotRange * S);

	//===== 蛍光灯 =====
	// 渡された指示は「光らせるだけ」だが、主光は真上からなので
	// 壁には何も当たらない。消したままだとシャッターも通用口も
	// 文字も沈んで見えなくなる。
	//
	// 点光は影を落とさないので、「影を落とさない」条件は満たしている
	{
		const Math::Vector3 col(GC::TubeLightCol[0], GC::TubeLightCol[1],
			        GC::TubeLightCol[2]);

		for (int i = 0; i < GC::TubeCount; ++i)
		{
			amb.AddPointLight(col, GC::TubeLightR * S,
				Math::Vector3(GC::TubeX[i] * S,
					      GC::TubeY * S,
					      GC::TubeZ * S));
		}
	}
}

//----------------------------------------------------------
// 影の元になる深度
//
// 車だけ流す。床や台は影を受ける側で、落とす側ではない。
// 入れると自分の影で自分が暗くなる
//----------------------------------------------------------
void HjCarPortrait::GenerateDepthMapFromLight()
{
	if (!m_car) { return; }

	m_car->DrawPortraitPlain(CarMatrix());
}


//----------------------------------------------------------
// 光るものだけをもう一度
//
// 蛍光灯と回す台の輪だけ。
// 部屋まで出すと画面ぜんぶが滲んで、輪郭が溶ける。
// 滲ませたいのは「光っているもの」であって、明るいものではない
//----------------------------------------------------------
void HjCarPortrait::DrawBright()
{
	if (m_ring.empty()) { return; }

	auto& shader = KdShaderManager::Instance().m_StandardShader;

	const auto tri = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// 深度は見る。
	// 切ると、車の後ろにあるものの滲みが車の上に乗る
	const auto zon = KdDepthStencilState::ZEnable;

	//===== 蛍光灯 =====
	{
		const float g = GC::TubeLit * GC::TubeGlow;

		const Math::Color c(GC::PaperCol[0] * g, GC::PaperCol[1] * g,
			            GC::PaperCol[2] * g, 1.0f);

		shader.DrawVertices(m_tube, Math::Matrix::Identity, c, zon, tri);
	}

	//===== 回す台の輪 =====
	// 白へ飛ばさない。飛ぶとUIの主色と別の色になる
	{
		const float fl = 1.0f + m_flash * (GC::SwapFlash - 1.0f);
		const float g  = GC::RingLit * GC::RingGlow * fl;

		const Math::Color c(GC::AcidCol[0] * g, GC::AcidCol[1] * g,
			            GC::AcidCol[2] * g, 1.0f);

		shader.DrawVertices(m_ring, Math::Matrix::Identity, c, zon, tri);
	}
}

//----------------------------------------------------------
// 場所と車
//
// 床まわりも光の当たる側で描く。影を受けるのはこちらなので、
// 陰影のない側(UnLit)へ置くと影が落ちてこない
//----------------------------------------------------------
void HjCarPortrait::DrawLit()
{
	if (!m_car) { return; }

	// 映り込みは作らない。
	// コンクリの床は物を映さない。車が置かれていることは、
	// 真上からの光が落とす影だけで足りる
	DrawStage();

	m_car->DrawPortrait(CarMatrix(), GC::PortraitOutlineMul);
}
