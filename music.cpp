// music.c

#include "stdafx.h"

#include <string.h>

#include "main.h"
#include "music.h"
#include "gworld.h"
#include "gameticks.h"
#include "soundfx.h"
#include "graphics.h"

#ifdef USE_FMOD
#include "fmod/fmod.hpp"
#include "fmod/fmod_errors.h"
#else
#include "SDL2/SDL_sound.h"
int AddChannel(Sound_Sample* sample, int stream, int loop);
void Channel_SetVolume(int channel, float volume);
void Channel_SetSpeed(int channel, float speed);
void Channel_SetPause(int channel, int pause);
void Channel_Stop(int channel);
Sound_Sample* Channel_NewStreamSample(const char* name);
#endif

const int               k_noMusic = -1;
const int               k_songs = 14;

MBoolean                musicOn = true;
int                     musicSelection = k_noMusic;

static MBoolean         s_musicFast = false;
int                     s_musicPaused = 0;
#ifdef USE_FMOD
static FMOD::Channel*   s_musicChannel = NULL;
static FMOD::Sound*     s_musicModule = NULL;
#else
static int              s_musicChannel = -1;
static Sound_Sample*    s_musicModule = NULL;
#endif

void EnableMusic( MBoolean on )
{
#ifdef USE_FMOD
    if (s_musicChannel)
    {
        FMOD_RESULT result = s_musicChannel->setVolume(on? 0.75f: 0.0f);
        FMOD_ERRCHECK(result);
    }
#else
    if(s_musicChannel>-1)
        Channel_SetVolume(s_musicChannel, on?0.75f:0.0f);
#endif
}

void FastMusic( void )
{
    if (s_musicModule && !s_musicFast)
    {
#ifdef USE_FMOD
        FMOD_RESULT result = s_musicModule->setMusicSpeed(1.3f);
        FMOD_ERRCHECK(result);
#else
        Channel_SetSpeed(s_musicChannel, 1.3f);
#endif
        s_musicFast = true;
    }
}

void SlowMusic( void )
{
    if (s_musicModule && s_musicFast)
    {
#ifdef USE_FMOD
        FMOD_RESULT result = s_musicModule->setMusicSpeed(1.0f);
        FMOD_ERRCHECK(result);
#else
        Channel_SetSpeed(s_musicChannel, 1.0f);
#endif
        
        s_musicFast = false;
    }
}

void PauseMusic( void )
{
    if (s_musicChannel)
    {
#ifdef USE_FMOD
        FMOD_RESULT result = s_musicChannel->setPaused(true);
        FMOD_ERRCHECK(result);
#else
        Channel_SetPause(s_musicChannel, 1);
#endif
        
        s_musicPaused++;
    }
}

void ResumeMusic( void )
{
    if (s_musicChannel)
    {
#ifdef USE_FMOD
        FMOD_RESULT result = s_musicChannel->setPaused(false);
        FMOD_ERRCHECK(result);
#else
        Channel_SetPause(s_musicChannel, 0);
#endif
        
        s_musicPaused--;
    }
}

void ChooseMusic( short which )
{
#ifdef USE_FMOD
    if (s_musicChannel != NULL)
    {
        s_musicChannel->stop();
        s_musicChannel = NULL;
    }
    if (s_musicModule != NULL)
    {
        s_musicModule->release();
        s_musicModule = NULL;
    }
#else
    if (s_musicChannel != -1 )
    {
        Channel_Stop(s_musicChannel);
        s_musicChannel = -1;
    }
    s_musicModule = NULL;   // it is free part of the "Stop" command
#endif
    
   
    musicSelection = -1;
    
    if (which >= 0 && which <= k_songs)
    {
#ifdef USE_FMOD
        FMOD_RESULT result = g_fmod->createSound(QuickResourceName("mod", which+128, ""), FMOD_DEFAULT, 0, &s_musicModule);
        FMOD_ERRCHECK(result);
        
        result = g_fmod->playSound(/*FMOD_CHANNEL_FREE, */s_musicModule, 0, true, &s_musicChannel);
        FMOD_ERRCHECK(result);
        
        result = s_musicChannel->setPriority(10); // prioritize music first--WAVs should never knock out a MOD
        FMOD_ERRCHECK(result);
#else
        s_musicModule = Channel_NewStreamSample(QuickResourceName("mod", which+128, ""));
        if(s_musicModule==NULL) {
            printf("Error! Loading %s Failed\n", QuickResourceName("mod", which+128, ""));
            abort();
        }
        s_musicChannel = AddChannel(s_musicModule, 1, 1);
#endif
        EnableMusic(musicOn);
#ifdef USE_FMOD  
        result = s_musicModule->setLoopCount(-1);
        FMOD_ERRCHECK(result);
        
        result = s_musicChannel->setPaused(false);
        FMOD_ERRCHECK(result);
#endif
        musicSelection = which;
        s_musicPaused  = 0;
    }
}
