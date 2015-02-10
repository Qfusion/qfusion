OpenAL-MOB
==========

## Overview

OpenAL is a very popular API for game audio programming. There are many implementations of this including Apple's native implementation on iOS and Mac OS. To create really great sounding game audio, Head Related Transfer Functions are a must have feature. Many implementations do not support this or have a subpar implementation (like on iOS and Mac OS), so OpenAL-MOB is meant to provide the standard OpenAL audio programming interface with the power of HRTFs. 

This project is a fork of OpenAL-Soft and supports iOS, Android, Windows, and Mac. It has additional configuration options and the ability to toggle on and off HRTFs at runtime. 

## Requirements

OpenAL-MOB just requires a development environment for whatever platform you are trying to build it on. 

## Table of Contents

- [Getting Started](#getting-started)
  - [Download OpenAL-MOB](#download-openal-mob)
  - [Add the Library to Your Project](#add-the-library-to-your-project)
  - [Use the Standard OpenAL Interfaces](#use-the-standard-openal-interfaces)
- [Documentation](#documentation)
  - [Configuration](#configuration)
  - [Dynamically turning on and off HRTFs](#dynamically-turning-on-and-off-hrtfs)
- [Support](#support)
- [License](#license)

## Getting Started

#### Download OpenAL-MOB

You can download the latest release via the link below or clone it directly from this GitHub repository.

**Option 1:** Download OpenAL-MOB 1.0.0  
https://github.com/Jawbone/OpenAL-MOB/releases/tag/1.0.0

**Option 2:** Clone this repository from GitHub  
`git clone git@github.com:Jawbone/OpenAL-MOB.git`

#### Add the Library to Your Project

iOS - Add OpenAL-MOB-ios.xcodeproj to your XCode project. Be sure that you aren't including OpenAL.framework in your project. If you are, you'll get sybmol conflicts. 

Android - The makefiles for the library can be found in: build_android/jni/

Mac - Add OpenAL-MOB-mac.xcodeproj to your XCode project. Be sure that you aren't including OpenAL.framework in your project. If you are, you'll get sybmol conflicts. 

Windows - Add build_win32/OpenAL-MOB.vcxproj to your Visual Studio Solution.


#### Use the Standard OpenAL Interfaces

If you are migrating from an existing OpenAL implementation, it should be straightforward. Some effects that aren't part of the standard may have to be changed, but the basics work exactly the same. For more documentation on the non-standard effects, please check out the OpenAL-Soft documentation: http://kcat.strangesoft.net/openal.html

# Documentation

## Configuration

OpenAL-Soft pulls the configuration properties from a text file. Since that doesn't work well for mobile, we have a way to specify them at runtime. 

``` C++
#define AL_ALEXT_PROTOTYPES // Without this, it doesn't prototype the functions that OpenAL-Mob adds to the reference implementation. 
#include <AL/al.h>
#include <AL/alext.h>

const MOB_ConfigKeyValue g_soundConfig[] =
{
#if PLAT_WIN
  // The default sound output on Windows can't be forced to 44.1 KHz. Outputting at 44.1 KHz is essential to support HRTF, so adding this is on Windows is a good idea
	{ MOB_ConfigKey_root_drivers , "dsound" }, 
#endif // #if PLAT_WIN
	// If you want to use HRTFs, you should be outputting to Stereo sound
	{ MOB_ConfigKey_root_channels, "stereo" },
	{ MOB_ConfigKey_root_hrtf    , (const char*)1 }, // This is a union, and const char * is the first type, so we have to cast it.
	{ MOB_ConfigKey_NULL         , 0 }, // This is the terminator for the config array
};

int main( int argc, const char* argv[] )
{
	// Before you call alcOpenDevice, set your config
	alSetConfigMOB( g_soundConfig );
	ALCdevice  *device = alcOpenDevice(NULL);
	
	// the parameters
	const ALint params[] = 
	{
		ALC_FREQUENCY, 44100,   // The HRTF only works for 44.1KHz output.      
		0,			// Null terminator
	};
	ALCcontext *context = alcCreateContext(device, params);
	
	// Continue on with the standard OpenAL set up
	...
}

```
For more configuration options, look through mob\Include\alConfigMobDefs_inl.h. All the configuration options in OpenAL-Soft's text file are available using the MOB_ConfigKeyValue structure. 

## Dynamically turning on and off HRTFs
Because HRTFs are processor expensive, it is a good idea to turn on and off HRTFs when they won't produce a noticeable increase in audio fidelity. HRTFs are designed for playback systems that have binaural separation like headphones and Jambox speakers. To turn HRTFs on and off, use the following function:
``` C++
ALboolean alcDeviceEnableHrtfMOB( ALCdevice *device, ALboolean enable );
```
The return value is whether the function succeeded or not. 

On iOS, you can use the following function to determine whether or not HRTFs should be enabled:

``` objective-c
BOOL EnableHRTF()
{
    NSArray *outputs = [[[AVAudioSession sharedInstance] currentRoute ] outputs ];
    int outputCount = [outputs count];
    for( int i = 0; i < outputCount; ++i)
    {
        AVAudioSessionPortDescription *description = (AVAudioSessionPortDescription*)[outputs objectAtIndex:i];
        NSString *string = [description portType];

        if( [string isEqualToString:AVAudioSessionPortHeadphones] )
        {
            return YES;
        }
        if( [string isEqualToString:AVAudioSessionPortBluetoothA2DP] )
        {
            return YES;
        }
        if( [string isEqualToString:AVAudioSessionPortBluetoothHFP] )
        {
            return YES;
        }
        if( [string isEqualToString:AVAudioSessionPortBluetoothLE] )
        {
            return YES;
        }
    }
    return NO;
}
```

On Android, you need to make sure your app requests the permission: MODIFY_AUDIO_SETTINGS in order to get the true values from the functions. Then, put the following code in your activity:
``` java
public class MyActivity extends Activity {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        …
        audioManager = (AudioManager)getSystemService(Context.AUDIO_SERVICE);
    }

    private AudioManager audioManager;

    public boolean EnableHRTF()
    {
        return audioManager.isBluetoothA2dpOn() || audioManager.isWiredHeadsetOn();
    }
}
```

Be sure to call these functions every frame (or every few frames) so that you can handle the situation where the headphones are plugged in or removed while the game is running.

On Windows and Mac, there isn't a way to detect that the user has headphones or a Jambox speaker, so your should probably expose the setting to the user as a "headphone" mode. 

# Support

Contact the developer support team by sending an email to mgilgenbach@jawbone.com.

# License

Usage is provided under the LGPL (v2.0). See LICENSE for full details.
