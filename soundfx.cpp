// soundfx.c

#include "stdafx.h"
#include "main.h"
#include "soundfx.h"
#include "music.h"
#ifdef USE_FMOD
#include "fmod/fmod.hpp"
#include "fmod/fmod_errors.h"
#else
#include "SDL2/SDL_sound.h"
#endif
#include <stdio.h>
#ifdef USE_FMOD
FMOD::System              *g_fmod;
static FMOD::Sound        *s_sound[kNumSounds];
#else
static Sound_Sample*      s_sound[kNumSounds];
static void*              s_soundBuff[kNumSounds];
#define MAX_CHANNEL       64
#define MED_VAL           8192
#define MED_DEC           13
typedef struct {
    Sound_Sample    *sample;
    uint32_t        volume;
    uint32_t        left;
    uint32_t        right;
    uint32_t        speed;
    uint32_t        buffer_size;
    int             stream;
    int             loop;
    int             stop;
    int             pause;
    uint32_t        pos;
} Sound_Channel;
static Sound_Channel    s_channels[MAX_CHANNEL];
Sound_AudioInfo audioinf = {.format=AUDIO_S16, .channels=2, .rate=44100};
SDL_mutex*          SoundMutex = NULL;
#endif
MBoolean                   soundOn = true;

#ifdef USE_FMOD
void FMOD_ERRCHECK(int result)
{
    if (result != FMOD_OK)
    {
        printf("FMOD error! (%d) %s\n", result, FMOD_ErrorString(FMOD_RESULT(result)));
        abort();
    }
}
#else
int32_t* MixBuffer = NULL;
void AudioMixer(void* udata, Uint8 *stream, int len)
{
    // prepare
    int num_samples = len / 4;  // S16 + stereo
    memset(MixBuffer, 0, num_samples*2*sizeof(int32_t));
    // mix
    for (int index=0; index<MAX_CHANNEL; index++)
    {
        Sound_Sample* sample = s_channels[index].sample;
        if (sample)
        {
            if(s_channels[index].stop) {
                if(s_channels[index].stream) {
                    SDL_LockMutex(SoundMutex);
                    Sound_FreeSample(sample);
                    SDL_UnlockMutex(SoundMutex);
                }
                s_channels[index].stop = 0;
                s_channels[index].sample = NULL;
                sample = NULL;
            } else  if (!s_channels[index].pause) {
                int todecode = num_samples;
                uint32_t ppos = s_channels[index].pos;
                int mixpos = 0;
                while(todecode) {
                    uint32_t pos = (ppos>>MED_DEC)*4;
                    if(pos+3>=s_channels[index].buffer_size) {
                        Uint32 decoded = 0;
                        if (s_channels[index].stream) {
                            SDL_LockMutex(SoundMutex);
                            decoded = Sound_Decode(sample);
                            SDL_UnlockMutex(SoundMutex);
                        }
                        if(decoded==0 && s_channels[index].loop) {
                            if(s_channels[index].stream) {
                                SDL_LockMutex(SoundMutex);
                                Sound_Rewind(sample);
                                decoded = Sound_Decode(sample);
                                SDL_UnlockMutex(SoundMutex);
                            } else {
                                decoded = s_channels[index].buffer_size;
                            }
                        }
                        ppos -= ((s_channels[index].buffer_size/4)<<MED_DEC);
                        s_channels[index].buffer_size = decoded;
                        if(decoded==0) {todecode = 0; s_channels[index].stop = 1;}
                        pos = (ppos>>MED_DEC)*4;
                    }
                    if(todecode) {
                        int16_t *s;
                        //memcpy(s, (char*)sample->buffer+pos, 2*2);
                        s = (int16_t*)((char*)sample->buffer+pos);
                        int s32[2];
                        s32[0] = (s[0]*s_channels[index].left)>>MED_DEC;
                        s32[1] = (s[1]*s_channels[index].right)>>MED_DEC;
                        MixBuffer[mixpos+0] += (s32[0]*s_channels[index].volume)>>MED_DEC;    // left
                        MixBuffer[mixpos+1] += (s32[1]*s_channels[index].volume)>>MED_DEC;    // right
                        mixpos+=2;
                        ppos+=s_channels[index].speed;
                        --todecode;
                    }
                }
                s_channels[index].pos = ppos;
            }
        }
    }
    //clip and copy MixBuffer
    int16_t* dest = (int16_t*)stream;
    int32_t* src = MixBuffer;
    for (int i=0; i<num_samples*2; i++) // stereo, so 2 copy per samples
    {
        *(dest++) = *(src++);
    }
    // All done
}
#endif

void InitSound( void )
{
#ifdef USE_FMOD
    FMOD_RESULT   result = FMOD::System_Create(&g_fmod);
    FMOD_ERRCHECK(result);
    
    unsigned int  version;
    result = g_fmod->getVersion(&version);
    FMOD_ERRCHECK(result);
    
    if (version < FMOD_VERSION)
    {
        printf("Error!  You are using an old version of FMOD %08x.  This program requires %08x\n", version, FMOD_VERSION);
        abort();
    }
    
    result = g_fmod->init(64, FMOD_INIT_NORMAL, 0);
    FMOD_ERRCHECK(result);
    
    for (int index=0; index<kNumSounds; index++)
    {
        /* NOTE: don't replace the sound flags with FMOD_DEFAULT! This will make some WAVs loop (and fail to release their channels). */
        result = g_fmod->createSound(QuickResourceName("snd", index+128, ".wav"), FMOD_LOOP_OFF | FMOD_2D/* | FMOD_HARDWARE*/, 0, &s_sound[index]);
        FMOD_ERRCHECK(result);
    }
#else
    // start SDL with audio support
    if(SDL_InitSubSystem(SDL_INIT_AUDIO)==-1) {
        printf("Error! SDL_Init Failed: %s\n", SDL_GetError());
        abort();
    }
    // prepare channels
    for (int index=0; index<MAX_CHANNEL; index++)
    {
        s_channels[index].sample = NULL;
        s_channels[index].volume = MED_VAL>>1;
        s_channels[index].left = MED_VAL;
        s_channels[index].right = MED_VAL;
        s_channels[index].speed = MED_VAL;
        s_channels[index].stop = 0;
    }
    SoundMutex = SDL_CreateMutex();
    if(SoundMutex==NULL) {
        printf("Error! SDL_Mutex creation Failed: %s\n", SDL_GetError());
        abort();
    }
    // open 44.1KHz, signed 16bit, system byte order,
    //      stereo audio, using 4096 byte chunks
    SDL_AudioSpec wanted;
    wanted.freq = 44100;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 2;
    wanted.samples = 2048;
    wanted.callback = AudioMixer;
    wanted.userdata = 0;
    MixBuffer = (int*)malloc(wanted.samples*2*2*sizeof(int32_t));   // 2* margin in the MixBuffer...
    if(SDL_OpenAudio(&wanted, NULL)==-1) {
        printf("Error! SDL_OpenAudio Failed: %s\n", SDL_GetError());
        abort();
    }

    int initted=Sound_Init();
    if(!initted) {
        printf("Error! SDL_sound Failed to initialise:%s\n", Sound_GetError());
        abort();
    }
    // Load all sample in memory
    for (int index=0; index<kNumSounds; index++)
    {
        const char* name = QuickResourceName("snd", index+128, ".wav");
        FILE *f = fopen(name, "rb");
        if (!f) {
            printf("Error! Loading %s\n", name);
            abort();
        }
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        fseek(f, 0, SEEK_SET);
        s_soundBuff[index] = malloc(size);
        fread(s_soundBuff[index], 1, size, f);
        fclose(f);
        s_sound[index] = Sound_NewSample(SDL_RWFromMem(s_soundBuff[index], size),"wav", &audioinf , 8192);
        if(s_sound[index]==NULL) {
            printf("Error! Loading %s : %s\n", name, Sound_GetError());
            abort();
        }
        Sound_DecodeAll(s_sound[index]);
    }
    SDL_PauseAudio(0);
#endif
}

#ifndef USE_FMOD
int AddChannel(Sound_Sample* sample, int stream, int loop)
{
    int index = -1;
    for (int i=0; i<MAX_CHANNEL && index==-1; i++)
    {
        if(s_channels[i].sample==NULL)
            index=i;
    }
    if(index!=-1)
    {
        s_channels[index].volume = MED_VAL>>1;
        s_channels[index].left = MED_VAL;
        s_channels[index].right = MED_VAL;
        s_channels[index].speed = MED_VAL;
        s_channels[index].stream = stream;
        s_channels[index].loop = loop;
        s_channels[index].pause = 0;
        s_channels[index].pos = 0;
        s_channels[index].stop = 0;
        if(stream)
            s_channels[index].buffer_size = 0;
        else
            s_channels[index].buffer_size = sample->buffer_size;
        SDL_LockMutex(SoundMutex);
        s_channels[index].sample = sample;
        SDL_UnlockMutex(SoundMutex);
    }
    return index;
}
void Channel_SetVolume(int channel, float volume)
{
    if(channel<0 || channel>=MAX_CHANNEL)
        return;
    s_channels[channel].volume = (MED_VAL>>1)*volume;
}
void Channel_SetPanning(int channel, float left, float right)
{
    if(channel<0 || channel>=MAX_CHANNEL)
        return;
    s_channels[channel].left = MED_VAL*left;
    s_channels[channel].right = MED_VAL*right;
}
void Channel_SetSpeed(int channel, float speed)
{
    if(channel<0 || channel>=MAX_CHANNEL)
        return;
    s_channels[channel].speed = MED_VAL*speed;
}
void Channel_SetPause(int channel, int pause)
{
    if(channel<0 || channel>=MAX_CHANNEL)
        return;
    s_channels[channel].pause = MED_VAL*pause;
}
void Channel_Stop(int channel)
{
    if(channel<0 || channel>=MAX_CHANNEL)
        return;
    s_channels[channel].stop = 1;
}
Sound_Sample* Channel_NewStreamSample(const char* name)
{
    SDL_LockMutex(SoundMutex);
    Sound_Sample* sample = Sound_NewSample(SDL_RWFromFile(name, "rb"), "mod", &audioinf, 16384);
    SDL_UnlockMutex(SoundMutex);
    return sample;
}
#endif


void PlayMono( short which )
{
    PlayStereoFrequency(2, which, 0);
}

void PlayStereo( short player, short which )
{
    PlayStereoFrequency(player, which, 0);
}

void PlayStereoFrequency( short player, short which, short freq )
{
#ifdef USE_FMOD
    struct SpeakerMix
    {
        float left, right, center;
    };
    
    SpeakerMix speakerMixForPlayer[] =
    {
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 },
    };
    
    const SpeakerMix& mix = speakerMixForPlayer[player];
    
    if (soundOn)
    {
        FMOD::Channel*    channel = NULL;
        FMOD_RESULT       result = g_fmod->playSound(/*FMOD_CHANNEL_FREE,*/ s_sound[which], 0, true, &channel);
        FMOD_ERRCHECK(result);
        
     /*   result = channel->setSpeakerMix(mix.left, mix.right, mix.center, 0.0, 0.0, 0.0, 0.0, 0.0);
        FMOD_ERRCHECK(result);*/
        
        float channelFrequency;
        result = s_sound[which]->getDefaults(&channelFrequency, NULL/*, NULL, NULL*/);
        FMOD_ERRCHECK(result);
        
        result = channel->setFrequency((channelFrequency * (16 + freq)) / 16);
        FMOD_ERRCHECK(result);
        
        result = channel->setPaused(false);
        FMOD_ERRCHECK(result);
        
        UpdateSound();
    }
#else
    struct SpeakerMix
    {
        float left, right, volume;
    };
    
    SpeakerMix speakerMixForPlayer[] =
    {
        { 0.75f, 0.25f, 1.0f },
        { 0.25f, 0.75f, 1.0f },
        { 1.0f,  1.0f,  1.0f },
    };
    const SpeakerMix& mix = speakerMixForPlayer[player];

    if (soundOn)
    {
        int channel = AddChannel(s_sound[which], 0, 0);
        // panning...
        if(channel!=-1) {
            Channel_SetVolume(channel, mix.volume);
            Channel_SetPanning(channel, mix.left, mix.right);
        }
        // freq
        if(channel!=-1) {
            Channel_SetSpeed(channel, (16.0f + freq) / 16.0f);
        }
    }   
#endif
}

void UpdateSound()
{
#ifdef USE_FMOD
    g_fmod->update();
#endif
}
