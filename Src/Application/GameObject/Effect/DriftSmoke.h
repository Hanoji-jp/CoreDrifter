#pragma once

#include "../../Const/SmokeConst.h"

//==========================================================
// DriftSmoke
//   ドリフト時に後輪から立ち上るトゥーン調のスモークを管理する。
//   瘤(こぶ)状ブロブ4種のアトラスから粒ごとにランダムで選び、
//   カメラに正対するビルボードを UnLit(陰影なし)で描画する。
//   パーティクルはリングバッファで固定数を使い回す(new/deleteしない)。
//
//   使い方(所有者=CarBaseを想定):
//     Init()             … 起動時に1回
//     Emit(pos,vel,n)    … 後輪位置から n 枚放出
//     Update(dt)         … 毎フレーム寿命・移動を更新
//     DrawEffect()       … UnLitパス内でビルボード描画
//==========================================================
class DriftSmoke
{
public:
	void Init();
	void Emit(const Math::Vector3& pos, const Math::Vector3& baseVel, int count);
	void Update(float dt);
	void DrawEffect();

	// 煙の色味(NFS Unbound風にカラー煙にもできる。既定は白)
	void SetTint(const Math::Vector3& tint) { m_tint = tint; }

private:
	// 1粒のスモーク
	struct Particle
	{
		Math::Vector3 pos    = Math::Vector3::Zero;
		Math::Vector3 vel    = Math::Vector3::Zero;
		float         age    = 0.0f;   // 経過時間(秒)
		float         life   = 1.0f;   // 寿命(秒)
		float         rot    = 0.0f;   // ビルボード面内回転(rad)
		float         rotVel = 0.0f;   // 面内回転速度(rad/s)
		float         sizeMul = 1.0f;  // 粒ごとのサイズばらつき倍率
		int           variant = 0;     // アトラス内のブロブ種類(0〜3)
		bool          alive  = false;
	};

	std::vector<Particle>            m_particles;   // 固定長プール
	std::shared_ptr<KdSquarePolygon> m_poly;        // 共有ビルボード板ポリ
	Math::Vector3 m_tint = { 1.0f, 1.0f, 1.0f };    // 色味
	int          m_next = 0;                         // 次に上書きするスロット
	unsigned int m_rng  = 2463534242u;               // xorshift乱数の状態

	float Rand01();   // 0.0〜1.0 の乱数
};
