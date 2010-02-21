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

static void *ca_handle;

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

static int ca_callback(const void *inputBuffer, void *outputBuffer,
                       unsigned long framesPerBuffer, void *userData)
{
    ALCdevice *device = (ALCdevice*)userData;

    aluMixData(device, outputBuffer, framesPerBuffer);
    return 0;
}


static ALCboolean ca_open_playback(ALCdevice *device, const ALCchar *deviceName)
{
  printf("***** GH ***** Initializing CoreAudio -- ca_open_playback.\n");
    if(!deviceName)
        deviceName = ca_device;
    else if(strcmp(deviceName, ca_device) != 0)
        return ALC_FALSE;

    if(!ca_load())
        return ALC_FALSE;

    return ALC_TRUE;
}

static void ca_close_playback(ALCdevice *device)
{
printf("***** GH ***** Killing CoreAudio -- ca_close_playback.\n");
}

static ALCboolean ca_reset_playback(ALCdevice *device)
{
    return ALC_TRUE;
}

static void ca_stop_playback(ALCdevice *device)
{
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
