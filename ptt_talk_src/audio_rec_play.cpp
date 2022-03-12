/** @audio_rec_play.cpp
    @brief Record input into an ogg file; Playback recorded data.
    @author APercy - adapted code from Phil Burk  http://www.softsynth.com
*/
/*
 * $Id$
 *
 * This program uses the PortAudio Portable Audio Library.
 * For more information see: http://www.portaudio.com
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The text above constitutes the entire PortAudio license; however,
 * the PortAudio community also makes the following non-binding requests:
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version. It is also
 * requested that these non-binding requests be included along with the
 * license above.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sndfile.hh>
#include <iostream>
#include "portaudio.h"
#include "ptt_talk.h"
#include "audio_rec_play.h"


int done(paTestData * data, PaError err) {
    Pa_Terminate();
    if( data->recordedSamples )       /* Sure it is NULL or valid. */
        free( data->recordedSamples );
    if( err != paNoError )
    {
        fprintf( stderr, "An error occurred while using the portaudio stream\n" );
        fprintf( stderr, "Error number: %d\n", err );
        fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
        err = 1;          /* Always return 0 or 1, but no other return codes. */
    }
    return err;
}

static void encodeOgg (SAMPLE * recordedSamples, int size )
{

    SNDFILE        *infile, *outfile;
    SF_INFO        sfinfo,sf_in;
    int            readcount;

    //converting to mono
    SAMPLE * audioOut = new SAMPLE[size/2];
    for(int i = 0; i < size/2; i++)
    {
        audioOut[i] = 0;
        for(int j = 0; j < NUM_CHANNELS; j++)
            audioOut[i] += recordedSamples[(i*NUM_CHANNELS) + j];
        audioOut[i] /= NUM_CHANNELS;
    }
    //ending conversion

    fflush (stdout);
    sfinfo.samplerate=SAMPLE_RATE;
    sfinfo.channels=1;
    sfinfo.format = SF_FORMAT_OGG | SF_FORMAT_VORBIS;

    if (! (outfile = sf_open ("recorded.ogg", SFM_WRITE, &sfinfo))){
        std::cout << "Error : could not open output file" << std::endl;
        exit (1);
    }

    //sf_write_short (outfile, recordedSamples, size);
    sf_write_float(outfile, audioOut, size/2);

    sf_close (outfile);
    return;
}

/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int recordCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    paTestData *data = (paTestData*)userData;
    const SAMPLE *rptr = (const SAMPLE*)inputBuffer;
    SAMPLE *wptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
    long framesToCalc;
    long i;
    int finished;
    unsigned long framesLeft = data->maxFrameIndex - data->frameIndex;

    (void) outputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    if( framesLeft < framesPerBuffer )
    {
        framesToCalc = framesLeft;
        finished = paComplete;
    }
    else
    {
        framesToCalc = framesPerBuffer;
        finished = paContinue;
    }

    if( inputBuffer == NULL )
    {
        for( i=0; i<framesToCalc; i++ )
        {
            *wptr++ = SAMPLE_SILENCE;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = SAMPLE_SILENCE;  /* right */
        }
    }
    else
    {
        for( i=0; i<framesToCalc; i++ )
        {
            *wptr++ = *rptr++;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = *rptr++;  /* right */
        }
    }
    data->frameIndex += framesToCalc;
    return finished;
}

/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int playCallback( const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData )
{
    paTestData *data = (paTestData*)userData;
    SAMPLE *rptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
    SAMPLE *wptr = (SAMPLE*)outputBuffer;
    unsigned int i;
    int finished;
    unsigned int framesLeft = data->maxFrameIndex - data->frameIndex;

    (void) inputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    if( framesLeft < framesPerBuffer )
    {
        /* final buffer... */
        for( i=0; i<framesLeft; i++ )
        {
            *wptr++ = *rptr++;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = *rptr++;  /* right */
        }
        for( ; i<framesPerBuffer; i++ )
        {
            *wptr++ = 0;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = 0;  /* right */
        }
        data->frameIndex += framesLeft;
        finished = paComplete;
    }
    else
    {
        for( i=0; i<framesPerBuffer; i++ )
        {
            *wptr++ = *rptr++;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = *rptr++;  /* right */
        }
        data->frameIndex += framesPerBuffer;
        finished = paContinue;
    }
    return finished;
}

int recordAudio(PaStreamParameters  inputParameters,
                    PaStream*           stream,
                    PaError             err,
                    paTestData          data,
                    int                 numSamples,
                    bool*               should_stop,
                    bool*               is_recording) {


    SAMPLE              max, val;
    double              average;

    err = Pa_OpenStream(
              &stream,
              &inputParameters,
              NULL,                  /* &outputParameters, */
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              recordCallback,
              &data );
    if( err != paNoError ) return done(&data, err);

    err = Pa_StartStream( stream );
    if( err != paNoError ) return done(&data, err);
    printf("\n=== Now recording!! Please speak into the microphone. ===\n"); fflush(stdout);

    while( ( err = Pa_IsStreamActive( stream ) ) == 1 )
    {
        Pa_Sleep(500);
        printf("index = %d\n", data.frameIndex ); fflush(stdout);
        if(*should_stop == true) {
            err = Pa_StopStream( stream );
            printf("stopped\n" );
            if( err != paNoError ) return done( &data, err );
        }
    }
    if( err < 0 ) return done(&data, err);

    err = Pa_CloseStream( stream );
    if( err != paNoError ) return done(&data, err);

    /* Measure maximum peak amplitude. */
    /*max = 0;
    average = 0.0;
    for( int i=0; i<numSamples; i++ )
    {
        val = data.recordedSamples[i];
        if( val < 0 ) val = -val; // ABS
        if( val > max )
        {
            max = val;
        }
        average += val;
    }

    average = average / (double)numSamples;

    printf("sample max amplitude = "PRINTF_S_FORMAT"\n", max );
    printf("sample average = %lf\n", average );*/

    encodeOgg (data.recordedSamples, (data.frameIndex*NUM_CHANNELS) );

    sendMessage("recorded.ogg");

    *is_recording = false;

    return 0;
}

int playbackAudio(  PaStreamParameters  outputParameters,
                    PaStream*           stream,
                    PaError             err,
                    paTestData          data) {
    data.frameIndex = 0;

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default output device.\n");
        return done(&data, err);
    }
    outputParameters.channelCount = 2;                     /* stereo output */
    outputParameters.sampleFormat =  PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    printf("\n=== Now playing back. ===\n"); fflush(stdout);
    err = Pa_OpenStream(
              &stream,
              NULL, /* no input */
              &outputParameters,
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              playCallback,
              &data );
    if( err != paNoError ) return done(&data, err);

    if( stream )
    {
        err = Pa_StartStream( stream );
        if( err != paNoError ) return done(&data, err);

        printf("Waiting for playback to finish.\n"); fflush(stdout);

        while( ( err = Pa_IsStreamActive( stream ) ) == 1 ) Pa_Sleep(100);
        if( err < 0 ) return done(&data, err);

        err = Pa_CloseStream( stream );
        if( err != paNoError ) return done(&data, err);

        printf("Done.\n"); fflush(stdout);
    }
    return 0;
}

int initialize(PaStreamParameters*  inputParameters,
                    PaStream*           stream,
                    PaError             err,
                    paTestData*         data,
                    int*                numSamples,
                    int*                numBytes,
                    int*                totalFrames,
                    bool*               should_stop) {
    *totalFrames = NUM_SECONDS * SAMPLE_RATE; /* Record for a few seconds. */
    data->maxFrameIndex = *totalFrames;
    data->frameIndex = 0;
    *numSamples = (*totalFrames) * NUM_CHANNELS;
    *numBytes = (*numSamples) * sizeof(SAMPLE);
    data->recordedSamples = (SAMPLE *) malloc( *numBytes ); /* From now on, recordedSamples is initialised. */
    if( data->recordedSamples == NULL )
    {
        printf("Could not allocate record array.\n");
        return done(data, err);
    }
    for( int i=0; i<*numSamples; i++ ) data->recordedSamples[i] = 0;

    err = Pa_Initialize();
    if( err != paNoError ){
        return done(data, err);
    }

    inputParameters->device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters->device == paNoDevice) {
        fprintf(stderr,"Error: No default input device.\n");
        return done(data, err);
    }
    inputParameters->channelCount = 2;                    /* stereo input */
    inputParameters->sampleFormat = PA_SAMPLE_TYPE;
    inputParameters->suggestedLatency = Pa_GetDeviceInfo( inputParameters->device )->defaultLowInputLatency;
    inputParameters->hostApiSpecificStreamInfo = NULL;
    return 0;
}
