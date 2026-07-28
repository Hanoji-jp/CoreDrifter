#pragma once

#include "UIConst.h"

//==========================================================
// HjUI
//   Drift Project UI を描くための再利用可能な2Dツールキット(自作=Hj接頭辞)。
//   すべてデザイン座標(1536x864, 左上原点)で指定し、内部で画面座標(1280x720,
//   中心原点)へ変換する。各画面(タイトル/設定/ガレージ等)で共通利用する。
//==========================================================
namespace HjUI
{
	// ── 座標変換(デザイン1536x864 → 画面1280x720中心原点) ──
	inline float MapX(float dx) { return dx * UIConst::Scale - UIConst::HalfW; }
	inline float MapY(float dy) { return UIConst::HalfH - dy * UIConst::Scale; }

	// ── マウス入力(毎フレーム先頭で BeginInput を1回呼ぶ) ──
	void  BeginInput();                         // カーソル位置と左クリックのエッジを更新
	float MouseX();                             // カーソルX(デザイン座標)
	float MouseY();                             // カーソルY(デザイン座標)
	bool  Hover(float dx, float dy, float w, float h);   // 矩形の上にカーソルがあるか
	bool  Clicked(float dx, float dy, float w, float h); // 矩形内で左クリックした瞬間か

	// ── テキスト(左上基準。pxHはデザインのfont-size相当) ──
	// 返り値=描画後の右端X(screen)
	float Text(int fontId, float dx, float dy, float pxH, const char* str, const Math::Color& col);
	// 字間(トラッキング, デザインpx)付き。負で詰める。
	float TextTracked(int fontId, float dx, float dy, float pxH, const char* str, const Math::Color& col, float trackDesign);
	// 中央寄せテキスト(dCx=中心X, デザイン座標)
	void  TextC(int fontId, float dCx, float dy, float pxH, const char* str, const Math::Color& col);
	// 中心(dCx,dCy)基準でスケール補間して描く(スコアのパンチ演出用)。scale=1.0で等倍。
	void  TextCenteredScaled(int fontId, float dCx, float dCy, float scale, const char* str, const Math::Color& col);
	// アウトライン(縁だけ抽出=中空)文字。edge=縁色 / fill=中の塗り色(背景色を渡すと中空に見える)。
	void  TextOutline(int fontId, float dx, float dy, float pxH, const char* str,
		const Math::Color& edge, const Math::Color& fill);
	// 文字列の描画幅(screen px)を計測(トラッキング込み)
	float Measure(int fontId, const char* str, float trackDesign);
	// 横書きを時計回り90°回転した縦帯(writing-mode:vertical-rl)。中心(dCx,dCy)縦中央。
	void  TextRotated(int fontId, float dCx, float dCy, const char* str, const Math::Color& col, float trackDesign);

	// ── 図形(デザイン座標, 左上基準) ──
	void RectTL(float dx, float dy, float w, float h, const Math::Color& col, bool fill = true);
	void FrameTL(float dx, float dy, float w, float h, float px, const Math::Color& col);   // 太さpx(screen)の枠
	void LineD(float x1, float y1, float x2, float y2, float px, const Math::Color& col);
	void DotField(float dx, float dy, float w, float h, const Math::Color& col);            // ハーフトーン点
	void DotFieldTwinkle(float dx, float dy, float w, float h, const Math::Color& base, float phase);  // 明滅するドット
	// 波打つドット・ディゾルブ(矩形を密なドットで埋め、各ドットのサイズが波で0↔最大に=溶ける)
	void DotDissolve(float dx, float dy, float w, float h, const Math::Color& dot, float phase);
	void RingD(float cx, float cy, float r, const Math::Color& col);                        // 円(輪郭)
	// 画面端を暗くするビネット(strength=0..1)。ドリフト中の没入感演出などに。
	void Vignette(float strength);

	// ── クリップ(矩形外を描かせない。デザイン座標) ──
	void PushClip(float dx, float dy, float w, float h);   // この矩形内だけに描画を制限
	void PopClip();                                        // 画面全体に戻す

	// ── 選択カード用の中身演出(黒地の上にアクセント色で動く。すべて内部でクリップ) ──
	void FxStripes(float dx, float dy, float w, float h, const Math::Color& col, float t);   // ①流れる斜めハザードストライプ
	void FxChevrons(float dx, float dy, float w, float h, const Math::Color& col, float t);  // ②奥→手前に流れるシェブロン矢印
	void FxScanBar(float dx, float dy, float w, float h, const Math::Color& col, float t);   // ③上下に走査する光の帯
	void FxGiantType(float dx, float dy, float w, float h, const Math::Color& col, const char* label, float t); // ④背景に脈動する巨大タイポ

	// ── アニメーション(共有クロック) ──
	void  Tick(float dt);   // 毎フレーム1回、時間を進める(SceneManager::Updateから)
	float Time();           // 経過秒
	// うねる波線。(dx,dy)から長さlen、振幅amp、波数waves、流れる速さspeed、太さpx。
	void  WaveLine(float dx, float dy, float len, float amp, float waves, float speed, float px, const Math::Color& col);
	// 縦にうねる波線((dx,dy)から下へlen、横に振幅amp)
	void  WaveLineV(float dx, float dy, float len, float amp, float waves, float speed, float px, const Math::Color& col);

	// ── ウィジェット(すべてデザイン座標・左上基準) ──
	enum class BtnKind { Primary, Secondary, Ghost, Disabled };
	// 横長ボタン。primary=アシッド塗り/secondary=枠/ghost=文字のみ/disabled=半透明枠。
	// trailingを渡すと右端に添え文字(記号)を右寄せで描く。
	void Button(float dx, float dy, float w, float h, const char* label, BtnKind kind, const char* trailing = "");
	// 正方アイコンボタン(記号は幾何形状)。style:0=solid(黒) 1=outline 2=accent(緑)
	void IconButton(float dx, float dy, float s, int glyph, int style);
	// セグメントトグル(ON/OFF)。onFillはON時の塗り色(アシッド/黒など)。幅=124,高=34。
	void Toggle(float dx, float dy, bool on, const Math::Color& onFill);
	// タブ項目(active=アシッド塗り)
	void Tab(float dx, float dy, const char* label, bool active);
	// 2連バッジ(例: DRIVER01 | LV.23)。bBgは右側の背景色
	void Badge2(float dx, float dy, const char* a, const char* b, const Math::Color& bBg);
	// ステータスバー(ラベル＋0-1のバー)
	void StatBar(float dx, float dy, float w, const char* label, float value);
	// キーキャップ(枠付きキー＋説明ラベル)。戻り値=消費した幅(デザインpx)。
	float Keycap(float dx, float dy, const char* key, const char* label);
	// ステッパー/セレクト( < value > )。wは無視(内容で幅が決まる)。
	void Stepper(float dx, float dy, float w, const char* value);
	// ウィンドウヘッダー(タイトル＋✕)。bg=背景 fg=文字
	void WindowHeader(float dx, float dy, float w, const char* title, const Math::Color& bg, const Math::Color& fg);
	// パレット見本(色見本＋ラベル)
	void Swatch(float dx, float dy, const char* label, const Math::Color& col);
	// 見出し(番号付き。下線付き小見出し 例: "01 · BUTTON")
	void SectionLabel(float dx, float dy, const char* label);
}
