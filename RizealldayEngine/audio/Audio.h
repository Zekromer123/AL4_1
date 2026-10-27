#pragma once
#include <xaudio2.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <cstdint>

#pragma comment(lib, "xaudio2.lib")

class Audio
{
public:
	static Audio* GetInstance();

	struct SoundData {
		WAVEFORMATEX wfex{};
		BYTE* pBuffer = nullptr;
		unsigned int bufferSize = 0;
	};

	void Initialize();
	void Finalize();

	/// WAV音声読み込み (RIFF PCM + Media Foundation fallback)
	SoundData SoundLoadWave(const std::string& filename);
	/// 音声再生 (One-shot SFX)
	void SoundPlayWave(const SoundData& soundData, float volume = 1.0f, float pitch = 1.0f);
	/// PCM 16bit データを [startSec, startSec + lengthSec] に切り出し、短いフェードを適用
	void SoundTrim(SoundData* soundData, float startSec, float lengthSec, float fadeInSec = 0.003f, float fadeOutSec = 0.04f);
	/// 音声データ解放
	void SoundUnload(SoundData* soundData);

	/// BGM 再生 (Looping)
	void PlayBGM(const SoundData& soundData, float volume = 0.5f);
	/// BGM 停止
	void StopBGM();
	/// BGM 一時停止 / 再開
	void PauseBGM();
	void ResumeBGM();
	/// BGM 音量設定
	void SetBGMVolume(float volume);
	/// BGM 再生中かどうか
	bool IsBGMPlaying() const;

private:
	Audio() = default;
	~Audio() = default;
	Audio(const Audio&) = delete;
	Audio& operator=(const Audio&) = delete;

	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;

	IXAudio2SourceVoice* bgmVoice_ = nullptr;
	const BYTE* currentBgmBuffer_ = nullptr;
	std::vector<IXAudio2SourceVoice*> sfxVoices_;
};
