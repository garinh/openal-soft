
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AL/alc.h"
#include "AL/al.h"
#include "AL/alext.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef ALC_EXT_EFX
#define AL_FILTER_TYPE                                     0x8001
#define AL_EFFECT_TYPE                                     0x8001
#define AL_FILTER_NULL                                     0x0000
#define AL_FILTER_LOWPASS                                  0x0001
#define AL_FILTER_HIGHPASS                                 0x0002
#define AL_FILTER_BANDPASS                                 0x0003
#define AL_EFFECT_NULL                                     0x0000
#define AL_EFFECT_EAXREVERB                                0x8000
#define AL_EFFECT_REVERB                                   0x0001
#define AL_EFFECT_CHORUS                                   0x0002
#define AL_EFFECT_DISTORTION                               0x0003
#define AL_EFFECT_ECHO                                     0x0004
#define AL_EFFECT_FLANGER                                  0x0005
#define AL_EFFECT_FREQUENCY_SHIFTER                        0x0006
#define AL_EFFECT_VOCAL_MORPHER                            0x0007
#define AL_EFFECT_PITCH_SHIFTER                            0x0008
#define AL_EFFECT_RING_MODULATOR                           0x0009
#define AL_EFFECT_AUTOWAH                                  0x000A
#define AL_EFFECT_COMPRESSOR                               0x000B
#define AL_EFFECT_EQUALIZER                                0x000C
#define ALC_EFX_MAJOR_VERSION                              0x20001
#define ALC_EFX_MINOR_VERSION                              0x20002
#define ALC_MAX_AUXILIARY_SENDS                            0x20003
#endif
ALvoid (AL_APIENTRY *p_alGenFilters)(ALsizei,ALuint*);
ALvoid (AL_APIENTRY *p_alDeleteFilters)(ALsizei,ALuint*);
ALvoid (AL_APIENTRY *p_alFilteri)(ALuint,ALenum,ALint);
ALvoid (AL_APIENTRY *p_alGenEffects)(ALsizei,ALuint*);
ALvoid (AL_APIENTRY *p_alDeleteEffects)(ALsizei,ALuint*);
ALvoid (AL_APIENTRY *p_alEffecti)(ALuint,ALenum,ALint);

static const int indentation = 4;
static const int maxmimumWidth = 79;

static void printChar(int c, int *width)
{
    putchar(c);
    *width = ((c == '\n') ? 0 : ((*width) + 1));
}

static void indent(int *width)
{
    int i;
    for(i = 0; i < indentation; i++)
        printChar(' ', width);
}

static void printList(const char *header, char separator, const char *list)
{
    int width = 0, start = 0, end = 0;

    printf("%s:\n", header);
    if(list == NULL || list[0] == '\0')
        return;

    indent(&width);
    while(1)
    {
        if(list[end] == separator || list[end] == '\0')
        {
            if(width + end - start + 2 > maxmimumWidth)
            {
                printChar('\n', &width);
                indent(&width);
            }
            while(start < end)
            {
                printChar(list[start], &width);
                start++;
            }
            if(list[end] == '\0')
                break;
            start++;
            end++;
            if(list[end] == '\0')
                break;
            printChar(',', &width);
            printChar(' ', &width);
        }
        end++;
    }
    printChar('\n', &width);
}

static void die(const char *kind, const char *description)
{
    fprintf(stderr, "%s error %s occured\n", kind, description);
    exit(EXIT_FAILURE);
}

static void checkForErrors(void)
{
    {
        ALCdevice *device = alcGetContextsDevice(alcGetCurrentContext());
        ALCenum error = alcGetError(device);
        if(error != ALC_NO_ERROR)
            die("ALC", (const char*)alcGetString(device, error));
    }
    {
        ALenum error = alGetError();
        if(error != AL_NO_ERROR)
            die("AL", (const char*)alGetString(error));
    }
}

static void printDevices(ALCenum which, const char *kind)
{
    const char *s = alcGetString(NULL, which);
    printf("Available %s devices:\n", kind);
    if(s == NULL || *s == '\0')
        printf("    (none!)\n");
    else do {
        printf("    %s\n", s);
        while(*s++ != '\0')
            ;
    } while(*s != '\0');
}

static void printALCInfo (void)
{
    ALCint major, minor;
    ALCdevice *device;

    printf("Default device: %s\n",
           alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER));
    printf("Default capture device: %s\n",
           alcGetString(NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER));

    device = alcGetContextsDevice(alcGetCurrentContext());
    checkForErrors();

    alcGetIntegerv(device, ALC_MAJOR_VERSION, 1, &major);
    alcGetIntegerv(device, ALC_MINOR_VERSION, 1, &minor);
    checkForErrors();
    printf("ALC version: %d.%d\n", (int)major, (int)minor);

    printList("ALC extensions", ' ', alcGetString(device, ALC_EXTENSIONS));
    checkForErrors();
}

static void printALInfo(void)
{
    printf("OpenAL vendor string: %s\n", alGetString(AL_VENDOR));
    printf("OpenAL renderer string: %s\n", alGetString(AL_RENDERER));
    printf("OpenAL version string: %s\n", alGetString(AL_VERSION));
    printList("OpenAL extensions", ' ', alGetString(AL_EXTENSIONS));
    checkForErrors();
}

static void printEFXInfo(void)
{
    ALCint major, minor, sends;
    ALCdevice *device;
    ALuint obj;
    int i;
    const ALenum effects[] = {
        AL_EFFECT_EAXREVERB, AL_EFFECT_REVERB, AL_EFFECT_CHORUS,
        AL_EFFECT_DISTORTION, AL_EFFECT_ECHO, AL_EFFECT_FLANGER,
        AL_EFFECT_FREQUENCY_SHIFTER, AL_EFFECT_VOCAL_MORPHER,
        AL_EFFECT_PITCH_SHIFTER, AL_EFFECT_RING_MODULATOR, AL_EFFECT_AUTOWAH,
        AL_EFFECT_COMPRESSOR, AL_EFFECT_EQUALIZER, AL_EFFECT_NULL
    };
    char effectNames[] = "EAX Reverb,Reverb,Chorus,Distortion,Echo,Flanger,"
                         "Frequency Shifter,Vocal Morpher,Pitch Shifter,"
                         "Ring Modulator,Autowah,Compressor,Equalizer,";
    const ALenum filters[] = {
        AL_FILTER_LOWPASS, AL_FILTER_HIGHPASS, AL_FILTER_BANDPASS,
        AL_FILTER_NULL
    };
    char filterNames[] = "Low-pass,High-pass,Band-pass,";
    char *current;

    device = alcGetContextsDevice(alcGetCurrentContext());
    if(alcIsExtensionPresent(device, (const ALCchar*)"ALC_EXT_EFX") == AL_FALSE)
    {
        printf("EFX not available\n");
        return;
    }

    alcGetIntegerv(device, ALC_EFX_MAJOR_VERSION, 1, &major);
    alcGetIntegerv(device, ALC_EFX_MINOR_VERSION, 1, &minor);
    checkForErrors();
    printf("EFX version: %d.%d\n", (int)major, (int)minor);

    alcGetIntegerv(device, ALC_MAX_AUXILIARY_SENDS, 1, &sends);
    checkForErrors();
    printf("Max auxiliary sends: %d\n", (int)sends);

    p_alGenFilters = alGetProcAddress("alGenFilters");
    p_alDeleteFilters = alGetProcAddress("alDeleteFilters");
    p_alFilteri = alGetProcAddress("alFilteri");
    p_alGenEffects = alGetProcAddress("alGenEffects");
    p_alDeleteEffects = alGetProcAddress("alDeleteEffects");
    p_alEffecti = alGetProcAddress("alEffecti");
    checkForErrors();
    if(!p_alGenEffects || !p_alDeleteEffects || !p_alEffecti ||
       !p_alGenFilters || !p_alDeleteFilters || !p_alFilteri)
    {
        printf("Missing EFX functions!\n");
        return;
    }

    p_alGenFilters(1, &obj);
    checkForErrors();
    current = filterNames;
    for(i = 0;filters[i] != AL_FILTER_NULL;i++)
    {
        char *next = strchr(current, ',');

        p_alFilteri(obj, AL_FILTER_TYPE, filters[i]);
        if(alGetError() == AL_NO_ERROR)
            current = next+1;
        else
            memmove(current, next+1, strlen(next));
    }
    p_alDeleteFilters(1, &obj);
    checkForErrors();
    printList("Supported filters", ',', filterNames);

    p_alGenEffects(1, &obj);
    checkForErrors();
    current = effectNames;
    for(i = 0;effects[i] != AL_EFFECT_NULL;i++)
    {
        char *next = strchr(current, ',');

        p_alEffecti(obj, AL_EFFECT_TYPE, effects[i]);
        if(alGetError() == AL_NO_ERROR)
            current = next+1;
        else
            memmove(current, next+1, strlen(next));
    }
    p_alDeleteEffects(1, &obj);
    checkForErrors();
    printList("Supported effects", ',', effectNames);
}

int main()
{
    ALCdevice *device;
    ALCcontext *context;
    struct stat statInfo;
    ALuint buffer;
    ALuint source;
    ALuint state;
    FILE *file;
    char *memBuffer;
    int readMem;

    if(alcIsExtensionPresent(NULL, (const ALCchar*)"ALC_ENUMERATION_EXT") == AL_TRUE)
    {
        if(alcIsExtensionPresent(NULL, (const ALCchar*)"ALC_ENUMERATE_ALL_EXT") == AL_TRUE)
            printDevices(ALC_ALL_DEVICES_SPECIFIER, "playback");
        else
            printDevices(ALC_DEVICE_SPECIFIER, "playback");
        printDevices(ALC_CAPTURE_DEVICE_SPECIFIER, "capture");
    }
    else
        printf("No device enumeration available\n");

    device = alcOpenDevice(NULL);
    if(!device)
    {
        printf("Failed to open a device!\n");
        exit(EXIT_FAILURE);
    }
    context = alcCreateContext(device, NULL);
    if(!context || alcMakeContextCurrent(context) == ALC_FALSE)
    {
        printf("Failed to set a context!\n");
        exit(EXIT_FAILURE);
    }

    /*
    printALCInfo();
    printALInfo();
    printEFXInfo();
    checkForErrors();
    */

    // test for AL_EXT_MCFORMATS
    if (alIsExtensionPresent("AL_EXT_MCFORMATS") == AL_TRUE) {
	printf("Found MCFORMATS extension.\n");
    } else {
	printf("MCFORMATS extension not found.  Exiting.\n");
	exit(0);
    }

    stat("infile.raw", &statInfo);
    readMem = (int)(statInfo.st_size / 2688) * 2688; // make sure all our formats are happy with the length...

    printf("Input file is %d bytes long.\n", readMem);
    memBuffer = malloc(readMem);
    if (memBuffer == 0) {
      printf("malloc failed!\n");
      exit(-1);
    }

    printf("Reading file into membuffer...\n");
    file = fopen("quad_test.raw", "r");
    if (file == NULL) {
      printf("fopen failed!\n");
      exit(-1);
    }
    fread(memBuffer, readMem, 1, file);
    fclose(file);

    printf("Generating buffer and source...\n");
    alGenBuffers(1, &buffer);
    checkForErrors();
    alGenSources(1, &source);
    checkForErrors();

    alBufferData(buffer, AL_FORMAT_QUAD16, memBuffer, readMem, 48000);
    checkForErrors();

    alSourcei(source, AL_BUFFER, buffer);
    checkForErrors();

    printf("Starting playback...\n");
    alSourcePlay(source);

    printf("Waiting");
    state = AL_PLAYING;
    while (state != AL_STOPPED) {
      alGetSourcei(source, AL_SOURCE_STATE, &state);
      sleep(1);
      printf(".");
    }
    printf("\n");
    
    printf("Killing everything...\n");
    free(memBuffer);

    alcMakeContextCurrent(NULL);
    alcDestroyContext(context);
    alcCloseDevice(device);

    return EXIT_SUCCESS;
}
