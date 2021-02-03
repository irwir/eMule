/*CAsyncSocketEx by Tim Kosse (tim.kosse@filezilla-project.org)
			Version 1.1 (2002-11-01)
--------------------------------------------------------

Introduction:
-------------

CAsyncSocketEx is a replacement for the MFC class CAsyncSocket.
This class was written because CAsyncSocket is not the fastest WinSock
wrapper and it's very hard to add new functionality to CAsyncSocket
derived classes. This class offers the same functionality as CAsyncSocket.
Also, CAsyncSocketEx offers some enhancements which were not possible with
CAsyncSocket without some tricks.

How do I use it?
----------------
Basically exactly like CAsyncSocket.
To use CAsyncSocketEx, just replace all occurrences of CAsyncSocket in your
code with CAsyncSocketEx, if you did not enhance CAsyncSocket yourself in
any way, you won't have to change anything else in your code.

Why is CAsyncSocketEx faster?
-----------------------------

CAsyncSocketEx is slightly faster when dispatching notification event messages.
First have a look at the way CAsyncSocket works. For each thread that uses
CAsyncSocket, a window is created. CAsyncSocket calls WSAAsyncSelect with
the handle of that window. Until here, CAsyncSocketEx works the same way.
But CAsyncSocket uses only one window message (WM_SOCKET_NOTIFY) for all
sockets within one thread. When the window receive WM_SOCKET_NOTIFY, wParam
contains the socket handle and the window looks up an CAsyncSocket instance
using a map. CAsyncSocketEx works differently. Its helper window uses a
wide range of different window messages (WM_USER through 0xBFFF) and passes
a different message to WSAAsyncSelect for each socket. When a message in
the specified range is received, CAsyncSocketEx looks up the pointer to a
CAsyncSocketEx instance in an Array using the index of message - WM_USER.
As you can see, CAsyncSocketEx uses the helper window in a more efficient
way, as it don't have to use the slow maps to lookup its own instance.
Still, speed increase is not very much, but it may be noticeable when using
a lot of sockets at the same time.
Please note that the changes do not affect the raw data throughput rate,
CAsyncSocketEx only dispatches the notification messages faster.

What else does CAsyncSocketEx offer?
------------------------------------

CAsyncSocketEx offers a flexible layer system. One example is the proxy layer.
Just create an instance of the proxy layer, configure it and add it to the layer
chain of your CAsyncSocketEx instance. After that, you can connect through
proxies.
Benefit: You don't have to change much to use the layer system.
Another layer that is currently in development is the SSL layer to establish
SSL encrypted connections.

License
-------

Feel free to use this class, as long as you don't claim that you wrote it
and this copyright notice stays intact in the source files.
If you use this class in commercial applications, please send a short message
to tim.kosse@filezilla-project.org
*/
#include "stdafx.h"
#include "AsyncSocketExLayer.h"

#include "AsyncSocketEx.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Konstruktion/Destruktion
//////////////////////////////////////////////////////////////////////

CAsyncSocketExLayer::CAsyncSocketExLayer()
	: m_pOwnerSocket()
	, m_nLayerState(notsock)
	, m_nFamily(AF_UNSPEC)
	, m_lEvent()
	, m_nSocketPort()
	, m_addrInfo()
	, m_nextAddr()
	, m_nPendingEvents()
	, m_nCriticalError()
	, m_pNextLayer()
	, m_pPrevLayer()
{
}

CAsyncSocketExLayer* CAsyncSocketExLayer::AddLayer(CAsyncSocketExLayer *pLayer, CAsyncSocketEx *pOwnerSocket)
{
	ASSERT(pLayer);
	ASSERT(pOwnerSocket);
	if (m_pNextLayer)
		return m_pNextLayer->AddLayer(pLayer, pOwnerSocket);

	ASSERT(m_pOwnerSocket == pOwnerSocket);
	pLayer->Init(this, m_pOwnerSocket);
	m_pNextLayer = pLayer;
	return m_pNextLayer;
}

int CAsyncSocketExLayer::Receive(void *lpBuf, int nBufLen, int nFlags /*=0*/)
{
	return ReceiveNext(lpBuf, nBufLen, nFlags);
}

int CAsyncSocketExLayer::Send(const void *lpBuf, int nBufLen, int nFlags /*=0*/)
{
	return SendNext(lpBuf, nBufLen, nFlags);
}

void CAsyncSocketExLayer::OnReceive(int nErrorCode)
{
	if (m_pPrevLayer)
		m_pPrevLayer->OnReceive(nErrorCode);
	else
		if (m_pOwnerSocket->m_lEvent & FD_READ)
			m_pOwnerSocket->OnReceive(nErrorCode);
}

void CAsyncSocketExLayer::OnSend(int nErrorCode)
{
	if (m_pPrevLayer)
		m_pPrevLayer->OnSend(nErrorCode);
	else
		if (m_pOwnerSocket->m_lEvent & FD_WRITE)
			m_pOwnerSocket->OnSend(nErrorCode);
}

void CAsyncSocketExLayer::OnConnect(int nErrorCode)
{
	TriggerEvent(FD_CONNECT, nErrorCode, TRUE);
}

void CAsyncSocketExLayer::OnAccept(int nErrorCode)
{
	if (m_pPrevLayer)
		m_pPrevLayer->OnAccept(nErrorCode);
	else
		if (m_pOwnerSocket->m_lEvent & FD_ACCEPT)
			m_pOwnerSocket->OnAccept(nErrorCode);
}

void CAsyncSocketExLayer::OnClose(int nErrorCode)
{
	if (m_pPrevLayer)
		m_pPrevLayer->OnClose(nErrorCode);
	else
		if (m_pOwnerSocket->m_lEvent & FD_CLOSE)
			m_pOwnerSocket->OnClose(nErrorCode);
}

BOOL CAsyncSocketExLayer::TriggerEvent(long lEvent, int nErrorCode, BOOL bPassThrough /*=FALSE*/)
{
	ASSERT(m_pOwnerSocket);
	if (m_pOwnerSocket->m_SocketData.hSocket == INVALID_SOCKET)
		return FALSE;

	if (!bPassThrough) {
		if (m_nPendingEvents & lEvent)
			return TRUE;

		m_nPendingEvents |= lEvent;
	}

	if (lEvent & FD_CONNECT) {
		ASSERT(bPassThrough);
		if (!nErrorCode)
			ASSERT(bPassThrough && (GetLayerState() == connected || GetLayerState() == attached));
		else {
			SetLayerState(aborted);
			m_nCriticalError = nErrorCode;
		}
	} else if (lEvent & FD_CLOSE) {
		if (!nErrorCode)
			SetLayerState(closed);
		else {
			SetLayerState(aborted);
			m_nCriticalError = nErrorCode;
		}
	}
	ASSERT(m_pOwnerSocket->m_pLocalAsyncSocketExThreadData);
	ASSERT(m_pOwnerSocket->m_pLocalAsyncSocketExThreadData->m_pHelperWindow);
	ASSERT(m_pOwnerSocket->m_SocketData.nSocketIndex >= 0);
	t_LayerNotifyMsg *pMsg = new t_LayerNotifyMsg;
	pMsg->hSocket = m_pOwnerSocket->m_SocketData.hSocket;
	pMsg->lEvent = MAKELONG(lEvent, nErrorCode);
	pMsg->pLayer = bPassThrough ? m_pPrevLayer : this;
	if (!::PostMessage(m_pOwnerSocket->GetHelperWindowHandle(), WM_SOCKETEX_TRIGGER, (WPARAM)m_pOwnerSocket->m_SocketData.nSocketIndex, (LPARAM)pMsg)) {
		delete pMsg;
		return FALSE;
	}
	return TRUE;
}

void CAsyncSocketExLayer::Close()
{
	CloseNext();
}

void CAsyncSocketExLayer::CloseNext()
{
	if (m_addrInfo)
		freeaddrinfo(m_addrInfo);
	m_nextAddr = NULL;
	m_addrInfo = NULL;

	m_nPendingEvents = 0;

	SetLayerState(notsock);
	if (m_pNextLayer)
		m_pNextLayer->Close();
}

bool CAsyncSocketExLayer::Connect(const CString &sHostAddress, UINT nHostPort)
{
	return ConnectNext(sHostAddress, nHostPort);
}

BOOL CAsyncSocketExLayer::Connect(const LPSOCKADDR lpSockAddr, int nSockAddrLen)
{
	return ConnectNext(lpSockAddr, nSockAddrLen);
}

int CAsyncSocketExLayer::SendNext(const void *lpBuf, int nBufLen, int nFlags /*=0*/)
{
	if (m_nCriticalError) {
		WSASetLastError(m_nCriticalError);
		return SOCKET_ERROR;
	}
	switch (GetLayerState()) {
	case notsock:
		WSASetLastError(WSAENOTSOCK);
		return SOCKET_ERROR;
	case unconnected:
	case connecting:
	case listening:
		WSASetLastError(WSAENOTCONN);
		return SOCKET_ERROR;
	}
	if (!m_pNextLayer) {
		ASSERT(m_pOwnerSocket);
		return send(m_pOwnerSocket->GetSocketHandle(), (LPSTR)lpBuf, nBufLen, nFlags);
	}
	return m_pNextLayer->Send(lpBuf, nBufLen, nFlags);
}

int CAsyncSocketExLayer::ReceiveNext(void *lpBuf, int nBufLen, int nFlags /*=0*/)
{
	if (m_nCriticalError) {
		WSASetLastError(m_nCriticalError);
		return SOCKET_ERROR;
	}
	switch (GetLayerState()) {
	case notsock:
		WSASetLastError(WSAENOTSOCK);
		return SOCKET_ERROR;
	case unconnected:
	case connecting:
	case listening:
		WSASetLastError(WSAENOTCONN);
		return SOCKET_ERROR;
	}
	if (!m_pNextLayer) {
		ASSERT(m_pOwnerSocket);
		return recv(m_pOwnerSocket->GetSocketHandle(), (LPSTR)lpBuf, nBufLen, nFlags);
	}
	return m_pNextLayer->Receive(lpBuf, nBufLen, nFlags);
}

bool CAsyncSocketExLayer::ConnectNext(const CString &sHostAddress, UINT nHostPort)
{
	ASSERT(GetLayerState() == unconnected);
	ASSERT(m_pOwnerSocket);
	bool ret;
	if (m_pNextLayer)
		ret = m_pNextLayer->Connect(sHostAddress, nHostPort);
	else if (m_nFamily == AF_INET || m_nFamily == AF_INET6 || m_nFamily == AF_UNSPEC) {
		ASSERT(!sHostAddress.IsEmpty());

		freeaddrinfo(m_addrInfo);
		m_nextAddr = NULL;
		m_addrInfo = NULL;

		addrinfo hints = {};
		hints.ai_family = (int)m_nFamily;
		hints.ai_socktype = SOCK_STREAM;
		CStringA port;
		port.Format("%u", nHostPort);
		addrinfo *res0;
		if (getaddrinfo((CStringA)sHostAddress, port, &hints, &res0))
			return false;

		ret = false;
		addrinfo *res1;
		for (res1 = res0; res1; res1 = res1->ai_next) {
			SOCKET hSocket;
			if (m_nFamily == AF_UNSPEC)
				hSocket = socket(res1->ai_family, res1->ai_socktype, res1->ai_protocol);
			else
				hSocket = m_pOwnerSocket->GetSocketHandle();

			if (INVALID_SOCKET == hSocket)
				continue;

			if (m_nFamily == AF_UNSPEC) {
				m_pOwnerSocket->m_SocketData.hSocket = hSocket;
				m_pOwnerSocket->AttachHandle();
				if (!m_pOwnerSocket->AsyncSelect(m_lEvent)
					|| (m_pOwnerSocket->m_pFirstLayer && WSAAsyncSelect(m_pOwnerSocket->m_SocketData.hSocket, m_pOwnerSocket->GetHelperWindowHandle(), m_pOwnerSocket->m_SocketData.nSocketIndex + WM_SOCKETEX_NOTIFY, FD_DEFAULT)))
				{
					m_pOwnerSocket->Close();
					continue;
				}

				if (!m_pOwnerSocket->m_pendingCallbacks.empty())
					::PostMessage(m_pOwnerSocket->GetHelperWindowHandle(), WM_SOCKETEX_CALLBACK, (WPARAM)m_pOwnerSocket->m_SocketData.nSocketIndex, 0);

				m_pOwnerSocket->m_SocketData.nFamily = m_nFamily = (ADDRESS_FAMILY)res1->ai_family;
				if (!m_pOwnerSocket->Bind(m_nSocketPort, m_sSocketAddress)) {
					m_pOwnerSocket->m_SocketData.nFamily = m_nFamily = AF_UNSPEC;
					Close();
					continue;
				}
			}
			ret = !connect(m_pOwnerSocket->GetSocketHandle(), res1->ai_addr, (int)res1->ai_addrlen);
			if (!ret && WSAGetLastError() != WSAEWOULDBLOCK) {
				if (hints.ai_family == AF_UNSPEC) {
					m_nFamily = AF_UNSPEC;
					Close();
				}
				continue;
			}

			m_pOwnerSocket->m_SocketData.nFamily = m_nFamily = (ADDRESS_FAMILY)res1->ai_family;
			break;
		}

		if (res1)
			res1 = res0->ai_next;

		if (res1) {
			m_addrInfo = res0;
			m_nextAddr = res1;
		} else
			freeaddrinfo(res0);

		if (INVALID_SOCKET == m_pOwnerSocket->GetSocketHandle())
			ret = false;
	} else {
		WSASetLastError(WSAEPROTONOSUPPORT);
		return false;
	}

	if (ret || WSAGetLastError() == WSAEWOULDBLOCK)
		SetLayerState(connecting);

	return ret;
}

BOOL CAsyncSocketExLayer::ConnectNext(const LPSOCKADDR lpSockAddr, int nSockAddrLen)
{
	ASSERT(GetLayerState() == unconnected);
	ASSERT(m_pOwnerSocket);
	BOOL ret;
	if (m_pNextLayer)
		ret = m_pNextLayer->Connect(lpSockAddr, nSockAddrLen);
	else
		ret = !connect(m_pOwnerSocket->GetSocketHandle(), lpSockAddr, nSockAddrLen);

	if (ret || WSAGetLastError() == WSAEWOULDBLOCK)
		SetLayerState(connecting);
	return ret;
}

//Gets the address of the peer socket to which the socket is connected
bool CAsyncSocketExLayer::GetPeerName(CString &rPeerAddress, UINT &rPeerPort)
{
	return GetPeerNameNext(rPeerAddress, rPeerPort);
}

bool CAsyncSocketExLayer::GetPeerNameNext(CString &rPeerAddress, UINT &rPeerPort)
{
	if (m_pNextLayer)
		return m_pNextLayer->GetPeerName(rPeerAddress, rPeerPort);
	if (m_nFamily != AF_INET6 && m_nFamily != AF_INET)
		return false;

	int nSockAddrLen = (int)((m_nFamily == AF_INET6) ? sizeof(SOCKADDR_IN6) : sizeof(SOCKADDR_IN));
	LPSOCKADDR sockAddr = (LPSOCKADDR)new char[nSockAddrLen]();

	BOOL bResult = GetPeerName(sockAddr, &nSockAddrLen);
	if (bResult)
		if (m_nFamily == AF_INET6) {
			rPeerPort = ntohs(((LPSOCKADDR_IN6)sockAddr)->sin6_port);
			rPeerAddress = Inet6AddrToString(((LPSOCKADDR_IN6)sockAddr)->sin6_addr);
		} else {
			rPeerPort = ntohs(((LPSOCKADDR_IN)sockAddr)->sin_port);
			rPeerAddress = inet_ntoa(((LPSOCKADDR_IN)sockAddr)->sin_addr);
		}

	delete[] sockAddr;
	return bResult;
}

BOOL CAsyncSocketExLayer::GetPeerName(LPSOCKADDR lpPeerAddr, int *lpPeerAddrLen)
{
	return GetPeerNameNext(lpPeerAddr, lpPeerAddrLen);
}

BOOL CAsyncSocketExLayer::GetPeerNameNext(LPSOCKADDR lpPeerAddr, int *lpPeerAddrLen)
{
	if (m_pNextLayer)
		return m_pNextLayer->GetPeerName(lpPeerAddr, lpPeerAddrLen);

	ASSERT(m_pOwnerSocket);
	return !getpeername(m_pOwnerSocket->GetSocketHandle(), lpPeerAddr, lpPeerAddrLen);
}

//Gets the address of the sock socket to which the socket is connected
bool CAsyncSocketExLayer::GetSockName(CString &rSockAddress, UINT &rSockPort)
{
	return GetSockNameNext(rSockAddress, rSockPort);
}

bool CAsyncSocketExLayer::GetSockNameNext(CString &rSockAddress, UINT &rSockPort)
{
	if (m_pNextLayer)
		return m_pNextLayer->GetSockName(rSockAddress, rSockPort);
	if (m_nFamily != AF_INET6 && m_nFamily != AF_INET)
		return false;

	int nSockAddrLen = (int)((m_nFamily == AF_INET6) ? sizeof(SOCKADDR_IN6) : sizeof(SOCKADDR_IN));
	LPSOCKADDR sockAddr = (LPSOCKADDR)new char[nSockAddrLen]();

	BOOL bResult = GetSockName(sockAddr, &nSockAddrLen);
	if (bResult)
		if (m_nFamily == AF_INET6) {
			rSockPort = ntohs(((LPSOCKADDR_IN6)sockAddr)->sin6_port);
			rSockAddress = Inet6AddrToString(((LPSOCKADDR_IN6)sockAddr)->sin6_addr);
		} else {
			rSockPort = ntohs(((LPSOCKADDR_IN)sockAddr)->sin_port);
			rSockAddress = inet_ntoa(((LPSOCKADDR_IN)sockAddr)->sin_addr);
		}

	delete[] sockAddr;
	return bResult;
}

BOOL CAsyncSocketExLayer::GetSockName(LPSOCKADDR lpSockAddr, int *lpSockAddrLen)
{
	return GetSockNameNext(lpSockAddr, lpSockAddrLen);
}

BOOL CAsyncSocketExLayer::GetSockNameNext(LPSOCKADDR lpSockAddr, int *lpSockAddrLen)
{
	if (m_pNextLayer)
		return m_pNextLayer->GetSockName(lpSockAddr, lpSockAddrLen);

	ASSERT(m_pOwnerSocket);
	return !getsockname(m_pOwnerSocket->GetSocketHandle(), lpSockAddr, lpSockAddrLen);
}

void CAsyncSocketExLayer::Init(CAsyncSocketExLayer *pPrevLayer, CAsyncSocketEx *pOwnerSocket)
{
	ASSERT(pOwnerSocket);
	m_pPrevLayer = pPrevLayer;
	m_pOwnerSocket = pOwnerSocket;
	m_pNextLayer = NULL;
#ifndef NOSOCKETSTATES
	SetLayerState(pOwnerSocket->GetState());
#endif //NOSOCKETSTATES
}

int CAsyncSocketExLayer::GetLayerState() const
{
	return m_nLayerState;
}

void CAsyncSocketExLayer::SetLayerState(int nLayerState)
{
	ASSERT(m_pOwnerSocket);
	int nOldLayerState = GetLayerState();
	m_nLayerState = nLayerState;
	if (nOldLayerState != nLayerState)
		DoLayerCallback(LAYERCALLBACK_STATECHANGE, GetLayerState(), nOldLayerState);
}

void CAsyncSocketExLayer::CallEvent(int nEvent, int nErrorCode)
{
	if (m_nCriticalError)
		return;
	m_nCriticalError = nErrorCode;
	switch (nEvent) {
	case FD_READ:
	case FD_FORCEREAD:
		if (GetLayerState() == connecting && !nErrorCode) {
			m_nPendingEvents |= nEvent;
			break;
		}

		if (GetLayerState() == attached)
			SetLayerState(connected);
		m_nPendingEvents &= ~nEvent;
		if (GetLayerState() == connected || nErrorCode) {
			if (nErrorCode)
				SetLayerState(aborted);
			OnReceive(nErrorCode);
		}
		break;
	case FD_WRITE:
		if (GetLayerState() == connecting && !nErrorCode) {
			m_nPendingEvents |= nEvent;
			break;
		}
		if (GetLayerState() == attached)
			SetLayerState(connected);
		m_nPendingEvents &= ~FD_WRITE;
		if (GetLayerState() == connected || nErrorCode) {
			if (nErrorCode)
				SetLayerState(aborted);
			OnSend(nErrorCode);
		}
		break;
	case FD_CONNECT:
		if (GetLayerState() == connecting || GetLayerState() == attached) {
			if (!nErrorCode)
				SetLayerState(connected);
			else {
				if (!m_pNextLayer && m_nextAddr)
					if (TryNextProtocol()) {
						m_nCriticalError = 0;
						return;
					}
				SetLayerState(aborted);
			}
			m_nPendingEvents &= ~FD_CONNECT;
			OnConnect(nErrorCode);

			if (!nErrorCode) {
				if ((m_nPendingEvents & FD_READ) && GetLayerState() == connected)
					OnReceive(0);
				if ((m_nPendingEvents & FD_FORCEREAD) && GetLayerState() == connected)
					OnReceive(0);
				if ((m_nPendingEvents & FD_WRITE) && GetLayerState() == connected)
					OnSend(0);
			}
			m_nPendingEvents = 0;
		}
		break;
	case FD_ACCEPT:
		if (GetLayerState() == listening) {
			if (nErrorCode)
				SetLayerState(aborted);
			m_nPendingEvents &= ~FD_ACCEPT;
			OnAccept(nErrorCode);
		}
		break;
	case FD_CLOSE:
		if (GetLayerState() == connected || GetLayerState() == attached) {
			SetLayerState(nErrorCode ? aborted : closed);
			m_nPendingEvents &= ~FD_CLOSE;
			OnClose(nErrorCode);
		}
	}
}

//Creates a socket
bool CAsyncSocketExLayer::Create(UINT nSocketPort, int nSocketType
		, long lEvent, const CString &sSocketAddress, ADDRESS_FAMILY nFamily /*=AF_INET*/, bool reusable /*=false*/)
{
	return CreateNext(nSocketPort, nSocketType, lEvent, sSocketAddress, nFamily, reusable);
}

bool CAsyncSocketExLayer::CreateNext(UINT nSocketPort, int nSocketType, long lEvent, const CString &sSocketAddress, ADDRESS_FAMILY nFamily /*=AF_INET*/, bool reusable /*=false*/)
{
	ASSERT(GetLayerState() == notsock);
	bool ret;

	m_nFamily = nFamily;

	if (m_pNextLayer)
		ret = m_pNextLayer->Create(nSocketPort, nSocketType, lEvent, sSocketAddress, nFamily);
	else if (m_nFamily == AF_UNSPEC) {
		m_lEvent = lEvent;
		m_sSocketAddress = sSocketAddress;
		m_nSocketPort = nSocketPort;
		ret = true;
	} else {
		SOCKET hSocket = socket((int)nFamily, nSocketType, 0);
		if (hSocket == INVALID_SOCKET) {
			m_pOwnerSocket->Close();
			return false;
		}
		m_pOwnerSocket->m_SocketData.hSocket = hSocket;
		m_pOwnerSocket->AttachHandle();
		if (!m_pOwnerSocket->AsyncSelect(lEvent)) {
			m_pOwnerSocket->Close();
			return false;
		}
		if (m_pOwnerSocket->m_pFirstLayer && WSAAsyncSelect(m_pOwnerSocket->m_SocketData.hSocket, m_pOwnerSocket->GetHelperWindowHandle(), m_pOwnerSocket->m_SocketData.nSocketIndex + WM_SOCKETEX_NOTIFY, FD_DEFAULT)) {
			m_pOwnerSocket->Close();
			return false;
		}

		if (reusable && nSocketPort != 0) {
			BOOL value = TRUE;
			m_pOwnerSocket->SetSockOpt(SO_REUSEADDR, reinterpret_cast<const void*>(&value), sizeof value);
		}

		if (!m_pOwnerSocket->Bind(nSocketPort, sSocketAddress)) {
			m_pOwnerSocket->Close();
			return false;
		}
		ret = true;
	}
	if (ret)
		SetLayerState(unconnected);
	return ret;
}

int CAsyncSocketExLayer::DoLayerCallback(int nType, WPARAM wParam, LPARAM lParam, char *str /*=NULL*/)
{
	if (m_pOwnerSocket) {
		int nError = WSAGetLastError();

		t_callbackMsg msg;
		msg.pLayer = this;
		if (str) {
			rsize_t i = strlen(str) + 1;
			msg.str = new char[i];
			strcpy_s(msg.str, i, str);
		} else
			msg.str = NULL;
		msg.wParam = wParam;
		msg.lParam = lParam;
		msg.nType = nType;

		m_pOwnerSocket->AddCallbackNotification(msg);

		WSASetLastError(nError);
	}
	return 0;
}

BOOL CAsyncSocketExLayer::Listen(int nConnectionBacklog)
{
	return ListenNext(nConnectionBacklog);
}

BOOL CAsyncSocketExLayer::ListenNext(int nConnectionBacklog)
{
	ASSERT(GetLayerState() == unconnected);
	BOOL ret;
	if (m_pNextLayer)
		ret = m_pNextLayer->Listen(nConnectionBacklog);
	else
		ret = !listen(m_pOwnerSocket->GetSocketHandle(), nConnectionBacklog);

	if (ret)
		SetLayerState(listening);
	return ret;
}

BOOL CAsyncSocketExLayer::Accept(CAsyncSocketEx &rConnectedSocket, LPSOCKADDR lpSockAddr /*=NULL*/, int *lpSockAddrLen /*=NULL*/)
{
	return AcceptNext(rConnectedSocket, lpSockAddr, lpSockAddrLen);
}

BOOL CAsyncSocketExLayer::AcceptNext(CAsyncSocketEx &rConnectedSocket, LPSOCKADDR lpSockAddr /*=NULL*/, int *lpSockAddrLen /*=NULL*/)
{
	ASSERT(GetLayerState() == listening);
	if (m_pNextLayer)
		return m_pNextLayer->Accept(rConnectedSocket, lpSockAddr, lpSockAddrLen);

	SOCKET hTemp = accept(m_pOwnerSocket->m_SocketData.hSocket, lpSockAddr, lpSockAddrLen);
	if (hTemp == INVALID_SOCKET)
		return FALSE;
	VERIFY(rConnectedSocket.InitAsyncSocketExInstance());
	rConnectedSocket.m_SocketData.hSocket = hTemp;
	rConnectedSocket.AttachHandle();
	rConnectedSocket.SetFamily(GetFamily());
#ifndef NOSOCKETSTATES
	rConnectedSocket.SetState(connected);
#endif //NOSOCKETSTATES
	return TRUE;
}

BOOL CAsyncSocketExLayer::ShutDown(int nHow /*=CAsyncSocket::sends*/)
{
	return ShutDownNext(nHow);
}

BOOL CAsyncSocketExLayer::ShutDownNext(int nHow /*=CAsyncSocket::sends*/)
{
	if (m_nCriticalError) {
		WSASetLastError(m_nCriticalError);
		return FALSE;
	}
	switch (GetLayerState()) {
	case notsock:
		WSASetLastError(WSAENOTSOCK);
		return FALSE;
	case unconnected:
	case connecting:
	case listening:
		WSASetLastError(WSAENOTCONN);
		return FALSE;
	}
	if (!m_pNextLayer) {
		ASSERT(m_pOwnerSocket);
		return shutdown(m_pOwnerSocket->GetSocketHandle(), nHow);
	}
	return m_pNextLayer->ShutDownNext(nHow);
}

ADDRESS_FAMILY CAsyncSocketExLayer::GetFamily() const
{
	return m_nFamily;
}

bool CAsyncSocketExLayer::SetFamily(ADDRESS_FAMILY nFamily)
{
	if (m_nFamily != AF_UNSPEC)
		return false;

	m_nFamily = nFamily;
	return true;
}

bool CAsyncSocketExLayer::TryNextProtocol()
{
	closesocket(m_pOwnerSocket->m_SocketData.hSocket);
	m_pOwnerSocket->DetachHandle();

	bool ret = false;
	for (; m_nextAddr; m_nextAddr = m_nextAddr->ai_next) {
		m_pOwnerSocket->m_SocketData.hSocket = socket(m_nextAddr->ai_family, m_nextAddr->ai_socktype, m_nextAddr->ai_protocol);

		if (m_pOwnerSocket->m_SocketData.hSocket == INVALID_SOCKET)
			continue;

		m_pOwnerSocket->AttachHandle();

		if (m_pOwnerSocket->AsyncSelect(m_lEvent))
			if (!m_pOwnerSocket->m_pFirstLayer || !WSAAsyncSelect(m_pOwnerSocket->m_SocketData.hSocket, m_pOwnerSocket->GetHelperWindowHandle(), m_pOwnerSocket->m_SocketData.nSocketIndex + WM_SOCKETEX_NOTIFY, FD_DEFAULT)) {
				m_pOwnerSocket->m_SocketData.nFamily = m_nFamily = (ADDRESS_FAMILY)m_nextAddr->ai_family;
				if (m_pOwnerSocket->Bind(m_nSocketPort, m_sSocketAddress)) {
					ret = !connect(m_pOwnerSocket->GetSocketHandle(), m_nextAddr->ai_addr, (int)m_nextAddr->ai_addrlen) || WSAGetLastError() == WSAEWOULDBLOCK;
					if (ret) {
						SetLayerState(connecting);
						break;
					}
				}
			}

		closesocket(m_pOwnerSocket->m_SocketData.hSocket);
		m_pOwnerSocket->DetachHandle();
	}

	if (m_nextAddr)
		m_nextAddr = m_nextAddr->ai_next;

	if (!m_nextAddr) {
		freeaddrinfo(m_addrInfo);
		m_addrInfo = NULL;
	}

	return ret && m_pOwnerSocket->m_SocketData.hSocket != INVALID_SOCKET;
}