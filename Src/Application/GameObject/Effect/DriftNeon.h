#pragma once

#include "../../Const/NeonFxConst.h"
#include "HjCullView.h"   // 画面に映らない粒を頂点に積む前に捨てる

//==========================================================
// DriftNeon
//   ドリフト時にタイヤ周りへ出すネオンのライン画エフェクト(NFS Unbound風)。
//   ・リング：タイヤを囲む輪が広がりながら消える(渦・落書き感)
//   ・スパーク：接地点から短い線が飛び散る
//   線はテクスチャ無しの細い板ポリで描き、加算合成で発光させる。
//
//   使い方(所有者=CarBase):
//     Init()                        … 起動時に1回
//     Emit(pos, axis, carVel, n, m) … 後輪位置からリングn個・スパークm個
//     Update(dt)                    … 毎フレーム
//     DrawEffect()                  … UnLitパス内で描画
//==========================================================
class DriftNeon
{
public:
	void Init();
	// pos=接地点 / axis=タイヤの回転軸(車の右方向) / carVel=車速
	void Emit(const Math::Vector3& pos, const Math::Vector3& axis,
	          const Math::Vector3& carVel, int ringCount, int sparkCount);
	// ブースト発動時の爆発。全方向へ勢いよく弾けさせる(ニトロの"ぱぁん")
	void Burst(const Math::Vector3& pos, const Math::Vector3& carVel);

	void Update(float dt);
	void DrawEffect();
	// 輝度(LightBloom)パス用。ここへ描いた絵はポストプロセスでぼかされて加算される＝本物のグロー
	void DrawBrightPass();

	// 2色パレット(粒ごとに混色)
	void SetColors(const Math::Vector3& a, const Math::Vector3& b) { m_colA = a; m_colB = b; }

private:
	// bright=true なら輝度RTへグロー層のみを描く(ぼかされて加算される)
	void DrawInternal(bool bright);

	enum class Kind { Ring, Spark, Dot };   // Dot=丸い粒(線と混ぜて飛ばす)

	struct Particle
	{
		Kind          kind   = Kind::Ring;
		Math::Vector3 pos    = Math::Vector3::Zero;
		Math::Vector3 vel    = Math::Vector3::Zero;   // Spark用
		Math::Vector3 axisU  = Math::Vector3::Zero;   // Ring平面の基底1
		Math::Vector3 axisV  = Math::Vector3::Zero;   // Ring平面の基底2
		float         age    = 0.0f;
		float         life   = 1.0f;
		float         radius = 0.5f;   // Ring:初期半径 / Spark:長さの倍率(線の長短をばらす)
		float         rot    = 0.0f;   // Ring: 面内回転
		float         rotVel = 0.0f;
		float         mix    = 0.0f;   // 2色の混色比
		float         seed   = 0.0f;   // 半径揺らぎ用
		bool          alive  = false;
	};

	std::vector<Particle>            m_particles;
	std::shared_ptr<KdSquarePolygon> m_poly;      // 線分1本ぶんの板ポリ(使い回す)
	std::vector<KdPolygon::Vertex>   m_discVerts;   // 丸い粒用の円盤(半径1。ビルボードで描く)
	std::vector<KdPolygon::Vertex>   m_streakVerts; // 線用の紡錘形(両端が尖った菱形。長方形だと棒に見える)
	// 全粒をまとめた頂点列。これを1回で描く(粒ごとに描くと描画回数が数百回になる)
	std::vector<KdPolygon::Vertex>   m_batch;
	HjCullView m_cull;   // 画面に映る粒だけを積むための判定器
	Math::Vector3 m_colA = { NeonFxConst::ColorAR, NeonFxConst::ColorAG, NeonFxConst::ColorAB };
	Math::Vector3 m_colB = { NeonFxConst::ColorBR, NeonFxConst::ColorBG, NeonFxConst::ColorBB };
	int          m_next = 0;
	unsigned int m_rng  = 88675123u;

	float Rand01();
	Particle& NextSlot();
};
