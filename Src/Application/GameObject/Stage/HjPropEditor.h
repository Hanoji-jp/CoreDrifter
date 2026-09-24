#pragma once

class HjProps;
class HjHeightField;
class HjRoad;
class HjTerrainBrush;

//==========================================================
// HjPropEditor
//   飾り(木・低木)を置く操作。自作なので Hj 接頭辞。
//
//   ■ 見てから置く
//   置く前に、そのモデルを実際の大きさで地面に出す。
//   「置いてから直す」の往復が無くなるのが、この手の
//   置き場エディタで一番効く所。
//
//   ■ 手を離さずに全部やる
//   モデルを選ぶ・回す・大きさを変える・消す・掴んで動かすを、
//   左手の修飾キーとホイールだけで回す。
//   一覧へ戻って選び直す動きが入ると、並べる作業が続かない。
//
//   ■ 置く操作は1か所にまとめる
//   場面へ直接書くと、判定と入力と描画がそこへ溜まっていく。
//   道の編集(HjRoadEditor)や地形の筆(HjTerrainBrush)と同じ扱いにする
//==========================================================
class HjPropEditor
{
public:
	// 毎フレーム。置く・消す・掴む・下見を全部ここで回す
	void Update(HjProps& props, const HjHeightField& field, const HjRoad* road,
	            const HjTerrainBrush& picker);

	// 操作の説明と設定
	void DrawGui(HjProps& props);

	// 編集中か。入れている間は制御点を掴む処理を止める
	void SetEnabled(bool on);
	bool IsEnabled() const { return m_enabled; }

	// 掴んでいる最中は、道の編集へ渡さない
	bool IsBusy() const { return m_grabbed >= 0; }

private:
	// 何をしている最中か
	enum class Mode
	{
		Place,   // 置く
		Grab,    // 掴んで動かしている
	};

	// 押した瞬間か(押しっぱなしを弾く)
	static bool Pressed(int vk, bool& held);

	// 升目に合わせる
	float Snap(float v) const;

	bool m_enabled = false;

	Mode m_mode = Mode::Place;

	// 掴んでいるものの番号。-1 なら掴んでいない
	int m_grabbed = -1;

	// ホイールで足した向き(rad)と、掛ける大きさ
	float m_yaw   = 0.0f;
	float m_scale = 1.0f;

	// 升目に合わせるか
	bool  m_snap = false;

	// 地面の傾きに合わせて倒すか。
	// 木は倒したくないが、岩や柵は倒したい
	bool  m_align = false;

	// 押しっぱなしで引くときに、前に置いた場所
	Math::Vector3 m_lastPut = Math::Vector3(1e9f, 0.0f, 1e9f);

	bool m_lmbHeld = false;
	bool m_rmbHeld = false;
	bool m_keyRHeld = false;
};
