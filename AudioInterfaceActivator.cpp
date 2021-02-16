#include "pch.h"
#include "AudioInterfaceActivator.h"

concurrency::task<winrt::com_ptr<IAudioClient3>> AudioInterfaceActivator::ActivateAudioClientAsync(winrt::hstring deviceId)
{
    auto activator = winrt::make_self<AudioInterfaceActivator>();

    winrt::com_ptr<IActivateAudioInterfaceAsyncOperation> asyncOp;
    winrt::com_ptr<IActivateAudioInterfaceCompletionHandler> handler = activator;

    std::cout << "calling ActivateAudioInterfaceAsync" << std::endl;

    winrt::hresult hr = ActivateAudioInterfaceAsync(
        deviceId.c_str(),
        __uuidof(IAudioClient3),
        nullptr,
        handler.get(),
        asyncOp.put());

    winrt::check_hresult(hr);

    return concurrency::create_task(activator->m_ActivateCompleted);
}

HRESULT AudioInterfaceActivator::ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation)
{
    std::cout << "AudioInterfaceActivator::ActivateCompleted called " << std::endl;

    HRESULT hr = S_OK, hr2 = S_OK;
    winrt::com_ptr<IUnknown> audioInterface;
    // Get the audio activation result as IUnknown pointer
    hr2 = operation->GetActivateResult(&hr, audioInterface.put());

    if (audioInterface == nullptr)
        std::cout << "audioInterface is null" << std::endl;

    // Activation failure
    winrt::check_hresult(hr);
    // Failure to get activate result
    winrt::check_hresult(hr2);

    winrt::com_ptr<IAudioClient3> audioClient;

    audioClient = audioInterface.as<IAudioClient3>();

    m_ActivateCompleted.set(audioClient);
    return S_OK;
}
