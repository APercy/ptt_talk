

int done(paTestData * data, PaError err);

static void encodeOgg (SAMPLE * recordedSamples, int size );

static int recordCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData );

static int playCallback( const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData );

int recordAudio(PaStreamParameters  inputParameters,
                    PaStream*           stream,
                    PaError             err,
                    paTestData          data,
                    int                 numSamples,
                    bool*               should_stop,
                    bool*               is_recording);

int playbackAudio(  PaStreamParameters  outputParameters,
                    PaStream*           stream,
                    PaError             err,
                    paTestData          data);

int initialize(PaStreamParameters*  inputParameters,
                    PaStream*           stream,
                    PaError             err,
                    paTestData*         data,
                    int*                numSamples,
                    int*                numBytes,
                    int*                totalFrames,
                    bool*               should_stop);


