#pragma once

#include "UIConst.h"

//==========================================================
// TitleMenuUI
//   Drift Project UI(claude.ai/design)の SCREEN1「メインメニュー」を
//   2Dスプライトで再現するUIオブジェクト。上下キーで項目選択。
//   実際の画面遷移(Enter→Game)は TitleScene 側が担当する。
//==========================================================
class TitleMenuUI : public KdGameObject
{
public:
	void Init()       override;
	void Update()     override;
	void DrawSprite() override;

	// 現在選択中のメニュー番号(0=PLAY …)。TitleSceneが参照。
	int  GetSelected() const { return m_sel; }
	// マウスクリックで決定されたか(1回読むとクリアされる)。TitleSceneが遷移に使う。
	bool ConsumeActivated() { bool a = m_activated; m_activated = false; return a; }

private:
	// デザイン座標(1536x864, 左上原点)→画面座標(中心原点)へ変換
	float MapX(float dx) const { return dx * UIConst::Scale - UIConst::HalfW; }
	float MapY(float dy) const { return UIConst::HalfH - dy * UIConst::Scale; }

	// 左上原点で文字を描く(pos.yが下端になる仕様を吸収)
	void  DrawText(int fontId, float dLeft, float dTop, float pxHeight,
		const char* str, const Math::Color& col);
	// 字間(トラッキング, デザインpx)付きで描く。負で詰める。戻り値=描画後の右端X(screen)。
	float DrawTextTracked(int fontId, float dLeft, float dTop, float pxHeight,
		const char* str, const Math::Color& col, float trackDesign);
	// 文字列の描画幅(screen px)を計測(トラッキング込み)
	float MeasureText(int fontId, const char* str, float trackDesign);
	// 縦書き(1文字ずつ下へ)。vertical-rl相当。
	void  DrawTextVert(int fontId, float dLeft, float dTop, float pxStep,
		const char* str, const Math::Color& col);
	// 横書きを時計回り90°回転した縦帯(CSS writing-mode:vertical-rl)。中心(dCx,dCy)で縦中央揃え。
	void  DrawTextRotated(int fontId, float dCx, float dCy,
		const char* str, const Math::Color& col, float trackDesign);

	// デザイン矩形(左上dx,dy,幅w,高さh)を塗る/囲む
	void  DrawRectTL(float dx, float dy, float w, float h, const Math::Color& col, bool fill = true);
	// デザイン矩形の位置にシーンRTを1:1で窓抜き描画(ゲーム画面をボックス形状にマスク)。
	// 将来ここへショーケースカメラの映像を流す。tintでゲーム画面に色を乗算(二階調化)できる。
	void  DrawSceneWindow(float dx, float dy, float w, float h, const Math::Color& tint = { 1.0f, 1.0f, 1.0f, 1.0f });
	// 太さ付きの枠(2px罫線をデザイン通りに)
	void  DrawFrameTL(float dx, float dy, float w, float h, float px, const Math::Color& col);

	// ハーフトーンの点フィールド(左上dx,dy,幅w,高さh)
	void  DrawDotField(float dx, float dy, float w, float h, const Math::Color& col);

	// デザイン座標での線・十字・破線・円(輪郭)
	void  DrawLineD(float x1, float y1, float x2, float y2, float px, const Math::Color& col);
	void  DrawCross(float cx, float cy, float r, float px, const Math::Color& col);
	void  DrawDashed(float x1, float y1, float x2, float y2, const Math::Color& col);
	void  DrawRingD(float cx, float cy, float r, const Math::Color& col);
	void  DrawDotD(float cx, float cy, float r, const Math::Color& col);

	// メニューアイコン(幾何形状でユニコード字形を代替)
	void  DrawMenuIcon(int index, float cx, float cy, const Math::Color& col);

	int  m_sel       = 0;      // 選択中メニュー
	bool m_prevUp    = false;  // 上キーの前フレーム状態
	bool m_prevDn    = false;  // 下キーの前フレーム状態
	bool m_activated = false;  // マウスクリックで決定された
};
