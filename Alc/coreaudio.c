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

#define CA_VERBOSE 1 // toggle verbose tty output among CoreAudio code

static AudioUnit gOutputUnit;

static const ALCchar ca_device[] = "CoreAudio Software";

static int ca_callback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,
                       UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
{
    ALCdevice *device = (ALCdevice*)inRefCon;
    
    aluMixData(device, ioData->mBuffers[0].mData, ioData->mBuffers[0].mDataByteSize / (2 * aluChannelsFromFormat(device->Format)));
    return noErr;
}

static ALCboolean ca_open_playback(ALCdevice *device, const ALCchar *deviceName)
{
    OSStatus err = noErr;
    ComponentDescription desc;
    Component comp;

#if CA_VERBOSE
	printf("CA: ca_open_playback\n");
#endif

    if(!deviceName)
        deviceName = ca_device;
    else if(strcmp(deviceName, ca_device) != 0)
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
	
	return ALC_TRUE;
}

static void ca_close_playback(ALCdevice *device)
{
#if CA_VERBOSE
	printf("CA: ca_close_playback\n");
#endif
	
    CloseComponent(gOutputUnit);
}

static ALCboolean ca_reset_playback(ALCdevice *device)
{
	AURenderCallbackStruct input;
    AudioStreamBasicDescription streamFormat;
    UInt32 size;
	OSStatus err = noErr;
	
#if CA_VERBOSE
	printf("CA: ca_reset_playback\n");
#endif
	// int and start the default audio unit...
	err = AudioUnitInitialize(gOutputUnit);
    if (err) {
        return ALC_FALSE;
    }

    err = AudioOutputUnitStart(gOutputUnit);
    if (err) {
        return ALC_FALSE;
    }
	
		// retrieve default output unit's properties (output side)
	size = sizeof(AudioStreamBasicDescription);
	err = AudioUnitGetProperty (gOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &streamFormat, &size);
	if (err) {
#if CA_VERBOSE
      printf("CA: failed AudioUnitGetProperty\n");
#endif
      return ALC_FALSE;
    }
	
#if CA_VERBOSE
    printf("CA: Output streamFormat of default output unit -\n");
	printf("CA:   streamFormat.mFramesPerPacket = %d\n", streamFormat.mFramesPerPacket);
	printf("CA:   streamFormat.mChannelsPerFrame = %d\n", streamFormat.mChannelsPerFrame);
	printf("CA:   streamFormat.mBitsPerChannel = %d\n", streamFormat.mBitsPerChannel);
	printf("CA:   streamFormat.mBytesPerPacket = %d\n", streamFormat.mBytesPerPacket);
	printf("CA:   streamFormat.mBytesPerFrame = %d\n", streamFormat.mBytesPerFrame);
	printf("CA:   streamFormat.mSampleRate = %5.0f\n", streamFormat.mSampleRate);
#endif

	// check size
	assert(size == sizeof(AudioStreamBasicDescription));

    // set default output unit's input side to match output side
	err = AudioUnitSetProperty (gOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &streamFormat, size);
	if (err) {
#if CA_VERBOSE
      printf("CA: failed AudioUnitSetProperty\n");
#endif
      return ALC_FALSE;
    }
	
	// set AL device's format	
	switch (streamFormat.mChannelsPerFrame)
	{
		case 1:
			device->Format = AL_FORMAT_MONO16;
		    break;
		case 2:
			device->Format = AL_FORMAT_STEREO16;
		    break;
		case 4:
			device->Format = AL_FORMAT_QUAD16;
		    break;
		case 6:
		    device->Format = AL_FORMAT_51CHN16;
		    break;
		case 7:
		    device->Format = AL_FORMAT_61CHN16;
		    break;
		case 8:
			device->Format = AL_FORMAT_71CHN16;
		    break;
	}

	// set AL device's sample rate
	device->Frequency = (ALCuint)streamFormat.mSampleRate;
	
	// set AL device's headdampen
	device->HeadDampen = 0.0f;
	
	// set AL device's channel order
	SetDefaultChannelOrder(device); // use OpenAL-Soft default
	switch(aluChannelsFromFormat(device->Format)) // for 5.1, switch to SMTE standard; for 7.1, switch to WFE
    {
		case 6: device->DevChannels[0] = FRONT_LEFT;
				device->DevChannels[1] = FRONT_RIGHT;
				device->DevChannels[2] = FRONT_CENTER;
				device->DevChannels[3] = LFE;
				device->DevChannels[4] = BACK_LEFT;
				device->DevChannels[5] = BACK_RIGHT; break;
				
		case 8: device->DevChannels[0] = FRONT_LEFT;
				device->DevChannels[1] = FRONT_RIGHT;
				device->DevChannels[2] = FRONT_CENTER;
				device->DevChannels[3] = LFE;
				device->DevChannels[4] = BACK_LEFT;
				device->DevChannels[5] = BACK_RIGHT;
				device->DevChannels[6] = SIDE_LEFT;
				device->DevChannels[7] = SIDE_RIGHT; break;
    }
	
	// use channel count and sample rate from the default output unit's current parameters, but reset everything else
	streamFormat.mFramesPerPacket = 1;
	switch(aluBytesFromFormat(device->Format))
    {
		case 1:
			streamFormat.mBitsPerChannel = 8;
			streamFormat.mBytesPerPacket = streamFormat.mChannelsPerFrame;
			streamFormat.mBytesPerFrame = streamFormat.mChannelsPerFrame;
            break;
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
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger |
                                kAudioFormatFlagsNativeEndian |
                                kLinearPCMFormatFlagIsPacked;								
    err = AudioUnitSetProperty(gOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &streamFormat, sizeof(AudioStreamBasicDescription));
    if (err) {
        return ALC_FALSE;
    }
	
	// setup callback
    input.inputProc = ca_callback;
    input.inputProcRefCon = device;

    err = AudioUnitSetProperty (gOutputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &input, sizeof(AURenderCallbackStruct));
    if (err) {
      return ALC_FALSE;
    }

#if CA_VERBOSE
	printf("CA: ca_open_playback -- exit\n");
#endif
    return ALC_TRUE;
}

static void ca_stop_playback(ALCdevice *device)
{
    OSStatus err = noErr;

#if CA_VERBOSE
	printf("CA: ca_stop_playback\n");
#endif

	AudioOutputUnitStop(gOutputUnit);
    err = AudioUnitUninitialize(gOutputUnit);
#if CA_VERBOSE
    if (err) {
        printf("CA: ca_stop_playback -- AudioUnitUninitialize failed.\n");
    } else {
	printf("CA: ca_stop_playback -- AudioUnitUninitialize succeeded.\n");
    }
#endif
}

static ALCboolean ca_open_capture(ALCdevice *device, const ALCchar *deviceName)
{
#if CA_VERBOSE
	printf("CA: ca_open_capture\n");
#endif
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
#if CA_VERBOSE
	printf("CA: alc_ca_init\n");
#endif
    *func_list = ca_funcs;
}

void alc_ca_deinit(void)
{
#if CA_VERBOSE
	printf("CA: alc_ca_deinit\n");
#endif
}

void alc_ca_probe(int type)
{
#if CA_VERBOSE
	printf("CA: alc_ca_probe\n");
#endif
    if(type == DEVICE_PROBE)
        AppendDeviceList(ca_device);
    else if(type == ALL_DEVICE_PROBE)
        AppendAllDeviceList(ca_device);

}
