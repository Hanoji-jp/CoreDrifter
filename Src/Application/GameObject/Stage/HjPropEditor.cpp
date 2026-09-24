#include "HjPropEditor.h"

#include "HjProps.h"
#include "HjHeightField.h"
#include "HjRoad.h"
#include "HjTerrainBrush.h"
#include "HjRoadEditor.h"   // 画面から光線を作る所を借りる

#include "../../Const/PropConst.h"

namespace PC = PropConst;

namespace
{
	constexpr float Deg = 3.14159265f / 180.0f;
	constexpr float Pi2 = 6.28318530718f;

	bool Down(int vk)
	{
		return (GetAsyncKeyState(vk) & 0x8000) != 0;
	}

	// 座標から作る整数ハッシュ。引き直しに使う
	unsigned int Hash(int x, int z, unsigned int salt)
	{
		unsigned int h = static_cast<unsigned int>(x) * 374761393u
		               + static_cast<unsigned int>(z) * 668265263u
		               + salt * 2246822519u;

		h = (h ^ (h >> 13)) * 1274126177u;
		return h ^ (h >> 16);
	}

	float Rand01(unsigned int h)
	{
		return (h & 0xFFFFFFu) / static_cast<float>(0xFFFFFF);
	}
}

//----------------------------------------------------------
bool HjPropEditor::Pressed(int vk, bool& held)
{
	const bool now = Down(vk);
	const bool hit = now && !held;
	held = now;
	return hit;
}

//----------------------------------------------------------
void HjPropEditor::SetEnabled(bool on)
{
	m_enabled = on;

	// 抜けるときは掴んだままにしない。
	// 掴んだまま抜けると、次に入ったときに何も押していないのに動く
	if (!on) { m_grabbed = -1; m_mode = Mode::Place; }
}

//----------------------------------------------------------
float HjPropEditor::Snap(float v) const
{
	if (!m_snap) { return v; }

	const float g = PC::SnapSize;
	return floorf(v / g + 0.5f) * g;
}

//----------------------------------------------------------
// 毎フレーム
//
// 左手の修飾キーで、何をするかが変わる。
//   なし  : 置く(押しっぱなしで引ける)
//   Shift : 消す
//   Alt   : スポイト(下のものをブラシにする)
//   Ctrl  : 掴む
//----------------------------------------------------------
void HjPropEditor::Update(HjProps& props, const HjHeightField& field,
                          const HjRoad* road, const HjTerrainBrush& picker)
{
	if (!m_enabled)
	{
		props.ClearGhost();
		return;
	}

	// ゲーム画面の上でだけ効かせる。
	// 出ていないとマウスの位置が測れず、視線も作れない
	if (!KdDebugGUI::Instance().IsGameHovered())
	{
		props.ClearGhost();
		m_lmbHeld = Down(VK_LBUTTON);
		return;
	}

	const bool shift = Down(VK_SHIFT);
	const bool ctrl  = Down(VK_CONTROL);
	const bool alt   = Down(VK_MENU);

	//===== ホイール =====
	// そのまま回す。Ctrl と一緒なら大きさ
	{
		const float wheel = ImGui::GetIO().MouseWheel;
		if (wheel != 0.0f)
		{
			if (ctrl)
			{
				m_scale *= powf(PC::ScaleStep, wheel);
				m_scale = std::clamp(m_scale, PC::ScaleLimitMin, PC::ScaleLimitMax);
			}
			else
			{
				m_yaw += wheel * PC::YawStep * Deg;
			}
		}
	}

	//===== 視線を地面へ当てる =====
	float u = 0.0f, v = 0.0f;
	KdDebugGUI::Instance().GetGameUV(u, v);

	Math::Vector3 ro, rd;
	const bool hasRay = HjRoadEditor::ScreenRay(u, v, ro, rd);

	Math::Vector3 hit;
	const bool onGround = hasRay && picker.PickGround(field, hit);

	if (onGround)
	{
		hit.x = Snap(hit.x);
		hit.z = Snap(hit.z);
	}

	//===== R で引き直す =====
	// 向きと大きさを、いまの場所から作り直す。
	// 同じ姿が並ぶのを崩すのに使う
	if (onGround && Pressed('R', m_keyRHeld))
	{
		m_yaw   = Rand01(Hash(static_cast<int>(hit.x * 100.0f),
		                      static_cast<int>(hit.z * 100.0f), 7u)) * Pi2;
		m_scale = PC::ScaleMin + (PC::ScaleMax - PC::ScaleMin)
		        * Rand01(Hash(static_cast<int>(hit.x * 100.0f),
		                      static_cast<int>(hit.z * 100.0f), 13u));
	}

	//===== 掴んでいる最中 =====
	if (m_mode == Mode::Grab)
	{
		if (m_grabbed < 0 || m_grabbed >= props.PlacedCount())
		{
			m_grabbed = -1;
			m_mode = Mode::Place;
		}
		else
		{
			// 掴んだものをカーソルへ付いて来させる
			if (onGround)
			{
				float y = 0.0f;
				if (props.CanPlace(field, road, hit, y))
				{
					auto& p = props.WorkAt(m_grabbed);
					p.x = hit.x;
					p.z = hit.z;
					p.yaw = m_yaw;
					p.scale = m_scale;
					props.SetGroundY(m_grabbed, y);
				}
			}

			// もう一度押したら置く。右で取り消し
			if (Pressed(VK_LBUTTON, m_lmbHeld))
			{
				m_grabbed = -1;
				m_mode = Mode::Place;
			}
			else if (Pressed(VK_RBUTTON, m_rmbHeld))
			{
				m_grabbed = -1;
				m_mode = Mode::Place;
			}

			props.ClearGhost();
			return;
		}
	}

	//===== 下見 =====
	// 置く場所へ、そのモデルを実際の大きさで出す。
	// 消す・スポイト・掴むのときは出さない(置く動作ではないので)
	if (onGround && !shift && !alt && !ctrl)
	{
		float y = 0.0f;
		const bool ok = props.CanPlace(field, road, hit, y);

		if (ok)
		{
			Math::Matrix w = Math::Matrix::CreateScale(m_scale);

			// 地面の傾きに合わせて倒す。
			// 木は真っ直ぐ立っていてほしいので既定は切ってある
			if (m_align)
			{
				Math::Vector3 n;
				float yy = 0.0f;
				field.SampleAt(hit.x, hit.z, yy, n);

				if (n.y < 0.0f) { n = -n; }

				// 上向きを地面の法線へ倒す最小の回転
				const Math::Vector3 up = Math::Vector3::Up;
				Math::Vector3 axis = up.Cross(n);

				if (axis.LengthSquared() > 1e-8f)
				{
					axis.Normalize();
					const float ang = acosf(std::clamp(up.Dot(n), -1.0f, 1.0f));
					w *= Math::Matrix::CreateFromAxisAngle(axis, ang);
				}
			}

			w *= Math::Matrix::CreateRotationY(m_yaw);
			w *= Math::Matrix::CreateTranslation(hit.x, y - PC::Sink, hit.z);

			// 下見に出すのは、いま置かれるモデルそのもの。
			// ランダムのときはカーソルの位置から決まるので、
			// 見えているものがそのまま置かれる
			const int model = props.ResolveModel(hit.x, hit.z);

			if (model >= 0) { props.SetGhost(model, w); }
			else            { props.ClearGhost(); }
		}
		else
		{
			props.ClearGhost();
		}
	}
	else
	{
		props.ClearGhost();
	}

	//===== 押した =====
	const bool click = Pressed(VK_LBUTTON, m_lmbHeld);

	if (alt)
	{
		// スポイト。下のものをブラシにする。
		// 並べ直すときに一覧へ戻らずに済む
		if (click && hasRay)
		{
			const int i = props.Pick(ro, rd);
			if (i >= 0)
			{
				props.SetBrush(props.At(i).model);
				m_yaw   = props.At(i).yaw;
				m_scale = props.At(i).scale;
			}
		}
		return;
	}

	if (shift)
	{
		// 消す。押しっぱなしで撫でて消せる
		if (Down(VK_LBUTTON) && hasRay)
		{
			const int i = props.Pick(ro, rd);
			if (i >= 0) { props.RemoveAt(i); }
		}
		return;
	}

	if (ctrl)
	{
		// 掴む
		if (click && hasRay)
		{
			const int i = props.Pick(ro, rd);
			if (i >= 0)
			{
				m_grabbed = i;
				m_mode = Mode::Grab;
				m_yaw   = props.At(i).yaw;
				m_scale = props.At(i).scale;
			}
		}
		return;
	}

	//===== 置く =====
	if (!onGround) { return; }

	// 押した瞬間は必ず1つ。
	// そのまま引くと、間隔を空けて続けて置く
	bool put = click;

	if (!put && Down(VK_LBUTTON))
	{
		const float dx = hit.x - m_lastPut.x;
		const float dz = hit.z - m_lastPut.z;

		put = (dx * dx + dz * dz) >= (PC::DragSpacing * PC::DragSpacing);
	}

	if (!put) { return; }

	// 下見と同じ選び方をする。
	// ここで引き直すと、見えていたものと別のものが置かれる
	const int model = props.ResolveModel(hit.x, hit.z);

	if (props.AddExplicit(field, road, model, hit, m_yaw, m_scale))
	{
		m_lastPut = hit;
	}
}

//----------------------------------------------------------
void HjPropEditor::DrawGui(HjProps& props)
{
	bool on = m_enabled;
	if (ImGui::Checkbox(U8("置く##prop"), &on)) { SetEnabled(on); }

	ImGui::SameLine();
	ImGui::TextDisabled(U8("%d 個"), props.PlacedCount());

	if (!m_enabled) { return; }

	//===== 置くもの =====
	{
		const int n = props.ModelCount();
		if (n <= 0)
		{
			ImGui::TextDisabled(U8("Asset/Data/Map_Props にモデルが無い"));
		}
		else
		{
			const int grp = props.GetBrushGroup();

			// いま何を置くのか。まとまりならその名前
			char label[256] = {};
			if (grp >= 0)
			{
				snprintf(label, sizeof(label), U8("%s から1つ"), props.GroupName(grp));
			}
			else
			{
				snprintf(label, sizeof(label), "%s", props.ModelName(props.GetBrush()));
			}

			if (ImGui::BeginCombo(U8("置くもの"), label))
			{
				// まとまりを先に出す。
				// 木は木で、桜は桜で散らしたい、が普段の使い方なので、
				// 39個の一覧の下に埋めると毎回スクロールすることになる
				for (int g = 0; g < props.GroupCount(); ++g)
				{
					char gl[256] = {};
					snprintf(gl, sizeof(gl), U8("[ランダム] %s (%d)"),
					         props.GroupName(g), props.GroupMemberCount(g));

					const bool sel = (grp == g);
					if (ImGui::Selectable(gl, sel)) { props.SetBrushGroup(g); }
					if (sel) { ImGui::SetItemDefaultFocus(); }
				}

				ImGui::Separator();

				for (int k = 0; k < n; ++k)
				{
					const bool sel = (grp < 0 && k == props.GetBrush());
					if (ImGui::Selectable(props.ModelName(k), sel)) { props.SetBrush(k); }
					if (sel) { ImGui::SetItemDefaultFocus(); }
				}
				ImGui::EndCombo();
			}
		}
	}

	//===== 姿 =====
	float deg = m_yaw / Deg;
	if (ImGui::DragFloat(U8("向き(度)"), &deg, 1.0f, -360.0f, 360.0f))
	{
		m_yaw = deg * Deg;
	}

	ImGui::DragFloat(U8("大きさ"), &m_scale, 0.01f,
	                 PC::ScaleLimitMin, PC::ScaleLimitMax);

	ImGui::Checkbox(U8("升目に合わせる"), &m_snap);
	ImGui::SameLine();
	ImGui::Checkbox(U8("地面に倒す"), &m_align);
	ImGui::SetItemTooltip(U8("岩や柵向け。木は立てたままにする"));

	//===== 操作の説明 =====
	// 修飾キーで動きが変わるので、出しておかないと使えない
	ImGui::SeparatorText(U8("操作"));
	ImGui::TextDisabled(U8("左クリック    置く(押したまま引くと続けて置く)"));
	ImGui::TextDisabled(U8("ホイール      回す"));
	ImGui::TextDisabled(U8("Ctrl+ホイール 大きさ"));
	ImGui::TextDisabled(U8("R             向きと大きさを引き直す"));
	ImGui::TextDisabled(U8("Shift+左      消す(撫でて消せる)"));
	ImGui::TextDisabled(U8("Alt+左        スポイト(下のものをブラシに)"));
	ImGui::TextDisabled(U8("Ctrl+左       掴む → もう一度左で置く / 右で取り消し"));
}
