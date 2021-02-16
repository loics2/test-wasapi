#pragma once
#include "RingBuffer.h"

namespace h2i {

    class AudioStream
    {
    public:
        AudioStream();
        ~AudioStream();

        winrt::Windows::Foundation::IAsyncAction SetInput(const winrt::hstring& deviceId);
        winrt::Windows::Foundation::IAsyncAction SetOutput(const winrt::hstring& deviceId);

        void Start();
        void Stop();

        std::function<void(const float*, float*, int, int, int)> OnData;

    private:

        std::thread m_worker;
        bool m_isRunning{ false };

        winrt::handle m_inputReadyEvent;
        winrt::handle m_outputReadyEvent;

        winrt::com_ptr<IAudioClient3> m_inputClient;
        winrt::com_ptr<IAudioClient3> m_outputClient;

        winrt::com_ptr<IAudioCaptureClient> m_audioCaptureClient;
        winrt::com_ptr<IAudioRenderClient> m_audioRenderClient;

        void StreamWorker();
    };

}

