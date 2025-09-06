#include "audio/AudioEngine.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <stb_vorbis.c>
#include <cstring>
#include <cstdio>
#include <vector>

namespace engine
{
    bool AudioEngine::initialize()
    {
        m_device = alcOpenDevice(nullptr);
        if (!m_device) return false;
        m_context = alcCreateContext((ALCdevice*)m_device, nullptr);
        if (!m_context) return false;
        alcMakeContextCurrent((ALCcontext*)m_context);
        alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
        return true;
    }

    void AudioEngine::shutdown()
    {
        for (ALuint s : m_sources) alDeleteSources(1, &s);
        m_sources.clear();
        for (auto& kv : m_cache) alDeleteBuffers(1, &kv.second);
        m_cache.clear();
        if (m_context) { alcMakeContextCurrent(nullptr); alcDestroyContext((ALCcontext*)m_context); m_context = nullptr; }
        if (m_device) { alcCloseDevice((ALCdevice*)m_device); m_device = nullptr; }
    }

    static bool ends_with(const std::string& s, const char* suf)
    { size_t n=strlen(suf); return s.size()>=n && _stricmp(s.c_str()+s.size()-n,suf)==0; }

    // Minimal WAV (PCM16) loader
    static bool load_wav_pcm16(const std::string& path, std::vector<short>& outPCM, int& channels, int& sampleRate)
    {
        FILE* f=nullptr; fopen_s(&f, path.c_str(), "rb"); if (!f) return false;
        auto rd32=[&](){ unsigned char b[4]; if (fread(b,1,4,f)!=4) return 0u; return (unsigned)(b[0]|(b[1]<<8)|(b[2]<<16)|(b[3]<<24)); };
        auto rd16=[&](){ unsigned char b[2]; if (fread(b,1,2,f)!=2) return 0u; return (unsigned)(b[0]|(b[1]<<8)); };
        char riff[4]; if (fread(riff,1,4,f)!=4 || strncmp(riff,"RIFF",4)!=0) { fclose(f); return false; }
        (void)rd32(); char wave[4]; if (fread(wave,1,4,f)!=4 || strncmp(wave,"WAVE",4)!=0) { fclose(f); return false; }
        bool gotFmt=false, gotData=false; unsigned dataSize=0; unsigned short bits=0, fmt=0; channels=0; sampleRate=0; std::vector<unsigned char> data;
        while (!feof(f))
        {
            char id[4]; if (fread(id,1,4,f)!=4) break; unsigned sz = rd32(); long next = ftell(f) + (long)sz;
            if (strncmp(id,"fmt ",4)==0)
            {
                fmt = (unsigned short)rd16(); channels = (unsigned short)rd16(); sampleRate = (int)rd32(); (void)rd32(); (void)rd16(); bits = (unsigned short)rd16();
                gotFmt = true; fseek(f, next, SEEK_SET);
            }
            else if (strncmp(id,"data",4)==0)
            {
                data.resize(sz); if (sz>0) fread(data.data(),1,sz,f); dataSize = sz; gotData = true;
            }
            else fseek(f, next, SEEK_SET);
            if (gotFmt && gotData) break;
        }
        fclose(f);
        if (!gotFmt || !gotData || fmt != 1 || bits != 16 || channels < 1) return false;
        outPCM.resize(dataSize/2); memcpy(outPCM.data(), data.data(), dataSize);
        return true;
    }

    ALuint AudioEngine::loadSound(const std::string& path)
    {
        auto it = m_cache.find(path); if (it != m_cache.end()) return it->second;
        short* pcm = nullptr; int channels=0, sampleRate=0; size_t frames=0;
        if (ends_with(path, ".wav"))
        {
            std::vector<short> out; if (!load_wav_pcm16(path, out, channels, sampleRate)) return 0; frames = out.size()/channels; pcm = (short*)malloc(out.size()*sizeof(short)); memcpy(pcm, out.data(), out.size()*sizeof(short));
        }
        else if (ends_with(path, ".ogg"))
        {
            int err=0; stb_vorbis* v = stb_vorbis_open_filename(path.c_str(), &err, nullptr); if (!v) return 0;
            stb_vorbis_info info = stb_vorbis_get_info(v); channels = info.channels; sampleRate = info.sample_rate;
            int samples = stb_vorbis_stream_length_in_samples(v) * channels;
            pcm = (short*)malloc(samples * sizeof(short));
            int got = stb_vorbis_get_samples_short_interleaved(v, channels, pcm, samples);
            frames = got;
            stb_vorbis_close(v);
        }
        else return 0;

        ALuint buf=0; alGenBuffers(1,&buf);
        ALenum fmt = (channels==1)?AL_FORMAT_MONO16:AL_FORMAT_STEREO16;
        alBufferData(buf, fmt, pcm, (ALsizei)(frames*channels*sizeof(short)), sampleRate);
        free(pcm);
        m_cache[path]=buf;
        return buf;
    }

    ALuint AudioEngine::play(ALuint buffer, float gain, bool loop)
    {
        if (!buffer) return 0; ALuint src; alGenSources(1, &src); m_sources.push_back(src);
        alSourcei(src, AL_BUFFER, buffer);
        alSourcef(src, AL_GAIN, gain);
        alSourcei(src, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
        alSource3f(src, AL_POSITION, 0,0,0);
        alSourcePlay(src);
        return src;
    }

    ALuint AudioEngine::play3D(ALuint buffer, float x, float y, float z, float gain, bool loop)
    {
        if (!buffer) return 0; ALuint src; alGenSources(1, &src); m_sources.push_back(src);
        alSourcei(src, AL_BUFFER, buffer);
        alSourcef(src, AL_GAIN, gain);
        alSourcei(src, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
        alSource3f(src, AL_POSITION, x,y,z);
        alSourcePlay(src);
        return src;
    }

    void AudioEngine::stop(ALuint source)
    {
        alSourceStop(source);
    }

    void AudioEngine::setListener(float x, float y, float z, float fx, float fy, float fz, float ux, float uy, float uz)
    {
        alListener3f(AL_POSITION, x,y,z);
        float ori[6] = {fx,fy,fz, ux,uy,uz};
        alListenerfv(AL_ORIENTATION, ori);
    }

    void AudioEngine::update() {}
}


