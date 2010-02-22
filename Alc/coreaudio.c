/**
 * OpenAL cross platform audio library
 * Copyright (C) 1999-2007 by authors.
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
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alMain.h"
#include "AL/al.h"
#include "AL/alc.h"
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#include <CoreServices/CoreServices.h>
#include <unistd.h>
#include <AudioUnit/AudioUnit.h>

static void *ca_handle;
AudioUnit gOutputUnit;

static const ALCchar ca_device[] = "CoreAudio Software";
static volatile ALuint load_count = 0;

void *ca_load(void)
{
    if(load_count == 0)
    {
        ca_handle = (void*)0xDEADBEEF;
    }
    ++load_count;

    return ca_handle;
}

void ca_unload(void)
{
    if(load_count == 0 || --load_count > 0)
        return;

    ca_handle = NULL;
}

static int ca_callback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,
                       UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
{
  static UInt32 channelCount = 0; // ***** GH
  static UInt32 firstDataByteSize = 0;

    UInt32 channel;
    ALCdevice *device = (ALCdevice*)inRefCon;
    
    // ***** GH
    if (channelCount == 0)
      {
	channelCount = ioData->mNumberBuffers;
	firstDataByteSize = ioData->mBuffers[0].mDataByteSize;
	printf("***** GH ***** ca_callback channels = %d, dbs = %d\n", channelCount, firstDataByteSize);
      }

    /*for (channel = 0; channel < ioData->mNumberBuffers; channel++)
    {
      memset(ioData->mBuffers[channel].mData, 0, ioData->mBuffers[channel].mDataByteSize);
    }*/
    
    aluMixData(device, ioData->mBuffers[0].mData, ioData->mBuffers[0].mDataByteSize / 4);
    return noErr;
}

static ALCboolean ca_open_playback(ALCdevice *device, const ALCchar *deviceName)
{
    OSStatus err = noErr;
    ComponentDescription desc;
    Component comp;
    AURenderCallbackStruct input;
    AudioStreamBasicDescription streamFormat;
    Float64 outSampleRate;
    UInt32 size;

    printf("***** GH ***** Initializing CoreAudio -- ca_open_playback.\n");

    if(!deviceName)
        deviceName = ca_device;
    else if(strcmp(deviceName, ca_device) != 0)
        return ALC_FALSE;

    if(!ca_load())
        return ALC_FALSE;

    err = noErr;

    // open the default output unit
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    comp = FindNextComponent(NULL, &desc);
    if (comp == NULL) {
        return ALC_FALSE;
    }

    err = OpenAComponent(comp, &gOutputUnit);
    if (comp == NULL) {
        return ALC_FALSE;
    }

    // setup callback
    input.inputProc = ca_callback;
    input.inputProcRefCon = device;

    err = AudioUnitSetProperty (gOutputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &input, sizeof(input));
    if (err) {
      return ALC_FALSE;
    }
	
	printf("***** GH ***** device->Format = %d\n", device->Format);
	
	streamFormat.mFramesPerPacket = 1;
	streamFormat.mChannelsPerFrame = aluChannelsFromFormat(device->Format);
	switch(aluBytesFromFormat(device->Format))
    {
        case 2:
            streamFormat.mBitsPerChannel = 16;
			streamFormat.mBytesPerPacket = 2 * streamFormat.mChannelsPerFrame;
			streamFormat.mBytesPerFrame = 2 * streamFormat.mChannelsPerFrame;
            break;
        case 4:
            streamFormat.mBitsPerChannel = 32;
			streamFormat.mBytesPerPacket = 4 * streamFormat.mChannelsPerFrame;
			streamFormat.mBytesPerFrame = 4 * streamFormat.mChannelsPerFrame;		
            break;
        default:
            AL_PRINT("Unknown format: 0x%x\n", device->Format);
            return ALC_FALSE;
    }
    streamFormat.mSampleRate = (float)device->Frequency;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger |
                                kAudioFormatFlagsNativeEndian |
                                kLinearPCMFormatFlagIsPacked;
								
	SetDefaultChannelOrder(device);

    err = AudioUnitSetProperty(gOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &streamFormat, sizeof(AudioStreamBasicDescription));
    if (err) {
        return ALC_FALSE;
    }

    err = AudioUnitInitialize(gOutputUnit);
    if (err) {
        return ALC_FALSE;
    }

    size = sizeof(Float64);
    err = AudioUnitGetProperty(gOutputUnit, kAudioUnitProperty_SampleRate, kAudioUnitScope_Output, 0, &outSampleRate, &size);
    if (err) {
        return ALC_FALSE;
    }

    err = AudioOutputUnitStart(gOutputUnit);
    if (err) {
        return ALC_FALSE;
    }

    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 2, false);

    printf("***** GH ***** ca_open_playback complete\n");
    return ALC_TRUE;
}

static void ca_close_playback(ALCdevice *device)
{
    printf("***** GH ***** Killing CoreAudio -- ca_close_playback.\n");
    CloseComponent(gOutputUnit);
}

static ALCboolean ca_reset_playback(ALCdevice *device)
{
    return ALC_TRUE;
}

static void ca_stop_playback(ALCdevice *device)
{
    OSStatus err = noErr;

    AudioOutputUnitStop(gOutputUnit);
    err = AudioUnitUninitialize(gOutputUnit);
    if (err) {
        printf("***** GH ***** AudioUnitUninitialize failed.\n");
    } else {
	printf("***** GH ***** AudioUnitUninitialize succeeded.\n");
    }
}

static ALCboolean ca_open_capture(ALCdevice *device, const ALCchar *deviceName)
{
    return ALC_FALSE;
    (void)device;
    (void)deviceName;
}

static const BackendFuncs ca_funcs = {
    ca_open_playback,
    ca_close_playback,
    ca_reset_playback,
    ca_stop_playback,
    ca_open_capture,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

void alc_ca_init(BackendFuncs *func_list)
{
  printf("***** GH ***** Initializing coreaudio backend with alc_ca_init!\n");
    *func_list = ca_funcs;
}

void alc_ca_deinit(void)
{
}

void alc_ca_probe(int type)
{
    if(!ca_load()) return;

    if(type == DEVICE_PROBE)
        AppendDeviceList(ca_device);
    else if(type == ALL_DEVICE_PROBE)
        AppendAllDeviceList(ca_device);

}
