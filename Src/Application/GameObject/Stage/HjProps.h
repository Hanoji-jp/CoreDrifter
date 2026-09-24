#pragma once

class HjHeightField;
class HjRoad;

//==========================================================
// HjProps
//   手で置く飾り(木・低木)。自作なので Hj 接頭辞。
//
//   ■ モデルは名前で覚える
//   置いたものは props.txt へ「モデル名 x z 向き 大きさ」で書く。
//   番号で覚えると、フォルダへ1つ足した瞬間に
//   置いてある木が全部別のモデルに化ける。
//
//   ■ 高さは持たない
//   組むときに地形から取る。覚えてしまうと、
//   地形を彫り直した所で浮いたり埋まったりする。
//
//   ■ 焼き込まない
//   草(HjFoliage)は変換済みの頂点として焼いたが、こちらは焼かない。
//   モデルは材質とテクスチャを持っているので、頂点だけ写しても
//   絵にならない。手で置く前提なので数は数百のはず
//==========================================================
class HjProps : public KdGameObject
{
public:
	// 置いてある1つ
	struct Placed
	{
		int   model = 0;      // 目録の何番目か
		float x = 0.0f;
		float z = 0.0f;
		float yaw = 0.0f;
		float scale = 1.0f;
	};

	void Init() override;

	//===== 目録 =====
	int         ModelCount() const { return static_cast<int>(m_models.size()); }
	const char* ModelName(int i) const;

	// モデルの属するまとまり(フォルダ)
	int         GroupOf(int model) const;
	int         GroupCount() const { return static_cast<int>(m_groups.size()); }
	const char* GroupName(int g) const;
	int         GroupMemberCount(int g) const;

	//===== 置くときに使うもの =====
	// まとまりを指すと、その中からランダムで置く。
	// 木は木で、桜は桜で散らしたい、という使い方のため
	void SetBrush(int i) { m_brush = i; m_brushGroup = -1; }
	void SetBrushGroup(int g) { m_brushGroup = g; }

	int  GetBrush() const { return m_brush; }
	int  GetBrushGroup() const { return m_brushGroup; }

	// いま置かれるモデル。
	//
	// ランダムのときは、カーソルの位置から決める。
	// 呼ぶたびに引き直すと、下見に出ているものと置かれるものが食い違う
	int  ResolveModel(float x, float z) const;

	//===== 置く前の下見 =====
	// 置く場所へ、そのモデルを実際の大きさで出す。
	// 「置いてから直す」ではなく「見てから置く」ようにするため
	void SetGhost(int model, const Math::Matrix& world) { m_ghost = model; m_ghostW = world; }
	void ClearGhost() { m_ghost = -1; }

	//===== 置く・消す =====
	bool AddAt(const HjHeightField& field, const HjRoad* road,
	           const Math::Vector3& pos);

	// 向きと大きさを指定して置く。編集側が下見と同じ姿で置くのに使う
	bool AddExplicit(const HjHeightField& field, const HjRoad* road,
	                 int model, const Math::Vector3& pos, float yaw, float scale);

	// 置けるかどうかだけ見る。下見を赤くするのに使う
	bool CanPlace(const HjHeightField& field, const HjRoad* road,
	              const Math::Vector3& pos, float& outY) const;

	//===== 置いてあるものを触る =====
	const Placed& At(int i) const { return m_placed[i]; }
	Placed&       WorkAt(int i)   { return m_placed[i]; }
	void SetGroundY(int i, float y);

	void RemoveAt(int index);
	void Clear();

	int  PlacedCount() const { return static_cast<int>(m_placed.size()); }

	// 視線に一番近いものを選ぶ
	int Pick(const Math::Vector3& origin, const Math::Vector3& dir) const;

	//===== 出し入れ =====
	void Load();
	void Save() const;

	// 地形から高さを取り直す。地形を彫ったら呼ぶ
	void Refresh(const HjHeightField& field);

	void DrawLit() override;
	void GenerateDepthMapFromLight() override;

	// 1つずつ判定するので、オブジェクト単位の判定からは外す
	bool CheckInScreen(const DirectX::BoundingFrustum&) const override { return true; }

	int GetDrawnCount() const { return m_drawn; }

private:
	// 目録の1件
	struct Model
	{
		std::string name;
		std::string path;

		// 属するまとまり(フォルダ)の番号
		int group = 0;

		// 読み込みは最初に使うときまで遅らせる。
		// 39個を起動時に全部読むと、置かないものまで待たされる
		std::shared_ptr<KdModelWork> work;

		// モデルの実寸。読んだときに1回だけ測る。
		//
		// 画面外を切る球を決め打ちにすると、大きいモデルが
		// 画面の端で消える。小さくしたものは点になって、
		// 中心が外れた瞬間に消える
		Math::Vector3 bndCenter;
		float         bndRadius = 0.0f;
		bool          bndKnown  = false;
	};

	// フォルダをさらって目録を作る
	void ScanModels();

	// 埋め込み(pak)から目録を作る。
	//
	// 配布ビルドは Asset/ が exe の中にあり、フォルダとしては無い。
	// 目録が空のままだと props.txt の行は全部「知らないモデル」扱いで
	// 捨てられ、置いた飾りが1つも出ない
	void ScanModelsFromPak();

	// 名前から番号を引く。無ければ -1
	int FindModel(const std::string& name) const;

	// 必要になったら読む
	KdModelWork* WorkOf(int index);

	// 画面外を切る球。まだ測れていなければ false(切らずに描く)
	bool CullSphereOf(const Placed& p, float groundY,
	                  Math::Vector3& outCenter, float& outRadius) const;

	// 置いてあるものの世界行列
	Math::Matrix MatrixOf(const Placed& p) const;

	std::vector<Model>  m_models;

	// まとまりの名前(Map_Props からの相対フォルダ)
	std::vector<std::string> m_groups;
	std::vector<Placed> m_placed;

	// 置いてあるものの地面の高さ。Refresh で取り直す
	std::vector<float>  m_groundY;

	// 下見。-1 なら出さない
	int          m_ghost = -1;
	Math::Matrix m_ghostW;

	int m_brush = 0;

	// -1 ならモデルを直に指している。0以上ならそのまとまりからランダム
	int m_brushGroup = -1;
	int m_drawn = 0;
};
