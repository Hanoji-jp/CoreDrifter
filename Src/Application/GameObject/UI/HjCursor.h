#pragma once

//==========================================================
// HjCursor
//   自前で描くマウスポインタ。
//
//   OSのカーソルはWindowsの見た目そのままで、この作品のUIから浮く。
//   画面は角括弧・トンボ・アシッド緑で組んだ計測器のような意匠なので、
//   ポインタもその一部として基本図形で作る。
//
//   ■ 形は照準
//   中心を空けた十字＋四隅の角括弧。矢印にするとOSのものを真似る形になり、
//   しかも指している物を塗りつぶしてしまう。
//
//   ■ 動かしていない間は消す
//   走行中はキーボードで操作するので、ポインタは邪魔にしかならない。
//   一定時間動かさなければ薄くなって消え、動かせば戻る。
//   「走行中は出さない」と決め打ちするより、メニューでも走行中でも
//   同じ規則で自然に振る舞う。
//
//   ■ エディタを開いている間は描かない
//   ImGuiは掴む・広げるでカーソルの形が変わる。そこを自前の照準で
//   置き換えると、何ができるのか分からなくなる。エディタ中はOSに任せる。
//
//   使い方(SceneManagerから):
//     Update(dt)     … 毎フレーム
//     Draw()         … スプライトの一番最後(何より手前に出す)
//==========================================================
class HjCursor
{
public:
	static HjCursor& Instance()
	{
		static HjCursor inst;
		return inst;
	}

	// 位置の更新と、消え具合の計算
	void Update(float dt);
	// 描画。他のUIより手前に出すので、スプライトの最後で呼ぶ
	void Draw();

private:
	HjCursor() = default;
	HjCursor(const HjCursor&) = delete;
	void operator=(const HjCursor&) = delete;

	// OSのカーソルを出すか隠すかを切り替える。
	// ShowCursorは呼んだ回数を数える仕組みなので、
	// 変わった瞬間にだけ呼ぶ必要がある
	void ApplySystemCursor(bool show);

	// 前フレームのカーソル位置。動いたかどうかの判定に使う
	float m_prevX = 0.0f;
	float m_prevY = 0.0f;
	// 最後に動かしてからの経過秒
	float m_idle  = 0.0f;
	// 表示の濃さ(0〜1)
	float m_alpha = 1.0f;

	// 押している間だけ小さくする。今の縮み具合
	float m_press = 0.0f;

	// OSカーソルの現在の状態。切り替わった時だけ ShowCursor を呼ぶ
	bool m_systemShown = true;
	bool m_initialized = false;
};
