#pragma once

class KdSoundEffect;
class KdSoundInstance;
class KdSoundInstance3D;

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ゲーム内の音を全て管理するクラス
// ----- ----- ----- ----- ----- ----- ----- ----- ----- -----
// DirectXAudioEngineの管理
// 2Dと3Dサウンドの簡易再生関数
// 再生中のサウンドインスタンスの管理（全停止・一時停止などが可能）
// サウンドアセットの管理
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class KdAudioManager
{
public:

	// 初期化
	void Init();

	// 不要なインスタンス削除など
	void Update();

	void SetListnerMatrix(const Math::Matrix& mWorld);

	// サウンド再生
	std::shared_ptr<KdSoundInstance>  Play(std::string_view rName, bool loop = false);
	std::shared_ptr<KdSoundInstance3D> Play3D(std::string_view rName, const Math::Vector3& rPos, bool loop = false);

	void AddPlayList(const std::shared_ptr<KdSoundInstance>& rSound)
	{
		if (!rSound.get()) { return; }

		m_playList[(size_t)(rSound.get())] = rSound;
	}

	// 再生中の音をすべて停止する
	void StopAllSound();
	// 再生中の音をすべて一時停止する
	void PauseAllSound();
	// 再生中の音をすべて停止する
	void ResumeAllSound();

	// 再生中の音をすべて停止・サウンドアセットの解放を行う
	void SoundReset();

	// DirectXAudioEngine取得
	const std::unique_ptr<DirectX::AudioEngine>& GetAudioEngine() { return m_audioEng; }

	// XAudio2 本体取得（自前ソースボイス＝KdBgmVoice 用）
	IXAudio2* GetXAudio2() const { return m_audioEng ? m_audioEng->GetInterface() : nullptr; }

	const DirectX::AudioListener& GetListener() { return m_listener; }

	// サウンドアセットの一括読込
	void LoadSoundAssets(std::initializer_list<std::string_view>& fileName);

	// 解放
	void Release();

private:

	// サウンドデータの取得orロード
	std::shared_ptr<KdSoundEffect> GetSound(std::string_view fileName);

	// DirectXのAudioEngine本体
	std::unique_ptr<DirectX::AudioEngine>	m_audioEng;

	// 3Dサウンド用リスナー
	DirectX::AudioListener					m_listener;

	// 再生中のサウンドリスト
	std::map<size_t, std::shared_ptr<KdSoundInstance>>	m_playList;

	// サウント管理マップ
	std::unordered_map< std::string, std::shared_ptr<KdSoundEffect>> m_soundMap;

	// Media Foundation 初期化済みフラグ（mp3等のデコードに使用）
	bool m_mfInitialized = false;

	// シングルトンパターン
public:
	static KdAudioManager& Instance()
	{
		static KdAudioManager instance;
		return instance;
	}

private:
	KdAudioManager() {}
	~KdAudioManager() { Release(); }
};


// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// BGM専用 自前XAudio2ソースボイス
// ----- ----- ----- ----- ----- ----- ----- ----- ----- -----
// DirectXTKのSoundEffectInstanceはper-voiceフィルタを公開しないため、
// BGMだけ生XAudio2のソースボイス(USEFILTER付き)で鳴らし、ローパスで
// 「こもり」をリアルタイムに掛けられるようにする（muffled別ファイル不要）。
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class KdBgmVoice
{
public:
	KdBgmVoice() = default;
	~KdBgmVoice() { Stop(); }

	// BGMのPCMを事前デコードしてキャッシュに載せる（ゲーム開始ロードで呼ぶ＝再生時カクつき防止）
	static void Preload(std::string_view path);

	// path をデコードしてループ再生開始（音量0で開始。以後 SetVolume で上げる）
	bool Play(std::string_view path, bool loop);

	void SetVolume(float vol);     // 0..1
	void SetMuffle(bool on);       // ローパスON/OFF（こもり）
	void Stop();                   // 停止＆破棄
	bool IsValid() const { return m_voice != nullptr; }

private:
	IXAudio2SourceVoice* m_voice = nullptr;
	std::vector<uint8_t> m_pcm;    // 再生中は生かしておく必要がある
	WAVEFORMATEX         m_wfx{};

	KdBgmVoice(const KdBgmVoice&) = delete;
	void operator=(const KdBgmVoice&) = delete;
};


// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 2Dサウンド再生用のインスタンス
// ----- ----- ----- ----- ----- ----- ----- ----- ----- -----
// 再生や停止、再生状況の確認
// 各種パラメータの変更が可能
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class KdSoundInstance : public std::enable_shared_from_this<KdSoundInstance>
{
public:
	KdSoundInstance(const std::shared_ptr<KdSoundEffect>& soundEffect);

	virtual bool CreateInstance();

	virtual void Play(bool loop = false);
	void Stop() { if (m_instance) { m_instance->Stop(); } }
	void Pause() { if (m_instance) { m_instance->Pause(); } }
	void Resume() { if (m_instance) { m_instance->Resume(); } }

	// ・vol	… ボリューム設定(1.0が100%)
	void SetVolume(float vol);

	// ・pitch	… 振動設定(低音-1.0～1.0高音)
	void SetPitch(float pitch);

	// ローパスフィルタ（こもった音）。
	// freq01 : カットオフ 0〜1（1=ほぼ無効/原音、小さいほど高音が削れてこもる）
	// oneOverQ: 共鳴の鋭さの逆数（既定1.0でOK）
	void SetLowPass(float freq01, float oneOverQ = 1.0f);
	// フィルタ解除（原音に戻す）
	void ClearFilter();

	// 再生状態の取得
	bool IsPlaying();
	bool IsPause();
	bool IsStopped();

protected:

	// サウンドの再生インスタンス
	std::unique_ptr<DirectX::SoundEffectInstance>	m_instance;

	// 再生サウンドの元データ
	std::shared_ptr<KdSoundEffect>					m_soundData;

	// コピー禁止用
	KdSoundInstance(const KdSoundInstance& src) = delete;
	void operator=(const KdSoundInstance& src) = delete;
};


// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 3Dサウンド再生用のインスタンス
// ----- ----- ----- ----- ----- ----- ----- ----- ----- -----
// KdSoundInstanceに3D座標情報を追加
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class KdSoundInstance3D : public KdSoundInstance
{
public:
	KdSoundInstance3D(const std::shared_ptr<KdSoundEffect>& soundEffect, const DirectX::AudioListener& ownerListener);

	bool CreateInstance() override;

	void Play(bool loop = false) override;

	void SetPos(const Math::Vector3& rPos);

	// 減衰倍率設定 1:通常 FLT_MIN～FLT_MAX
	void SetCurveDistanceScaler(float val);

protected:

	// エミッター 主に3Dサウンドソースの定義
	DirectX::AudioEmitter			m_emitter;

	// 3Dサウンド用リスナー
	const DirectX::AudioListener&	m_ownerListener;

	// コピー禁止用
	KdSoundInstance3D(const KdSoundInstance3D& src) = delete;
	void operator=(const KdSoundInstance3D& src) = delete;

};


// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// サウンドデータを扱う
// ----- ----- ----- ----- ----- ----- ----- ----- ----- -----
// Wave形式のデータを格納できる
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
class KdSoundEffect {
public:

	KdSoundEffect() {}

	~KdSoundEffect() {
		m_soundEffect = nullptr;
	}

	std::unique_ptr<DirectX::SoundEffectInstance> CreateInstance(DirectX::SOUND_EFFECT_INSTANCE_FLAGS flag)
	{
		if (!m_soundEffect){ return nullptr; }

		return m_soundEffect->CreateInstance(flag);
	}

	// WAVEサウンド読み込み
	bool Load(std::string_view fileName, const std::unique_ptr<DirectX::AudioEngine>& engine);

private:
	// サウンドエフェクト
	std::unique_ptr<DirectX::SoundEffect>	m_soundEffect;

	// コピー禁止用:単一のデータはコピーできない
	KdSoundEffect(const KdSoundEffect& src) = delete;
	void operator=(const KdSoundEffect& src) = delete;
};
