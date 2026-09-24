#pragma once

class HjRoad;

//==========================================================
// HjRoadMark
//   路面標示(白線)。自作なので Hj 接頭辞。
//
//   ■ 飾りではない
//   一様な灰色の帯では、カーブでどこに車を置いているのかが読めない。
//   ドリフトは進入位置がすべてなので、線は計器として要る。
//
//   ■ 道と同じ仕掛けで作る
//   断面を刻みごとに並べて押し出す。道の刻みの決め方と
//   ミター接合をそのまま借りるので、ヘアピンでも破綻しない。
//
//   ■ 路面から浮かせる
//   同じ高さに置くと、どちらが手前か決まらずちらつく。
//   2cm 浮かせて、深度の争いを避ける
//==========================================================
class HjRoadMark : public KdGameObject
{
public:
	// 道から組む。道を作り直したら、こちらも呼ぶ
	void Build(const HjRoad& road);

	void DrawLit() override;
	void GenerateDepthMapFromLight() override;

	// 道と同じで数百メートルにわたるので、球では収まらない
	bool CheckInScreen(const DirectX::BoundingFrustum&) const override { return true; }

	void SetVisible(bool on) { m_visible = on; }
	bool IsVisible() const { return m_visible; }

	float GetLength() const { return m_length; }

private:
	// 1本ぶんの帯を積む。
	//   offset : 中心線からの横のずれ(m)
	//   width  : 線の太さ(m)
	//   dashed : 破線にするか
	// 1本ぶんの帯を積む。
	//
	//   skip : この道のりの区間では引かない。
	//          ヘアピンでセンターラインを切るのに使う
	void BuildStripe(const HjRoad& road, float offset, float width,
	                 bool dashed, unsigned int color,
	                 std::vector<KdMeshVertex>& verts,
	                 std::vector<KdMeshFace>& faces,
	                 const std::vector<std::pair<float, float>>* skip = nullptr);

	// カーブ手前の減速マーク。
	// 間隔を詰めながら横向きの帯を並べる
	void BuildSlowBars(const HjRoad& road, unsigned int color,
	                   std::vector<KdMeshVertex>& verts,
	                   std::vector<KdMeshFace>& faces);

	// カーブ1つぶん。曲がりの強さで出すものが変わる
	struct Curve
	{
		float entryS = 0.0f;
		float exitS  = 0.0f;

		// 区間の中で一番強かった曲がり。符号が曲がる向き(正=右)
		float peak = 0.0f;

		// 0=何も出さない 1=外側だけ 2=両車線にも 3=ヘアピン(1車線)
		int tier = 0;
	};

	// 道を1回なめて、カーブの区間と強さを拾う
	static void CollectCurves(const HjRoad& road, std::vector<Curve>& out);

	// センターラインを消す区間。
	//
	// ヘアピンが続く所は道幅が足りなくて1車線扱いになる。
	// 1つあるだけでは消さない。連続していることが条件
	static void CenterCutRanges(const std::vector<Curve>& curves,
	                            std::vector<std::pair<float, float>>& out);

	// カーブ1つぶん。外側に並べる。
	//
	//   sign  : 外側。右が + 、左が -
	//   inner : 中心線からどこまで内側へ引くか
	void AddOuterBars(const HjRoad& road, float entryS, float exitS, float sign,
	                  float inner, unsigned int color,
	                  std::vector<KdMeshVertex>& verts,
	                  std::vector<KdMeshFace>& faces);

	// 進入側の車線へ、その人の手前から並べる。
	//
	//   entryS : その人にとってのカーブ入口(道のり)
	//   dir    : 進む向き。+1 なら道のりが増える向きへ走っている
	//   sign   : 帯を置く車線。右が + 、左が -
	void AddApproach(const HjRoad& road, float entryS, float dir, float sign,
	                 unsigned int color,
	                 std::vector<KdMeshVertex>& verts,
	                 std::vector<KdMeshFace>& faces);

	// 道のりを指す位置に、片側の車線だけへ帯を1本置く
	void AddLaneBar(const HjRoad& road, float s, float sign,
	                float inner, float outer, float len, unsigned int color,
	                std::vector<KdMeshVertex>& verts,
	                std::vector<KdMeshFace>& faces);

	std::shared_ptr<KdMesh> m_spMesh;
	std::vector<KdMaterial> m_materials;

	bool  m_visible = true;
	float m_length  = 0.0f;
};
