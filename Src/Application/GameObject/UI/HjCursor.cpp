#include "HjCursor.h"
#include "HjUI.h"
#include "../../Const/CursorConst.h"

//----------------------------------------------------------
// OSのカーソルの出し入れ。
//
// ShowCursor は「表示したい回数」を内部で数えていて、
// false を呼ぶたびに減り true を呼ぶたびに増える。
// 毎フレーム呼ぶとその数が際限なくずれて、あとで true を呼んでも
// 出てこなくなる。切り替わった瞬間にだけ呼ぶ。
//----------------------------------------------------------
void HjCursor::ApplySystemCursor(bool show)
{
	if (m_initialized && show == m_systemShown) { return; }

	ShowCursor(show);
	m_systemShown  = show;
	m_initialized  = true;
}

void HjCursor::Update(float dt)
{
	namespace C = CursorConst;

	// エディタ(F2)を開いている間はOSに任せる。
	// ImGuiは掴む・広げるでカーソルの形が変わるので、
	// 自前の照準で置き換えると何ができるのか分からなくなる
	const bool editor = KdDebugGUI::Instance().IsGameViewport();
	ApplySystemCursor(editor);

	if (editor) { return; }

	// 動いたかどうかを見る。HjUIがデザイン座標へ直した値を使うので、
	// 解像度が変わっても同じ感度になる
	const float x = HjUI::MouseX();
	const float y = HjUI::MouseY();

	const float dx = x - m_prevX;
	const float dy = y - m_prevY;
	m_prevX = x;
	m_prevY = y;

	// ごく小さな揺れは動いたとみなさない。
	// マウスは静止していても1ドット単位で震えることがある
	const bool moved = (dx * dx + dy * dy) > 0.25f;

	m_idle = moved ? 0.0f : (m_idle + dt);

	// 一定時間動かさなければ薄くなって消える。
	// 走行中はキーボードで操作するので、ポインタは邪魔にしかならない
	float target = 1.0f;
	if (m_idle > C::IdleHideTime)
	{
		const float over = m_idle - C::IdleHideTime;
		target = std::clamp(1.0f - over / std::max(C::FadeTime, 0.01f), 0.0f, 1.0f);
	}

	// 出るときは即座、消えるときはゆっくり。
	// 動かした瞬間に出ないと、探している間に迷子になる
	m_alpha = (target > m_alpha) ? target
	                             : (m_alpha + (target - m_alpha) * std::min(dt * 12.0f, 1.0f));

	// 押している間は小さくする。効いたかどうかが形で分かる
	const bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	const float pressTarget = down ? 1.0f : 0.0f;
	m_press += (pressTarget - m_press) * std::min(dt * 20.0f, 1.0f);
}

//----------------------------------------------------------
// 矢印を描く。
//
// 影(ずらした墨) → 本体(アシッド緑の塗り) → 縁(細い墨)の順。
// UIの他の要素(チップ・銘板)と同じ組み方にして、画面から浮かせる。
//----------------------------------------------------------
void HjCursor::Draw()
{
	namespace C = CursorConst;

	if (KdDebugGUI::Instance().IsGameViewport()) { return; }
	if (m_alpha <= 0.01f) { return; }

	// 先端をカーソル位置に合わせる。
	// 矢印は先端で指すものなので、中心に合わせるとずれて感じる
	const float tipX = HjUI::MouseX();
	const float tipY = HjUI::MouseY();

	// 押している間は少し縮める。効いたかどうかが形で分かる
	const float s = 1.0f - (1.0f - C::PressScale) * m_press;

	// 形は先端を原点とした相対座標。ずらしながら3回使う
	float pts[C::ShapePointCount * 2] = {};
	auto build = [&](float ox, float oy)
	{
		for (int i = 0; i < C::ShapePointCount; ++i)
		{
			pts[i * 2 + 0] = tipX + ox + C::Shape[i * 2 + 0] * s;
			pts[i * 2 + 1] = tipY + oy + C::Shape[i * 2 + 1] * s;
		}
	};

	// ① 影。ぼかさずにずらすだけ。UIの他の要素と揃える
	build(C::ShadowOffD, C::ShadowOffD);
	{
		Math::Color col = C::Shadow;
		col.w *= m_alpha * C::ShadowAlpha;
		HjUI::PolyFillD(pts, C::ShapePointCount, col);
	}

	// ② 本体の塗り。面積が出るのはここ
	build(0.0f, 0.0f);
	{
		Math::Color col = C::Fill;
		col.w *= m_alpha;
		HjUI::PolyFillD(pts, C::ShapePointCount, col);
	}

	// ③ 縁。塗りだけだと明るい背景に溶けるので、細い墨で切り離す。
	//    輪郭を閉じるため、最後に先頭の点へ戻る
	{
		float loop[(C::ShapePointCount + 1) * 2] = {};
		for (int i = 0; i < C::ShapePointCount * 2; ++i) { loop[i] = pts[i]; }
		loop[C::ShapePointCount * 2 + 0] = pts[0];
		loop[C::ShapePointCount * 2 + 1] = pts[1];

		Math::Color col = C::Edge;
		col.w *= m_alpha;
		HjUI::PolylineD(loop, C::ShapePointCount + 1, C::EdgePx, col);
	}
}
