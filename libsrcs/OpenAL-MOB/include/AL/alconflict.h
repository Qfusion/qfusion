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

// If you set this to 1, then it will rename prefix all al*() functions with ALMOB_. This allows you
// to link two separate OpenAL implementations into the same program.
#define ALMOB_UNIQUE_NAMES     (TARGET_OS_IPHONE)
#define ALMOB_REMOVE_CONFLICTS (ALMOB_UNIQUE_NAMES && !defined(AL_BUILD_LIBRARY))

#if ALMOB_UNIQUE_NAMES
#  define alAuxiliaryEffectSlotf                   ALMOB_alAuxiliaryEffectSlotf           
#  define alAuxiliaryEffectSlotfv                  ALMOB_alAuxiliaryEffectSlotfv          
#  define alAuxiliaryEffectSloti                   ALMOB_alAuxiliaryEffectSloti           
#  define alAuxiliaryEffectSlotiv                  ALMOB_alAuxiliaryEffectSlotiv          
#  define alBuffer3f                               ALMOB_alBuffer3f                       
#  define alBuffer3i                               ALMOB_alBuffer3i                       
#  define alBufferData                             ALMOB_alBufferData                     
#  define alBufferDataStatic                       ALMOB_alBufferDataStatic               
#  define alBufferSamplesSOFT                      ALMOB_alBufferSamplesSOFT              
#  define alBufferSubDataSOFT                      ALMOB_alBufferSubDataSOFT              
#  define alBufferSubSamplesSOFT                   ALMOB_alBufferSubSamplesSOFT           
#  define alBufferf                                ALMOB_alBufferf                        
#  define alBufferfv                               ALMOB_alBufferfv                       
#  define alBufferi                                ALMOB_alBufferi                        
#  define alBufferiv                               ALMOB_alBufferiv                       
#  define alDeleteAuxiliaryEffectSlots             ALMOB_alDeleteAuxiliaryEffectSlots     
#  define alDeleteBuffers                          ALMOB_alDeleteBuffers                  
#  define alDeleteEffects                          ALMOB_alDeleteEffects                  
#  define alDeleteFilters                          ALMOB_alDeleteFilters                  
#  define alDeleteSources                          ALMOB_alDeleteSources                  
#  define alDisable                                ALMOB_alDisable                        
#  define alDistanceModel                          ALMOB_alDistanceModel                  
#  define alDopplerFactor                          ALMOB_alDopplerFactor                  
#  define alDopplerVelocity                        ALMOB_alDopplerVelocity                
#  define alSetConfigMOB	                       ALMOB_alSetConfigMOB
#  define alEffectf                                ALMOB_alEffectf                        
#  define alEffectfv                               ALMOB_alEffectfv                       
#  define alEffecti                                ALMOB_alEffecti                        
#  define alEffectiv                               ALMOB_alEffectiv                       
#  define alEnable                                 ALMOB_alEnable                         
#  define alFilterf                                ALMOB_alFilterf                        
#  define alFilterfv                               ALMOB_alFilterfv                       
#  define alFilteri                                ALMOB_alFilteri                        
#  define alFilteriv                               ALMOB_alFilteriv                       
#  define alGenAuxiliaryEffectSlots                ALMOB_alGenAuxiliaryEffectSlots        
#  define alGenBuffers                             ALMOB_alGenBuffers                     
#  define alGenEffects                             ALMOB_alGenEffects                     
#  define alGenFilters                             ALMOB_alGenFilters                     
#  define alGenSources                             ALMOB_alGenSources                     
#  define alGetAuxiliaryEffectSlotf                ALMOB_alGetAuxiliaryEffectSlotf        
#  define alGetAuxiliaryEffectSlotfv               ALMOB_alGetAuxiliaryEffectSlotfv       
#  define alGetAuxiliaryEffectSloti                ALMOB_alGetAuxiliaryEffectSloti        
#  define alGetAuxiliaryEffectSlotiv               ALMOB_alGetAuxiliaryEffectSlotiv       
#  define alGetBoolean                             ALMOB_alGetBoolean                     
#  define alGetBooleanv                            ALMOB_alGetBooleanv                    
#  define alGetBuffer3f                            ALMOB_alGetBuffer3f                    
#  define alGetBuffer3i                            ALMOB_alGetBuffer3i                    
#  define alGetBufferSamplesSOFT                   ALMOB_alGetBufferSamplesSOFT           
#  define alGetBufferf                             ALMOB_alGetBufferf                     
#  define alGetBufferfv                            ALMOB_alGetBufferfv                    
#  define alGetBufferi                             ALMOB_alGetBufferi                     
#  define alGetBufferiv                            ALMOB_alGetBufferiv                    
#  define alGetDouble                              ALMOB_alGetDouble                      
#  define alGetDoublev                             ALMOB_alGetDoublev                     
#  define alGetEffectf                             ALMOB_alGetEffectf                     
#  define alGetEffectfv                            ALMOB_alGetEffectfv                    
#  define alGetEffecti                             ALMOB_alGetEffecti                     
#  define alGetEffectiv                            ALMOB_alGetEffectiv                    
#  define alGetEnumValue                           ALMOB_alGetEnumValue                   
#  define alGetError                               ALMOB_alGetError                       
#  define alGetFilterf                             ALMOB_alGetFilterf                     
#  define alGetFilterfv                            ALMOB_alGetFilterfv                    
#  define alGetFilteri                             ALMOB_alGetFilteri                     
#  define alGetFilteriv                            ALMOB_alGetFilteriv                    
#  define alGetFloat                               ALMOB_alGetFloat                       
#  define alGetFloatv                              ALMOB_alGetFloatv                      
#  define alGetInteger                             ALMOB_alGetInteger                     
#  define alGetIntegerv                            ALMOB_alGetIntegerv                    
#  define alGetListener3f                          ALMOB_alGetListener3f                  
#  define alGetListener3i                          ALMOB_alGetListener3i                  
#  define alGetListenerf                           ALMOB_alGetListenerf                   
#  define alGetListenerfv                          ALMOB_alGetListenerfv                  
#  define alGetListeneri                           ALMOB_alGetListeneri                   
#  define alGetListeneriv                          ALMOB_alGetListeneriv                  
#  define alGetProcAddress                         ALMOB_alGetProcAddress                 
#  define alGetSource3dSOFT                        ALMOB_alGetSource3dSOFT                
#  define alGetSource3f                            ALMOB_alGetSource3f                    
#  define alGetSource3i                            ALMOB_alGetSource3i                    
#  define alGetSource3i64SOFT                      ALMOB_alGetSource3i64SOFT              
#  define alGetSourcedSOFT                         ALMOB_alGetSourcedSOFT                 
#  define alGetSourcedvSOFT                        ALMOB_alGetSourcedvSOFT                
#  define alGetSourcef                             ALMOB_alGetSourcef                     
#  define alGetSourcefv                            ALMOB_alGetSourcefv                    
#  define alGetSourcei                             ALMOB_alGetSourcei                     
#  define alGetSourcei64SOFT                       ALMOB_alGetSourcei64SOFT               
#  define alGetSourcei64vSOFT                      ALMOB_alGetSourcei64vSOFT              
#  define alGetSourceiv                            ALMOB_alGetSourceiv                    
#  define alGetString                              ALMOB_alGetString                      
#  define alIsAuxiliaryEffectSlot                  ALMOB_alIsAuxiliaryEffectSlot          
#  define alIsBuffer                               ALMOB_alIsBuffer                       
#  define alIsBufferFormatSupportedSOFT            ALMOB_alIsBufferFormatSupportedSOFT    
#  define alIsEffect                               ALMOB_alIsEffect                       
#  define alIsEnabled                              ALMOB_alIsEnabled                      
#  define alIsExtensionPresent                     ALMOB_alIsExtensionPresent             
#  define alIsFilter                               ALMOB_alIsFilter                       
#  define alIsSource                               ALMOB_alIsSource                       
#  define alListener3f                             ALMOB_alListener3f                     
#  define alListener3i                             ALMOB_alListener3i                     
#  define alListenerf                              ALMOB_alListenerf                      
#  define alListenerfv                             ALMOB_alListenerfv                     
#  define alListeneri                              ALMOB_alListeneri                      
#  define alListeneriv                             ALMOB_alListeneriv                     
#  define alRequestFoldbackStart                   ALMOB_alRequestFoldbackStart           
#  define alRequestFoldbackStop                    ALMOB_alRequestFoldbackStop            
#  define alSource3dSOFT                           ALMOB_alSource3dSOFT                   
#  define alSource3f                               ALMOB_alSource3f                       
#  define alSource3i                               ALMOB_alSource3i                       
#  define alSource3i64SOFT                         ALMOB_alSource3i64SOFT                 
#  define alSourcePause                            ALMOB_alSourcePause                    
#  define alSourcePausev                           ALMOB_alSourcePausev                   
#  define alSourcePlay                             ALMOB_alSourcePlay                     
#  define alSourcePlayv                            ALMOB_alSourcePlayv                    
#  define alSourceQueueBuffers                     ALMOB_alSourceQueueBuffers             
#  define alSourceRewind                           ALMOB_alSourceRewind                   
#  define alSourceRewindv                          ALMOB_alSourceRewindv                  
#  define alSourceStop                             ALMOB_alSourceStop                     
#  define alSourceStopv                            ALMOB_alSourceStopv                    
#  define alSourceUnqueueBuffers                   ALMOB_alSourceUnqueueBuffers           
#  define alSourcedSOFT                            ALMOB_alSourcedSOFT                    
#  define alSourcedvSOFT                           ALMOB_alSourcedvSOFT                   
#  define alSourcef                                ALMOB_alSourcef                        
#  define alSourcefv                               ALMOB_alSourcefv                       
#  define alSourcei                                ALMOB_alSourcei                        
#  define alSourcei64SOFT                          ALMOB_alSourcei64SOFT                  
#  define alSourcei64vSOFT                         ALMOB_alSourcei64vSOFT                 
#  define alSourceiv                               ALMOB_alSourceiv                       
#  define alSpeedOfSound                           ALMOB_alSpeedOfSound                   
#  define alcCaptureCloseDevice                    ALMOB_alcCaptureCloseDevice            
#  define alcCaptureOpenDevice                     ALMOB_alcCaptureOpenDevice             
#  define alcCaptureSamples                        ALMOB_alcCaptureSamples                
#  define alcCaptureStart                          ALMOB_alcCaptureStart                  
#  define alcCaptureStop                           ALMOB_alcCaptureStop                   
#  define alcCloseDevice                           ALMOB_alcCloseDevice                   
#  define alcCreateContext                         ALMOB_alcCreateContext                 
#  define alcDestroyContext                        ALMOB_alcDestroyContext                
#  define alcDeviceEnableHrtfMOB                   ALMOB_alcDeviceEnableHrtfMOB
#  define alcGetContextsDevice                     ALMOB_alcGetContextsDevice             
#  define alcGetCurrentContext                     ALMOB_alcGetCurrentContext             
#  define alcGetEnumValue                          ALMOB_alcGetEnumValue                  
#  define alcGetError                              ALMOB_alcGetError                      
#  define alcGetIntegerv                           ALMOB_alcGetIntegerv                   
#  define alcGetProcAddress                        ALMOB_alcGetProcAddress                
#  define alcGetString                             ALMOB_alcGetString                     
#  define alcGetThreadContext                      ALMOB_alcGetThreadContext              
#  define alcIsExtensionPresent                    ALMOB_alcIsExtensionPresent            
#  define alcIsRenderFormatSupportedSOFT           ALMOB_alcIsRenderFormatSupportedSOFT   
#  define alcLoopbackOpenDeviceSOFT                ALMOB_alcLoopbackOpenDeviceSOFT        
#  define alcMakeContextCurrent                    ALMOB_alcMakeContextCurrent            
#  define alcOpenDevice                            ALMOB_alcOpenDevice                    
#  define alcProcessContext                        ALMOB_alcProcessContext                
#  define alcRenderSamplesSOFT                     ALMOB_alcRenderSamplesSOFT             
#  define alcSetThreadContext                      ALMOB_alcSetThreadContext              
#  define alcSuspendContext                        ALMOB_alcSuspendContext                
#  define alcGetDeviceReferenceCount	           ALMOB_alcGetDeviceReferenceCount
#endif // ALMOB_UNIQUE_NAMES
