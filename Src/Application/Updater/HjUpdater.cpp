#include "HjUpdater.h"

#include "../main.h"

#include <windows.h>
#include <wininet.h>
#include <fstream>
#include <sstream>

#include "json.hpp"

#pragma comment(lib, "wininet.lib")

using Json = nlohmann::json;
namespace UC = UpdaterConst;

HjUpdater::~HjUpdater()
{
	// 終わるときに通信が残っていることがある。
	// 待つとゲームが閉じないので、やめる印を立てて切り離す
	m_cancel = true;
	try
	{
		if (m_thread.joinable()) { m_thread.detach(); }
	}
	catch (...) {}
}

//----------------------------------------------------------
// 手元の版
//----------------------------------------------------------
std::string HjUpdater::GetCurrentVersion()
{
	std::ifstream ifs(UC::VersionFile);
	if (!ifs) { return UC::UnknownVersion; }

	std::string v;
	std::getline(ifs, v);

	// 改行や余白が混ざると、版の比べ方が狂う
	while (!v.empty() && (v.back() == '\r' || v.back() == '\n' || v.back() == ' '))
	{
		v.pop_back();
	}
	return v.empty() ? UC::UnknownVersion : v;
}

std::string HjUpdater::GetLatestVersion() const
{
	std::lock_guard<std::mutex> lk(m_mutex);
	return m_latestVersion;
}

std::string HjUpdater::GetErrorMessage() const
{
	std::lock_guard<std::mutex> lk(m_mutex);
	return m_errorMsg;
}

const char* HjUpdater::StateText() const
{
	switch (m_state)
	{
	case State::Idle:        return "";
	case State::Checking:    return U8("更新を確認しています");
	case State::Available:   return U8("新しい版があります");
	case State::UpToDate:    return U8("最新です");
	case State::Downloading: return U8("受け取っています");
	case State::Ready:       return U8("再起動すると入れ替わります");
	case State::Failed:      return U8("確認できませんでした");
	}
	return "";
}

//----------------------------------------------------------
// 版くらべ
//----------------------------------------------------------
bool HjUpdater::IsNewer(const std::string& a, const std::string& b)
{
	// "v1.2.3" を数の並びにする。
	// 数字でないものは区切りとして読み飛ばす
	auto parse = [](const std::string& s)
	{
		std::vector<int> out;
		int cur = 0;
		bool has = false;

		for (char c : s)
		{
			if (c >= '0' && c <= '9')
			{
				cur = cur * 10 + (c - '0');
				has = true;
			}
			else if (has)
			{
				out.push_back(cur);
				cur = 0;
				has = false;
			}
		}
		if (has) { out.push_back(cur); }
		return out;
	};

	const std::vector<int> va = parse(a);
	const std::vector<int> vb = parse(b);

	// 桁数が違うことがある("v1.2" と "v1.2.0")。
	// 足りないほうは0として比べる
	const size_t n = std::max(va.size(), vb.size());
	for (size_t i = 0; i < n; ++i)
	{
		const int x = (i < va.size()) ? va[i] : 0;
		const int y = (i < vb.size()) ? vb[i] : 0;
		if (x != y) { return x > y; }
	}
	return false;   // 同じ
}

//----------------------------------------------------------
// スレッドの起こし方
//----------------------------------------------------------
bool HjUpdater::Launch(void (HjUpdater::*fn)(), const char* failMsg)
{
	try
	{
		// 前のものが残っていたら片付ける。
		// 片付けずに入れ直すと terminate で落ちる
		if (m_thread.joinable()) { m_thread.join(); }
		m_thread = std::thread(fn, this);
		return true;
	}
	catch (...)
	{
		std::lock_guard<std::mutex> lk(m_mutex);
		m_errorMsg = failMsg;
		m_state = State::Failed;
		return false;
	}
}

void HjUpdater::StartCheck()
{
	if (IsWorking()) { return; }

	{
		std::lock_guard<std::mutex> lk(m_mutex);
		m_errorMsg.clear();
	}
	m_state = State::Checking;
	Launch(&HjUpdater::CheckThread, U8("確認を始められませんでした"));
}

void HjUpdater::StartDownload()
{
	if (IsWorking()) { return; }

	{
		std::lock_guard<std::mutex> lk(m_mutex);
		m_errorMsg.clear();
	}
	m_progress = 0.0f;
	m_cancel   = false;
	m_state    = State::Downloading;
	Launch(&HjUpdater::DownloadThread, U8("受け取りを始められませんでした"));
}

void HjUpdater::CancelDownload()
{
	if (m_state != State::Downloading) { return; }

	m_cancel = true;
	m_state  = State::Idle;
}

//----------------------------------------------------------
// 問い合わせ
//----------------------------------------------------------
void HjUpdater::CheckThread()
{
	// ここから例外を外へ出さない。
	// スレッドの外へ抜けると、その場でゲームが落ちる
	try
	{
		const std::string path =
			std::string("/repos/") + UC::Owner + "/" + UC::Repo + "/releases/latest";

		std::string body;
		if (!HttpGet(UC::ApiHost, path, body))
		{
			std::lock_guard<std::mutex> lk(m_mutex);
			m_errorMsg = U8("GitHub へ繋げませんでした");
			m_state = State::Failed;
			return;
		}

		std::string version, url;
		if (!ParseRelease(body, version, url))
		{
			std::lock_guard<std::mutex> lk(m_mutex);
			m_errorMsg = U8("リリースの情報を読み取れませんでした");
			m_state = State::Failed;
			return;
		}

		{
			std::lock_guard<std::mutex> lk(m_mutex);
			m_latestVersion = version;
			m_downloadUrl   = url;
		}

		m_state = IsNewer(version, GetCurrentVersion())
			? State::Available : State::UpToDate;
	}
	catch (...)
	{
		std::lock_guard<std::mutex> lk(m_mutex);
		m_errorMsg = U8("確認に失敗しました");
		m_state = State::Failed;
	}
}

void HjUpdater::DownloadThread()
{
	try
	{
		std::string url;
		{
			std::lock_guard<std::mutex> lk(m_mutex);
			url = m_downloadUrl;
		}

		const bool ok = DownloadFile(url, UC::ZipTemp, UC::MaxRedirect);

		// やめた場合。
		// 途中まで書いたものを残すと、次に「揃っている」と誤って扱う
		if (m_cancel)
		{
			::DeleteFileA(UC::ZipTemp);
			return;
		}

		if (!ok)
		{
			::DeleteFileA(UC::ZipTemp);
			std::lock_guard<std::mutex> lk(m_mutex);
			m_errorMsg = U8("受け取りに失敗しました");
			m_state = State::Failed;
			return;
		}

		m_progress = 1.0f;
		m_state    = State::Ready;
	}
	catch (...)
	{
		::DeleteFileA(UC::ZipTemp);
		std::lock_guard<std::mutex> lk(m_mutex);
		m_errorMsg = U8("受け取りに失敗しました");
		m_state = State::Failed;
	}
}

//----------------------------------------------------------
// 通信
//----------------------------------------------------------
bool HjUpdater::HttpGet(const std::string& host, const std::string& path,
                        std::string& outBody)
{
	HINTERNET hNet = InternetOpenA(UC::UserAgent, INTERNET_OPEN_TYPE_PRECONFIG,
	                               nullptr, nullptr, 0);
	if (!hNet) { return false; }

	HINTERNET hConn = InternetConnectA(hNet, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT,
	                                   nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
	if (!hConn) { InternetCloseHandle(hNet); return false; }

	const char* accepts[] = { "*/*", nullptr };
	HINTERNET hReq = HttpOpenRequestA(hConn, "GET", path.c_str(), nullptr, nullptr,
	                                  accepts, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
	if (!hReq) { InternetCloseHandle(hConn); InternetCloseHandle(hNet); return false; }

	// GitHub は名乗らない相手を弾く
	const std::string headers =
		std::string("User-Agent: ") + UC::UserAgent + "\r\n"
		"Accept: application/vnd.github+json\r\n";
	HttpAddRequestHeadersA(hReq, headers.c_str(),
	                       static_cast<DWORD>(-1), HTTP_ADDREQ_FLAG_ADD);

	bool ok = false;
	if (HttpSendRequestA(hReq, nullptr, 0, nullptr, 0))
	{
		std::vector<char> buf(UC::BodyChunk + 1);
		DWORD read = 0;
		while (InternetReadFile(hReq, buf.data(), UC::BodyChunk, &read) && read > 0)
		{
			outBody.append(buf.data(), read);
		}
		ok = true;
	}

	InternetCloseHandle(hReq);
	InternetCloseHandle(hConn);
	InternetCloseHandle(hNet);
	return ok;
}

bool HjUpdater::DownloadFile(const std::string& url, const std::string& destPath,
                             int redirectLeft)
{
	// 回り続ける相手に当たったときのために、追う回数を区切る
	if (redirectLeft <= 0) { return false; }

	URL_COMPONENTSA uc = {};
	uc.dwStructSize = sizeof(uc);
	char hostBuf[256] = {}, pathBuf[2048] = {};
	uc.lpszHostName = hostBuf;  uc.dwHostNameLength = sizeof(hostBuf);
	uc.lpszUrlPath  = pathBuf;  uc.dwUrlPathLength  = sizeof(pathBuf);
	if (!InternetCrackUrlA(url.c_str(), 0, 0, &uc)) { return false; }

	HINTERNET hNet = InternetOpenA(UC::UserAgent, INTERNET_OPEN_TYPE_PRECONFIG,
	                               nullptr, nullptr, 0);
	if (!hNet) { return false; }

	HINTERNET hConn = InternetConnectA(hNet, hostBuf, uc.nPort, nullptr, nullptr,
	                                   INTERNET_SERVICE_HTTP, 0, 0);
	if (!hConn) { InternetCloseHandle(hNet); return false; }

	DWORD flags = INTERNET_FLAG_RELOAD;
	if (uc.nScheme == INTERNET_SCHEME_HTTPS) { flags |= INTERNET_FLAG_SECURE; }

	const char* accepts[] = { "*/*", nullptr };
	HINTERNET hReq = HttpOpenRequestA(hConn, "GET", pathBuf, nullptr, nullptr,
	                                  accepts, flags, 0);
	if (!hReq) { InternetCloseHandle(hConn); InternetCloseHandle(hNet); return false; }

	const std::string ua = std::string("User-Agent: ") + UC::UserAgent + "\r\n";
	HttpAddRequestHeadersA(hReq, ua.c_str(),
	                       static_cast<DWORD>(-1), HTTP_ADDREQ_FLAG_ADD);

	auto closeAll = [&]()
	{
		InternetCloseHandle(hReq);
		InternetCloseHandle(hConn);
		InternetCloseHandle(hNet);
	};

	if (!HttpSendRequestA(hReq, nullptr, 0, nullptr, 0))
	{
		// GitHub のリリースは配信元へ回されるので、行き先を辿る
		DWORD status = 0, sLen = sizeof(status);
		HttpQueryInfoA(hReq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
		               &status, &sLen, nullptr);

		if (status == HTTP_STATUS_MOVED || status == HTTP_STATUS_REDIRECT)
		{
			char location[2048] = {};
			DWORD lLen = sizeof(location);
			if (HttpQueryInfoA(hReq, HTTP_QUERY_LOCATION, location, &lLen, nullptr))
			{
				closeAll();
				return DownloadFile(location, destPath, redirectLeft - 1);
			}
		}
		closeAll();
		return false;
	}

	// 全体の大きさ。進み具合を出すのに使う。
	// 分からないこともあるので、0のときは進み具合を出さない
	DWORD total = 0;
	{
		char clBuf[32] = {};
		DWORD clLen = sizeof(clBuf);
		if (HttpQueryInfoA(hReq, HTTP_QUERY_CONTENT_LENGTH, clBuf, &clLen, nullptr))
		{
			total = static_cast<DWORD>(atol(clBuf));
		}
	}

	std::ofstream ofs(destPath, std::ios::binary);
	if (!ofs) { closeAll(); return false; }

	std::vector<char> buf(UC::ReadChunk);
	DWORD read = 0;
	DWORD got  = 0;

	while (InternetReadFile(hReq, buf.data(), UC::ReadChunk, &read) && read > 0)
	{
		if (m_cancel) { ofs.close(); closeAll(); return false; }

		ofs.write(buf.data(), read);
		got += read;

		if (total > 0)
		{
			m_progress = static_cast<float>(got) / static_cast<float>(total);
		}
	}
	ofs.close();
	closeAll();

	return got > 0;
}

//----------------------------------------------------------
// リリースの読み取り
//----------------------------------------------------------
bool HjUpdater::ParseRelease(const std::string& json,
                             std::string& outVersion, std::string& outUrl)
{
	// 壊れた応答で落とさない。
	// 通信の相手が必ず正しいものを返すとは限らない
	const Json root = Json::parse(json, nullptr, false);
	if (root.is_discarded() || !root.is_object()) { return false; }

	const auto tag = root.find("tag_name");
	if (tag == root.end() || !tag->is_string()) { return false; }
	outVersion = tag->get<std::string>();

	const auto assets = root.find("assets");
	if (assets == root.end() || !assets->is_array()) { return false; }

	// 名前の終わりが .zip のものを選ぶ。
	// 先頭を決め打ちすると、pdb などが先に並んだときに掴んでしまう
	const std::string suffix = UC::AssetSuffix;
	for (const auto& a : *assets)
	{
		const auto name = a.find("name");
		const auto url  = a.find("browser_download_url");
		if (name == a.end() || url == a.end())   { continue; }
		if (!name->is_string() || !url->is_string()) { continue; }

		const std::string n = name->get<std::string>();
		if (n.size() < suffix.size()) { continue; }
		if (n.compare(n.size() - suffix.size(), suffix.size(), suffix) != 0) { continue; }

		outUrl = url->get<std::string>();
		break;
	}

	return !outVersion.empty() && !outUrl.empty();
}

//----------------------------------------------------------
// 入れ替え
//----------------------------------------------------------
void HjUpdater::Apply()
{
	if (m_state != State::Ready) { return; }

	// 動いている exe は掴まれていて置き換えられない。
	// 「閉じるのを待ってから入れ替える」役を、別のプロセスへ渡す
	const DWORD pid = GetCurrentProcessId();

	std::ofstream bat(UC::BatchFile);
	if (!bat) { return; }

	bat <<
		"@echo off\n"
		"echo Waiting for game to exit...\n"
		":wait\n"
		"tasklist /FI \"PID eq " << pid << "\" 2>NUL | find /I \"" << pid << "\" >NUL\n"
		"if not errorlevel 1 (\n"
		"    timeout /t " << UC::WaitTickSec << " /nobreak >NUL\n"
		"    goto wait\n"
		")\n"
		"echo Extracting update...\n"
		"powershell -NoProfile -Command \""
			"Expand-Archive -Path '" << UC::ZipTemp << "' -DestinationPath '.' -Force\"\n"
		"del " << UC::ZipTemp << "\n"
		"echo Update complete! Starting game...\n"
		"start " << UC::ExeName << "\n"
		"del \"%~f0\"\n";   // 走り終わったら自分を消す
	bat.close();

	STARTUPINFOA si = {};
	si.cb          = sizeof(si);
	si.dwFlags     = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_MINIMIZE;
	PROCESS_INFORMATION pi = {};

	std::string cmd = std::string("cmd.exe /c ") + UC::BatchFile;
	CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0,
	               nullptr, nullptr, &si, &pi);

	if (pi.hProcess) { CloseHandle(pi.hProcess); }
	if (pi.hThread)  { CloseHandle(pi.hThread); }

	// ここでゲームを閉じる。あとはバッチが引き継ぐ
	Application::Instance().End();
}
