#include <stdafx.h>
#include "emule.h"
#include "OtherFunctions.h"
#include "WebSocket.h"
#include "WebServer.h"
#include "Preferences.h"
#include "StringConversion.h"
#include "Log.h"
#include "TLSthreading.h"

#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl_cache.h"
#include "mbedtls/sha1.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static HANDLE s_hTerminate = NULL;
static CWinThread *s_pSocketThread = NULL;

mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
mbedtls_ssl_config conf;
mbedtls_x509_crt srvcert;
mbedtls_x509_crt cachain;
mbedtls_pk_context pkey;
mbedtls_ssl_cache_context cache;

typedef struct
{
	void	*pThis;
	SOCKET	hSocket;
	in_addr incomingaddr;
} SocketData;

void CWebSocket::SetParent(CWebServer *pParent)
{
	m_pParent = pParent;
}

void CWebSocket::OnRequestReceived(const char *pHeader, DWORD dwHeaderLen, const char *pData, DWORD dwDataLen, const in_addr &inad)
{
	CStringA sHeader(pHeader, dwHeaderLen);
	CStringA sURL;

	if (sHeader.Left(3) == "GET")
		sURL = sHeader.Trim();
	else if (sHeader.Left(4) == "POST") {
		CStringA sData(pData, dwDataLen);
		sURL = '?' + sData.Trim();	// '?' to imitate GET syntax for ParseURL
	}
	if (sURL.Find(' ') >= 0)
		sURL = sURL.Mid(sURL.Find(' ') + 1, sURL.GetLength());
	if (sURL.Find(' ') >= 0)
		sURL = sURL.Left(sURL.Find(' '));
	bool filereq = sURL.GetLength() >= 3 && sURL.Find("..") < 0; // prevent file access in the eMule's webserver folder
	if (filereq) {
		CStringA ext(sURL.Right(5).MakeLower());
		int i = ext.ReverseFind('.');
		ext.Delete(0, i);
		filereq = i >= 0 && ext.GetLength() > 2 && (ext == ".gif" || ext == ".jpg" || ext == ".png"
			|| ext == ".ico" || ext == ".css" || ext == ".bmp" || ext == ".js" || ext == ".jpeg");
	}
	ThreadData Data;
	Data.sURL = sURL;
	Data.pThis = (void*)m_pParent;
	Data.inadr = inad;
	Data.pSocket = this;

	if (!filereq)
		m_pParent->_ProcessURL(Data);
	else
		m_pParent->_ProcessFileReq(Data);

	Disconnect();
}

void CWebSocket::OnReceived(void *pData, DWORD dwSize, const in_addr &inad)
{
	static const DWORD SIZE_PRESERVE = 0x1000u;

	if (m_dwBufSize < dwSize + m_dwRecv) {
		// reallocate
		char *pNewBuf;
		try {
			m_dwBufSize = dwSize + m_dwRecv + SIZE_PRESERVE;
			pNewBuf = new char[m_dwBufSize];
		} catch (...) {
			m_bValid = false; // internal problem
			return;
		}

		if (m_pBuf) {
			memcpy(pNewBuf, m_pBuf, m_dwRecv);
			delete[] m_pBuf;
		}

		m_pBuf = pNewBuf;
	}
	if (pData != NULL) {
		memcpy(&m_pBuf[m_dwRecv], pData, dwSize);
		m_dwRecv += dwSize;
	}
	// check if we have all that we want
	if (!m_dwHttpHeaderLen) {
		// try to find it
		bool bPrevEndl = false;
		for (DWORD dwPos = 0; dwPos < m_dwRecv; ++dwPos)
			if ('\n' == m_pBuf[dwPos]) {
				if (bPrevEndl) {
					// We just found the end of the http header
					// Now write the message's position into two first DWORDs of the buffer
					m_dwHttpHeaderLen = dwPos + 1;

					// try to find now the 'Content-Length' header
					for (dwPos = 0; dwPos < m_dwHttpHeaderLen;) {
						// Elandal: pPtr is actually a char*, not a void*
						char *pPtr = (char*)memchr(m_pBuf + dwPos, '\n', m_dwHttpHeaderLen - dwPos);
						if (!pPtr)
							break;
						// Elandal: And thus now the pointer subtraction works as it should
						DWORD dwNextPos = (DWORD)(pPtr - m_pBuf);

						// check this header
						static const char szMatch[] = "content-length";
						if (!_strnicmp(m_pBuf + dwPos, szMatch, (sizeof szMatch) - 1)) {
							dwPos += (sizeof szMatch) - 1;
							pPtr = (char*)memchr(m_pBuf + dwPos, ':', m_dwHttpHeaderLen - dwPos);
							if (pPtr)
								m_dwHttpContentLen = atol(pPtr + 1);

							break;
						}
						dwPos = dwNextPos + 1;
					}

					break;
				}
				bPrevEndl = true;
			} else if ('\r' != m_pBuf[dwPos])
				bPrevEndl = false;

	}
	if (m_dwHttpHeaderLen && !m_bCanRecv && !m_dwHttpContentLen)
		m_dwHttpContentLen = m_dwRecv - m_dwHttpHeaderLen; // of course

	if (m_dwHttpHeaderLen && m_dwHttpContentLen < m_dwRecv && (!m_dwHttpContentLen || (m_dwHttpHeaderLen + m_dwHttpContentLen <= m_dwRecv))) {
		OnRequestReceived(m_pBuf, m_dwHttpHeaderLen, m_pBuf + m_dwHttpHeaderLen, m_dwHttpContentLen, inad);

		if (m_bCanRecv && (m_dwRecv > m_dwHttpHeaderLen + m_dwHttpContentLen)) {
			// move our data
			m_dwRecv -= m_dwHttpHeaderLen + m_dwHttpContentLen;
			memmove(m_pBuf, m_pBuf + m_dwHttpHeaderLen + m_dwHttpContentLen, m_dwRecv);
		} else
			m_dwRecv = 0;

		m_dwHttpHeaderLen = 0;
		m_dwHttpContentLen = 0;
	}
}

void CWebSocket::SendData(const void *pData, DWORD dwDataSize)
{
	ASSERT(pData);
	if (m_bValid && m_bCanSend) {
		if (!m_pHead) {
			if (thePrefs.GetWebUseHttps()) {
				for (;;) {
					int nRes = mbedtls_ssl_write(m_ssl, (unsigned char*)pData, dwDataSize);
					if (nRes > 0) {
						reinterpret_cast<const char *&>(pData) += nRes;
						dwDataSize -= nRes;
						if (dwDataSize)
							continue;
					}
					if (!dwDataSize)
						break;
					if (nRes == MBEDTLS_ERR_NET_CONN_RESET || (nRes != MBEDTLS_ERR_SSL_WANT_READ && nRes != MBEDTLS_ERR_SSL_WANT_WRITE)) {
						m_bValid = false;
						break;
					}
				}
			} else {
				// try to send it directly
				//-- remember: in "nRes" could be "-1" after "send" call
				int nRes = send(m_hSocket, (char*)pData, dwDataSize, 0);

				if (nRes < (int)dwDataSize && WSAEWOULDBLOCK != WSAGetLastError())
					m_bValid = false;

				if (nRes > 0) {
					reinterpret_cast<const char *&>(pData) += nRes;
					dwDataSize -= nRes;
				}
			}
		}

		if (dwDataSize && m_bValid) {
			// push it to our tails
			CChunk *pChunk = NULL;
			try {
				pChunk = new CChunk;
			} catch (...) {
				return;
			}
			pChunk->m_pNext = NULL;
			pChunk->m_dwSize = dwDataSize;
			try {
				pChunk->m_pData = new char[dwDataSize];
			} catch (...) {
				delete pChunk; // oops, no memory (???)
				return;
			}
			//-- data should be copied into "pChunk->m_pData" anyhow
			//-- possible solution is simple:

			memcpy(pChunk->m_pData, pData, dwDataSize);

			// push it to the end of our queue
			pChunk->m_pToSend = pChunk->m_pData;
			if (m_pTail)
				m_pTail->m_pNext = pChunk;
			else
				m_pHead = pChunk;
			m_pTail = pChunk;
		}
	}
}

void CWebSocket::SendReply(LPCSTR szReply)
{
	CStringA sBuf;
	sBuf.Format("%s\r\n", szReply);
	if (!sBuf.IsEmpty())
		SendData(sBuf, sBuf.GetLength());
}

void CWebSocket::SendContent(LPCSTR szStdResponse, const void *pContent, DWORD dwContentSize)
{
	CStringA sBuf;
	sBuf.Format("HTTP/1.1 200 OK\r\n%sContent-Length: %lu\r\n\r\n", szStdResponse, dwContentSize);
	if (!sBuf.IsEmpty()) {
		SendData(sBuf, sBuf.GetLength());
		SendData(pContent, dwContentSize);
	}
}

void CWebSocket::SendContent(LPCSTR szStdResponse, const CString &rstr)
{
	CStringA strA(wc2utf8(rstr));
	SendContent(szStdResponse, strA, strA.GetLength());
}

void CWebSocket::Disconnect()
{
	if (m_bValid && m_bCanSend) {
		m_bCanSend = false;
		if (m_pTail)
			try {
				// push an empty chunk as a tail
				m_pTail->m_pNext = new CChunk();
			} catch (...) {
			}
		else if (shutdown(m_hSocket, SD_SEND))
			m_bValid = false;
	}
}

UINT AFX_CDECL WebSocketAcceptedFunc(LPVOID pD)
{
	DbgSetThreadName("WebSocketAccepted");

	srand((unsigned)time(NULL));
	InitThreadLocale();

	const SocketData *pData = static_cast<SocketData*>(pD);
	CWebServer *pThis = static_cast<CWebServer*>(pData->pThis);
	SOCKET hSocket = pData->hSocket;
	const in_addr &ad(pData->incomingaddr);
	pThis->SetIP(ad.s_addr);
	delete pData;

	ASSERT(INVALID_SOCKET != hSocket);

	HANDLE hEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if (hEvent) {
		if (!WSAEventSelect(hSocket, hEvent, FD_READ | FD_CLOSE | FD_WRITE)) {
			mbedtls_ssl_context ssl;
			CWebSocket stWebSocket;
			stWebSocket.SetParent(pThis);
			stWebSocket.m_pHead = NULL;
			stWebSocket.m_pTail = NULL;
			stWebSocket.m_bValid = true;
			stWebSocket.m_bCanRecv = true;
			stWebSocket.m_bCanSend = true;
			stWebSocket.m_hSocket = hSocket;
			stWebSocket.m_pBuf = NULL;
			stWebSocket.m_dwRecv = 0;
			stWebSocket.m_dwBufSize = 0;
			stWebSocket.m_dwHttpHeaderLen = 0;
			stWebSocket.m_dwHttpContentLen = 0;
			stWebSocket.m_ssl = &ssl;

			if (thePrefs.GetWebUseHttps()) {
				mbedtls_ssl_init(&ssl);
				int ret = mbedtls_ssl_setup(&ssl, &conf);
				if (ret)
					goto thread_exit;
				mbedtls_ssl_set_bio(&ssl, (void*)&hSocket, mbedtls_net_send, mbedtls_net_recv, NULL);
				while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
					if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
						DebugLogWarning(_T("Web Interface handshake failed: %s"), (LPCTSTR)SSLerror(ret));
						goto thread_exit;
					}
			}
			HANDLE pWait[] = {hEvent, s_hTerminate};

			while (WAIT_OBJECT_0 == ::WaitForMultipleObjects(2, pWait, FALSE, INFINITE)) {
				while (stWebSocket.m_bValid) {
					WSANETWORKEVENTS stEvents;
					if (WSAEnumNetworkEvents(hSocket, NULL, &stEvents))
						stWebSocket.m_bValid = false;
					else {
						if (!stEvents.lNetworkEvents)
							break; //no more events till now

						if (FD_READ & stEvents.lNetworkEvents)
							for (;;) {
								char pBuf[0x1000];
								int nRes;
								if (thePrefs.GetWebUseHttps())
									nRes = mbedtls_ssl_read(stWebSocket.m_ssl, (unsigned char*)pBuf, sizeof pBuf);
								else
									nRes = recv(hSocket, pBuf, sizeof pBuf, 0);
								if (nRes == MBEDTLS_ERR_SSL_WANT_READ || nRes == MBEDTLS_ERR_SSL_WANT_WRITE)
									continue;
								if (nRes <= 0) {
									if (!nRes) {
										stWebSocket.m_bCanRecv = false;
										stWebSocket.OnReceived(NULL, 0, ad);
									} else if (WSAEWOULDBLOCK != WSAGetLastError())
										stWebSocket.m_bValid = false;
									break;
								}
								stWebSocket.OnReceived(pBuf, nRes, ad);
							}

						if (FD_CLOSE & stEvents.lNetworkEvents)
							stWebSocket.m_bCanRecv = false;

						if (FD_WRITE & stEvents.lNetworkEvents)
							// send what is left in our tails
							while (stWebSocket.m_pHead) {
								if (stWebSocket.m_pHead->m_pToSend) {
									if (thePrefs.GetWebUseHttps()) {
										for (;;) {
											int nRes = mbedtls_ssl_write(stWebSocket.m_ssl, (unsigned char*)stWebSocket.m_pHead->m_pToSend, stWebSocket.m_pHead->m_dwSize);
											if (nRes > 0) {
												stWebSocket.m_pHead->m_pToSend += nRes;
												stWebSocket.m_pHead->m_dwSize -= nRes;
												if (stWebSocket.m_pHead->m_dwSize)
													continue;
											}
											if (!stWebSocket.m_pHead->m_dwSize)
												break;
											if (nRes == MBEDTLS_ERR_NET_CONN_RESET || (nRes != MBEDTLS_ERR_SSL_WANT_READ && nRes != MBEDTLS_ERR_SSL_WANT_WRITE))
												goto thread_exit;
										};
									} else {
										int nRes = send(hSocket, stWebSocket.m_pHead->m_pToSend, stWebSocket.m_pHead->m_dwSize, 0);
										if (nRes != (int)stWebSocket.m_pHead->m_dwSize) {
											if (nRes)
												if ((nRes > 0) && (nRes < (int)stWebSocket.m_pHead->m_dwSize)) {
													stWebSocket.m_pHead->m_pToSend += nRes;
													stWebSocket.m_pHead->m_dwSize -= nRes;

												} else if (WSAEWOULDBLOCK != WSAGetLastError())
													stWebSocket.m_bValid = false;
												break;
										}
									}
								} else if (shutdown(hSocket, SD_SEND)) {
									stWebSocket.m_bValid = false;
									break;
								}

								// erase this chunk
								CWebSocket::CChunk *pNext = stWebSocket.m_pHead->m_pNext;
								delete stWebSocket.m_pHead;
								stWebSocket.m_pHead = pNext;
								if (stWebSocket.m_pHead == NULL)
									stWebSocket.m_pTail = NULL;
							}
					}
				}

				if (!stWebSocket.m_bValid || (!stWebSocket.m_bCanRecv && !stWebSocket.m_pHead))
					break;
			}
thread_exit:
			stWebSocket.m_bValid = false;
			while (stWebSocket.m_pHead) {
				CWebSocket::CChunk *pNext = stWebSocket.m_pHead->m_pNext;
				delete stWebSocket.m_pHead;
				stWebSocket.m_pHead = pNext;
			}
			delete[] stWebSocket.m_pBuf;
			if (thePrefs.GetWebUseHttps()) {
				int ret;
				while ((ret = mbedtls_ssl_close_notify(stWebSocket.m_ssl)) < 0)
					if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
						break;
				mbedtls_ssl_free(stWebSocket.m_ssl);
			}
		}
		VERIFY(::CloseHandle(hEvent));
	}
	if (thePrefs.GetWebUseHttps())
		mbedtls_net_free((mbedtls_net_context*)&hSocket);
	else
		VERIFY(!closesocket(hSocket));
	return 0;
}

UINT AFX_CDECL WebSocketListeningFunc(LPVOID pThis)
{
	DbgSetThreadName("WebSocketListening");

	srand((unsigned)time(NULL));
	InitThreadLocale();

	SOCKET hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
	if (INVALID_SOCKET != hSocket) {
		SOCKADDR_IN stAddr;
		stAddr.sin_family = AF_INET;
		stAddr.sin_port = htons(thePrefs.GetWSPort());
		stAddr.sin_addr.s_addr = thePrefs.GetBindAddrA() ? inet_addr(thePrefs.GetBindAddrA()) : INADDR_ANY;

		if (!bind(hSocket, (LPSOCKADDR)&stAddr, sizeof stAddr) && !listen(hSocket, SOMAXCONN)) {
			HANDLE hEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
			if (hEvent) {
				if (!WSAEventSelect(hSocket, hEvent, FD_ACCEPT)) {
					HANDLE pWait[] = {hEvent, s_hTerminate};
					while (WAIT_OBJECT_0 == ::WaitForMultipleObjects(2, pWait, FALSE, INFINITE)) {
						for (;;) {
							SOCKADDR_IN their_addr;
							int sin_size = (int)sizeof(SOCKADDR_IN);

							SOCKET hAccepted = accept(hSocket, (LPSOCKADDR)&their_addr, &sin_size);
							if (INVALID_SOCKET == hAccepted)
								break;

							if (!thePrefs.GetAllowedRemoteAccessIPs().IsEmpty()) {
								bool bAllowedIP = false;
								for (int i = 0; i < thePrefs.GetAllowedRemoteAccessIPs().GetCount(); ++i) {
									if (their_addr.sin_addr.s_addr == thePrefs.GetAllowedRemoteAccessIPs()[i]) {
										bAllowedIP = true;
										break;
									}
								}
								if (!bAllowedIP) {
									LogWarning(_T("Web Interface: Rejected connection attempt from %s"), (LPCTSTR)ipstr(their_addr.sin_addr.s_addr));
									VERIFY(!closesocket(hAccepted));
									break;
								}
							}

							if (thePrefs.GetWSIsEnabled()) {
								SocketData *pData = new SocketData;
								pData->pThis = pThis;
								pData->hSocket = hAccepted;
								pData->incomingaddr = their_addr.sin_addr;
								// - do NOT use Windows API 'CreateThread' to create a thread which uses MFC/CRT -> lot of mem leaks!
								// - 'AfxBeginThread' is excessive for our needs.
								CWinThread *pAcceptThread = new CWinThread(WebSocketAcceptedFunc, (LPVOID)pData);
								if (!pAcceptThread->CreateThread()) {
									delete pAcceptThread;
									VERIFY(!closesocket(hAccepted));
								}
							} else
								VERIFY(!closesocket(hAccepted));
						}
					}
				}
				VERIFY(::CloseHandle(hEvent));
			}
		}
		VERIFY(!closesocket(hSocket));
	}

	return 0;
}

int StartSSL()
{
	static const char pers[] = "eMule_WebSrv";
	if (!thePrefs.GetWebUseHttps())
		return 0; //success
	mbedtls_threading_set_alt(threading_mutex_init_alt, threading_mutex_free_alt, threading_mutex_lock_alt, threading_mutex_unlock_alt);
	mbedtls_ssl_cache_init(&cache);
	mbedtls_x509_crt_init(&srvcert);
	mbedtls_x509_crt_init(&cachain);
	mbedtls_ssl_config_init(&conf);
	mbedtls_ctr_drbg_init(&ctr_drbg);
	mbedtls_entropy_init(&entropy);
	int ret = mbedtls_x509_crt_parse_file(&srvcert, thePrefs.GetWebCertPath());
	if (!ret) {
		mbedtls_pk_init(&pkey);
		ret = mbedtls_pk_parse_keyfile(&pkey, thePrefs.GetWebKeyPath(), NULL);
		if (!ret) {
			ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (unsigned char*)pers, strlen(pers));
			if (!ret) {
				ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
				if (!ret) {
					mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
					mbedtls_ssl_conf_session_cache(&conf, &cache, mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);
					mbedtls_ssl_conf_ca_chain(&conf, &cachain, NULL);
					ret = mbedtls_ssl_conf_own_cert(&conf, &srvcert, &pkey);
				}
			}
		}
	}
	if (ret)
		DebugLogError(_T("Web Interface start failed: %s"), (LPCTSTR)SSLerror(ret));
	else {
		unsigned char fingerprint[20];
		mbedtls_sha1(srvcert.raw.p, srvcert.raw.len, fingerprint);
		DebugLog(_T("Loaded certificate: %s"), (LPCTSTR)GetCertHash(fingerprint, (int)(sizeof fingerprint)));
	}
	return ret;
}

void StopSSL()
{
	if (thePrefs.GetWebUseHttps()) {
		mbedtls_x509_crt_free(&srvcert);
		mbedtls_pk_free(&pkey);
		mbedtls_ssl_cache_free(&cache);
		mbedtls_ctr_drbg_free(&ctr_drbg);
		mbedtls_entropy_free(&entropy);
		mbedtls_ssl_config_free(&conf);
	}
}

void StartSockets(CWebServer *pThis)
{
	ASSERT(s_hTerminate == NULL);
	ASSERT(s_pSocketThread == NULL);
	s_hTerminate = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (s_hTerminate != NULL) {
		// - do NOT use Windows API 'CreateThread' to create a thread which uses MFC/CRT -> lot of mem leaks!
		// - because we want to wait on the thread handle,
		//   we have to disable 'CWinThread::m_AutoDelete' -> can't use 'AfxBeginThread'
		s_pSocketThread = new CWinThread(WebSocketListeningFunc, (LPVOID)pThis);
		s_pSocketThread->m_bAutoDelete = FALSE;
		if (!s_pSocketThread->CreateThread() || StartSSL())
			StopSockets();
	}
}

void StopSockets()
{
	StopSSL();
	if (s_pSocketThread) {
		VERIFY(::SetEvent(s_hTerminate));

		if (s_pSocketThread->m_hThread) {
			// because we want to wait on the thread handle we must not use 'CWinThread::m_AutoDelete'.
			// otherwise we may run into the situation that the CWinThread was already auto-deleted and
			// the CWinThread::m_hThread is invalid.
			ASSERT(!s_pSocketThread->m_bAutoDelete);

			DWORD dwWaitRes = ::WaitForSingleObject(s_pSocketThread->m_hThread, 1300);
			if (dwWaitRes == WAIT_TIMEOUT) {
				TRACE("*** Failed to wait for websocket thread termination - Timeout\n");
				VERIFY(::TerminateThread(s_pSocketThread->m_hThread, _UI32_MAX));
				VERIFY(::CloseHandle(s_pSocketThread->m_hThread));
			} else if (dwWaitRes == WAIT_FAILED) {
				TRACE("*** Failed to wait for websocket thread termination - Error %d\n", CAsyncSocket::GetLastError());
				ASSERT(0); // probably invalid thread handle
			}
		}
		delete s_pSocketThread;
		s_pSocketThread = NULL;
	}
	if (s_hTerminate) {
		VERIFY(::CloseHandle(s_hTerminate));
		s_hTerminate = NULL;
	}
}