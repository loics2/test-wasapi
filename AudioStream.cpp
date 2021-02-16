#include "pch.h"

#include "AudioStream.h"
#include "AudioInterfaceActivator.h"

#include <cmath>
#include <pplawait.h>


namespace h2i {
    typedef HANDLE(__stdcall* TAvSetMmThreadCharacteristicsPtr)(LPCWSTR TaskName, LPDWORD TaskIndex);

    AudioStream::AudioStream()
    {
        // initialize Media Foundation
        winrt::check_hresult(MFStartup(MF_VERSION));

        m_inputReadyEvent = winrt::handle{ ::CreateEventExA(nullptr, nullptr, 0, EVENT_ALL_ACCESS) };
        winrt::check_bool(bool{ m_inputReadyEvent });

        m_outputReadyEvent = winrt::handle{ ::CreateEventExA(nullptr, nullptr, 0, EVENT_ALL_ACCESS) };
        winrt::check_bool(bool{ m_outputReadyEvent });
    }

    AudioStream::~AudioStream() {
        // shutdown Media Foundation
        MFShutdown();
    }

    winrt::Windows::Foundation::IAsyncAction AudioStream::SetInput(const winrt::hstring& deviceId)
    {
        m_inputClient = co_await AudioInterfaceActivator::ActivateAudioClientAsync(deviceId);

        AudioClientProperties audioProps = { 0 };
        audioProps.cbSize = sizeof(audioProps);
        audioProps.bIsOffload = false;
        audioProps.eCategory = AudioCategory_Media;
        audioProps.Options = AUDCLNT_STREAMOPTIONS_RAW;
        winrt::check_hresult(m_inputClient->SetClientProperties(&audioProps));

    }

    winrt::Windows::Foundation::IAsyncAction AudioStream::SetOutput(const winrt::hstring& deviceId)
    {
        m_outputClient = co_await AudioInterfaceActivator::ActivateAudioClientAsync(deviceId);
        AudioClientProperties audioProps = { 0 };
        audioProps.cbSize = sizeof(audioProps);
        audioProps.bIsOffload = false;
        audioProps.eCategory = AudioCategory_Media;
        audioProps.Options = AUDCLNT_STREAMOPTIONS_RAW;
        winrt::check_hresult(m_outputClient->SetClientProperties(&audioProps));
    }

    void AudioStream::Start()
    {
        m_isRunning = true;
        m_worker = std::thread(&AudioStream::StreamWorker, this);
    }

    void AudioStream::Stop()
    {
        m_isRunning = false;
        m_worker.join();
    }

    void AudioStream::StreamWorker()
    {
        WAVEFORMATEX* captureFormat = nullptr;
        WAVEFORMATEX* renderFormat = nullptr;

        RingBuffer<float> captureBuffer;
        RingBuffer<float> renderBuffer;

        BYTE* streamBuffer = nullptr;
        unsigned int streamBufferSize = 0;
        unsigned int bufferFrameCount = 0;
        unsigned int numFramesPadding = 0;
        unsigned int inputBufferSize = 0;
        unsigned int outputBufferSize = 0;
        DWORD captureFlags = 0;

        winrt::hresult hr = S_OK;

        if (m_inputClient) {

            hr = m_inputClient->GetMixFormat(&captureFormat);
            if (FAILED(hr)) {
                std::cerr << "Could not get the input mix format\n";
                goto exit;
            }

            // initializing IAudioCaptureClient
            if (!m_audioCaptureClient) {

                hr = m_inputClient->Initialize(
                    AUDCLNT_SHAREMODE_SHARED,
                    AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                    0, // TODO: pass buffer duration
                    0, // TODO: pass buffer duration
                    captureFormat,
                    nullptr);
                if (FAILED(hr)) {
                    std::cerr << "Could not initialize the input IAudioClient\n";
                    goto exit;
                }

                hr = m_inputClient->GetService(__uuidof(IAudioCaptureClient), m_audioCaptureClient.put_void());
                if (FAILED(hr)) {
                    std::cerr << "Could not get the IAudioCaptureClient\n";
                    goto exit;
                }

                hr = m_inputClient->SetEventHandle(m_inputReadyEvent.get());
                if (FAILED(hr)) {
                    std::cerr << "Could not set the inputReadyEventHandle\n";
                    goto exit;
                }

                hr = m_inputClient->Reset();
                if (FAILED(hr)) {
                    std::cerr << "Could not reset the input\n";
                    goto exit;
                }

                hr = m_inputClient->Start();
                if (FAILED(hr)) {
                    std::cerr << "Could not start the input\n";
                    goto exit;
                }
            }
        }

        hr = m_inputClient->GetBufferSize(&inputBufferSize);
        if (FAILED(hr)) {
            std::cerr << "Could not get the input buffer size\n";
            goto exit;
        }

        // multiplying the buffer size by the number of channels
        inputBufferSize *= 2;

        if (m_outputClient) {
            hr = m_outputClient->GetMixFormat(&renderFormat);
            if (FAILED(hr)) {
                std::cerr << "Could not get the output mix format\n";
                goto exit;
            }

            if (!m_audioRenderClient) {
                hr = m_outputClient->Initialize(
                    AUDCLNT_SHAREMODE_SHARED,
                    AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                    0, // TODO: pass buffer duration
                    0, // TODO: pass buffer duration
                    captureFormat,
                    nullptr);
                if (FAILED(hr)) {
                    std::cerr << "Could notinitialize the output IAudioClient\n";
                    goto exit;
                }

                hr = m_outputClient->GetService(__uuidof(IAudioRenderClient), m_audioRenderClient.put_void());
                if (FAILED(hr)) {
                    std::cerr << "Could not get the IAudioRenderClient\n";
                    goto exit;
                }

                hr = m_outputClient->SetEventHandle(m_outputReadyEvent.get());
                if (FAILED(hr)) {
                    std::cerr << "Could not set the  outputReadyEvent handle\n";
                    goto exit;
                }

                hr = m_outputClient->Reset();
                if (FAILED(hr)) {
                    std::cerr << "Could not reset the output\n";
                    goto exit;
                }

                hr = m_outputClient->Start();
                if (FAILED(hr)) {
                    std::cerr << "Could not start the output\n";
                    goto exit;
                }
            }
        }

        hr = m_outputClient->GetBufferSize(&outputBufferSize);
        if (FAILED(hr)) {
            std::cerr << "Could not get the output buffer size\n";
            goto exit;
        }


        // multiplying the buffer size by the number of channels
        outputBufferSize *= 2;

        std::cout << "buffer sizes: input = " << inputBufferSize << ", output = " << outputBufferSize << '\n';
        while (m_isRunning)
        {
            // ===== INPUT =====

            // waiting for the capture event
            WaitForSingleObject(m_inputReadyEvent.get(), INFINITE);

            // getting the input buffer data
            hr = m_audioCaptureClient->GetNextPacketSize(&bufferFrameCount);

            while (SUCCEEDED(hr) && bufferFrameCount > 0) {
                m_audioCaptureClient->GetBuffer(&streamBuffer, &bufferFrameCount, &captureFlags, nullptr, nullptr);
                if (bufferFrameCount != 0) {
                    captureBuffer.write(reinterpret_cast<float*>(streamBuffer), bufferFrameCount * 2);

                    hr = m_audioCaptureClient->ReleaseBuffer(bufferFrameCount);
                    if (FAILED(hr)) {
                        // inform WASAPI that capture was unsuccessfull
                        m_audioCaptureClient->ReleaseBuffer(0);
                        std::cerr << "Could not release the input buffer\n";
                    }
                }
                else
                {
                    // inform WASAPI that capture was unsuccessfull
                    m_audioCaptureClient->ReleaseBuffer(0);
                    std::cerr << "Could not release the input buffer\n";
                }

                hr = m_audioCaptureClient->GetNextPacketSize(&bufferFrameCount);
            }

            // ===== CALLBACK =====

            auto size = captureBuffer.size();
            float* userInputData = (float*)calloc(size, sizeof(float));
            float* userOutputData = (float*)calloc(size, sizeof(float));
            captureBuffer.read(userInputData, size);

            OnData(userInputData, userOutputData, size / 2, 2, 48000);

            renderBuffer.write(userOutputData, size);

            free(userInputData);
            free(userOutputData);

            // ===== OUTPUT =====

            // waiting for the render event
            WaitForSingleObject(m_outputReadyEvent.get(), INFINITE);

            // getting information about the output buffer
            hr = m_outputClient->GetBufferSize(&bufferFrameCount);
            if (FAILED(hr))
                goto exit;

            hr = m_outputClient->GetCurrentPadding(&numFramesPadding);
            if (FAILED(hr))
                goto exit;


            // adjust the frame count with the padding
            bufferFrameCount -= numFramesPadding;

            if (bufferFrameCount != 0) {
                hr = m_audioRenderClient->GetBuffer(bufferFrameCount, &streamBuffer);
                if (FAILED(hr))
                    goto exit;

                auto count = (bufferFrameCount * 2);
                if (renderBuffer.read(reinterpret_cast<float*>(streamBuffer), count) < count) {
                    std::cerr << "captureBuffer is not full enough, " << count << " samples needed\n";
                    // TODO: fill the remainder with 0
                }

                hr = m_audioRenderClient->ReleaseBuffer(bufferFrameCount, 0);
                if (FAILED(hr)) {
                    // inform WASAPI that capture was unsuccessfull
                    m_audioRenderClient->ReleaseBuffer(0, 0);
                    std::cerr << "Could not release the input buffer\n";
                }
            }
            else
            {
                // inform WASAPI that capture was unsuccessfull
                m_audioRenderClient->ReleaseBuffer(0, 0);
                std::cerr << "Could not release the input buffer\n";
            }
        }

    exit:
        CoTaskMemFree(captureFormat);
        CoTaskMemFree(renderFormat);

    }
}