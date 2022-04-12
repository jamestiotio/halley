#pragma once
#include "audio_position.h"
#include "behaviours/audio_voice_behaviour.h"
#include "audio_buffer.h"
#include <limits>

namespace Halley {
	class AudioFilterResample;
	class AudioBufferPool;
	class AudioMixer;
	class AudioVoiceBehaviour;
	class AudioSource;

	class AudioVoice {
    public:
		AudioVoice(AudioEngine& engine, std::shared_ptr<AudioSource> source, AudioPosition sourcePos, float gain, float pitch, uint8_t group);
		~AudioVoice();

		void start();
		void stop();
		void pause();
		void resume();

		bool isPlaying() const;
		bool isReady() const;
		bool isDone() const;

		void setBaseGain(float gain);
		float getBaseGain() const;
		void setUserGain(float gain);
		float getUserGain() const;
		float& getDynamicGainRef();

		void setPitch(float pitch);

		void setAudioSourcePosition(Vector3f position);
		void setAudioSourcePosition(AudioPosition sourcePos);

		size_t getNumberOfChannels() const;

		void update(gsl::span<const AudioChannelData> channels, const AudioListenerData& listener, float groupGain);
		void mixTo(size_t numSamples, gsl::span<AudioBuffer*> dst, AudioMixer& mixer, AudioBufferPool& pool);
		
		void setIds(uint32_t uniqueId, uint32_t sourceId = 0, uint32_t audioObjectId = 0);
		uint32_t getUniqueId() const;
		uint32_t getSourceId() const;
		uint32_t getAudioObjectId() const;

		void addBehaviour(std::unique_ptr<AudioVoiceBehaviour> behaviour);
		
		uint8_t getGroup() const;

	private:
		AudioEngine& engine;
		
		uint32_t uniqueId = std::numeric_limits<uint32_t>::max();
		uint32_t sourceId = 0;
		uint32_t audioObjectId = 0;
		uint8_t group = 0;
		uint8_t nChannels = 0;
		bool playing : 1;
		bool paused : 1;
		bool done : 1;
		bool isFirstUpdate : 1;
    	float baseGain = 1.0f;
		float dynamicGain = 1.0f;
		float userGain = 1.0f;
		float elapsedTime = 0.0f;

		std::shared_ptr<AudioSource> source;
		std::shared_ptr<AudioFilterResample> resample;
		std::unique_ptr<AudioVoiceBehaviour> behaviour;
    	AudioPosition sourcePos;

		std::array<float, 16> channelMix;
		std::array<float, 16> prevChannelMix;

		void advancePlayback(size_t samples);
    };
}
