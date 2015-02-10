/**
 * OpenAL-MOB cross platform audio library
 * Copyright (C) 2013 by Jawbone.
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 *
 * This file is unique to OpenAL-MOB and is not part of the OpenAL-Soft package.
 */
#if ALMOB_REMOVE_CONFLICTS
#  undef alAuxiliaryEffectSlotf
#  undef alAuxiliaryEffectSlotfv
#  undef alAuxiliaryEffectSloti
#  undef alAuxiliaryEffectSlotiv
#  undef alBuffer3f
#  undef alBuffer3i
#  undef alBufferData
#  undef alBufferDataStatic
#  undef alBufferSamplesSOFT
#  undef alBufferSubDataSOFT
#  undef alBufferSubSamplesSOFT
#  undef alBufferf
#  undef alBufferfv
#  undef alBufferi
#  undef alBufferiv
#  undef alDeleteAuxiliaryEffectSlots
#  undef alDeleteBuffers
#  undef alDeleteEffects
#  undef alDeleteFilters
#  undef alDeleteSources
#  undef alDisable
#  undef alDistanceModel
#  undef alDopplerFactor
#  undef alDopplerVelocity
#  undef alEX_SetConfig
#  undef alEffectf
#  undef alEffectfv
#  undef alEffecti
#  undef alEffectiv
#  undef alEnable
#  undef alFilterf
#  undef alFilterfv
#  undef alFilteri
#  undef alFilteriv
#  undef alGenAuxiliaryEffectSlots
#  undef alGenBuffers
#  undef alGenEffects
#  undef alGenFilters
#  undef alGenSources
#  undef alGetAuxiliaryEffectSlotf
#  undef alGetAuxiliaryEffectSlotfv
#  undef alGetAuxiliaryEffectSloti
#  undef alGetAuxiliaryEffectSlotiv
#  undef alGetBoolean
#  undef alGetBooleanv
#  undef alGetBuffer3f
#  undef alGetBuffer3i
#  undef alGetBufferSamplesSOFT
#  undef alGetBufferf
#  undef alGetBufferfv
#  undef alGetBufferi
#  undef alGetBufferiv
#  undef alGetDouble
#  undef alGetDoublev
#  undef alGetEffectf
#  undef alGetEffectfv
#  undef alGetEffecti
#  undef alGetEffectiv
#  undef alGetEnumValue
#  undef alGetError
#  undef alGetFilterf
#  undef alGetFilterfv
#  undef alGetFilteri
#  undef alGetFilteriv
#  undef alGetFloat
#  undef alGetFloatv
#  undef alGetInteger
#  undef alGetIntegerv
#  undef alGetListener3f
#  undef alGetListener3i
#  undef alGetListenerf
#  undef alGetListenerfv
#  undef alGetListeneri
#  undef alGetListeneriv
#  undef alGetProcAddress
#  undef alGetSource3dSOFT
#  undef alGetSource3f
#  undef alGetSource3i
#  undef alGetSource3i64SOFT
#  undef alGetSourcedSOFT
#  undef alGetSourcedvSOFT
#  undef alGetSourcef
#  undef alGetSourcefv
#  undef alGetSourcei
#  undef alGetSourcei64SOFT
#  undef alGetSourcei64vSOFT
#  undef alGetSourceiv
#  undef alGetString
#  undef alIsAuxiliaryEffectSlot
#  undef alIsBuffer
#  undef alIsBufferFormatSupportedSOFT
#  undef alIsEffect
#  undef alIsEnabled
#  undef alIsExtensionPresent
#  undef alIsFilter
#  undef alIsSource
#  undef alListener3f
#  undef alListener3i
#  undef alListenerf
#  undef alListenerfv
#  undef alListeneri
#  undef alListeneriv
#  undef alRequestFoldbackStart
#  undef alRequestFoldbackStop
#  undef alSource3dSOFT
#  undef alSource3f
#  undef alSource3i
#  undef alSource3i64SOFT
#  undef alSourcePause
#  undef alSourcePausev
#  undef alSourcePlay
#  undef alSourcePlayv
#  undef alSourceQueueBuffers
#  undef alSourceRewind
#  undef alSourceRewindv
#  undef alSourceStop
#  undef alSourceStopv
#  undef alSourceUnqueueBuffers
#  undef alSourcedSOFT
#  undef alSourcedvSOFT
#  undef alSourcef
#  undef alSourcefv
#  undef alSourcei
#  undef alSourcei64SOFT
#  undef alSourcei64vSOFT
#  undef alSourceiv
#  undef alSpeedOfSound
#  undef alcCaptureCloseDevice
#  undef alcCaptureOpenDevice
#  undef alcCaptureSamples
#  undef alcCaptureStart
#  undef alcCaptureStop
#  undef alcCloseDevice
#  undef alcCreateContext
#  undef alcDestroyContext
#  undef alcDeviceEnableHrtfSOFTEX
#  undef alcGetContextsDevice
#  undef alcGetCurrentContext
#  undef alcGetEnumValue
#  undef alcGetError
#  undef alcGetIntegerv
#  undef alcGetProcAddress
#  undef alcGetString
#  undef alcGetThreadContext
#  undef alcIsExtensionPresent
#  undef alcIsRenderFormatSupportedSOFT
#  undef alcLoopbackOpenDeviceSOFT
#  undef alcMakeContextCurrent
#  undef alcOpenDevice
#  undef alcProcessContext
#  undef alcRenderSamplesSOFT
#  undef alcSetThreadContext
#  undef alcSuspendContext
#  undef alcGetDeviceReferenceCount
#  undef ALMOB_REMOVE_CONFLICTS
#endif // ALMOB_REMOVE_CONFLICTS
