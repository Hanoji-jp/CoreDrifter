#pragma once

#include "../../Const/SmokeConst.h"
#include "HjCullView.h"   // 画面に映らない粒を頂点に積む前に捨てる

//==========================================================
// DriftSmoke
//   ドリフト時に後輪から立ち上るトゥーン調のスモーク(NFS Unbound風)。
//   板ポリのビルボードではなく、変形した球の「本物の3Dメッシュ(頂点)」を粒ごとに描く。
//     ・面法線で平行光のトゥーン陰影を計算し、頂点カラーへ焼き込む＝立体に光が当たる
//     ・寿命に沿って色A→色Bへグラデーション＝根元と先端で色が変わる
//     ・シルエット輪郭(KdPostProcessShaderのSmokeOutline)が効いて塊に見える
//   メッシュは起動時に数種類だけ生成して使い回す(粒ごとに位置と大きさだけ変える)。
//
//   使い方(所有者=CarBaseを想定):
//     Init()             … 起動時に1回(メッシュ生成)
//     Emit(pos,vel,n)    … 後輪位置から n 個放出
//     Update(dt)         … 毎フレーム寿命・移動を更新
//     DrawEffect()       … UnLitパス内で描画
//==========================================================
class DriftSmoke
{
public:
	void Init();
	// pos=放出位置 / baseVel=車速の逆向きに引きずる速度 / outward=車の外側(横)方向
	// sizeScale=粒の大きさの倍率(前輪の擦れなど、控えめに出したい時に下げる)
	void Emit(const Math::Vector3& pos, const Math::Vector3& baseVel,
	          const Math::Vector3& outward, int count, float sizeScale = 1.0f);
	void Update(float dt);
	void DrawEffect();

	// 煙の色。発生源から離れるほど A(手前) → B(奥) へグラデーションする。
	void SetTint(const Math::Vector3& tint)  { m_tint = tint; }
	void SetTintB(const Math::Vector3& tint) { m_tintB = tint; }
	// グラデーションが色Bになりきるまでの距離(m)
	void SetGradDist(float dist) { m_gradDist = dist; }
	// 一番光が当たる面の色(既定は白)
	void SetHighlight(const Math::Vector3& col) { m_hiColor = col; }

private:
	// 1粒のスモーク
	struct Particle
	{
		Math::Vector3 pos     = Math::Vector3::Zero;
		Math::Vector3 vel     = Math::Vector3::Zero;
		float         age     = 0.0f;   // 経過時間(秒)
		float         life    = 1.0f;   // 寿命(秒)
		float         phase   = 0.0f;   // 乱流の位相ずらし
		float         sizeMul = 1.0f;   // 粒ごとのサイズばらつき倍率
		float         yaw     = 0.0f;   // Y軸回転(同じメッシュでも別の形に見せる)
		float         squashX = 1.0f;   // ローカルX方向の潰し/伸ばし
		float         squashY = 1.0f;   // 縦方向の潰し/伸ばし
		float         squashZ = 1.0f;   // ローカルZ方向。Xと変えると細長い筋になる
		int           variant = 0;      // 使う塊メッシュの種類
		bool          alive   = false;
	};

	// 変形した球のメッシュを1つ作る(面法線でトゥーン陰影を頂点カラーへ焼き込む)
	void BuildBlobMesh(std::vector<KdPolygon::Vertex>& out, unsigned int seed);

	std::vector<Particle> m_particles;                        // 固定長プール
	std::vector<std::vector<KdPolygon::Vertex>> m_meshes;     // 塊メッシュ(種類ぶん)
	// 全粒をワールド空間へ変換して繋げた頂点列。これを1回で描く。
	// 粒ごとにDrawVerticesを呼ぶと、定数バッファ転送・状態変更・頂点転送が
	// 粒数ぶん走って極端に重くなるため、まとめて1ドローにする。
	std::vector<KdPolygon::Vertex> m_batch;
	HjCullView m_cull;   // 画面に映る粒だけを積むための判定器

	Math::Vector3 m_tint  = { 1.0f, 1.0f, 1.0f };        // 色A(発生源の手前)
	Math::Vector3 m_tintB = { 1.0f, 1.0f, 1.0f };        // 色B(奥。距離でこちらへ寄る)
	float         m_gradDist = SmokeConst::SmokeGradDist; // 色Bになりきる距離(m)
	Math::Vector3 m_hiColor = { 1.0f, 1.0f, 1.0f };      // ハイライトの色
	// 煙の発生源(全粒で共有する陰影・グラデーションの基準)
	float m_baseY   = 0.0f;   // 根元のワールド高さ
	float m_originX = 0.0f;   // 発生源のワールドX
	float m_originZ = 0.0f;   // 発生源のワールドZ
	int          m_next = 0;                         // 次に上書きするスロット
	unsigned int m_rng  = 2463534242u;               // xorshift乱数の状態

	float Rand01();   // 0.0〜1.0 の乱数
};
