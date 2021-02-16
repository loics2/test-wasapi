#pragma once

class AudioInterfaceActivator
    : public winrt::implements<AudioInterfaceActivator, IActivateAudioInterfaceCompletionHandler >
{
public:
    static concurrency::task<winrt::com_ptr<IAudioClient3>> ActivateAudioClientAsync(winrt::hstring deviceId);

private:
    STDMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation);

    concurrency::task_completion_event<winrt::com_ptr<IAudioClient3>> m_ActivateCompleted;
};

