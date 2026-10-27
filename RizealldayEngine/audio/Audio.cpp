#include "audio/Audio.h"
#include <cassert>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

Audio* Audio::GetInstance()
{
	static Audio instance;
	return &instance;
}

void Audio::Initialize()
{
	HRESULT result;
	result = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(result));
	result = xAudio2_->CreateMasteringVoice(&masterVoice_);
	assert(SUCCEEDED(result));

	MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
}

void Audio::Finalize()
{
	StopBGM();

	for (IXAudio2SourceVoice* voice : sfxVoices_)
	{
		if (voice)
		{
			voice->Stop();
			voice->FlushSourceBuffers();
			voice->DestroyVoice();
		}
	}
	sfxVoices_.clear();

	if (xAudio2_)
	{
		xAudio2_.Reset();
	}
	masterVoice_ = nullptr;

	MFShutdown();
}

static Audio::SoundData LoadViaMediaFoundation(const std::string& filename)
{
	Audio::SoundData soundData{};
	int wlen = MultiByteToWideChar(CP_UTF8, 0, filename.c_str(), -1, nullptr, 0);
	if (wlen <= 0) return soundData;
	std::wstring wpath(wlen, 0);
	MultiByteToWideChar(CP_UTF8, 0, filename.c_str(), -1, &wpath[0], wlen);

	IMFSourceReader* pReader = nullptr;
	HRESULT hr = MFCreateSourceReaderFromURL(wpath.c_str(), nullptr, &pReader);
	if (FAILED(hr) || !pReader) return soundData;

	IMFMediaType* pPartialType = nullptr;
	hr = MFCreateMediaType(&pPartialType);
	if (SUCCEEDED(hr))
	{
		pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		pPartialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
		hr = pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPartialType);
		pPartialType->Release();
	}
	if (FAILED(hr))
	{
		pReader->Release();
		return soundData;
	}

	IMFMediaType* pUncompressedType = nullptr;
	hr = pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pUncompressedType);
	if (FAILED(hr) || !pUncompressedType)
	{
		pReader->Release();
		return soundData;
	}

	WAVEFORMATEX* pWfex = nullptr;
	UINT32 cbFormat = 0;
	hr = MFCreateWaveFormatExFromMFMediaType(pUncompressedType, &pWfex, &cbFormat);
	pUncompressedType->Release();
	if (FAILED(hr) || !pWfex)
	{
		pReader->Release();
		return soundData;
	}

	std::vector<BYTE> pcmData;
	while (true)
	{
		DWORD flags = 0;
		IMFSample* pSample = nullptr;
		hr = pReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &pSample);
		if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM))
		{
			if (pSample) pSample->Release();
			break;
		}

		if (pSample)
		{
			IMFMediaBuffer* pBuffer = nullptr;
			hr = pSample->ConvertToContiguousBuffer(&pBuffer);
			if (SUCCEEDED(hr))
			{
				BYTE* pAudioBytes = nullptr;
				DWORD cbBuffer = 0;
				hr = pBuffer->Lock(&pAudioBytes, nullptr, &cbBuffer);
				if (SUCCEEDED(hr))
				{
					size_t curSize = pcmData.size();
					pcmData.resize(curSize + cbBuffer);
					memcpy(pcmData.data() + curSize, pAudioBytes, cbBuffer);
					pBuffer->Unlock();
				}
				pBuffer->Release();
			}
			pSample->Release();
		}
	}
	pReader->Release();

	if (!pcmData.empty())
	{
		soundData.wfex = *pWfex;
		soundData.bufferSize = static_cast<unsigned int>(pcmData.size());
		soundData.pBuffer = new BYTE[soundData.bufferSize];
		memcpy(soundData.pBuffer, pcmData.data(), soundData.bufferSize);
	}

	CoTaskMemFree(pWfex);
	return soundData;
}

Audio::SoundData Audio::SoundLoadWave(const std::string& filename)
{
	struct ChunkHeader { char id[4]; int32_t size; };
	struct RiffHeader { ChunkHeader chunk; char type[4]; };
	struct FormatChunk { ChunkHeader chunk; WAVEFORMATEX fmt; };

	std::ifstream file(filename, std::ios_base::binary);
	if (!file.is_open())
	{
		return LoadViaMediaFoundation(filename);
	}

	RiffHeader riff{};
	file.read((char*)&riff, sizeof(riff));
	if (std::strncmp(riff.chunk.id, "RIFF", 4) != 0 || std::strncmp(riff.type, "WAVE", 4) != 0)
	{
		file.close();
		return LoadViaMediaFoundation(filename);
	}

	FormatChunk format = {};
	ChunkHeader chunkHeader{};
	bool foundFmt = false;
	while (file.read((char*)&chunkHeader, sizeof(ChunkHeader)))
	{
		if (std::strncmp(chunkHeader.id, "fmt ", 4) == 0)
		{
			format.chunk = chunkHeader;
			uint32_t readSize = (chunkHeader.size < sizeof(format.fmt)) ? static_cast<uint32_t>(chunkHeader.size) : static_cast<uint32_t>(sizeof(format.fmt));
			file.read((char*)&format.fmt, readSize);
			if (chunkHeader.size > static_cast<int32_t>(readSize))
			{
				file.seekg(chunkHeader.size - readSize, std::ios_base::cur);
			}
			if (format.fmt.wFormatTag == WAVE_FORMAT_PCM)
			{
				format.fmt.cbSize = 0;
			}
			foundFmt = true;
			break;
		}
		else
		{
			file.seekg(chunkHeader.size, std::ios_base::cur);
		}
	}

	if (!foundFmt)
	{
		file.close();
		return LoadViaMediaFoundation(filename);
	}

	ChunkHeader data = {};
	bool foundData = false;
	while (file.read((char*)&chunkHeader, sizeof(ChunkHeader)))
	{
		if (std::strncmp(chunkHeader.id, "data", 4) == 0)
		{
			data = chunkHeader;
			foundData = true;
			break;
		}
		else
		{
			file.seekg(chunkHeader.size, std::ios_base::cur);
		}
	}

	if (!foundData || data.size <= 0)
	{
		file.close();
		return LoadViaMediaFoundation(filename);
	}

	char* pBuffer = new char[data.size];
	file.read(pBuffer, data.size);
	file.close();

	SoundData soundData = {};
	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = data.size;
	return soundData;
}

void Audio::SoundTrim(SoundData* soundData, float startSec, float lengthSec, float fadeInSec, float fadeOutSec)
{
	if (!soundData || !soundData->pBuffer || soundData->bufferSize == 0) return;
	const WAVEFORMATEX& fmt = soundData->wfex;
	if (fmt.wFormatTag != WAVE_FORMAT_PCM || fmt.wBitsPerSample != 16 || fmt.nBlockAlign == 0 || fmt.nChannels == 0) return;

	const uint32_t blockAlign = fmt.nBlockAlign;
	const uint32_t totalFrames = soundData->bufferSize / blockAlign;
	uint32_t startFrame = static_cast<uint32_t>((std::max)(startSec, 0.0f) * fmt.nSamplesPerSec);
	if (startFrame >= totalFrames) return;
	uint32_t frameCount = static_cast<uint32_t>((std::max)(lengthSec, 0.0f) * fmt.nSamplesPerSec);
	frameCount = (std::min)(frameCount, totalFrames - startFrame);
	if (frameCount == 0) return;

	const uint32_t newSize = frameCount * blockAlign;
	BYTE* newBuffer = new BYTE[newSize];
	memcpy(newBuffer, soundData->pBuffer + startFrame * blockAlign, newSize);

	// Linear fade in/out to avoid clicks at the cut points
	int16_t* samples = reinterpret_cast<int16_t*>(newBuffer);
	const uint32_t fadeInFrames = (std::min)(static_cast<uint32_t>(fadeInSec * fmt.nSamplesPerSec), frameCount);
	const uint32_t fadeOutFrames = (std::min)(static_cast<uint32_t>(fadeOutSec * fmt.nSamplesPerSec), frameCount);
	for (uint32_t f = 0; f < frameCount; ++f)
	{
		float gain = 1.0f;
		if (f < fadeInFrames) gain = static_cast<float>(f) / fadeInFrames;
		if (f >= frameCount - fadeOutFrames) gain = (std::min)(gain, static_cast<float>(frameCount - 1 - f) / fadeOutFrames);
		if (gain >= 1.0f) continue;
		for (uint32_t c = 0; c < fmt.nChannels; ++c)
		{
			int16_t& s = samples[f * fmt.nChannels + c];
			s = static_cast<int16_t>(s * gain);
		}
	}

	delete[] soundData->pBuffer;
	soundData->pBuffer = newBuffer;
	soundData->bufferSize = newSize;
}

void Audio::SoundPlayWave(const SoundData& soundData, float volume, float pitch)
{
	if (!xAudio2_ || !soundData.pBuffer || soundData.bufferSize == 0) return;

	// Clean up finished SFX voices
	for (auto it = sfxVoices_.begin(); it != sfxVoices_.end(); )
	{
		XAUDIO2_VOICE_STATE state;
		(*it)->GetState(&state);
		if (state.BuffersQueued == 0)
		{
			(*it)->DestroyVoice();
			it = sfxVoices_.erase(it);
		}
		else
		{
			++it;
		}
	}

	HRESULT result;
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	result = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	if (FAILED(result) || !pSourceVoice) return;

	pSourceVoice->SetVolume(volume);
	pSourceVoice->SetFrequencyRatio(pitch);

	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.pBuffer;
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;

	result = pSourceVoice->SubmitSourceBuffer(&buf);
	if (FAILED(result))
	{
		pSourceVoice->DestroyVoice();
		return;
	}

	result = pSourceVoice->Start();
	if (FAILED(result))
	{
		pSourceVoice->DestroyVoice();
		return;
	}

	sfxVoices_.push_back(pSourceVoice);
}

void Audio::PlayBGM(const SoundData& soundData, float volume)
{
	if (!xAudio2_ || !soundData.pBuffer || soundData.bufferSize == 0) return;

	// If this BGM is already playing, keep playing without restarting
	if (bgmVoice_ != nullptr && currentBgmBuffer_ == soundData.pBuffer)
	{
		bgmVoice_->SetVolume(volume);
		return;
	}

	StopBGM();

	HRESULT result;
	result = xAudio2_->CreateSourceVoice(&bgmVoice_, &soundData.wfex);
	if (FAILED(result) || !bgmVoice_) return;

	bgmVoice_->SetVolume(volume);

	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.pBuffer;
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;
	buf.LoopCount = XAUDIO2_LOOP_INFINITE;

	result = bgmVoice_->SubmitSourceBuffer(&buf);
	if (FAILED(result))
	{
		bgmVoice_->DestroyVoice();
		bgmVoice_ = nullptr;
		return;
	}

	result = bgmVoice_->Start();
	if (FAILED(result))
	{
		bgmVoice_->DestroyVoice();
		bgmVoice_ = nullptr;
		return;
	}

	currentBgmBuffer_ = soundData.pBuffer;
}

void Audio::StopBGM()
{
	if (bgmVoice_)
	{
		bgmVoice_->Stop();
		bgmVoice_->FlushSourceBuffers();
		bgmVoice_->DestroyVoice();
		bgmVoice_ = nullptr;
		currentBgmBuffer_ = nullptr;
	}
}

void Audio::PauseBGM()
{
	if (bgmVoice_)
	{
		bgmVoice_->Stop();
	}
}

void Audio::ResumeBGM()
{
	if (bgmVoice_)
	{
		bgmVoice_->Start();
	}
}

void Audio::SetBGMVolume(float volume)
{
	if (bgmVoice_)
	{
		bgmVoice_->SetVolume(volume);
	}
}

bool Audio::IsBGMPlaying() const
{
	if (!bgmVoice_) return false;
	XAUDIO2_VOICE_STATE state;
	bgmVoice_->GetState(&state);
	return (state.BuffersQueued > 0);
}

void Audio::SoundUnload(SoundData* soundData)
{
	if (!soundData) return;
	if (soundData->pBuffer)
	{
		delete[] soundData->pBuffer;
		soundData->pBuffer = nullptr;
	}
	soundData->bufferSize = 0;
	soundData->wfex = {};
}
