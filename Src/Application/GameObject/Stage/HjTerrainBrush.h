#pragma once

#include "../../Const/TerrainBrushConst.h"

class HjHeightField;
class HjTerrain;
class HjRoad;
class KdDebugWireFrame;

//==========================================================
// HjTerrainBrush
//   地形を筆で彫る。自作なので Hj 接頭辞。
//
//   ■ 書くのは素地
//   道は地形を削る。彫った形へ直接書くと、道を動かした瞬間に
//   消えるか、削りが二重に掛かって掘り進む。
//
//   筆が書くのは素地(HjHeightField::SetBaseAtCell)。
//   道は毎回そこから削り直す。
//
//   ■ どこを指しているか
//   メッシュを1枚ずつ調べる必要はない。
//   高さマップがあるので、光線を刻んで進めて、
//   高さを下回った所で二分すれば出る。
//
//   ギズモで狙いが合わなかったのは、視線を軸へ落とす計算が
//   ずれていたから。高さマップ相手なら、その計算自体が要らない。
//==========================================================
class HjTerrainBrush
{
public:
	// 何をする筆か
	enum class Mode
	{
		Raise,     // 盛る
		Lower,     // 削る
		Smooth,    // 均す
		Flatten,   // 決めた高さへ平らに
		Noise,     // ざらつきを足す
	};

	// 筆を使う。押している間だけ効く。
	// 掴んでいる間は毎フレーム呼ぶ
	// road を渡すと、道が持っている所では筆が効かなくなる。
	// 渡さないと、盛ったときに地形が路面を突き抜ける
	void Update(HjHeightField& field, HjTerrain& terrain, const HjRoad* road);

	// 調整の画面。ImGui の中で呼ぶ
	void DrawGui(HjHeightField& field);

	// 筆の輪を出す。物の描画の番で呼ぶ
	void PushRing(KdDebugWireFrame& dbg) const;

	// 地形を変えたか。一度読むと下りる。
	//
	// 裾は地形に沿って溶けるので、地形を彫ったら道も作り直す必要がある。
	// 作り直さないと、彫る前の地形に沿った裾が残って、浮くか埋まる
	bool ConsumeTerrainChanged()
	{
		const bool v = m_terrainChanged;
		m_terrainChanged = false;
		return v;
	}

	// 使うかどうか。切っているときは光線も飛ばさない
	bool IsEnabled() const { return m_enabled; }
	void SetEnabled(bool on) { m_enabled = on; }

private:


public:
	// 画面の位置から光線を作り、地形へ当てる。
	//
	// 光線を刻んで進めて、高さを下回った所で二分する。
	// 面を1枚ずつ調べるより速く、しかも抜けない。
	//
	// 木を置くときも同じ当て方を使う。
	// 2つ持つと、片方だけ直したときに狙いが食い違う
	bool PickGround(const HjHeightField& field, Math::Vector3& outHit) const;

	// 彫った形を書き出す。
	// ボタンは持たない。書き出しは1か所にまとめてある
	bool Save(const HjHeightField& field);

private:
	// 触る範囲をマス目で出す
	void CellRange(const HjHeightField& field, const Math::Vector3& center,
	               int& outX0, int& outZ0, int& outX1, int& outZ1) const;

	// 一筆ぶんを当てる
	void Apply(HjHeightField& field, const HjRoad* road,
	           const Math::Vector3& center, float dt);

	// 押した瞬間に、触る範囲の素地を控える
	void PushUndo(const HjHeightField& field, const Math::Vector3& center);

	// 控えた形へ戻す
	void Undo(HjHeightField& field, HjTerrain& terrain);

	//===== 取り消し =====
	// 一手ぶん。触った四角と、その中の素地
	struct Stroke
	{
		int x0 = 0, z0 = 0, x1 = 0, z1 = 0;
		std::vector<float> before;
	};
	std::vector<Stroke> m_undo;

	//===== 状態 =====
	bool  m_enabled = false;
	Mode  m_mode    = Mode::Raise;

	float m_radius   = TerrainBrushConst::RadiusInit;
	float m_strength = TerrainBrushConst::StrengthInit;
	float m_falloff  = TerrainBrushConst::FalloffInit;

	// 「平らに」で寄せる高さ。押した所の高さを拾う
	float m_flatHeight = 0.0f;

	// 道をどれだけ避けるか。0で完全に守る、1で覆いが効かない
	float m_roadMask = TerrainBrushConst::RoadMaskInit;

	// いま指している所と、当たっているか
	Math::Vector3 m_hit;
	bool m_hasHit = false;

	// 押しているか(前フレームとの差で、押した瞬間を取る)
	bool m_lmbPrev = false;

	// 地形を変えたか。筆を離したときと、戻したときに立てる。
	//
	// 撫でている間ずっと道を作り直すと、1回20〜50msかかって
	// 筆が引っかかる。一筆終わってからでよい
	bool m_terrainChanged = false;

	// 書き出せたか。黙って失敗されると分からない
	bool m_saved      = false;
	bool m_savedShown = false;
};
