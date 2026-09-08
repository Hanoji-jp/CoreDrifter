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
	// ── 大きさを指定しない入口(推奨) ──
	// HjUI::Text の pxH は縦位置を決める値で、文字の大きさは変えない。
	// 大きさはフォントIDで決まっているので、そこから引いて渡す。
	// 呼ぶ側が数字を書かない＝食い違いようがない。
	float TextAt (int fontId, float dx, float dy, const char* str, const Math::Color& col);
	void  TextAtC(int fontId, float dCx, float dy, const char* str, const Math::Color& col);
	void  TextAtR(int fontId, float dRightX, float dy, const char* str, const Math::Color& col);
	float TextAtTracked(int fontId, float dx, float dy, const char* str,
		const Math::Color& col, float trackDesign);

	// フォントに無い大きさで出したいとき。左上を(dx,dy)に置いて拡大縮小する。
	// 大きな数字(速度など)は専用フォントを増やすより、これで伸ばす方が扱いやすい。
	void  TextScaled(int fontId, float dx, float dy, float targetPx,
		const char* str, const Math::Color& col);
	// 同じものを右端・中央に合わせて置く。
	// 伸ばした後の幅は元の幅×倍率なので、呼ぶ側で毎回計算しないで済むようにする。
	void  TextScaledR(int fontId, float dRightX, float dy, float targetPx,
		const char* str, const Math::Color& col);
	void  TextScaledC(int fontId, float dCx, float dy, float targetPx,
		const char* str, const Math::Color& col);

	// 中心基準・スケール付きの縁取り文字。
	// 3D画面の上に文字を置くと、背景の明暗が毎フレーム変わって読めなくなる。
	// 縁を付けると背景に関係なく形が立つので、演出文字には必須。
	void  TextCenteredScaledOutline(int fontId, float dCx, float dCy, float scale, const char* str,
		const Math::Color& edge, const Math::Color& fill);
	// アウトライン(縁だけ抽出=中空)文字。edge=縁色 / fill=中の塗り色(背景色を渡すと中空に見える)。
	void  TextOutline(int fontId, float dx, float dy, float pxH, const char* str,
		const Math::Color& edge, const Math::Color& fill);
	// 文字列の描画高さ(screen px)。文字テクスチャの実寸で、指定サイズとは違う。
	// 焼いた画像の縦横比を求めるのに要る。
	float MeasureHeight(int fontId, const char* str);
	// 文字列の描画幅(screen px)を計測(トラッキング込み)
	float Measure(int fontId, const char* str, float trackDesign);
	// 横書きを時計回り90°回転した縦帯(writing-mode:vertical-rl)。中心(dCx,dCy)縦中央。
	void  TextRotated(int fontId, float dCx, float dCy, const char* str, const Math::Color& col, float trackDesign);

	// ── 図形(デザイン座標, 左上基準) ──
	void RectTL(float dx, float dy, float w, float h, const Math::Color& col, bool fill = true);
	void FrameTL(float dx, float dy, float w, float h, float px, const Math::Color& col);   // 太さpx(screen)の枠
	void LineD(float x1, float y1, float x2, float y2, float px, const Math::Color& col);
	// 画像。デザイン座標の左上基準で、指定した枠いっぱいに引き伸ばす。
	// レンダーターゲットに描いた3Dの絵をUIへ貼るのに使う
	void TexRectTL(const KdTexture* tex, float dx, float dy, float w, float h,
	               const Math::Color& col = kWhiteColor);

	void DotField(float dx, float dy, float w, float h, const Math::Color& col);            // ハーフトーン点
	void DotFieldTwinkle(float dx, float dy, float w, float h, const Math::Color& base, float phase);  // 明滅するドット
	// 波打つドット・ディゾルブ(矩形を密なドットで埋め、各ドットのサイズが波で0↔最大に=溶ける)
	void DotDissolve(float dx, float dy, float w, float h, const Math::Color& dot, float phase);
	void RingD(float cx, float cy, float r, const Math::Color& col);                        // 円(輪郭)
	// 画面端を暗くするビネット(strength=0..1)。ドリフト中の没入感演出などに。
	void Vignette(float strength);

	// 右端を dRightX に合わせて描く。表の数値のように「右が揃う」方が
	// 一覧として読みやすい場面で使う。
	void  TextR(int fontId, float dRightX, float dy, float pxH, const char* str, const Math::Color& col);

	// ── 図形(追加分) ──
	void DiscD(float cx, float cy, float r, const Math::Color& col);   // 塗りつぶした円
	// 円弧。角度はラジアン、0=右、増える向きは画面上の時計回り(デザイン座標はY下向き)
	void ArcD(float cx, float cy, float r, float a0, float a1, float px, const Math::Color& col);
	// 折れ線。xy は {x0,y0, x1,y1, ...} が count 点ぶん
	void PolylineD(const float* xy, int count, float px, const Math::Color& col);
	// 多角形の塗りつぶし。xyは頂点の並び(x,y,x,y,...)。
	// 用意されている図形が矩形・線・円しかないので、
	// 横1本ずつの線に分けて塗る(走査線)。
	// 頂点の順番はどちら回りでもよい。凹んだ形にも使える。
	void PolyFillD(const float* xy, int count, const Math::Color& col);

	// ── 状態チップ ──
	// hot ならアシッド塗り、そうでなければ細い枠だけ。
	// 「点いている/いない」を面と線の差で見せるので、色を増やさずに済む。
	// 戻り値=消費した幅(デザインpx)。並べるときに使う。
	float Chip(float dx, float dy, const char* label, bool hot, const Math::Color& ink);

	// ── 表の見出し帯 ──
	// 黒帯に白文字。列の左端は colX[] で個別に指定する(等分割にしない)。
	void TableHead(float dx, float dy, float w, const char* const* cols,
		const float* colX, int count);

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
	// 枠付きキー＋説明ラベル。戻り値=消費した幅(デザインpx)。
	// col は枠と文字の色。映像の上に重ねる画面では墨だと沈むので、
	// 呼ぶ側で紙色を渡せるようにしてある(既定は従来どおり墨)。
	float Keycap(float dx, float dy, const char* key, const char* label);
	float Keycap(float dx, float dy, const char* key, const char* label, const Math::Color& col);
	// ステッパー/セレクト( < value > )。wは無視(内容で幅が決まる)。
	void Stepper(float dx, float dy, float w, const char* value);
	// ウィンドウヘッダー(タイトル＋✕)。bg=背景 fg=文字
	void WindowHeader(float dx, float dy, float w, const char* title, const Math::Color& bg, const Math::Color& fg);
	// パレット見本(色見本＋ラベル)
	void Swatch(float dx, float dy, const char* label, const Math::Color& col);
	// 見出し(番号付き。下線付き小見出し 例: "01 · BUTTON")
	void SectionLabel(float dx, float dy, const char* label);
}

//==========================================================
// HjUI::Deco
//   画面の余白を締めるための装飾。
//   どれも中身の情報を持たないので、配置を変えても意味は壊れない。
//   「線と点だけで作る」という方針を守るためにここへ集める。
//==========================================================
namespace HjUI::Deco
{
	// 同じ大きさの円を左上へずらしながら重ねる(タイトルのドリフト円)
	void DriftCircles(float startX, float startY, float r, int count,
		float stepX, float stepY, const Math::Color& col);
	// 枠の中に×。アシッド枠＋墨の×
	void BoxedX(float dx, float dy, float s);
	// 等間隔の点。cols×rows を gap 間隔で並べる
	void DotGrid(float dx, float dy, int cols, int rows, float gap, const Math::Color& col);
	// バーコード状の縦線。幅をばらつかせて印刷物の質感を出す
	void Barcode(float dx, float dy, float h, int count);

	// 薄い縦のガイド線。y0〜y1 だけを引く。
	// 途中で切って CropTick を置くと、製図の補助線のように見える。
	void Guide(float dx, float dy0, float dy1, const Math::Color& col);
	void CropTick(float dx, float dy, const Math::Color& col);

	// 画面四隅のかぎ括弧。中身を囲わずに枠を示すので、
	// 映像の上に置いても画面を塞がない。
	void CornerBrackets(float inset, float len, float w, float h,
		float px, const Math::Color& col);
}
