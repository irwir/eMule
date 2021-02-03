/*CAsyncSocketEx by Tim Kosse (tim.kosse@filezilla-project.org)
			Version 1.3 (2003-04-26)
--------------------------------------------------------

Introduction:
-------------

CAsyncSocketEx used to be a replacement for the MFC class CAsyncSocket.
This class was written because CAsyncSocket is not the fastest WinSock
wrapper and it's very hard to add new functionality to CAsyncSocket
derived classes. This class offers the same functionality as CAsyncSocket.
Also, CAsyncSocketEx offers some enhancements which were not possible with
CAsyncSocket without some tricks.

How do I use it?
----------------
Basically similar to CAsyncSocket.
To use CAsyncSocketEx, just replace all occurrences of CAsyncSocket in your
code with CAsyncSocketEx. If you did not enhance CAsyncSocket yourself in
any way, you won't have to change anything else in your code.

Why is CAsyncSocketEx faster?
-----------------------------

CAsyncSocketEx is slightly faster when dispatching notification event messages.
First have a look at the way CAsyncSocket works. For each thread that uses
CAsyncSocket, a window is created. CAsyncSocket calls WSAAsyncSelect with
the handle of that window. Until here, CAsyncSocketEx works the same way.
But CAsyncSocket uses only one window message (WM_SOCKET_NOTIFY) for all
sockets within one thread. When the window receive WM_SOCKET_NOTIFY, wParam
contains the socket handle and the window looks up a CAsyncSocket instance
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

#pragma once
#if _MSC_VER <= 1800 //VS 2013
#define THREADLOCAL __declspec(thread)
#else
#define THREADLOCAL thread_local
#endif

#define FD_FORCEREAD (1<<15)
#define FD_DEFAULT (FD_READ | FD_WRITE | FD_OOB | FD_ACCEPT | FD_CONNECT | FD_CLOSE)

#include <list>

#define _afxSockThreadState AfxGetModuleThreadState()
#define _AFX_SOCK_THREAD_STATE AFX_MODULE_THREAD_STATE

class CAsyncSocketExHelperWindow;

#define WM_SOCKETEX_TRIGGER		(WM_USER + 0x101 + 0)				// 0x0501 event sent by a layer
#define WM_SOCKETEX_GETHOST		(WM_USER + 0x101 + 1)				// 0x0502 WSAAsyncGetHostByName reply
#define WM_SOCKETEX_CALLBACK	(WM_USER + 0x101 + 2)				// 0x0503
#define WM_SOCKETEX_NOTIFY		(WM_USER + 0x101 + 3)				// 0x0504 socket notification message
#define MAX_SOCKETS				(0xBFFF - WM_SOCKETEX_NOTIFY + 1)	// 0xBAFD (47869 decimal)

class CAsyncSocketExLayer;

struct t_callbackMsg
{
	CAsyncSocketExLayer *pLayer;
	char *str;
	WPARAM wParam;
	LPARAM lParam;
	int nType;
};

enum AsyncSocketExState
{
	notsock,
	unconnected,
	connecting,
	listening,
	connected,
	closed,
	aborted,
	attached
};

class CAsyncSocketEx : public CObject
{
	DECLARE_DYNAMIC(CAsyncSocketEx)
	friend CAsyncSocketExHelperWindow;
	friend CAsyncSocketExLayer;

public:
	///////////////////////////////////////
	//Functions that imitate CAsyncSocket//
	///////////////////////////////////////

	//Construction
	//------------

	//Constructs a CAsyncSocketEx object.
	CAsyncSocketEx();
	virtual	~CAsyncSocketEx();

	//Creates a socket.
	bool Create(UINT nSocketPort = 0
			, int nSocketType = SOCK_STREAM
			, long lEvent = FD_DEFAULT
			, const CString &sSocketAddress = CString()
			, ADDRESS_FAMILY nFamily = AF_INET
			, bool reusable = false);

	//Attributes
	//----------

	//Attaches a socket handle to a CAsyncSocketEx object.
	BOOL Attach(SOCKET hSocket, long lEvent = FD_DEFAULT);

	//Detaches a socket handle from a CAsyncSocketEx object.
	SOCKET Detach();

	//Gets the error status for the last operation that failed.
	static int GetLastError();

	//Gets the address of the peer socket to which the socket is connected.
	bool GetPeerName(CString &rPeerAddress, UINT &rPeerPort);
	BOOL GetPeerName(LPSOCKADDR lpSockAddr, int *lpSockAddrLen);

	//Gets the local name for a socket.
	bool GetSockName(CString &rSocketAddress, UINT &rSocketPort);
	BOOL GetSockName(LPSOCKADDR lpSockAddr, int *lpSockAddrLen);

	//Retrieves a socket option.
	BOOL GetSockOpt(int nOptionName, void *lpOptionValue, int *lpOptionLen, int nLevel = SOL_SOCKET);

	//Sets a socket option.
	BOOL SetSockOpt(int nOptionName, const void *lpOptionValue, int nOptionLen, int nLevel = SOL_SOCKET);

	//Gets the socket family
	ADDRESS_FAMILY GetFamily() const;

	//Sets the socket family
	bool SetFamily(ADDRESS_FAMILY nFamily);

	//Operations
	//----------

	//Accepts a connection on the socket.
	virtual BOOL Accept(CAsyncSocketEx &rConnectedSocket, LPSOCKADDR lpSockAddr = NULL, int *lpSockAddrLen = NULL);

	//Requests event notification for the socket.
	BOOL AsyncSelect(long lEvent = FD_DEFAULT);

	//Associates a local address with the socket.
	bool Bind(UINT nSocketPort, const CString &sSocketAddress = CString());
	BOOL Bind(const LPSOCKADDR lpSockAddr, int nSockAddrLen);

	//Closes the socket.
	virtual void Close();

	//Establishes a connection to a peer socket.
	virtual bool Connect(const CString &sHostAddress, UINT nHostPort);
	virtual BOOL Connect(const LPSOCKADDR lpSockAddr, int nSockAddrLen);

	//Controls the mode of the socket.
	BOOL IOCtl(long lCommand, DWORD *lpArgument);

	//Establishes a socket to listen for incoming connection requests.
	BOOL Listen(int nConnectionBacklog = 5);

	//Receives data from the socket.
	virtual int Receive(void *lpBuf, int nBufLen, int nFlags = 0);

	//Sends data to a connected socket.
	virtual int Send(const void *lpBuf, int nBufLen, int nFlags = 0);

	//Disables Send and/or Receive calls on the socket.
	BOOL ShutDown(int nHow = CAsyncSocket::sends);

	//Overridable Notification Functions
	//----------------------------------

	//Notifies a listening socket that it can accept pending connection requests by calling Accept.
	virtual void OnAccept(int nErrorCode);

	//Notifies a socket that the socket connected to it has closed.
	virtual void OnClose(int nErrorCode);

	//Notifies a connecting socket that the connection attempt is complete, whether successfully or in error.
	virtual void OnConnect(int nErrorCode);

	//Notifies a listening socket that there is data to be retrieved by calling Receive.
	virtual void OnReceive(int nErrorCode);

	//Notifies a socket that it can send data by calling Send.
	virtual void OnSend(int nErrorCode);

	virtual bool OnHostNameResolved(const SOCKADDR_IN *pSockAddr);

	////////////////////////
	//Additional functions//
	////////////////////////

	//Resets layer chain.
	virtual void RemoveAllLayers();

	//Attaches a new layer to the socket.
	BOOL AddLayer(CAsyncSocketExLayer *pLayer);

	//Is a layer attached to the socket?
	bool IsLayerAttached() const;

	//Returns the handle of the socket.
	SOCKET GetSocketHandle();

	//Trigers an event on the socket
	// Any combination of FD_READ, FD_WRITE, FD_CLOSE, FD_ACCEPT, FD_CONNECT and FD_FORCEREAD is valid for lEvent.
	BOOL TriggerEvent(long lEvent);

#ifdef _DEBUG
	// Diagnostic Support
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext &dc) const;
#endif

protected:
	//Strucure to hold the socket data
	struct t_AsyncSocketExData
	{
		addrinfo *addrInfo, *nextAddr; // Iterate through protocols on connect failure
		SOCKET hSocket; //Socket handle
		int nSocketIndex; //Index of socket, required by CAsyncSocketExHelperWindow
		ADDRESS_FAMILY nFamily;
		bool bIsClosing; // Set to true on first received FD_CLOSE event
	} m_SocketData;

	//If using layers, only the events specified with m_lEvent will send to the event handlers.
	long m_lEvent;

	//AsyncGetHostByName
	char *m_pAsyncGetHostByNameBuffer; //Buffer for hostend structure
	HANDLE m_hAsyncGetHostByNameHandle; //TaskHandle
	USHORT m_nAsyncGetHostByNamePort; //Port to connect to

	//Returns the handle of the helper window
	HWND GetHelperWindowHandle();

	//Attaches socket handle to helper window
	void AttachHandle(/*SOCKET hSocket*/);

	//Detaches socket handle from helper window
	void DetachHandle();

	//Pointer to the data of the local thread
	struct t_AsyncSocketExThreadData
	{
		CAsyncSocketExHelperWindow *m_pHelperWindow;
		std::list<CAsyncSocketEx*> layerCloseNotify;
		int nInstanceCount;
	} *m_pLocalAsyncSocketExThreadData;

	//List of the data structures for all threads
	static THREADLOCAL t_AsyncSocketExThreadData *thread_local_data;

	//Initializes Thread data and helper window, fills m_pLocalAsyncSocketExThreadData
	bool InitAsyncSocketExInstance();

	//Destroys helper window after last instance of CAsyncSocketEx in current thread has been closed
	void FreeAsyncSocketExInstance();

	// Iterate through protocols on failure
	bool TryNextProtocol();

	void ResendCloseNotify();

	// Add a new notification to the list of pending callbacks
	void AddCallbackNotification(const t_callbackMsg &msg);

#ifndef NOSOCKETSTATES
	int m_nPendingEvents;

	int GetState() const;
	void SetState(int nState);

	int m_nState;
#endif //NOSOCKETSTATES

	//Layer chain
	CAsyncSocketExLayer *m_pFirstLayer;
	CAsyncSocketExLayer *m_pLastLayer;

	//Called by the layers to notify application of some events
	virtual int OnLayerCallback(std::list<t_callbackMsg> &callbacks);

	// Used by Bind with AF_UNSPEC sockets
	UINT m_nSocketPort;
	CString m_sSocketAddress;

	// Pending callbacks
	std::list<t_callbackMsg> m_pendingCallbacks;
};

#define LAYERCALLBACK_STATECHANGE 0
#define LAYERCALLBACK_LAYERSPECIFIC 1

inline CString Inet6AddrToString(const in6_addr &addr)
{
	CString buf;
	buf.Format(_T("%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x")
		, addr.s6_bytes[0], addr.s6_bytes[1], addr.s6_bytes[2], addr.s6_bytes[3]
		, addr.s6_bytes[4], addr.s6_bytes[5], addr.s6_bytes[6], addr.s6_bytes[7]
		, addr.s6_bytes[8], addr.s6_bytes[9], addr.s6_bytes[10], addr.s6_bytes[11]
		, addr.s6_bytes[12], addr.s6_bytes[13], addr.s6_bytes[14], addr.s6_bytes[15]);
	return buf;
}