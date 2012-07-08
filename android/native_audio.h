#pragma once

#include "native-audio-so.h"
#include <string>

// This is the file you should include from your program. It dynamically loads
// the native_audio.so shared object and sets up the function pointers.

// Do not call this if you have detected that the android version is below
// 2.2, as it will fail miserably.

bool AndroidAudio_Init(AndroidAudioCallback cb, std::string libraryDir);
bool AndroidAudio_Pause();
bool AndroidAudio_Resume();
void AndroidAudio_Shutdown();
