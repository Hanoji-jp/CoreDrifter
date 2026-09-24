#include "HjProps.h"

#include "HjHeightField.h"
#include "HjRoad.h"

#include "../../Const/PropConst.h"
#include "../../Const/TerrainConst.h"
#include "../../Util/HjProfiler.h"
#include "../../Util/AssetVault.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
namespace PC = PropConst;

namespace
{
	constexpr float Pi2 = 6.28318530718f;

	// 座標から作る整数ハッシュ。
	// 乱数を使うと、同じ場所へ置き直すだけで姿が変わる
	unsigned int Hash(int x, int z, unsigned int salt)
	{
		unsigned int h = static_cast<unsigned int>(x) * 374761393u
		               + static_cast<unsigned int>(z) * 668265263u
		               + salt * 2246822519u;

		h = (h ^ (h >> 13)) * 1274126177u;
		return h ^ (h >> 16);
	}

	float Rand01(unsigned int h)
	{
		return (h & 0xFFFFFFu) / static_cast<float>(0xFFFFFF);
	}

	unsigned int SeedOf(float x, float z, float salt)
	{
		return Hash(static_cast<int>(x * 100.0f),
		            static_cast<int>(z * 100.0f),
		            static_cast<unsigned int>(salt));
	}
}

//----------------------------------------------------------
void HjProps::Init()
{
	m_drawType = eDrawTypeLit;

	ScanModels();
	Load();
}

//----------------------------------------------------------
// フォルダをさらって目録を作る
//
// 階層は問わない。Trees/Tree/Tree1/... のように分かれているので、
// 下まで潜って .gltf を全部拾う
//----------------------------------------------------------
void HjProps::ScanModels()
{
	m_models.clear();

	std::error_code ec;

	// 配布ビルドは Asset/ が exe の中にあり、フォルダとしては無い。
	// ここで空のまま返すと、置いた飾りが1つも出なくなる
	if (!fs::exists(PC::Dir, ec) || !fs::is_directory(PC::Dir, ec))
	{
		ScanModelsFromPak();
		return;
	}

	// 例外ではなくエラーコードで受ける版を使う。
	// 読めないフォルダがあっても、そこで止まらず次へ進みたい
	for (const auto& ent : fs::recursive_directory_iterator(PC::Dir, ec))
	{
		if (ec) { break; }
		if (static_cast<int>(m_models.size()) >= PC::MaxModels) { break; }

		if (!ent.is_regular_file(ec)) { continue; }

		const fs::path& p = ent.path();

		std::string ext = p.extension().string();
		for (auto& ch : ext) { ch = static_cast<char>(tolower(ch)); }

		if (ext != ".gltf" && ext != ".glb") { continue; }

		Model m;

		// 名前は Map_Props からの相対パス(拡張子なし)。
		//
		// ファイル名だけだと重複する。実際 Bush13 と Bush14 は
		// Bush/ と Trees/Tree/Tree2/ の両方にあり、名前で保存すると
		// 読み戻したときに別のモデルになる
		{
			const fs::path rel = fs::relative(p, PC::Dir, ec);
			fs::path base = ec ? p.stem() : rel;
			if (!ec) { base.replace_extension(); }

			m.name = base.generic_string();
			if (ec) { m.name = p.stem().string(); }
		}

		// フレームワークのアセット管理はパスの文字列をそのまま鍵にするので、
		// 区切りを揃えておく。揃えないと同じファイルが二重に読み込まれる
		m.path = p.generic_string();

		m_models.push_back(std::move(m));
	}

	// 名前で並べる。フォルダを走る順は環境で変わるので、
	// 一覧に出す並びが起動ごとに違うと選びにくい
	std::sort(m_models.begin(), m_models.end(),
	          [](const Model& a, const Model& b) { return a.name < b.name; });

	//===== まとまり(フォルダ)を作る =====
	// 名前は Map_Props からの相対パスなので、最後の "/" より前が
	// そのままフォルダになる。
	//
	// 種別を名前から当てようとしない。Trees/Tree/Tree2 の中身は
	// Bush13 のような名前をしているが、あれは木のパックに入った低木で、
	// 名前で振り分けると持ち主のフォルダから離れてしまう
	m_groups.clear();

	for (auto& m : m_models)
	{
		const size_t cut = m.name.rfind('/');
		const std::string dir = (cut == std::string::npos)
		                      ? std::string("(root)")
		                      : m.name.substr(0, cut);

		auto it = std::find(m_groups.begin(), m_groups.end(), dir);
		if (it == m_groups.end())
		{
			m.group = static_cast<int>(m_groups.size());
			m_groups.push_back(dir);
		}
		else
		{
			m.group = static_cast<int>(std::distance(m_groups.begin(), it));
		}
	}
}

//----------------------------------------------------------
// 埋め込み(pak)から飾りの目録を作る
//
// 名前の付け方はフォルダ走査版と同じにする。
// 違うと、props.txt に書いた名前で引けなくなる
//----------------------------------------------------------
void HjProps::ScanModelsFromPak()
{
	std::vector<std::string> paths;
	AssetVault::List(PC::Dir, paths);

	for (const std::string& path : paths)
	{
		if (static_cast<int>(m_models.size()) >= PC::MaxModels) { break; }

		const fs::path p(path);

		std::string ext = p.extension().string();
		for (auto& ch : ext) { ch = static_cast<char>(tolower(ch)); }

		if (ext != ".gltf" && ext != ".glb") { continue; }

		Model m;

		// 名前は Map_Props からの相対パス(拡張子なし)。
		// 実体が無いので、文字列だけで相対を出す
		{
			fs::path base = p.lexically_relative(PC::Dir);
			if (base.empty()) { base = p.stem(); }
			base.replace_extension();

			m.name = base.generic_string();
		}

		m.path = p.generic_string();

		m_models.push_back(std::move(m));
	}
}

//----------------------------------------------------------
const char* HjProps::ModelName(int i) const
{
	if (i < 0 || i >= static_cast<int>(m_models.size())) { return ""; }
	return m_models[i].name.c_str();
}

//----------------------------------------------------------
int HjProps::GroupOf(int model) const
{
	if (model < 0 || model >= static_cast<int>(m_models.size())) { return -1; }
	return m_models[model].group;
}

const char* HjProps::GroupName(int g) const
{
	if (g < 0 || g >= static_cast<int>(m_groups.size())) { return ""; }
	return m_groups[g].c_str();
}

int HjProps::GroupMemberCount(int g) const
{
	int n = 0;
	for (const auto& m : m_models) { if (m.group == g) { ++n; } }
	return n;
}

//----------------------------------------------------------
// いま置かれるモデル
//
// ランダムのときは、カーソルの位置から決める。
//
// 呼ぶたびに引き直すと、下見に出ているものと実際に置かれるものが
// 食い違う。位置から決めれば、下見がそのまま置かれるうえ、
// カーソルを動かすたびに次の候補が見えて選びやすい
//----------------------------------------------------------
int HjProps::ResolveModel(float x, float z) const
{
	if (m_models.empty()) { return -1; }

	if (m_brushGroup < 0)
	{
		return std::clamp(m_brush, 0, static_cast<int>(m_models.size()) - 1);
	}

	// そのまとまりの中で、LOD を除いたもの
	std::vector<int> pool;
	pool.reserve(m_models.size());

	const std::string skip = PC::RandomSkipSuffix;

	for (size_t i = 0; i < m_models.size(); ++i)
	{
		if (m_models[i].group != m_brushGroup) { continue; }

		// LOD は遠景用の粗いモデル。近くに置くと形が破綻して見える
		const std::string& nm = m_models[i].name;
		if (nm.size() >= skip.size()
		 && nm.compare(nm.size() - skip.size(), skip.size(), skip) == 0)
		{
			continue;
		}

		pool.push_back(static_cast<int>(i));
	}

	if (pool.empty()) { return -1; }

	const unsigned int h = SeedOf(x, z, PC::RandomSalt);
	return pool[h % pool.size()];
}

//----------------------------------------------------------
int HjProps::FindModel(const std::string& name) const
{
	for (size_t i = 0; i < m_models.size(); ++i)
	{
		if (m_models[i].name == name) { return static_cast<int>(i); }
	}
	return -1;
}

//----------------------------------------------------------
// 必要になったら読む
//
// 39個を起動時に全部読むと、置かないものまで待たされる
//----------------------------------------------------------
KdModelWork* HjProps::WorkOf(int index)
{
	if (index < 0 || index >= static_cast<int>(m_models.size())) { return nullptr; }

	Model& m = m_models[index];
	if (!m.work)
	{
		m.work = std::make_shared<KdModelWork>();
		m.work->SetModelData(m.path);

		// 読めなかったものは、次に呼ばれたときに読み直さない。
		// 毎フレーム読み込みを試すと、1つ壊れているだけで止まる
		if (!m.work->GetData()) { return nullptr; }

		//===== 実寸を測る =====
		// 画面外を切る球に使う。読んだ直後に1回だけ
		{
			bool any = false;
			Math::Vector3 lo, hi;

			for (const auto& node : m.work->GetData()->GetOriginalNodes())
			{
				if (!node.m_spMesh) { continue; }

				const auto& box = node.m_spMesh->GetBoundingBox();

				// 箱の8隅をノードの行列に通す。
				// 中心だけ運ぶと、ノード側に縮尺が入っているモデルで
				// 大きさを取り違える
				for (int k = 0; k < 8; ++k)
				{
					const Math::Vector3 corner = {
						box.Center.x + ((k & 1) ? box.Extents.x : -box.Extents.x),
						box.Center.y + ((k & 2) ? box.Extents.y : -box.Extents.y),
						box.Center.z + ((k & 4) ? box.Extents.z : -box.Extents.z),
					};

					const Math::Vector3 w =
						Math::Vector3::Transform(corner, node.m_worldTransform);

					if (!any) { lo = w; hi = w; any = true; }
					else
					{
						lo = Math::Vector3::Min(lo, w);
						hi = Math::Vector3::Max(hi, w);
					}
				}
			}

			if (any)
			{
				m.bndCenter = (lo + hi) * 0.5f;
				m.bndRadius = ((hi - lo) * 0.5f).Length();
				m.bndKnown  = true;
			}
		}
	}

	return m.work->GetData() ? m.work.get() : nullptr;
}

//----------------------------------------------------------
//----------------------------------------------------------
// 置けるか
//
// 急な斜面と、地形の面が無い所には置かせない。
//
// 高さマップは当たり判定用なので、道の裾に覆われて四角を張っていない
// マス目でも高さを返す。それを鵜呑みにすると面の無い空中に木が立つ
//----------------------------------------------------------
// 画面外を切る球
//
// モデルの実寸から出す。決め打ちにすると、大きいモデルが
// 画面の端で消え、小さくしたものは中心が外れた瞬間に消える。
//
// まだ読んでいないモデルは測りようがないので false を返す。
// そのときは切らずに描く。1度描けば読まれて、次からは測れる
//----------------------------------------------------------
bool HjProps::CullSphereOf(const Placed& p, float groundY,
                           Math::Vector3& outCenter, float& outRadius) const
{
	if (p.model < 0 || p.model >= static_cast<int>(m_models.size())) { return false; }

	const Model& m = m_models[p.model];
	if (!m.bndKnown) { return false; }

	// 中心はモデルのローカル。向きは回るので、
	// 水平のずれは半径へ足して包んでしまう
	const float off = sqrtf(m.bndCenter.x * m.bndCenter.x
	                      + m.bndCenter.z * m.bndCenter.z) * p.scale;

	outCenter = Math::Vector3(p.x,
	                          groundY - PC::Sink + m.bndCenter.y * p.scale,
	                          p.z);

	outRadius = m.bndRadius * p.scale + off;
	return true;
}

//----------------------------------------------------------
bool HjProps::CanPlace(const HjHeightField& field, const HjRoad* road,
                       const Math::Vector3& pos, float& outY) const
{
	outY = 0.0f;

	if (!field.Contains(pos.x, pos.z)) { return false; }

	Math::Vector3 n;
	field.SampleAt(pos.x, pos.z, outY, n);

	if (outY <= TerrainConst::OutsideHeight) { return false; }

	// 面の巻き方で法線が下を向くことがあるので大きさだけ見る
	const float up = (n.y < 0.0f) ? -n.y : n.y;
	if (up < PC::UpMin) { return false; }

	// 道の面の上にも置かせる。
	//
	// 裾に飾りを置きたい、という要望がある。裾は道のメッシュなので、
	// 地形の四角が張られていない(IsCoveredCell)所に当たるが、
	// そこに面が無いわけではない。
	//
	// 高さは地形のものを使う。DeformTerrain が地形を道の高さへ
	// 寄せてあるので、裾の面とはほぼ同じ高さに来る。
	// 残る差は Sink(10cm 埋める)が吸う。
	//
	// 道の真ん中にも置けてしまうが、これは手で置く道具なので、
	// 置ける場所を機械が決めるより、置けるほうがよい
	(void)road;
	return true;
}

//----------------------------------------------------------
// 向きと大きさを指定して置く
//----------------------------------------------------------
bool HjProps::AddExplicit(const HjHeightField& field, const HjRoad* road,
                          int model, const Math::Vector3& pos, float yaw, float scale)
{
	if (static_cast<int>(m_placed.size()) >= PC::MaxPlaced) { return false; }
	if (m_models.empty()) { return false; }

	float y = 0.0f;
	if (!CanPlace(field, road, pos, y)) { return false; }

	Placed p;
	if (model < 0) { return false; }
	p.model = std::clamp(model, 0, static_cast<int>(m_models.size()) - 1);
	p.x = pos.x;
	p.z = pos.z;
	p.yaw = yaw;
	p.scale = std::clamp(scale, PC::ScaleLimitMin, PC::ScaleLimitMax);

	m_placed.push_back(p);
	m_groundY.push_back(y);
	return true;
}

//----------------------------------------------------------
// 置く(向きと大きさは座標から作る)
//----------------------------------------------------------
bool HjProps::AddAt(const HjHeightField& field, const HjRoad* road,
                    const Math::Vector3& pos)
{
	const float yaw = Rand01(SeedOf(pos.x, pos.z, PC::YawSalt)) * Pi2;
	const float sc  = PC::ScaleMin
	                + (PC::ScaleMax - PC::ScaleMin)
	                * Rand01(SeedOf(pos.x, pos.z, PC::ScaleSalt));

	return AddExplicit(field, road, ResolveModel(pos.x, pos.z), pos, yaw, sc);
}

//----------------------------------------------------------
void HjProps::SetGroundY(int i, float y)
{
	if (i < 0 || i >= static_cast<int>(m_groundY.size())) { return; }
	m_groundY[i] = y;
}

//----------------------------------------------------------
void HjProps::RemoveAt(int index)
{
	if (index < 0 || index >= static_cast<int>(m_placed.size())) { return; }

	m_placed.erase(m_placed.begin() + index);

	if (index < static_cast<int>(m_groundY.size()))
	{
		m_groundY.erase(m_groundY.begin() + index);
	}
}

//----------------------------------------------------------
void HjProps::Clear()
{
	m_placed.clear();
	m_groundY.clear();
}

//----------------------------------------------------------
// 視線に一番近いものを選ぶ
//
// 上から見た距離で拾う。モデルの形に厳密に当てても、
// 掴めた感じは良くならない
//----------------------------------------------------------
int HjProps::Pick(const Math::Vector3& origin, const Math::Vector3& dir) const
{
	int   best  = -1;
	float bestT = 1e18f;

	const Math::Vector3 rd(dir.x, 0.0f, dir.z);
	const float len2 = rd.LengthSquared();
	if (len2 < 1e-8f) { return -1; }

	for (size_t i = 0; i < m_placed.size(); ++i)
	{
		const float dx0 = m_placed[i].x - origin.x;
		const float dz0 = m_placed[i].z - origin.z;

		const float t = (dx0 * rd.x + dz0 * rd.z) / len2;
		if (t <= 0.0f) { continue; }

		const float px = origin.x + rd.x * t;
		const float pz = origin.z + rd.z * t;

		const float dx = px - m_placed[i].x;
		const float dz = pz - m_placed[i].z;

		const float r = std::max(PC::PickRadius * m_placed[i].scale,
		                         PC::PickRadiusMin);
		if (dx * dx + dz * dz > r * r) { continue; }

		if (t < bestT) { bestT = t; best = static_cast<int>(i); }
	}

	return best;
}

//----------------------------------------------------------
// 読む
//
// 1行に「モデル名 x z 向き 大きさ」。
// 目録に無い名前の行は捨てる(モデルを消したときのため)
//----------------------------------------------------------
void HjProps::Load()
{
	m_placed.clear();
	m_groundY.clear();

	KdAssetIStream ifs(PC::SavePath);
	if (!ifs) { return; }

	std::string line;
	while (std::getline(ifs, line))
	{
		if (line.empty() || line[0] == '#') { continue; }

		std::istringstream ss(line);

		std::string name;
		Placed p;
		if (!(ss >> name >> p.x >> p.z >> p.yaw >> p.scale)) { continue; }

		const int m = FindModel(name);
		if (m < 0) { continue; }

		p.model = m;
		m_placed.push_back(p);
		m_groundY.push_back(0.0f);
	}
}

//----------------------------------------------------------
void HjProps::Save() const
{
	std::ofstream ofs(PC::SavePath);
	if (!ofs) { return; }

	ofs << "# 置いた飾り。1行に モデル名 x z 向き 大きさ\n";
	ofs << "# 高さは書かない。地形から取るので、彫り直しても地面に乗ったまま\n";

	for (const auto& p : m_placed)
	{
		if (p.model < 0 || p.model >= static_cast<int>(m_models.size())) { continue; }

		ofs << m_models[p.model].name << " "
		    << p.x << " " << p.z << " " << p.yaw << " " << p.scale << "\n";
	}
}

//----------------------------------------------------------
// 地面の高さを取り直す
//----------------------------------------------------------
void HjProps::Refresh(const HjHeightField& field)
{
	m_groundY.assign(m_placed.size(), 0.0f);

	for (size_t i = 0; i < m_placed.size(); ++i)
	{
		m_groundY[i] = field.HeightAt(m_placed[i].x, m_placed[i].z);
	}
}

//----------------------------------------------------------
Math::Matrix HjProps::MatrixOf(const Placed& p) const
{
	const size_t i = static_cast<size_t>(&p - m_placed.data());
	const float  y = (i < m_groundY.size()) ? m_groundY[i] : 0.0f;

	return Math::Matrix::CreateScale(p.scale)
	     * Math::Matrix::CreateRotationY(p.yaw)
	     * Math::Matrix::CreateTranslation(p.x, y - PC::Sink, p.z);
}

//----------------------------------------------------------
void HjProps::DrawLit()
{
	if (m_placed.empty()) { return; }

	HjScopedTimer _t(U8(" 飾りの描画"));

	// 画面に映るものだけ描く。
	// 1つずつ描くので、切らないと置いた数だけ命令が飛ぶ
	const auto& cam = KdShaderManager::Instance().GetCameraCB();

	DirectX::BoundingFrustum frustum;
	DirectX::BoundingFrustum::CreateFromMatrix(frustum, cam.mProj);

	Math::Matrix invView = cam.mView;
	invView = invView.Invert();
	frustum.Transform(frustum, invView);

	auto& shader = KdShaderManager::Instance().m_StandardShader;

	// 葉は板で作られている。片面だと裏から見たときに消える
	KdShaderManager::Instance().ChangeRasterizerState(KdRasterizerState::CullNone);

	m_drawn = 0;
	for (const auto& p : m_placed)
	{
		const size_t i = static_cast<size_t>(&p - m_placed.data());
		const float  y = (i < m_groundY.size()) ? m_groundY[i] : 0.0f;

		// 画面外を切る。モデルの実寸から出した球で見る
		Math::Vector3 cc;
		float cr = 0.0f;
		if (CullSphereOf(p, y, cc, cr))
		{
			if (!frustum.Intersects(DirectX::BoundingSphere(cc, cr))) { continue; }
		}

		KdModelWork* w = WorkOf(p.model);
		if (!w) { continue; }

		shader.DrawModel(*w, MatrixOf(p));
		++m_drawn;
	}

	//===== 下見 =====
	// 置く前に、そのモデルを実際の大きさで地面に出す。
	// 色を変えて「まだ置いていない」ことを見せる
	if (m_ghost >= 0)
	{
		if (KdModelWork* g = WorkOf(m_ghost))
		{
			shader.DrawModel(*g, m_ghostW,
			                 Math::Vector4(PC::GhostR, PC::GhostG, PC::GhostB, 1.0f));
		}
	}

	KdShaderManager::Instance().UndoRasterizerState();
}

//----------------------------------------------------------
void HjProps::GenerateDepthMapFromLight()
{
	if (m_placed.empty()) { return; }

	auto& shader = KdShaderManager::Instance().m_StandardShader;

	// 影は光から描くので、カメラの範囲では切らない
	for (const auto& p : m_placed)
	{
		KdModelWork* w = WorkOf(p.model);
		if (!w) { continue; }

		shader.DrawModel(*w, MatrixOf(p));
	}
}
