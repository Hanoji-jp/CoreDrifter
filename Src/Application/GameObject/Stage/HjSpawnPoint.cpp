#include "HjSpawnPoint.h"

#include <fstream>
#include <sstream>

namespace
{
	// 道の制御点と同じ場所に置く。作る側のデータなので、
	// 遊ぶ側の設定(save.dat)とは分ける
	constexpr const char* kPath = "Asset/Data/terrain/spawn.txt";
}

//----------------------------------------------------------
void HjSpawnPoint::Load()
{
	m_has = false;

	KdAssetIStream ifs(kPath);
	if (!ifs) { return; }

	// 覚え書きの行を飛ばす。
	//
	// 書き出す側が見出しを付けているのに、読む側が素の >> で
	// 受けていたので、先頭の '#' で必ず失敗していた。
	// 道や飾りの読み込みと同じ作法にする
	std::string line;
	while (std::getline(ifs, line))
	{
		if (line.empty() || line[0] == '#') { continue; }

		std::istringstream ss(line);

		float x = 0.0f, y = 0.0f, z = 0.0f, yaw = 0.0f;
		if (!(ss >> x >> y >> z >> yaw)) { continue; }

		m_pos = Math::Vector3(x, y, z);
		m_yaw = yaw;
		m_has = true;
		return;
	}
}

//----------------------------------------------------------
void HjSpawnPoint::Save() const
{
	// 決めていないなら、ファイルごと消す。
	//
	// 0 を書いて「決めていない」を表すと、
	// 原点に置いたのか決めていないのかが読めなくなる
	if (!m_has)
	{
		std::remove(kPath);
		return;
	}

	std::ofstream ofs(kPath);
	if (!ofs) { return; }

	ofs << "# 走り出す場所。x y z 向き(rad)\n";
	ofs << m_pos.x << " " << m_pos.y << " " << m_pos.z << " " << m_yaw << "\n";
}
