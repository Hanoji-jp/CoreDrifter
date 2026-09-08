#include "HjTerrainBrush.h"

#include "HjHeightField.h"
#include "HjTerrain.h"
#include "HjRoad.h"
#include "HjRoadEditor.h"   // 画面から光線を作る所を借りる

namespace TB = TerrainBrushConst;

namespace
{
	// 決まった場所で決まった値を返す散らばり。
	//
	// 乱数だと押すたびに形が変わって、同じ所を2回撫でると
	// でこぼこが積み上がる。位置から決めれば、何度撫でても同じ形
	float ValueNoise(float x, float z)
	{
		const float fx = x / TB::NoiseCell;
		const float fz = z / TB::NoiseCell;

		const int ix = static_cast<int>(floorf(fx));
		const int iz = static_cast<int>(floorf(fz));

		auto at = [](int a, int b)
		{
			// 整数から散らばった値を作る
			unsigned int h = static_cast<unsigned int>(a) * 374761393u
			               + static_cast<unsigned int>(b) * 668265263u;
			h = (h ^ (h >> 13)) * 1274126177u;
			return static_cast<float>((h ^ (h >> 16)) & 0xFFFF) / 65535.0f * 2.0f - 1.0f;
		};

		const float tx = fx - static_cast<float>(ix);
		const float tz = fz - static_cast<float>(iz);

		// 角で折れないよう滑らかに
		const float sx = tx * tx * (3.0f - 2.0f * tx);
		const float sz = tz * tz * (3.0f - 2.0f * tz);

		const float a = at(ix,     iz    );
		const float b = at(ix + 1, iz    );
		const float c = at(ix,     iz + 1);
		const float d = at(ix + 1, iz + 1);

		return (a + (b - a) * sx) * (1.0f - sz) + (c + (d - c) * sx) * sz;
	}
}

//----------------------------------------------------------
// 画面の位置から光線を作り、地形へ当てる
//
// 光線を刻んで進めて、高さを下回った所で二分する。
//
// 面を1枚ずつ調べるより速く、しかも抜けない。
// ギズモで狙いが合わなかったのは、視線を軸へ落とす計算が
// ずれていたから。高さマップ相手なら、その計算自体が要らない
//----------------------------------------------------------
bool HjTerrainBrush::PickGround(const HjHeightField& field, Math::Vector3& outHit) const
{
	if (!field.IsValid()) { return false; }

	float u = 0.0f, v = 0.0f;
	KdDebugGUI::Instance().GetGameUV(u, v);

	if (!KdDebugGUI::Instance().IsGameHovered()) { return false; }

	Math::Vector3 ro, rd;
	if (!HjRoadEditor::ScreenRay(u, v, ro, rd)) { return false; }

	// 上を向いている光線は当たらない
	if (rd.y > -1e-4f && ro.y > field.HeightAt(ro.x, ro.z)) { return false; }

	float prevT = 0.0f;
	float prevD = ro.y - field.HeightAt(ro.x, ro.z);

	for (float t = TB::MarchStep; t < TB::MarchFar; t += TB::MarchStep)
	{
		const Math::Vector3 p = ro + rd * t;
		const float d = p.y - field.HeightAt(p.x, p.z);

		// 符号が変わった所をまたいだ
		if (d <= 0.0f && prevD > 0.0f)
		{
			// 前後を狭めて詰める
			float lo = prevT, hi = t;
			for (int i = 0; i < TB::MarchRefine; ++i)
			{
				const float mid = (lo + hi) * 0.5f;
				const Math::Vector3 q = ro + rd * mid;

				if (q.y - field.HeightAt(q.x, q.z) > 0.0f) { lo = mid; }
				else                                       { hi = mid; }
			}

			outHit = ro + rd * ((lo + hi) * 0.5f);
			return true;
		}

		prevT = t;
		prevD = d;
	}
	return false;
}

//----------------------------------------------------------
// 触る範囲をマス目で出す
//----------------------------------------------------------
void HjTerrainBrush::CellRange(const HjHeightField& field, const Math::Vector3& center,
                               int& outX0, int& outZ0, int& outX1, int& outZ1) const
{
	float cx = 0.0f, cz = 0.0f;
	field.WorldToCell(center.x, center.z, cx, cz);

	const int reach = static_cast<int>(m_radius / std::max(field.GetCellSize(), 0.01f)) + 2;

	outX0 = std::clamp(static_cast<int>(cx) - reach, 0, field.GetSizeX() - 1);
	outX1 = std::clamp(static_cast<int>(cx) + reach, 0, field.GetSizeX() - 1);
	outZ0 = std::clamp(static_cast<int>(cz) - reach, 0, field.GetSizeZ() - 1);
	outZ1 = std::clamp(static_cast<int>(cz) + reach, 0, field.GetSizeZ() - 1);
}

//----------------------------------------------------------
// 押した瞬間に、触る範囲の素地を控える
//
// 一手まるごとを控える。マス目ごとに控えると、
// 撫でている途中の値が混ざって、戻したときに筆跡が残る
//----------------------------------------------------------
void HjTerrainBrush::PushUndo(const HjHeightField& field, const Math::Vector3& center)
{
	Stroke st;
	CellRange(field, center, st.x0, st.z0, st.x1, st.z1);

	// 撫でている間に筆が動くので、少し広めに取る
	const int pad = static_cast<int>(m_radius / std::max(field.GetCellSize(), 0.01f));

	st.x0 = std::max(st.x0 - pad, 0);
	st.z0 = std::max(st.z0 - pad, 0);
	st.x1 = std::min(st.x1 + pad, field.GetSizeX() - 1);
	st.z1 = std::min(st.z1 + pad, field.GetSizeZ() - 1);

	st.before.reserve(static_cast<size_t>(st.x1 - st.x0 + 1) * (st.z1 - st.z0 + 1));
	for (int iz = st.z0; iz <= st.z1; ++iz)
	{
		for (int ix = st.x0; ix <= st.x1; ++ix)
		{
			st.before.push_back(field.BaseAtCell(ix, iz));
		}
	}

	m_undo.push_back(std::move(st));

	// 古い手から捨てる
	while (static_cast<int>(m_undo.size()) > TB::UndoDepth)
	{
		m_undo.erase(m_undo.begin());
	}
}

//----------------------------------------------------------
// 控えた形へ戻す
//----------------------------------------------------------
void HjTerrainBrush::Undo(HjHeightField& field, HjTerrain& terrain)
{
	if (m_undo.empty()) { return; }

	const Stroke& st = m_undo.back();

	size_t k = 0;
	for (int iz = st.z0; iz <= st.z1; ++iz)
	{
		for (int ix = st.x0; ix <= st.x1; ++ix, ++k)
		{
			field.SetBaseAtCell(ix, iz, st.before[k]);
			field.SetHeightAtCell(ix, iz, st.before[k]);
		}
	}

	const float cs = field.GetCellSize();
	const Math::Vector3 mn(st.x0 * cs - field.GetWorldW() * 0.5f, 0.0f,
	                       st.z0 * cs - field.GetWorldD() * 0.5f);
	const Math::Vector3 mx(st.x1 * cs - field.GetWorldW() * 0.5f, 0.0f,
	                       st.z1 * cs - field.GetWorldD() * 0.5f);
	terrain.RebuildInArea(mn, mx);

	m_undo.pop_back();
}

//----------------------------------------------------------
// 一筆ぶんを当てる
//
// 書くのは素地。仕上がりへも同じぶんを足して、その場で見えるようにする。
//
// 素地だけに書くと、道を動かすまで形が変わらない。
// 仕上がりだけに書くと、道を動かした瞬間に消える
//----------------------------------------------------------
void HjTerrainBrush::Apply(HjHeightField& field, const HjRoad* road,
                           const Math::Vector3& center, float dt)
{
	int x0, z0, x1, z1;
	CellRange(field, center, x0, z0, x1, z1);

	const float cs = field.GetCellSize();
	const float w  = field.GetWorldW() * 0.5f;
	const float d  = field.GetWorldD() * 0.5f;

	const float amount = m_strength * dt;

	for (int iz = z0; iz <= z1; ++iz)
	{
		for (int ix = x0; ix <= x1; ++ix)
		{
			const float wx = ix * cs - w;
			const float wz = iz * cs - d;

			const float dx = wx - center.x;
			const float dz = wz - center.z;
			const float dist = sqrtf(dx * dx + dz * dz);
			if (dist > m_radius) { continue; }

			// 中心で1、縁で0。falloff で効きの形を変える
			const float t = 1.0f - (dist / std::max(m_radius, 0.01f));
			float fall = powf(t, m_falloff);

			// 道が持っている所では効かせない。
			//
			// 盛ると地形が路面を突き抜けて、そのたびに削って直すことになる。
			// 道が地形を寄せた強さをそのまま覆いに使うので、
			// 路面の上では0、削りの端に向けて元へ戻る。境目に段差が出ない
			if (road)
			{
				const int cell = iz * field.GetSizeX() + ix;
				const float own = road->OwnWeightAt(cell);
				fall *= 1.0f - own * (1.0f - m_roadMask);
			}

			if (fall <= 0.0f) { continue; }

			const float base = field.BaseAtCell(ix, iz);
			float next = base;

			switch (m_mode)
			{
			case Mode::Raise:
				next = base + amount * fall;
				break;

			case Mode::Lower:
				next = base - amount * fall;
				break;

			case Mode::Smooth:
			{
				// 周りの平均へ寄せる。
				// 一気に平均にすると、押した瞬間に平らになって使えない
				const float avg = (field.BaseAtCell(ix - 1, iz)
				                 + field.BaseAtCell(ix + 1, iz)
				                 + field.BaseAtCell(ix, iz - 1)
				                 + field.BaseAtCell(ix, iz + 1)) * 0.25f;

				const float rate = std::clamp(TB::SmoothRate * dt * fall, 0.0f, 1.0f);
				next = base + (avg - base) * rate;
				break;
			}

			case Mode::Flatten:
			{
				// 押した所の高さへ寄せる
				const float rate = std::clamp(TB::SmoothRate * dt * fall, 0.0f, 1.0f);
				next = base + (m_flatHeight - base) * rate;
				break;
			}

			case Mode::Noise:
				// 位置から決まる散らばり。
				// 乱数だと、同じ所を撫でるたびに積み上がる
				next = base + ValueNoise(wx, wz) * TB::NoiseScale * amount * fall;
				break;
			}

			// 素地と仕上がりの両方へ。
			// 仕上がりは、道が削った跡の上へ同じぶんを足す
			const float delta = next - base;

			field.SetBaseAtCell(ix, iz, next);
			field.SetHeightAtCell(ix, iz, field.HeightAtCell(ix, iz) + delta);
		}
	}
}

//----------------------------------------------------------
// 筆を使う
//----------------------------------------------------------
void HjTerrainBrush::Update(HjHeightField& field, HjTerrain& terrain,
                            const HjRoad* road)
{
	m_hasHit = false;
	if (!m_enabled || !field.IsValid()) { m_lmbPrev = false; return; }

	// ※ ImGui の WantCaptureMouse では見ない。
	//
	//    ゲームの画面自体が ImGui のウィンドウなので、
	//    そこへマウスを置くと常に true になる。
	//    見ると、描きたい場所でだけ筆が効かなくなる。
	//
	//    代わりに IsGameHovered() を使う(PickGround の中)。
	//    こちらは「ゲームの絵の上か」だけを見るので、
	//    滑り子の上では当たらない

	m_hasHit = PickGround(field, m_hit);

	const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

	// 押した瞬間。控えるのはここだけ
	if (lmb && !m_lmbPrev && m_hasHit)
	{
		PushUndo(field, m_hit);

		// 「平らに」で寄せる高さは、押した所の高さ
		m_flatHeight = field.HeightAt(m_hit.x, m_hit.z);
	}

	if (lmb && m_hasHit)
	{
		const float dt = KdFPSController::GetDt();
		Apply(field, road, m_hit, dt);

		// 触った所だけ組み直す。
		// 全部組み直すと、数千個ぶんの頂点バッファを毎フレーム作る
		int x0, z0, x1, z1;
		CellRange(field, m_hit, x0, z0, x1, z1);

		const float cs = field.GetCellSize();
		const Math::Vector3 mn(x0 * cs - field.GetWorldW() * 0.5f, 0.0f,
		                       z0 * cs - field.GetWorldD() * 0.5f);
		const Math::Vector3 mx(x1 * cs - field.GetWorldW() * 0.5f, 0.0f,
		                       z1 * cs - field.GetWorldD() * 0.5f);
		terrain.RebuildInArea(mn, mx);
	}

	// 筆を離したら、道を作り直してもらう。
	//
	// 裾は地形に沿って溶けるので、彫る前の地形のままだと浮くか埋まる。
	// 撫でている間ずっと作り直すと、1回20〜50msかかって引っかかる
	if (!lmb && m_lmbPrev) { m_terrainChanged = true; }

	m_lmbPrev = lmb;

	// 取り消し
	static bool zPrev = false;
	const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
	const bool z    = (GetAsyncKeyState('Z') & 0x8000) != 0;

	if (ctrl && z && !zPrev)
	{
		Undo(field, terrain);
		m_terrainChanged = true;
	}
	zPrev = z;
}

//----------------------------------------------------------
// 筆の輪を出す
//
// どこをどれだけ触るのかが見えないと、狙って彫れない
//----------------------------------------------------------
void HjTerrainBrush::PushRing(KdDebugWireFrame& dbg) const
{
	if (!m_enabled || !m_hasHit) { return; }

	auto ring = [&](float r, const Math::Color& col)
	{
		Math::Vector3 prev;
		for (int i = 0; i <= TB::RingSegments; ++i)
		{
			const float a = (6.2831853f * i) / TB::RingSegments;

			// 地形に沿わせる。真っ平らな輪だと、斜面で地面へ潜る
			const Math::Vector3 p(m_hit.x + cosf(a) * r,
			                      m_hit.y + TB::RingLift,
			                      m_hit.z + sinf(a) * r);

			if (i > 0) { dbg.AddDebugLine(prev, p, col); }
			prev = p;
		}
	};

	// 外側は効きの端、内側は強く効く所
	ring(m_radius, Math::Color(1.0f, 0.8f, 0.2f, 1.0f));
	ring(m_radius * TB::InnerRingRatio, Math::Color(1.0f, 0.5f, 0.1f, 1.0f));

	// 中心に縦線。斜面でどこを指しているか分かる
	dbg.AddDebugLine(m_hit, m_hit + Math::Vector3(0.0f, m_radius * 0.3f, 0.0f),
	                 Math::Color(1.0f, 0.9f, 0.4f, 1.0f));
}

//----------------------------------------------------------
// 調整の画面
//----------------------------------------------------------
void HjTerrainBrush::DrawGui(HjHeightField& field)
{
	ImGui::Checkbox(U8("筆を使う"), &m_enabled);
	ImGui::SameLine();
	ImGui::TextDisabled(U8("押している間だけ効く / Ctrl+Z で1手戻す"));

	if (!m_enabled) { return; }

	//===== 何をする筆か =====
	int mode = static_cast<int>(m_mode);
	ImGui::RadioButton(U8("盛る"), &mode, 0);   ImGui::SameLine();
	ImGui::RadioButton(U8("削る"), &mode, 1);   ImGui::SameLine();
	ImGui::RadioButton(U8("均す"), &mode, 2);   ImGui::SameLine();
	ImGui::RadioButton(U8("平らに"), &mode, 3); ImGui::SameLine();
	ImGui::RadioButton(U8("ざらつき"), &mode, 4);
	m_mode = static_cast<Mode>(mode);

	ImGui::SetItemTooltip(U8("平らに: 押した所の高さへ周りを寄せる"));

	//===== 筆の形 =====
	ImGui::DragFloat(U8("半径(m)"), &m_radius, 0.5f,
	                 TB::RadiusMin, TB::RadiusMax);
	ImGui::DragFloat(U8("強さ(m/秒)"), &m_strength, 0.1f,
	                 TB::StrengthMin, TB::StrengthMax);
	ImGui::DragFloat(U8("効きの落ち方"), &m_falloff, 0.02f,
	                 TB::FalloffMin, TB::FalloffMax);
	ImGui::SetItemTooltip(U8("1で直線。大きいほど中心が尖り、小さいほど縁まで平たく効く"));

	ImGui::SliderFloat(U8("道を避ける"), &m_roadMask, 0.0f, 1.0f, "%.2f");
	ImGui::SetItemTooltip(U8("0で道を完全に守る(既定)。1にすると道の上も彫れて、路面を突き抜ける"));

	//===== 状態 =====
	if (m_hasHit)
	{
		ImGui::Text(U8("指している所  x %.1f  y %.1f  z %.1f"),
		            m_hit.x, m_hit.y, m_hit.z);
	}
	else
	{
		ImGui::TextDisabled(U8("地形の上へマウスを置く"));
	}

	ImGui::Text(U8("戻せる手 %d / %d"),
	            static_cast<int>(m_undo.size()), TB::UndoDepth);

	//===== 保存 =====
	// 書き出すのは素地。仕上がりを書くと道の削りが焼き込まれて、
	// 次に道を動かしたときに二重に削れる
	if (ImGui::Button(U8("彫った形を書き出す")))
	{
		// 書き出し先は取り込んだ標高データとは別のファイル。
		// 同じにすると、保存したときに DEM が消える
		m_saved = field.SaveToFile(TerrainConst::EditPath);
		m_savedShown = true;
	}
	ImGui::SameLine();
	ImGui::TextDisabled(TerrainConst::EditPath);

	// 黙って失敗されると、保存できたのか分からない
	if (m_savedShown)
	{
		if (m_saved)
		{
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
				U8("書き出した。次に起動したときはこの形から始まる"));
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
				U8("書き出せなかった。Asset/Data/terrain フォルダがあるか確かめて"));
		}
	}
}
