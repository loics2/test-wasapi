#include "pch.h"
#include "AudioStream.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Devices::Usb;
using namespace Windows::Media::Devices;

using namespace h2i;

int main()
{
    init_apartment();

    auto outputDeviceId = MediaDevice::GetDefaultAudioRenderId(AudioDeviceRole::Default);
    auto inputDeviceId = MediaDevice::GetDefaultAudioCaptureId(AudioDeviceRole::Default);

    std::cout << "Using devices\n\tout: " << winrt::to_string(outputDeviceId)
        << "\n\tin: " << winrt::to_string(inputDeviceId) << std::endl;


    AudioStream stream;

    stream.SetInput(inputDeviceId).get();
    stream.SetOutput(outputDeviceId).get();

    stream.OnData = [](const float* input, float* output, int frameCount, int channelCount, int samplerate) {
        //std::cout << "callback for " << frameCount << " frames, " << channelCount << " channels\n";
        memcpy(output, input, frameCount * channelCount * sizeof(float));
    };

    stream.Start();
    std::this_thread::sleep_for(std::chrono::seconds(20));
    stream.Stop();
}


/*
outputDevice.OnOutput = [&](float* buffer, int frameCount, int channels, int samplerate) {
        double frequency = 100;
        double increment = frequency * (M_PI * 2) / (double)samplerate;

        for (int i = 0; i < frameCount * channels; i += channels) {
            double value = 0.3 * sin(angle);
            for (int j = 0; j < channels; j++) {
                buffer[i + j] = (float)value;
            }
            angle += increment;
        }
    };
*/