#pragma once

template<class T>
class AsyncCallback
	: public winrt::implements<AsyncCallback<T>, IMFAsyncCallback >
{
public:
	typedef HRESULT(T::* InvokeFn)(IMFAsyncResult* asyncResult);

	AsyncCallback(T* parent, InvokeFn fn)
		: m_parent(parent)
		, m_invokeFn(fn)
	{}

	STDMETHODIMP GetParameters(DWORD* flags, DWORD* queue)
	{
		*queue = m_queueId;
		*flags = 0;
		return S_OK;
	}

	STDMETHODIMP Invoke(IMFAsyncResult* asyncResult)
	{
		return (m_parent->*m_invokeFn)(asyncResult);
	}

	void SetQueueID(DWORD queueId)
	{
		m_queueId = queueId;
	}

private:
	DWORD m_queueId { 0 };
	T* m_parent;
	InvokeFn m_invokeFn;
};