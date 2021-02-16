#include "pch.h"
#include "AudioOutput.h"

namespace h2i {

    AudioOutput::AudioOutput(const winrt::hstring& deviceId)
        : AudioDevice(deviceId)
        , m_renderCallback(winrt::make_self<AsyncCallback<AudioOutput>>(this, &AudioOutput::OnRenderCallback))
    {
        winrt::check_bool(InitializeCriticalSectionEx(&m_criticalSection, 0, 0));
    }

    AudioOutput::~AudioOutput()
    {
        Stop();

        DeleteCriticalSection(&m_criticalSection);
    }

    void AudioOutput::OnWasapiInitialized()
    {
        winrt::check_hresult(m_audioClient->GetService(__uuidof(IAudioRenderClient), m_audioRenderClient.put_void()));
    }

    void AudioOutput::OnMediaFoundationInitialized()
    {
        DWORD taskId = 0;
        winrt::check_hresult(MFLockSharedWorkQueue(L"Pro Audio", 0, &taskId, &m_workQueueId));

        m_renderCallback->SetQueueID(m_workQueueId);
        winrt::check_hresult(MFCreateAsyncResult(nullptr, m_renderCallback.get(), nullptr, m_callbackResult.put()));
    }

    HRESULT AudioOutput::OnRenderCallback(IMFAsyncResult*)
    {
        bool isPlaying = true;
        BYTE* data = nullptr;
        UINT32 padding;
        UINT availableFrames = 0;
        int bufferSize;
        int channels = this->m_mixFormat->nChannels;
        winrt::hresult hr = S_OK;

        EnterCriticalSection(&m_criticalSection);

        hr = m_audioClient->GetCurrentPadding(&padding);
        if (FAILED(hr))
        {
            goto exit;
        }


        availableFrames = m_bufferSize - padding;

        if (availableFrames > 0) {
            hr = m_audioRenderClient->GetBuffer(availableFrames, &data);
            if (FAILED(hr))
            {
                goto exit;
            }

            bufferSize = availableFrames * channels * (this->m_mixFormat->wBitsPerSample / 8);
            float* sampleBuffer = reinterpret_cast<float*>(data);

            auto count = availableFrames * channels;
            if (m_buffer.size() < count) {
                hr = m_audioRenderClient->ReleaseBuffer(availableFrames, AUDCLNT_BUFFERFLAGS_SILENT);
            }
            else {
                auto read = m_buffer.read(sampleBuffer, count);
               /* if (read < count) {
                    std::cout << "not enough samples\n";
                    memset(sampleBuffer + read, 0, (count - read) * sizeof(float));
                }*/
            }

            hr = m_audioRenderClient->ReleaseBuffer(availableFrames, 0);

            //GenerateSineWave(sampleBuffer, (bufferSize / sizeof(float)) / channels, channels, this->m_mixFormat->nSamplesPerSec);
            /*if (OnData) {
                OnData(sampleBuffer, (bufferSize / sizeof(float)) / channels, channels, this->m_mixFormat->nSamplesPerSec);
            }
            else {
                hr = m_audioRenderClient->ReleaseBuffer(availableFrames, AUDCLNT_BUFFERFLAGS_SILENT);
            }*/
        }

    exit:
        LeaveCriticalSection(&m_criticalSection);

        if (AUDCLNT_E_RESOURCES_INVALIDATED == hr) {
            std::cout << "ressource invalidated" << std::endl;
        }
        // Prepare to render again
        hr = MFPutWaitingWorkItem(m_callbackEvent.get(), 0, m_callbackResult.get(), &m_callbackKey);

        return hr;
    }

}
