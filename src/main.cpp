#include "logging.h"
#include "miniaudio.h"

void mini_audio_test(){
    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        // Error.
    }

    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    ma_device_info* pCaptureInfos;
    ma_uint32 captureCount;
    if (ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount) != MA_SUCCESS) {
        // Error.
    }

    // Loop over each device info and do something with it. Here we just print the name with their index. You may want
    // to give the user the opportunity to choose which device they'd prefer.
    for (ma_uint32 iDevice = 0; iDevice < playbackCount; iDevice += 1) {
        ilog("%d - %s", iDevice, pPlaybackInfos[iDevice].name);
    }

    // ma_device_config config = ma_device_config_init(ma_device_type_playback);
    // config.playback.pDeviceID = &pPlaybackInfos[chosenPlaybackDeviceIndex].id;
    // config.playback.format    = MY_FORMAT;
    // config.playback.channels  = MY_CHANNEL_COUNT;
    // config.sampleRate         = MY_SAMPLE_RATE;
    // config.dataCallback       = data_callback;
    // config.pUserData          = pMyCustomData;

    // ma_device device;
    // if (ma_device_init(&context, &config, &device) != MA_SUCCESS) {
    //     // Error
    // }

    // ...
    //ma_device_uninit(&device);
    ma_context_uninit(&context);    
}

int main(int argc, char **argv)
{
    mini_audio_test();
    //cli_example_main(argc, argv);
}
