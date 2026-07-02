#include "KdAudio.h"
#include "../../Application/Util/AssetVault.h"   // 配布ビルド：埋め込みpakからメモリ読み込み

// ── mp3 等を PCM へデコードするための Media Foundation ──
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wrl/client.h>
#include <shlwapi.h>      // SHCreateMemStream（メモリ→IStream）
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cmath>
#include <unordered_map>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")

// XAudio2エンジンが生存中か（終了時にエンジン破棄後のボイス操作で落ちるのを防ぐ）
static bool g_audioAlive = false;

namespace
{
	// 拡張子（小文字）で末尾一致
	bool EndsWithExtCI(std::string_view name, std::string_view ext)
	{
		if (name.size() < ext.size()) { return false; }
		for (size_t i = 0; i < ext.size(); ++i)
		{
			const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(name[name.size() - ext.size() + i])));
			const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[i])));
			if (a != b) { return false; }
		}
		return true;
	}

	// 既に生成した IMFSourceReader から 16bit PCM を全デコードする共通処理。
	bool DecodeReaderToPcm(IMFSourceReader* reader, std::vector<uint8_t>& outPcm, WAVEFORMATEX& outWfx)
	{
		using Microsoft::WRL::ComPtr;
		if (!reader) { return false; }

		// 音声ストリームのみ選択
		reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
		reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), TRUE);

		// 出力を 16bit PCM に指定（デコーダが自動で挟まる）
		ComPtr<IMFMediaType> pcmType;
		if (FAILED(MFCreateMediaType(pcmType.GetAddressOf()))) { return false; }
		pcmType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		pcmType->SetGUID(MF_MT_SUBTYPE,    MFAudioFormat_PCM);
		pcmType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
		if (FAILED(reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, pcmType.Get())))
		{
			return false;
		}

		// 実際に決まった出力フォーマットを取得
		ComPtr<IMFMediaType> actual;
		if (FAILED(reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), actual.GetAddressOf())))
		{
			return false;
		}

		UINT32 channels = 0, sampleRate = 0, bits = 16;
		actual->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS,     &channels);
		actual->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
		actual->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,  &bits);
		if (channels == 0 || sampleRate == 0 || bits == 0) { return false; }

		ZeroMemory(&outWfx, sizeof(outWfx));
		outWfx.wFormatTag      = WAVE_FORMAT_PCM;
		outWfx.nChannels       = static_cast<WORD>(channels);
		outWfx.nSamplesPerSec  = sampleRate;
		outWfx.wBitsPerSample  = static_cast<WORD>(bits);
		outWfx.nBlockAlign     = static_cast<WORD>(channels * bits / 8);
		outWfx.nAvgBytesPerSec = sampleRate * outWfx.nBlockAlign;
		outWfx.cbSize          = 0;

		// 全サンプルを読み出して連結
		for (;;)
		{
			DWORD flags = 0;
			ComPtr<IMFSample> sample;
			if (FAILED(reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
				0, nullptr, &flags, nullptr, sample.GetAddressOf())))
			{
				return false;
			}
			if (flags & MF_SOURCE_READERF_ENDOFSTREAM) { break; }
			if (!sample) { continue; }

			ComPtr<IMFMediaBuffer> buffer;
			if (FAILED(sample->ConvertToContiguousBuffer(buffer.GetAddressOf()))) { return false; }

			BYTE* data = nullptr; DWORD len = 0;
			if (FAILED(buffer->Lock(&data, nullptr, &len))) { return false; }
			outPcm.insert(outPcm.end(), data, data + len);
			buffer->Unlock();
		}

		return !outPcm.empty();
	}

	// ファイルパス(URL)から 16bit PCM へデコード。
	bool DecodeAudioToPcm(const wchar_t* path, std::vector<uint8_t>& outPcm, WAVEFORMATEX& outWfx)
	{
		using Microsoft::WRL::ComPtr;
		ComPtr<IMFSourceReader> reader;
		if (FAILED(MFCreateSourceReaderFromURL(path, nullptr, reader.GetAddressOf()))) { return false; }
		return DecodeReaderToPcm(reader.Get(), outPcm, outWfx);
	}

	// メモリ上の圧縮音源(mp3等)から 16bit PCM へデコード（配布ビルドのVFS用）。
	// originName は拡張子付きファイル名（MFがデコーダを選ぶヒント）。
	bool DecodeAudioToPcmMem(const uint8_t* data, size_t size, const wchar_t* originName,
		std::vector<uint8_t>& outPcm, WAVEFORMATEX& outWfx)
	{
		using Microsoft::WRL::ComPtr;
		if (!data || size == 0) { return false; }

		// メモリ → IStream（SHCreateMemStream は内部にコピーを持つので data は後で解放可）
		ComPtr<IStream> stream;
		stream.Attach(SHCreateMemStream(data, static_cast<UINT>(size)));
		if (!stream) { return false; }

		ComPtr<IMFByteStream> byteStream;
		if (FAILED(MFCreateMFByteStreamOnStream(stream.Get(), byteStream.GetAddressOf()))) { return false; }

		// メモリbyte streamは拡張子/MIMEが無くMFがデコーダを選べないので、形式ヒントを付与
		ComPtr<IMFAttributes> attr;
		if (SUCCEEDED(byteStream.As(&attr)))
		{
			attr->SetString(MF_BYTESTREAM_CONTENT_TYPE, L"audio/mpeg");
			if (originName && *originName) { attr->SetString(MF_BYTESTREAM_ORIGIN_NAME, originName); }
		}

		ComPtr<IMFSourceReader> reader;
		if (FAILED(MFCreateSourceReaderFromByteStream(byteStream.Get(), nullptr, reader.GetAddressOf()))) { return false; }

		return DecodeReaderToPcm(reader.Get(), outPcm, outWfx);
	}

	// メモリデコードが不調な環境向けフォールバック：%TEMP% に一時ファイルとして
	// 書き出し、実証済みの URL デコーダで読み、直後に削除する（ディスク露出は一瞬）。
	bool DecodeAudioViaTempFile(const uint8_t* data, size_t size, std::vector<uint8_t>& outPcm, WAVEFORMATEX& outWfx)
	{
		if (!data || size == 0) { return false; }

		wchar_t tmpDir[MAX_PATH] = {};
		if (GetTempPathW(MAX_PATH, tmpDir) == 0) { return false; }
		wchar_t tmpName[MAX_PATH] = {};
		if (GetTempFileNameW(tmpDir, L"kda", 0, tmpName) == 0) { return false; }

		// GetTempFileName が作る .tmp は MF が形式判定しにくいので .mp3 にして書き出す
		std::wstring placeholder = tmpName;
		std::wstring mp3Path     = placeholder + L".mp3";

		{
			std::ofstream ofs(mp3Path, std::ios::binary | std::ios::trunc);
			if (!ofs) { ::DeleteFileW(placeholder.c_str()); return false; }
			ofs.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
		}
		::DeleteFileW(placeholder.c_str());

		const bool ok = DecodeAudioToPcm(mp3Path.c_str(), outPcm, outWfx);
		::DeleteFileW(mp3Path.c_str());
		return ok;
	}
}

// ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### #####
//
// KdAudioManager
// 
// ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### #####

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 初期化
// ・DirectXAudioEngineの初期化 
// ・3Dリスナーの設定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::Init()
{
	// AudioEngine初期化
	DirectX::AUDIO_ENGINE_FLAGS eflags = DirectX::AudioEngine_ReverbUseFilters;

	m_audioEng = std::make_unique<DirectX::AudioEngine>(eflags);
	m_audioEng->SetReverb(DirectX::Reverb_Default);
	g_audioAlive = true;   // KdBgmVoice がボイス操作してよい

	m_listener.OrientFront = { 0, 0, 1 };

	// Media Foundation 初期化（mp3 等のデコード用）。COM は AudioEngine 生成時点で初期化済み前提。
	if (!m_mfInitialized)
	{
		if (SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_LITE)))
		{
			m_mfInitialized = true;
		}
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 更新
// ・DirectXAudioEngineの更新
// ・プレイリストから不要なサウンドインスタンスを削除
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::Update()
{
	// 実はこれを実行しなくても音はなる:定期的に実行する必要はある
	if (m_audioEng != nullptr)
	{
		m_audioEng->Update();
	}

	// ストップさせたインスタンスは終了したと判断してリストから削除
	for (auto iter = m_playList.begin(); iter != m_playList.end();)
	{
		if (iter->second->IsStopped())
		{
			iter = m_playList.erase(iter);

			continue;
		}

		++iter;
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// リスナーの座標と正面方向の設定
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::SetListnerMatrix(const Math::Matrix& mWorld)
{
	// 座標
	m_listener.SetPosition(mWorld.Translation());

	// 正面方向
	m_listener.OrientFront = mWorld.Backward();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 2Dサウンドの再生
// ・サウンドアセットの取得orロード
// ・再生用インスタンスの生成
// ・管理用プレイリストへの追加
// ・戻り値で再生インスタンスを取得可能（音量・ピッチなどを変更する場合に必要
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
std::shared_ptr<KdSoundInstance> KdAudioManager::Play(std::string_view rName, bool loop)
{
	if (!m_audioEng) { return nullptr; }

	std::shared_ptr<KdSoundEffect> soundData = GetSound(rName);

	if (!soundData) { return nullptr; }

	std::shared_ptr<KdSoundInstance> instance = std::make_shared<KdSoundInstance>(soundData);

	if(!instance->CreateInstance()){ return nullptr; }

	instance->Play(loop);

	AddPlayList(instance);

	return instance;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 3Dサウンドの再生
// ・サウンドアセットの取得orロード
// ・再生用インスタンスの生成、3D座標のセット
// ・管理用プレイリストへの追加
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
std::shared_ptr<KdSoundInstance3D> KdAudioManager::Play3D(std::string_view rName, const Math::Vector3& rPos, bool loop)
{
	if (!m_audioEng) { return nullptr; }

	std::shared_ptr<KdSoundEffect> soundData = GetSound(rName);

	if (!soundData) { return nullptr; }

	std::shared_ptr<KdSoundInstance3D> instance = std::make_shared<KdSoundInstance3D>(soundData, m_listener);

	if (!instance->CreateInstance()) { return nullptr; }

	instance->Play(loop);

	instance->SetPos(rPos);

	AddPlayList(instance);

	return instance;
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 再生リストの全ての音を停止する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::StopAllSound()
{
	auto it = m_playList.begin();
	while (it != m_playList.end()) {
		(*it).second->Stop();
		++it;
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 再生リストの全ての音を一時停止する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::PauseAllSound()
{
	auto it = m_playList.begin();
	while (it != m_playList.end()) {
		(*it).second->Pause();
		++it;
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 再生リストの全ての音を再開する
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::ResumeAllSound()
{
	auto it = m_playList.begin();
	while (it != m_playList.end()) {
		(*it).second->Resume();
		++it;
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 再生中のサウンドの停止・サウンドアセットの解放
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::SoundReset()
{
	StopAllSound();

	m_soundMap.clear();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// サウンドアセットの読み込み
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::LoadSoundAssets(std::initializer_list<std::string_view>& fileNames)
{
	for (std::string_view fileName : fileNames)
	{
		auto itFound = m_soundMap.find(fileName.data());

		// ロード済みならスキップ
		if (itFound != m_soundMap.end()) { continue; }

		// 生成 & 読み込み
		auto newSound = std::make_shared<KdSoundEffect>();
		if (!newSound->Load(fileName, m_audioEng))
		{
			// 読み込み失敗時
			assert(0 && "LoadSoundAssets:ファイル名のデータが存在しません。名前を確認してください。");

			continue;
		}

		// リスト(map)に登録
		m_soundMap.emplace(fileName, newSound);

	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 解放
// ・再生中のプレイリストの解放
// ・サウンドアセットの解放
// ・エンジンの解放
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void KdAudioManager::Release()
{
	g_audioAlive = false;   // 以後 KdBgmVoice はボイス操作をしない（エンジン破棄でボイスも解放される）

	StopAllSound();

	m_playList.clear();

	m_soundMap.clear();

	m_audioEng = nullptr;

	// Media Foundation 終了
	if (m_mfInitialized)
	{
		MFShutdown();
		m_mfInitialized = false;
	}
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// サウンドアセットの取得
// ・
// ・サウンドアセットの解放
// ・エンジンの解放
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
std::shared_ptr<KdSoundEffect> KdAudioManager::GetSound(std::string_view fileName)
{
	// filenameのサウンドアセットがロード済みか？
	auto itFound = m_soundMap.find(fileName.data());

	// ロード済み
	if (itFound != m_soundMap.end())
	{
		return (*itFound).second;
	}
	else
	{
		// 生成 & 読み込み
		auto newSound = std::make_shared<KdSoundEffect>();
		if (!newSound->Load(fileName, m_audioEng))
		{
			// 読み込み失敗時は、nullを返す
			return nullptr;
		}
		// リスト(map)に登録
		m_soundMap.emplace(fileName, newSound);

		// リソースを返す
		return newSound;
	}
}


// ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### #####
// 
// KdSoundEffect
// 
// ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### #####

// 音データの読み込み
bool KdSoundEffect::Load(std::string_view fileName, const std::unique_ptr<DirectX::AudioEngine>& engine)
{
	if (engine.get() != nullptr)
	{
		try
		{
			// wstringに変換
			std::wstring wFilename = sjis_to_wide(fileName.data());

			// mp3/m4a/wma 等の圧縮音源は Media Foundation で PCM デコードして読み込む。
			// wav は従来どおり DirectXTK で直接読み込む（速い）。
			const bool compressed =
				EndsWithExtCI(fileName, ".mp3") ||
				EndsWithExtCI(fileName, ".m4a") ||
				EndsWithExtCI(fileName, ".aac") ||
				EndsWithExtCI(fileName, ".wma");

			if (compressed)
			{
				std::vector<uint8_t> pcm;
				WAVEFORMATEX wfx{};

				// 配布ビルド：埋め込みpakに在ればメモリからデコード（ディスクに出さない）
				std::vector<uint8_t> vaultBytes;
				bool decoded = false;
				if (AssetVault::Read(std::string(fileName), vaultBytes))
				{
					// ① メモリから直接デコード（ディスクに出さない）
					decoded = DecodeAudioToPcmMem(vaultBytes.data(), vaultBytes.size(), wFilename.c_str(), pcm, wfx);
					// ② 環境によりメモリデコード不可なら一時ファイル経由（即削除）
					if (!decoded)
					{
						decoded = DecodeAudioViaTempFile(vaultBytes.data(), vaultBytes.size(), pcm, wfx);
					}
				}
				// それでもダメなら従来どおりディスクのファイルからデコード（開発ビルド）
				if (!decoded)
				{
					decoded = DecodeAudioToPcm(wFilename.c_str(), pcm, wfx);
				}
				if (!decoded)
				{
					assert(0 && "Sound File Decode Error (Media Foundation)");
					return false;
				}

				// [WAVEFORMATEX][PCMデータ] の連続バッファを作る。
				// SoundEffect は wfx ポインタを保持するため、所有権を渡すバッファ内に同居させる。
				const size_t hdr = sizeof(WAVEFORMATEX);
				auto wavData = std::make_unique<uint8_t[]>(hdr + pcm.size());
				std::memcpy(wavData.get(), &wfx, hdr);
				std::memcpy(wavData.get() + hdr, pcm.data(), pcm.size());

				const WAVEFORMATEX* pwfx = reinterpret_cast<const WAVEFORMATEX*>(wavData.get());
				const uint8_t* startAudio = wavData.get() + hdr;

				m_soundEffect = std::make_unique<DirectX::SoundEffect>(
					engine.get(), wavData, pwfx, startAudio, pcm.size());
			}
			else
			{
				// wav 読み込み
				m_soundEffect = std::make_unique<DirectX::SoundEffect>(engine.get(), wFilename.c_str());
			}
		}
		catch (...)
		{
			assert(0 && "Sound File Load Error");

			return false;
		}
	}

	return true;
}


// ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### #####
// 
// KdSoundInstance
// 
// ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### ##### #####

KdSoundInstance::KdSoundInstance(const std::shared_ptr<KdSoundEffect>& soundEffect)
	:m_soundData(soundEffect){}

bool KdSoundInstance::CreateInstance()
{
	if (!m_soundData) { return false; }

	DirectX::SOUND_EFFECT_INSTANCE_FLAGS flags = DirectX::SoundEffectInstance_Default;

	m_instance = (m_soundData->CreateInstance(flags));

	return true;
}

void KdSoundInstance::Play(bool loop)
{
	if (!m_instance) { return; }

	// 再生状態がどんな状況か分からないので一度停止してから
	Stop();

	m_instance->Play(loop);
}

void KdSoundInstance::SetVolume(float vol)
{
	if (m_instance == nullptr) { return; }

	m_instance->SetVolume(vol);
}

void KdSoundInstance::SetPitch(float pitch)
{
	if (m_instance == nullptr) { return; }

	m_instance->SetPitch(pitch);
}

void KdSoundInstance::SetLowPass(float freq01, float oneOverQ)
{
	// ※このDirectXTK版は per-voice フィルタ(SetFilterParameters)を持たないため未対応。
	//   こもった音にはライブラリ更新（フィルタ対応）か生XAudio2が必要。今は何もしない。
	(void)freq01; (void)oneOverQ;
}

void KdSoundInstance::ClearFilter()
{
	// 同上：未対応のため何もしない
}

bool KdSoundInstance::IsPlaying()
{
	if (!m_instance) { return false; }

	return (m_instance->GetState() == DirectX::SoundState::PLAYING);
}

bool KdSoundInstance::IsPause()
{
	if (!m_instance) { return false; }

	return (m_instance->GetState() == DirectX::SoundState::PAUSED);
}

bool KdSoundInstance::IsStopped()
{
	if (!m_instance) { return false; }

	return (m_instance->GetState() == DirectX::SoundState::STOPPED);
}



// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 
// KdSoundInstance3D
// 
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

KdSoundInstance3D::KdSoundInstance3D(const std::shared_ptr<KdSoundEffect>& soundEffect, const DirectX::AudioListener& ownerListener)
	:KdSoundInstance(soundEffect), m_ownerListener(ownerListener){}

bool KdSoundInstance3D::CreateInstance()
{
	if (!m_soundData) { return false; }

	DirectX::SOUND_EFFECT_INSTANCE_FLAGS flags = DirectX::SoundEffectInstance_Default |
		DirectX::SoundEffectInstance_Use3D | DirectX::SoundEffectInstance_ReverbUseFilters;

	m_instance = (m_soundData->CreateInstance(flags));

	return true;
}

void KdSoundInstance3D::Play(bool loop)
{
	if (!m_instance) { return; }

	KdSoundInstance::Play(loop);
}

void KdSoundInstance3D::SetPos(const Math::Vector3& rPos)
{
	if (!m_instance) { return; }

	m_emitter.SetPosition(rPos);

	m_instance->Apply3D(m_ownerListener, m_emitter, false);
}

void KdSoundInstance3D::SetCurveDistanceScaler(float val)
{
	if (!m_instance) { return; }

	m_emitter.CurveDistanceScaler = val;

	m_instance->Apply3D(m_ownerListener, m_emitter, false);
}


// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
//
// KdBgmVoice（BGM専用 自前XAudio2ソースボイス：ローパスで「こもり」可能）
//
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////

namespace
{
	// BGMのデコード済みPCMキャッシュ（ゲーム開始ロードで温めておく＝再生時カクつき防止）
	struct BgmPcm { std::vector<uint8_t> data; WAVEFORMATEX wfx{}; };
	std::unordered_map<std::string, std::shared_ptr<BgmPcm>> g_bgmCache;

	// path をデコードしてPCMを返す（VFS優先→メモリ→一時ファイル→ディスク）。キャッシュ付き。
	std::shared_ptr<BgmPcm> GetOrDecodeBgm(const std::string& path)
	{
		auto it = g_bgmCache.find(path);
		if (it != g_bgmCache.end()) { return it->second; }

		auto out = std::make_shared<BgmPcm>();
		const std::wstring wpath = sjis_to_wide(path);

		bool decoded = false;
		std::vector<uint8_t> vault;
		if (AssetVault::Read(path, vault))
		{
			decoded = DecodeAudioToPcmMem(vault.data(), vault.size(), wpath.c_str(), out->data, out->wfx);
			if (!decoded) { decoded = DecodeAudioViaTempFile(vault.data(), vault.size(), out->data, out->wfx); }
		}
		if (!decoded) { decoded = DecodeAudioToPcm(wpath.c_str(), out->data, out->wfx); }
		if (!decoded || out->data.empty()) { return nullptr; }

		g_bgmCache[path] = out;
		return out;
	}
}

void KdBgmVoice::Preload(std::string_view path)
{
	if (path.empty()) { return; }
	GetOrDecodeBgm(std::string(path));   // デコードしてキャッシュに載せるだけ
}

bool KdBgmVoice::Play(std::string_view path, bool loop)
{
	Stop();
	if (path.empty()) { return false; }

	// キャッシュ（無ければここでデコード）からPCM取得
	std::shared_ptr<BgmPcm> ref = GetOrDecodeBgm(std::string(path));
	if (!ref || ref->data.empty()) { return false; }

	IXAudio2* xa = KdAudioManager::Instance().GetXAudio2();
	if (!xa) { return false; }

	m_pcm = ref->data;     // 再生用にコピー（デコードは済んでいるので軽い）
	m_wfx = ref->wfx;

	// USEFILTER 付きでソースボイス作成（後でローパスを掛けられる）
	if (FAILED(xa->CreateSourceVoice(&m_voice, &m_wfx, XAUDIO2_VOICE_USEFILTER)))
	{
		m_voice = nullptr;
		return false;
	}

	XAUDIO2_BUFFER buf{};
	buf.AudioBytes = static_cast<UINT32>(m_pcm.size());
	buf.pAudioData = m_pcm.data();
	buf.Flags      = XAUDIO2_END_OF_STREAM;
	buf.LoopCount  = loop ? XAUDIO2_LOOP_INFINITE : 0;
	if (FAILED(m_voice->SubmitSourceBuffer(&buf))) { Stop(); return false; }

	m_voice->SetVolume(0.0f);
	SetMuffle(false);             // 初期は原音
	if (FAILED(m_voice->Start(0))) { Stop(); return false; }
	return true;
}

void KdBgmVoice::SetVolume(float vol)
{
	if (m_voice) { m_voice->SetVolume(vol); }
}

void KdBgmVoice::SetMuffle(bool on)
{
	if (!m_voice) { return; }

	XAUDIO2_FILTER_PARAMETERS fp{};
	fp.Type     = LowPassFilter;
	fp.OneOverQ = 1.0f;
	if (on)
	{
		const float sr     = (m_wfx.nSamplesPerSec > 0) ? static_cast<float>(m_wfx.nSamplesPerSec) : 44100.0f;
		const float cutoff = 700.0f;   // これより上の高音を削ってこもらせる
		float f = 2.0f * std::sin(3.14159265f * cutoff / sr);
		if (f > XAUDIO2_MAX_FILTER_FREQUENCY) { f = XAUDIO2_MAX_FILTER_FREQUENCY; }
		fp.Frequency = f;
	}
	else
	{
		fp.Frequency = XAUDIO2_MAX_FILTER_FREQUENCY;   // ほぼ原音（フィルタ無効相当）
	}
	m_voice->SetFilterParameters(&fp);
}

void KdBgmVoice::Stop()
{
	// エンジン破棄後（終了時）はボイスも解放済みなので触らない（ダングリング回避）
	if (m_voice && g_audioAlive)
	{
		m_voice->Stop(0);
		m_voice->FlushSourceBuffers();
		m_voice->DestroyVoice();
	}
	m_voice = nullptr;
	m_pcm.clear();
}
