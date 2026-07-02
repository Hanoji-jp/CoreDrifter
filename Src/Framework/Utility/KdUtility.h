#pragma once

#include "../../Application/Util/AssetVault.h"   // 配布ビルド：埋め込みpak(VFS)の存在確認
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iterator>

class KdTexture;

//===========================================
//
// 便利機能
//
//===========================================
// 算術系短縮名
namespace Math = DirectX::SimpleMath;

// 角度変換
constexpr float KdToRadians = (3.141592654f / 180.0f);
constexpr float KdToDegrees = (180.0f / 3.141592654f);

// 安全にReleaseするための関数
template<class T>
void KdSafeRelease(T*& p)
{
	if (p)
	{
		p->Release();
		p = nullptr;
	}
}

// 安全にDeleteするための関数
template<class T>
void KdSafeDelete(T*& p)
{
	if (p)
	{
		delete p;
		p = nullptr;
	}
}

template<class T>
void DebugOutputNumber(T num)
{
	std::stringstream stream;
	stream << std::fixed << std::setprecision(2) << num;
	std::string str = stream.str();

	OutputDebugStringA(str.c_str());
}


//===========================================
//
// 色定数
//
//===========================================
static const Math::Color	kWhiteColor		= Math::Color(1.0f, 1.0f, 1.0f, 1.0f);
static const Math::Color	kBlackColor		= Math::Color(0.0f, 0.0f, 0.0f, 1.0f);
static const Math::Color	kRedColor		= Math::Color(1.0f, 0.0f, 0.0f, 1.0f);
static const Math::Color	kGreenColor		= Math::Color(0.0f, 1.0f, 0.0f, 1.0f);
static const Math::Color	kBlueColor		= Math::Color(0.0f, 0.0f, 1.0f, 1.0f);
static const Math::Color	kNormalColor	= Math::Color(0.5f, 0.5f, 1.0f, 1.0f);	// 垂直に伸びる法線情報


//===========================================
//
// ファイル
//
//===========================================
// ファイルの存在確認
inline bool KdFileExistence(std::string_view path)
{
	// 配布ビルド：埋め込みpak(VFS)にあればディスクに無くても存在扱い
	if (AssetVault::Exists(std::string(path))) { return true; }

	std::ifstream ifs(std::string(path).c_str());

	bool isExistence = ifs.is_open();

	ifs.close();

	return isExistence;
}

//===========================================
// アセット読み込み用 入力ストリーム
//   ディスク優先 → 無ければ埋め込みpak(VFS) から内容を読む istream。
//   既存の `std::ifstream ifs(path);` を `KdAssetIStream ifs(path);` に
//   置換するだけで、以降の if(!ifs) / getline(ifs,..) / ifs>>x はそのまま動く。
//   ・セーブ等のディスク書込ファイル … ディスク優先なので従来通り
//   ・配布ビルドのレベルデータ      … ディスクに無いので pak から読む
//   第2引数(openmode)は std::ifstream 互換のため受けるだけ（無視）。
//===========================================
class KdAssetIStream : public std::istringstream
{
public:
	explicit KdAssetIStream(const std::string& path, std::ios::openmode = std::ios::in)
	{
		std::string content;
		bool found = false;

		// ① ディスク優先
		{
			std::ifstream f(path, std::ios::binary);
			if (f)
			{
				content.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
				found = true;
			}
		}
		// ② 無ければ埋め込みpak(VFS)
		if (!found)
		{
			std::vector<uint8_t> bytes;
			if (AssetVault::Read(path, bytes))
			{
				content.assign(bytes.begin(), bytes.end());
				found = true;
			}
		}

		if (found) { str(content); }
		else       { setstate(std::ios::failbit); }   // 見つからない → if(!ifs) が真
	}
};

// ファイルパスから、親ディレクトリまでのパスを取得
inline std::string KdGetDirFromPath(const std::string &path)
{
	const std::string::size_type pos = std::max<signed>((signed)path.find_last_of('/'), (signed)path.find_last_of('\\'));
	return (pos == std::string::npos) ? std::string() : path.substr(0, pos + 1);
}

// ファイルパスから、拡張子抜きのパスを取得
inline std::string KdGetNameFromPath(const std::string& path, bool onlyFileName = false)
{
	std::string::size_type dirPos = 0;

	if (onlyFileName)
	{
		dirPos = std::max(0, std::max<signed>((signed)path.find_last_of('/'), (signed)path.find_last_of('\\'))) + 1;
	}

	const std::string::size_type extPos = path.find_last_of('.');

	return (extPos == std::string::npos) ? std::string() : path.substr(dirPos, extPos - dirPos);
}


//===========================================
//
// 文字列関係
//
//===========================================
// std::string版 sprintf
template <typename ... Args>
std::string KdFormat(std::string_view fmt, Args ... args)
{
	size_t len = std::snprintf(nullptr, 0, fmt, args ...);
	std::vector<char> buf(len + 1);
	std::snprintf(&buf[0], len + 1, fmt, args ...);
	return std::string(&buf[0], &buf[0] + len);
}

void KdGetTextureInfo(ID3D11View* view, D3D11_TEXTURE2D_DESC& outDesc);


//===========================================
//
// 座標変換関係
//
//===========================================
bool ConvertRectToUV(const KdTexture* srcTex, const Math::Rectangle& src, Math::Vector2& uvMin, Math::Vector2& uvMax);

//===========================================
//
// 算術計算関係
//
//===========================================
float EaseInOutSine(float progress);

inline Math::Vector3 ConvertToRadian(const Math::Vector3& _degree)
{
	Math::Vector3 vec3;
	vec3.x = _degree.x * (3.141592f / 180.0f);
	vec3.y = _degree.y * (3.141592f / 180.0f);
	vec3.z = _degree.z * (3.141592f / 180.0f);
	return vec3;
}
