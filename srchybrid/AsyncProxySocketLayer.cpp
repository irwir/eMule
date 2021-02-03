/*CAsyncProxySocketLayer by Tim Kosse (Tim.Kosse@filezilla-project.org)
				 Version 1.6 (2003-03-26)
--------------------------------------------------------

Introduction:
-------------

This class is layer class for CAsyncSocketEx. With this class you
can connect through SOCKS4/5 and HTTP 1.1 proxies. This class works
as semi-transparent layer between CAsyncSocketEx and the actual socket.
This class is used in FileZilla, a powerful open-source FTP client.
It can be found under http://sourceforge.net/projects/filezilla
For more information about SOCKS4/5 goto
http://www.socks.nec.com/socksprot.html
For more information about HTTP 1.1 goto http://www.rfc-editor.org
and search for RFC2616

How to use?
-----------

You don't have to change much in you already existing code to use
CAsyncProxySocketLayer.
To use it, create an instance of CAsyncProxySocketLayer, call SetProxy
and attach it to a CAsyncSocketEx instance.
You have to process OnLayerCallback in you CAsyncSocketEx instance as it will
receive all layer notifications.
The following notifications are sent:

//Error codes
PROXYERROR_NOERROR 0
PROXYERROR_NOCONN 1 //Can't connect to proxy server, use GetLastError for more information
PROXYERROR_REQUESTFAILED 2 //Request failed, can't send data
PROXYERROR_AUTHREQUIRED 3 //Authentication required
PROXYERROR_AUTHTYPEUNKNOWN 4 //Authtype unknown or not supported
PROXYERROR_AUTHFAILED 5  //Authentication failed
PROXYERROR_AUTHNOLOGON 6
PROXYERROR_CANTRESOLVEHOST 7

//Status messages
PROXYSTATUS_LISTENSOCKETCREATED 8 //Called when a listen socket was created successfully. Unlike the normal listen function,
								//a socksified socket has to connect to the proxy to negotiate the details with the server
								//on which the listen socket will be created
								//The two parameters will contain the ip and port of the listen socket on the server.

If you want to use CAsyncProxySocketLayer to create a listen socket, you
have to use this overloaded function:
BOOL PrepareListen(unsigned long serverIp);
serverIP is the IP of the server you are already connected
through the SOCKS proxy. You can't use listen sockets over a
SOCKS proxy without a primary connection. Listen sockets are only
supported by SOCKS proxies, this won't work with HTTP proxies.
When the listen socket is created successfully, the PROXYSTATUS_LISTENSOCKETCREATED
notification is sent. The parameters  will tell you the ip and the port of the listen socket.
After it you have to handle the OnAccept message and accept the
connection.
Be careful when calling Accept: rConnected socket will NOT be filled! Instead use the instance which created the
listen socket, it will handle the data connection.
If you want to accept more than one connection, you have to create a listing socket for each of them!

Description of important functions and their parameters:
--------------------------------------------------------

void SetProxy(int nProxyType);
void SetProxy(int nProxyType, const CString &pProxyHost, USHORT nProxyPort);
void SetProxy(int nProxyType, const CString &pProxyHost, USHORT nProxyPort, const CString &pProxyUser, const CString &pProxyPass);

Call one of this functions to set the proxy type.
Parameters:
- nProxyType specifies the Proxy Type.
- ProxyHost and nProxyPort specify the address of the proxy
- ProxyUser and ProxyPass are only available for SOCKS5 proxies.

supported proxy types:
PROXYTYPE_NOPROXY
PROXYTYPE_SOCKS4
PROXYTYPE_SOCKS4A
PROXYTYPE_SOCKS5
PROXYTYPE_HTTP10
PROXYTYPE_HTTP11

There are also some other functions:

GetProxyPeerName
Like GetPeerName of CAsyncSocket, but returns the address of the
server connected through the proxy.	If using proxies, GetPeerName
only returns the address of the proxy.

int GetProxyType();
Returns the used proxy

const int GetLastProxyError() const;
Returns the last proxy error

License
-------

Feel free to use this class, as long as you don't claim that you wrote it
and this copyright notice stays intact in the source files.
If you use this class in commercial applications, please send a short message
to tim.kosse@filezilla-project.org

Version history
---------------

- 1.6 got rid of MFC
- 1.5 released CAsyncSocketExLayer version
- 1.4 added UNICODE support
- 1.3 added basic HTTP1.1 authentication
	  fixed memory leak in SOCKS5 code
	  OnSocksOperationFailed will be called after Socket has been closed
	  fixed some minor bugs
- 1.2 renamed into CAsyncProxySocketLayer
	  added HTTP1.1 proxy support
- 1.1 fixes all known bugs, mostly with SOCKS5 authentication
- 1.0 initial release
*/

#include "stdafx.h"
#include <atlenc.h>
#include "AsyncProxySocketLayer.h"
#include "opcodes.h"
#include "otherfunctions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CStringA GetSocks4Error(char ver, char cd)
{
	CStringA strError;
	if (ver != 0)
		strError.Format("Unknown protocol version: %u", (unsigned)ver);
	else {
		LPCSTR pError = NULL;
		switch (cd) {
		case 90: //success
			break;
		case 91:
			pError = "Request rejected or failed";
			break;
		case 92:
			pError = "Failed to connect to 'identd'";
			break;
		case 93:
			pError = "'identd' user-id error";
			break;
		default:
			strError.Format("Unknown command: %i", cd);
		}
		if (pError)
			strError = pError;
	}
	return strError;
}

CStringA GetSocks5Error(char rep)
{
	static LPCSTR const pError[] =
	{
		"",
		"General SOCKS server failure",
		"Connection not allowed by ruleset",
		"Network unreachable",
		"Host unreachable",
		"Connection refused",
		"TTL expired",
		"Command not supported",
		"Address type not supported"
	};

	if (rep < 0 || rep >= _countof(pError)) {
		CStringA strError;
		strError.Format("Unknown reply: %i", rep);
		return strError;
	}
	return CStringA(pError[rep]);
}

//////////////////////////////////////////////////////////////////////
// Konstruktion/Destruktion
//////////////////////////////////////////////////////////////////////

CAsyncProxySocketLayer::CAsyncProxySocketLayer()
	: m_pRecvBuffer()
	, m_nRecvBufferLen()
	, m_nRecvBufferPos()
	, m_pStrBuffer()
	, m_nProxyOpState()
	, m_nProxyOpID()
	, m_nProxyPeerIp()
	, m_nProxyPeerPort()
{
	m_ProxyData.nProxyType = PROXYTYPE_NOPROXY;
}

CAsyncProxySocketLayer::~CAsyncProxySocketLayer()
{
	ClearBuffer();
}

/////////////////////////////////////////////////////////////////////////////
// Member Functions CAsyncProxySocketLayer

void CAsyncProxySocketLayer::SetProxy(int nProxyType)
{
	//Validate the parameters
	ASSERT(nProxyType == PROXYTYPE_NOPROXY);
	m_ProxyData.nProxyType = nProxyType;
}

void CAsyncProxySocketLayer::SetProxy(int nProxyType, const CString &pProxyHost, USHORT ProxyPort)
{
	//Validate the parameters
	ASSERT(nProxyType == PROXYTYPE_SOCKS4
		|| nProxyType == PROXYTYPE_SOCKS4A
		|| nProxyType == PROXYTYPE_SOCKS5
		|| nProxyType == PROXYTYPE_HTTP10
		|| nProxyType == PROXYTYPE_HTTP11);
	ASSERT(!m_nProxyOpID);
	ASSERT(!pProxyHost.IsEmpty());
	ASSERT(ProxyPort > 0);

	m_ProxyData.pProxyUser.Empty();
	m_ProxyData.pProxyPass.Empty();

	m_ProxyData.nProxyType = nProxyType;
	m_ProxyData.pProxyHost = pProxyHost;
	m_ProxyData.nProxyPort = ProxyPort;
	m_ProxyData.bUseLogon = false;
}

void CAsyncProxySocketLayer::SetProxy(int nProxyType, const CString &pProxyHost, USHORT ProxyPort, const CString &pProxyUser, const CString &pProxyPass)
{
	//Validate the parameters
	ASSERT(nProxyType == PROXYTYPE_SOCKS5 || nProxyType == PROXYTYPE_HTTP10 || nProxyType == PROXYTYPE_HTTP11);
	ASSERT(!m_nProxyOpID);
	ASSERT(!pProxyHost.IsEmpty());
	ASSERT(ProxyPort > 0);

	m_ProxyData.nProxyType = nProxyType;
	m_ProxyData.pProxyHost = pProxyHost;
	m_ProxyData.nProxyPort = ProxyPort;
	m_ProxyData.pProxyUser = pProxyUser;
	m_ProxyData.pProxyPass = pProxyPass;
	m_ProxyData.bUseLogon = true;
}

void CAsyncProxySocketLayer::OnReceive(int nErrorCode)
{
	//Here we handle the responses from the SOCKS proxy
	if (!m_nProxyOpID) {
		TriggerEvent(FD_READ, nErrorCode, TRUE);
		return;
	}
	if (nErrorCode)
		TriggerEvent(FD_READ, nErrorCode, TRUE);

	if (!m_nProxyOpState) //We should not receive a response yet!
		return;

	if (m_ProxyData.nProxyType == PROXYTYPE_SOCKS4 || m_ProxyData.nProxyType == PROXYTYPE_SOCKS4A) {
		if (m_nProxyOpState == 1) { //Both for PROXYOP_CONNECT and PROXYOP_BIND
			if (!m_pRecvBuffer)
				m_pRecvBuffer = new char[8];
			int numread = ReceiveNext(m_pRecvBuffer + m_nRecvBufferPos, 8 - m_nRecvBufferPos);
			if (numread == SOCKET_ERROR) {
				if (WSAGetLastError() != WSAEWOULDBLOCK) {
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
					TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAGetLastError(), TRUE);
					Reset();
					ClearBuffer();
				}
				return;
			}
			m_nRecvBufferPos += numread;
			//                +----+----+----+----+----+----+----+----+
			//                | VN | CD | DSTPORT |      DSTIP        |
			//                +----+----+----+----+----+----+----+----+
			// # of bytes:       1    1      2              4
			//
			// VN is the version of the reply code and should be 0. CD is the result
			// code with one of the following values:
			//
			//        90: request granted
			//        91: request rejected or failed
			//        92: request rejected because SOCKS server cannot connect to
			//            identd on the client
			//        93: request rejected because the client program and identd
			//            report different user-ids

			if (m_nRecvBufferPos == 8) {
				TRACE(_T("SOCKS4 response: VN=%u  CD=%u  DSTPORT=%u  DSTIP=%s\n"), (BYTE)m_pRecvBuffer[0], (BYTE)m_pRecvBuffer[1], ntohs(*(u_short*)&m_pRecvBuffer[2]), (LPCTSTR)ipstr(*(u_long*)&m_pRecvBuffer[4]));
				if (m_pRecvBuffer[0] != 0 || m_pRecvBuffer[1] != 90) {
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0, (LPSTR)(LPCSTR)GetSocks4Error(m_pRecvBuffer[0], m_pRecvBuffer[1]));
					TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAECONNABORTED, TRUE);
					Reset();
					ClearBuffer();
					return;
				}
				if (m_nProxyOpID == PROXYOP_CONNECT) {
					//OK, we are connected with the remote server
					Reset();
					ClearBuffer();
					TriggerEvent(FD_CONNECT, 0, TRUE);
					TriggerEvent(FD_READ, 0, TRUE);
					TriggerEvent(FD_WRITE, 0, TRUE);
					return;
				}
				//Listen socket created
				++m_nProxyOpState;
				unsigned long ip = *(unsigned long*)&m_pRecvBuffer[4];
				if (!ip) { //No IP return, use the IP of the proxy server
					SOCKADDR sockAddr = {};
					int sockAddrLen = sizeof sockAddr;
					if (!GetPeerName(&sockAddr, &sockAddrLen)) {
						DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
						TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAECONNABORTED, TRUE);
						Reset();
						ClearBuffer();
						return;
					}
					ip = ((LPSOCKADDR_IN)&sockAddr)->sin_addr.s_addr;
				}
				t_ListenSocketCreatedStruct data;
				data.ip = ip;
				data.nPort = (UINT)*(unsigned short*)&m_pRecvBuffer[2];
				DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYSTATUS_LISTENSOCKETCREATED, (LPARAM)&data);
				// Wait for 2nd response to bind request
				ClearBuffer();
			}
		} else if (m_nProxyOpID == PROXYOP_BIND) {
			if (!m_pRecvBuffer)
				m_pRecvBuffer = new char[8];
			int numread = ReceiveNext(m_pRecvBuffer + m_nRecvBufferPos, 8 - m_nRecvBufferPos);
			if (numread == SOCKET_ERROR) {
				if (WSAGetLastError() != WSAEWOULDBLOCK) {
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
					TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAGetLastError(), TRUE);
					Reset();
					ClearBuffer();
				}
				return;
			}
			m_nRecvBufferPos += numread;
			if (m_nRecvBufferPos == 8) {
				if (m_pRecvBuffer[0] != 0 || m_pRecvBuffer[1] != 90) {
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
					TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAECONNABORTED, TRUE);
					Reset();
					ClearBuffer();
					return;
				}
				//Connection to remote server established
				Reset();
				ClearBuffer();
				TriggerEvent(FD_ACCEPT, 0, TRUE);
				TriggerEvent(FD_READ, 0, TRUE);
				TriggerEvent(FD_WRITE, 0, TRUE);
			}
		}
	} else if (m_ProxyData.nProxyType == PROXYTYPE_SOCKS5) {
		if (m_nProxyOpState == 1) { //Get response to initialization message
			if (!m_pRecvBuffer)
				m_pRecvBuffer = new char[2];
			int numread = ReceiveNext(m_pRecvBuffer + m_nRecvBufferPos, 2 - m_nRecvBufferPos);
			if (numread == SOCKET_ERROR) {
				if (WSAGetLastError() != WSAEWOULDBLOCK) {
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
					TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAGetLastError(), TRUE);
					Reset();
				}
				return;
			}
			m_nRecvBufferPos += numread;
			if (m_nRecvBufferPos == 2) {
				TRACE(_T("SOCKS5 response: VER=%u  METHOD=%u\n"), (BYTE)m_pRecvBuffer[0], (BYTE)m_pRecvBuffer[1]);
				if (m_pRecvBuffer[0] != 5) {
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0, (LPSTR)(LPCSTR)GetSocks5Error(m_pRecvBuffer[1]));
					TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAECONNABORTED, TRUE);
					Reset();
					ClearBuffer();
					return;
				}
				if (m_pRecvBuffer[1]) { //Auth needed
					if (m_pRecvBuffer[1] != 2) { //Unknown auth type
						DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_AUTHTYPEUNKNOWN, 0);
						TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAECONNABORTED, TRUE);
						Reset();
						ClearBuffer();
						return;
					}

					if (!m_ProxyData.bUseLogon) {
						DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_AUTHNOLOGON, 0);
						TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAECONNABORTED, TRUE);
						Reset();
						ClearBuffer();
						return;
					}
					//Send authentication
					//
					// RFC 1929 - Username/Password Authentication for SOCKS V5
					//
					// +----+------+----------+------+----------+
					// |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
					// +----+------+----------+------+----------+
					// | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
					// +----+------+----------+------+----------+
					//
					// The VER field contains the current version of the sub-negotiation,
					// which is X'01'. The ULEN field contains the length of the UNAME field
					// that follows. The UNAME field contains the username as known to the
					// source operating system. The PLEN field contains the length of the
					// PASSWD field that follows. The PASSWD field contains the password
					// association with the given UNAME.

					const CStringA &sAsciiUser((CStringA)m_ProxyData.pProxyUser);
					const CStringA &sAsciiPass((CStringA)m_ProxyData.pProxyPass);
					int nLenUser = sAsciiUser.GetLength();
					int nLenPass = sAsciiPass.GetLength();
					ASSERT(nLenUser <= 255);
					ASSERT(nLenPass <= 255);
					unsigned char *buffer = new unsigned char[3 + nLenUser + nLenPass];
					buffer[0] = 1;
					buffer[1] = static_cast<unsigned char>(nLenUser);
					if (nLenUser)
						strncpy((char*)buffer + 2, sAsciiUser, nLenUser);
					buffer[2 + nLenUser] = static_cast<unsigned char>(nLenPass);
					if (nLenPass)
						strncpy((char*)buffer + 3 + nLenUser, sAsciiPass, nLenPass);
					int nBufLen = 3 + nLenUser + nLenPass;
					int res = SendNext(buffer, nBufLen);
					delete[] buffer;
					if (res == SOCKET_ERROR || res < nBufLen) {
						if ((WSAGetLastError() != WSAEWOULDBLOCK) || res < nBufLen) {
							DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
							TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAGetLastError(), TRUE);
							Reset();
							return;
						}
					}
					ClearBuffer();
					++m_nProxyOpState;
					return;
				}
			}
			//No auth needed
			//Send connection request
			const CStringA sAsciiHost(m_pProxyPeerHost);
			size_t nlen = sAsciiHost.GetLength();
			char *command = new char[10 + nlen + 1]();
			command[0] = 5;
			command[1] = static_cast<char>(m_nProxyOpID);
			//command[2]=0;
			command[3] = m_nProxyPeerIp ? 1 : 3;
			int nBufLen = 4;
			if (m_nProxyPeerIp) {
				*(ULONG*)&command[nBufLen] = m_nProxyPeerIp;
				nBufLen += 4;
			} else {
				command[nBufLen] = static_cast<char>(nlen);
				strncpy(&command[++nBufLen], sAsciiHost, nlen);
				nBufLen += (int)nlen;
			}
			*(USHORT*)&command[nBufLen] = m_nProxyPeerPort;
			nBufLen += 2;
			int res = SendNext(command, nBufLen);
			delete[] command;
			if (res == SOCKET_ERROR || res < nBufLen) {
				if ((WSAGetLastError() != WSAEWOULDBLOCK) || res < nBufLen) {
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
					TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAGetLastError(), TRUE);
					Reset();
					return;
				}
			}
			m_nProxyOpState += 2;
			ClearBuffer();
			return;
		}
		if (m_nProxyOpState == 2) { //Response to the auth request
			//	+---- + ------ +
			//	| VER | STATUS |
			//	+---- + ------ +
			//	|  1  |    1   |
			//	+---- + ------ +
			// A STATUS field of X'00' indicates success
			if (!m_pRecvBuffer)
				m_pRecvBuffer = new char[2];
			int numread = ReceiveNext(m_pRecvBuffer + m_nRecvBufferPos, 2 - m_nRecvBufferPos);
			if (numread == SOCKET_ERROR) {
				if (WSAGetLastError() != WSAEWOULDBLOCK) {
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
					TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAGetLastError(), TRUE);
					Reset();
				}
				return;
			}
			m_nRecvBufferPos += numread;
			if (m_nRecvBufferPos == 2) {
				if (m_pRecvBuffer[1] != 0) {
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_AUTHFAILED, 0);
					TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAECONNABORTED, TRUE);
					Reset();
					ClearBuffer();
					return;
				}
				const CStringA sAsciiHost(m_pProxyPeerHost);
				size_t nlen = sAsciiHost.GetLength();
				char *command = new char[10 + nlen + 1]();
				command[0] = 5;
				command[1] = static_cast<char>(m_nProxyOpID);
				//command[2]=0;
				command[3] = m_nProxyPeerIp ? 1 : 3;
				int nBufLen = 4;
				if (m_nProxyPeerIp) {
					*(ULONG*)&command[nBufLen] = m_nProxyPeerIp;
					nBufLen += 4;
				} else {
					command[nBufLen] = static_cast<char>(nlen);
					strncpy(&command[++nBufLen], sAsciiHost, nlen);
					nBufLen += (int)nlen;
				}
				*(USHORT*)&command[nBufLen] = m_nProxyPeerPort;
				nBufLen += 2;
				int res = SendNext(command, nBufLen);
				delete[] command;
				if (res == SOCKET_ERROR || res < nBufLen) {
					if ((WSAGetLastError() != WSAEWOULDBLOCK) || res < nBufLen) {
						DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
						TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAGetLastError(), TRUE);
						Reset();
						return;
					}
				}
				++m_nProxyOpState;
				ClearBuffer();
				return;
			}
		} else if (m_nProxyOpState == 3) { //Response to the connection request
			if (!m_pRecvBuffer) {
				m_pRecvBuffer = new char[10];
				m_nRecvBufferLen = 5;
			}
			int numread = ReceiveNext(m_pRecvBuffer + m_nRecvBufferPos, m_nRecvBufferLen - m_nRecvBufferPos);
			if (numread == SOCKET_ERROR) {
				if (WSAGetLastError() != WSAEWOULDBLOCK) {
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
					TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAGetLastError(), TRUE);
					Reset();
				}
				return;
			}
			m_nRecvBufferPos += numread;
			if (m_nRecvBufferPos == m_nRecvBufferLen) {
				//Check for errors
				if (m_pRecvBuffer[0] != 5 || m_pRecvBuffer[1] != 0) {
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
					TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAECONNABORTED, TRUE);
					Reset();
					ClearBuffer();
					return;
				}
				if (m_nRecvBufferLen == 5) { //Check which kind of address the response contains
					switch (m_pRecvBuffer[3]) {
					case 1: //IP V4
						m_nRecvBufferLen = 10;
						break;
					case 3: //FQDN
						{
							m_nRecvBufferLen += m_pRecvBuffer[4] + 2;
							char *tmp = new char[m_nRecvBufferLen];
							memcpy(tmp, m_pRecvBuffer, 5);
							delete[] m_pRecvBuffer;
							m_pRecvBuffer = tmp;
						}
						break;
					case 4: //IP V6
						ASSERT(0); //not tested at all!
						m_nRecvBufferLen = 22; //address is 16 bytes long.
					}
					return;
				}

				if (m_nProxyOpID == PROXYOP_CONNECT) {
					//OK, we are connected with the remote server
					Reset();
					ClearBuffer();
					TriggerEvent(FD_CONNECT, 0, TRUE);
					TriggerEvent(FD_READ, 0, TRUE);
					TriggerEvent(FD_WRITE, 0, TRUE);
				} else {
					//Listen socket created
					++m_nProxyOpState;
					ASSERT(m_pRecvBuffer[3] == 1);
					t_ListenSocketCreatedStruct data;
					data.ip = *(unsigned long*)&m_pRecvBuffer[4];
					data.nPort = (UINT)*(unsigned short*)&m_pRecvBuffer[8];
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYSTATUS_LISTENSOCKETCREATED, (LPARAM)&data);
				}
				ClearBuffer();
			}
		} else if (m_nProxyOpState == 4) {
			if (!m_pRecvBuffer)
				m_pRecvBuffer = new char[10];
			int numread = ReceiveNext(m_pRecvBuffer + m_nRecvBufferPos, 10 - m_nRecvBufferPos);
			if (numread == SOCKET_ERROR) {
				if (WSAGetLastError() != WSAEWOULDBLOCK) {
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
					TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAGetLastError(), TRUE);
					Reset();
				}
				return;
			}
			m_nRecvBufferPos += numread;
			TRACE(_T("SOCKS5 response: VER=%u  REP=%u  RSV=%u  ATYP=%u  BND.ADDR=%s  BND.PORT=%u\n"), (BYTE)m_pRecvBuffer[0], (BYTE)m_pRecvBuffer[1], (BYTE)m_pRecvBuffer[2], (BYTE)m_pRecvBuffer[3], (LPCTSTR)ipstr(*(u_long*)&m_pRecvBuffer[4]), ntohs(*(u_short*)&m_pRecvBuffer[8]));
			if (m_nRecvBufferPos == 10) {
				if (m_pRecvBuffer[1] != 0) {
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0, (LPSTR)(LPCSTR)GetSocks5Error(m_pRecvBuffer[1]));
					if (m_nProxyOpID == PROXYOP_CONNECT)
						TriggerEvent(FD_CONNECT, WSAECONNABORTED, TRUE);
					else {
						ASSERT(m_nProxyOpID == PROXYOP_BIND);
						TriggerEvent(FD_ACCEPT, WSAECONNABORTED, TRUE);
					}
					Reset();
					ClearBuffer();
					return;
				}
				//Connection to remote server established
				Reset();
				ClearBuffer();
				TriggerEvent(FD_ACCEPT, 0, TRUE);
				TriggerEvent(FD_READ, 0, TRUE);
				TriggerEvent(FD_WRITE, 0, TRUE);
			}
		}
	}
	if (m_ProxyData.nProxyType == PROXYTYPE_HTTP10 || m_ProxyData.nProxyType == PROXYTYPE_HTTP11) {
		ASSERT(m_nProxyOpID == PROXYOP_CONNECT);
		char buffer[9];
		for (;;) {
			int numread = ReceiveNext(buffer, m_pStrBuffer ? 1 : 8);
			if (numread == SOCKET_ERROR) {
				nErrorCode = WSAGetLastError();
				if (nErrorCode != WSAEWOULDBLOCK) {
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
					Reset();
					ClearBuffer();
					TriggerEvent(FD_CONNECT, nErrorCode, TRUE);
				}
				return;
			}

			buffer[numread] = '\0';
			size_t nLen1 = strlen(buffer) + 1;
			if (!m_pStrBuffer) {
				m_pStrBuffer = new char[nLen1];
				strcpy_s(m_pStrBuffer, nLen1, buffer);
			} else {
				char *tmp = m_pStrBuffer;
				size_t nBufLen = strlen(tmp) + nLen1;
				m_pStrBuffer = new char[nBufLen];
				strcpy_s(m_pStrBuffer, nBufLen, tmp);
				strcpy_s(m_pStrBuffer + nBufLen - nLen1, nLen1, buffer);
				delete[] tmp;
			}
			//Response begins with HTTP/
			static const char start[] = "HTTP/";
			if (memcmp(start, m_pStrBuffer, mini((sizeof start - 1), strlen(m_pStrBuffer))) != 0) {
				DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0, "No valid HTTP response");
				Reset();
				ClearBuffer();
				TriggerEvent(FD_CONNECT, WSAECONNABORTED, TRUE);
				return;
			}
			char *pos = strstr(m_pStrBuffer, "\r\n");
			if (pos) {
				char *pos2 = strchr(m_pStrBuffer, ' ');
				if (!pos2 || pos2[1] != '2' || pos2 > pos) {
					CStringA serr(m_pStrBuffer, (int)(pos - m_pStrBuffer));
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0, (LPSTR)(LPCSTR)serr);
					Reset();
					ClearBuffer();
					TriggerEvent(FD_CONNECT, WSAECONNABORTED, TRUE);
					return;
				}
			}
			size_t slen = strlen(m_pStrBuffer);
			if (slen > 3 && !memcmp(m_pStrBuffer + slen - 4, "\r\n\r\n", 4)) { //End of the HTTP header
				Reset();
				ClearBuffer();
				TriggerEvent(FD_CONNECT, 0, TRUE);
				TriggerEvent(FD_READ, 0, TRUE);
				TriggerEvent(FD_WRITE, 0, TRUE);
				return;
			}
		}
	}
}

bool CAsyncProxySocketLayer::Connect(const CString &sHostAddress, UINT nHostPort)
{
	if (m_ProxyData.nProxyType == PROXYTYPE_NOPROXY)
		//Connect normally because there is no proxy
		return ConnectNext(sHostAddress, nHostPort);

	//Translate the host address
	const CStringA sAscii(sHostAddress);
	ASSERT(!sAscii.IsEmpty());
	if (m_ProxyData.nProxyType != PROXYTYPE_SOCKS4) {
		// We can send hostname to proxy, no need to resolve it

		//Connect to proxy server
		bool res = ConnectNext(m_ProxyData.pProxyHost, m_ProxyData.nProxyPort);
		if (!res && WSAGetLastError() != WSAEWOULDBLOCK) {
			DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_NOCONN, 0);
			return false;
		}

		m_nProxyPeerPort = htons((u_short)nHostPort);
		m_nProxyPeerIp = 0;
		m_pProxyPeerHost = sAscii;
		m_nProxyOpID = PROXYOP_CONNECT;
		return true;
	}

	SOCKADDR_IN sockAddr = {};
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = inet_addr(sAscii);

	if (sockAddr.sin_addr.s_addr == INADDR_NONE) {
		addrinfo hints = {};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		CStringA port;
		port.Format("%u", nHostPort);
		addrinfo *ipv4;
		if (getaddrinfo(sAscii, port, &hints, &ipv4)) {
			//Can't resolve hostname
			DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_CANTRESOLVEHOST, 0);
			WSASetLastError(WSAEINVAL);
			return false;
		}
		sockAddr.sin_addr.s_addr = reinterpret_cast<LPSOCKADDR_IN>(ipv4->ai_addr)->sin_addr.s_addr;
		freeaddrinfo(ipv4);
	}

	sockAddr.sin_port = htons((u_short)nHostPort);

	bool res = Connect((LPSOCKADDR)&sockAddr, sizeof sockAddr);
	if (res || WSAGetLastError() == WSAEWOULDBLOCK)
		m_pProxyPeerHost = sAscii;

	return res;
}

BOOL CAsyncProxySocketLayer::Connect(const LPSOCKADDR lpSockAddr, int nSockAddrLen)
{
	if (m_ProxyData.nProxyType == PROXYTYPE_NOPROXY)
		//Connect normally because there is no proxy
		return ConnectNext(lpSockAddr, nSockAddrLen);

	LPSOCKADDR_IN sockAddr = (LPSOCKADDR_IN)lpSockAddr;
	//Save server details
	m_nProxyPeerIp = sockAddr->sin_addr.s_addr;
	m_nProxyPeerPort = sockAddr->sin_port;
	m_pProxyPeerHost.Empty();
	m_nProxyOpID = PROXYOP_CONNECT;

	BOOL res = ConnectNext(m_ProxyData.pProxyHost, m_ProxyData.nProxyPort);
	if (!res && WSAGetLastError() != WSAEWOULDBLOCK)
		DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_NOCONN, 0);
	return res;
}

void CAsyncProxySocketLayer::OnConnect(int nErrorCode)
{
	if (m_ProxyData.nProxyType == PROXYTYPE_NOPROXY) {
		TriggerEvent(FD_CONNECT, nErrorCode, TRUE);
		return;
	}
	if (!m_nProxyOpID) {
		ASSERT(0); //This should not happen
		return;
	}

	if (nErrorCode) { //Can't connect to proxy
		DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_NOCONN, 0);
		TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, nErrorCode, TRUE);
		Reset();
		ClearBuffer();
		return;
	}
	if ((m_nProxyOpID == PROXYOP_CONNECT || m_nProxyOpID == PROXYOP_BIND) && !m_nProxyOpState) {
		//m_nProxyOpState prevents calling OnConnect more than once
		ASSERT(m_ProxyData.nProxyType != PROXYTYPE_NOPROXY);
		ClearBuffer();

		//Send the initial request
		switch (m_ProxyData.nProxyType) {
		case PROXYTYPE_SOCKS4: //SOCKS4 proxy
		case PROXYTYPE_SOCKS4A:
			//Send request
			// SOCKS 4
			// ---------------------------------------------------------------------------
			//            +----+----+----+----+----+----+----+----+----+----+....+----+
			//            | VN | CD | DSTPORT |      DSTIP        | USERID       |NULL|
			//            +----+----+----+----+----+----+----+----+----+----+....+----+
			//# of bytes:   1    1      2              4           variable       1
			{
				const CStringA sAscii(m_pProxyPeerHost);
				ASSERT(!sAscii.IsEmpty());

				size_t nLen1 = sAscii.GetLength() + 1;
				char *command = new char[9 + nLen1]();
				int nBufLen = 9;
				command[0] = 4;
				command[1] = static_cast<char>(m_nProxyOpID); //CONNECT or BIND request
				*(USHORT*)&command[2] = m_nProxyPeerPort; //Copy target address
				if (!m_nProxyPeerIp || m_ProxyData.nProxyType == PROXYTYPE_SOCKS4A) {
					ASSERT(m_ProxyData.nProxyType == PROXYTYPE_SOCKS4A);
					// For version 4A, if the client cannot resolve the destination host's
					// domain name to find its IP address, it should set the first three bytes
					// of DSTIP to NULL and the last byte to a non-zero value. (This corresponds
					// to IP address 0.0.0.x, with x nonzero.)

					// DSTIP: Set the IP to 0.0.0.x (x is nonzero)
					//command[4]=0;
					//command[5]=0;
					//command[6]=0;
					command[7] = 1;
					//command[8]=0;	// Terminating NUL byte for USERID
					//Add host as URL
					strcpy_s(&command[9], nLen1, sAscii);
					nBufLen += (int)nLen1;
				} else
					*(ULONG*)&command[4] = m_nProxyPeerIp;
				int res = SendNext(command, nBufLen); //Send command
				delete[] command;
				if (res == SOCKET_ERROR) { //nErrorCode!=WSAEWOULDBLOCK)
					nErrorCode = WSAGetLastError();
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
					if (m_nProxyOpID == PROXYOP_CONNECT)
						TriggerEvent(FD_CONNECT, (nErrorCode == WSAEWOULDBLOCK) ? WSAECONNABORTED : nErrorCode, TRUE);
					else
						TriggerEvent(FD_ACCEPT, nErrorCode, TRUE);
					Reset();
					ClearBuffer();
					return;
				}
				if (res < nBufLen) {
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
					TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAECONNABORTED, TRUE);
					Reset();
					ClearBuffer();
					return;
				}
			}
			break;
		case PROXYTYPE_SOCKS5: //SOCKS5 proxy
			// -------------------------------------------------------------------------------------------
			// The client connects to the server, and sends a version identifier/method selection message:
			//                +----+----------+----------+
			//                |VER | NMETHODS | METHODS  |
			//                +----+----------+----------+
			//                | 1  |    1     | 1 to 255 |
			//                +----+----------+----------+
			//
			// The values currently defined for METHOD are:
			//
			//       o  X'00' NO AUTHENTICATION REQUIRED
			//       o  X'01' GSSAPI
			//       o  X'02' USERNAME/PASSWORD
			//       o  X'03' to X'7F' IANA ASSIGNED
			//       o  X'80' to X'FE' RESERVED FOR PRIVATE METHODS
			//       o  X'FF' NO ACCEPTABLE METHODS

			//Send initialization request
			//CAsyncProxySocketLayer supports two logon types: No logon and
			//clear text username/password (if set) logon
			//unsigned char command[10] = { 5 };
			//command[1] = m_ProxyData.bUseLogon ? 2 : 1; //Number of logon types
			//command[2] = m_ProxyData.bUseLogon ? 2 : 0; //2=user/pass, 0=no logon
			{
				int nBufLen = m_ProxyData.bUseLogon ? 4 : 3; //length of request
				const char *command = (m_ProxyData.bUseLogon ? "\5\2\2" : "\5\1");
				int res = SendNext(command, nBufLen);

				if (res == SOCKET_ERROR) { //nErrorCode!=WSAEWOULDBLOCK)
					nErrorCode = WSAGetLastError();
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
					if (m_nProxyOpID == PROXYOP_CONNECT)
						TriggerEvent(FD_CONNECT, (nErrorCode == WSAEWOULDBLOCK) ? WSAECONNABORTED : nErrorCode, TRUE);
					else
						TriggerEvent(FD_ACCEPT, nErrorCode, TRUE);
					Reset();
					ClearBuffer();
					return;
				}
				if (res < nBufLen) {
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
					TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAECONNABORTED, TRUE);
					Reset();
					ClearBuffer();
					return;
				}
			}
			break;
		case PROXYTYPE_HTTP10:
		case PROXYTYPE_HTTP11:
			{
				CStringA pHost;
				if (!m_pProxyPeerHost.IsEmpty())
					pHost = m_pProxyPeerHost;
				else
					pHost.Format("%lu.%lu.%lu.%lu", m_nProxyPeerIp & 0xff, (m_nProxyPeerIp >> 8) & 0xff, (m_nProxyPeerIp >> 16) & 0xff, m_nProxyPeerIp >> 24);

				CStringA sconn;
				if (!m_ProxyData.bUseLogon) {
					if (m_ProxyData.nProxyType == PROXYTYPE_HTTP10)
						// The reason why we offer HTTP/1.0 support is just because it
						// allows us to *not *send the "Host" field, thus saving overhead.
						sconn.Format(
							"CONNECT %s:%u HTTP/1.0\r\n"
							"\r\n"
							, (LPCSTR)pHost, ntohs(m_nProxyPeerPort));
					else
						// "Host" field is a MUST for HTTP/1.1 according to RFC 2161
						sconn.Format(
							"CONNECT %s:%u HTTP/1.1\r\n"
							"Host: %s:%u\r\n\r\n"
							, (LPCSTR)pHost, ntohs(m_nProxyPeerPort), (LPCSTR)pHost, ntohs(m_nProxyPeerPort));
				} else {
					CStringA userpass, base64str;
					userpass.Format("%s:%s", (LPCSTR)(CStringA)m_ProxyData.pProxyUser, (LPCSTR)(CStringA)m_ProxyData.pProxyPass);

					int base64Length = Base64EncodeGetRequiredLength(userpass.GetLength(), ATL_BASE64_FLAG_NOCRLF);
					if (!Base64Encode(reinterpret_cast<const BYTE*>((LPCSTR)userpass), userpass.GetLength()
						, base64str.GetBuffer(base64Length), &base64Length, ATL_BASE64_FLAG_NOCRLF))
					{
						base64str.ReleaseBuffer(0);
						DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
						TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAECONNABORTED, TRUE);
						Reset();
						ClearBuffer();
						return;
					}
					base64str.ReleaseBuffer(base64Length);
					if (m_ProxyData.nProxyType == PROXYTYPE_HTTP10) {
						// The reason why we offer HTTP/1.0 support is just because
						// it allows us to *not *send the "Host" field, thus saving overhead.
						sconn.Format(
							"CONNECT %s:%u HTTP/1.0\r\n"
							"Authorization: Basic %s\r\n"
							"Proxy-Authorization: Basic %s\r\n"
							"\r\n"
							, (LPCSTR)pHost, ntohs(m_nProxyPeerPort)
							, (LPCSTR)base64str, (LPCSTR)base64str);
					} else {
						// "Host" field is a MUST for HTTP/1.1 according to RFC 2161
						sconn.Format(
							"CONNECT %s:%u HTTP/1.1\r\n"
							"Host: %s:%u\r\n"
							"Authorization: Basic %s\r\n"
							"Proxy-Authorization: Basic %s\r\n"
							"\r\n"
							, (LPCSTR)pHost, ntohs(m_nProxyPeerPort)
							, (LPCSTR)pHost, ntohs(m_nProxyPeerPort)
							, (LPCSTR)base64str, (LPCSTR)base64str);
					}
				}

				int numsent = SendNext(sconn, sconn.GetLength());
				if (numsent == SOCKET_ERROR) { //nErrorCode!=WSAEWOULDBLOCK)
					nErrorCode = WSAGetLastError();
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
					if (m_nProxyOpID == PROXYOP_CONNECT)
						TriggerEvent(FD_CONNECT, (nErrorCode == WSAEWOULDBLOCK) ? WSAECONNABORTED : nErrorCode, TRUE);
					else
						TriggerEvent(FD_ACCEPT, nErrorCode, TRUE);
					Reset();
					ClearBuffer();
					return;
				}
				if (numsent < sconn.GetLength()) {
					DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_REQUESTFAILED, 0);
					TriggerEvent((m_nProxyOpID == PROXYOP_CONNECT) ? FD_CONNECT : FD_ACCEPT, WSAECONNABORTED, TRUE);
					Reset();
					ClearBuffer();
					return;
				}
			}
			break;
		default:
			ASSERT(0);
		}
		//Now we'll wait for the response, handled in OnReceive
		++m_nProxyOpState;
	}
}

void CAsyncProxySocketLayer::ClearBuffer()
{
	delete[] m_pStrBuffer;
	m_pStrBuffer = NULL;
	delete[] m_pRecvBuffer;
	m_pRecvBuffer = NULL;
	m_nRecvBufferLen = 0;
	m_nRecvBufferPos = 0;
}

BOOL CAsyncProxySocketLayer::Listen(int nConnectionBacklog)
{
	if (GetProxyType() == PROXYTYPE_NOPROXY)
		return ListenNext(nConnectionBacklog);

	//Connect to proxy server
	bool res = ConnectNext(m_ProxyData.pProxyHost, m_ProxyData.nProxyPort);
	if (res || WSAGetLastError() == WSAEWOULDBLOCK) {
		m_nProxyPeerPort = 0;
		m_nProxyPeerIp = (ULONG)nConnectionBacklog;
		m_nProxyOpID = PROXYOP_BIND;
		return TRUE;
	}
	DoLayerCallback(LAYERCALLBACK_LAYERSPECIFIC, PROXYERROR_NOCONN, 0);
	return FALSE;
}

bool CAsyncProxySocketLayer::GetPeerName(CString &rPeerAddress, UINT &rPeerPort)
{
	if (m_ProxyData.nProxyType == PROXYTYPE_NOPROXY)
		return GetPeerNameNext(rPeerAddress, rPeerPort);
	if (GetLayerState() == notsock) {
		WSASetLastError(WSAENOTSOCK);
		return false;
	}
	if (GetLayerState() != connected) {
		WSASetLastError(WSAENOTCONN);
		return false;
	}
	if (!m_nProxyPeerIp || !m_nProxyPeerPort) {
		WSASetLastError(WSAENOTCONN);
		return false;
	}
	ASSERT(m_ProxyData.nProxyType);
	bool res = GetPeerNameNext(rPeerAddress, rPeerPort);
	if (res) {
		rPeerPort = ntohs(m_nProxyPeerPort);
		rPeerAddress.Format(_T("%lu.%lu.%lu.%lu"), m_nProxyPeerIp & 0xff, (m_nProxyPeerIp >> 8) & 0xff, (m_nProxyPeerIp >> 16) & 0xff, m_nProxyPeerIp >> 24);
	}
	return res;
}

BOOL CAsyncProxySocketLayer::GetPeerName(LPSOCKADDR lpSockAddr, int *lpSockAddrLen)
{
	if (m_ProxyData.nProxyType == PROXYTYPE_NOPROXY)
		return GetPeerNameNext(lpSockAddr, lpSockAddrLen);
	if (GetLayerState() == notsock) {
		WSASetLastError(WSAENOTSOCK);
		return false;
	}
	if (GetLayerState() != connected) {
		WSASetLastError(WSAENOTCONN);
		return false;
	}
	if (!m_nProxyPeerIp || !m_nProxyPeerPort) {
		WSASetLastError(WSAENOTCONN);
		return false;
	}
	ASSERT(m_ProxyData.nProxyType);
	bool res = GetPeerNameNext(lpSockAddr, lpSockAddrLen);
	if (res) {
		LPSOCKADDR_IN addr = (LPSOCKADDR_IN)lpSockAddr;
		addr->sin_port = m_nProxyPeerPort;
		addr->sin_addr.s_addr = m_nProxyPeerIp;
	}
	return res;
}

int CAsyncProxySocketLayer::GetProxyType() const
{
	return m_ProxyData.nProxyType;
}

void CAsyncProxySocketLayer::Close()
{
	m_ProxyData.pProxyUser.Empty();
	m_ProxyData.pProxyPass.Empty();
	m_pProxyPeerHost.Empty();
	Reset();
	ClearBuffer();
	CloseNext();
}

void CAsyncProxySocketLayer::Reset()
{
	m_nProxyOpState = 0;
	m_nProxyOpID = 0;
}

int CAsyncProxySocketLayer::Send(const void *lpBuf, int nBufLen, int nFlags)
{
	if (!m_nProxyOpID)
		return SendNext(lpBuf, nBufLen, nFlags);
	WSASetLastError(WSAEWOULDBLOCK);
	return SOCKET_ERROR;
}

int CAsyncProxySocketLayer::Receive(void *lpBuf, int nBufLen, int nFlags)
{
	if (!m_nProxyOpID)
		return ReceiveNext(lpBuf, nBufLen, nFlags);
	WSASetLastError(WSAEWOULDBLOCK);
	return SOCKET_ERROR;
}

BOOL CAsyncProxySocketLayer::PrepareListen(unsigned long ip)
{
	if (GetLayerState() != notsock && GetLayerState() != unconnected)
		return FALSE;
	m_nProxyPeerIp = ip;
	return TRUE;
}

BOOL CAsyncProxySocketLayer::Accept(CAsyncSocketEx &rConnectedSocket, LPSOCKADDR lpSockAddr /*=NULL*/, int *lpSockAddrLen /*=NULL*/)
{
	if (m_ProxyData.nProxyType == PROXYTYPE_NOPROXY)
		return AcceptNext(rConnectedSocket, lpSockAddr, lpSockAddrLen);
	GetPeerName(lpSockAddr, lpSockAddrLen);
	return TRUE;
}

CString GetProxyError(int nErrorCode)
{
	LPCTSTR p;
	switch (nErrorCode) {
	case PROXYERROR_NOERROR:
		p = _T("No proxy error");
		break;
	case PROXYERROR_NOCONN:
		p = _T("Proxy connection failed");
		break;
	case PROXYERROR_REQUESTFAILED:
		p = _T("Proxy request failed");
		break;
	case PROXYERROR_AUTHREQUIRED:
	case PROXYERROR_AUTHNOLOGON:
		p = _T("Proxy authentication required");
		break;
	case PROXYERROR_AUTHTYPEUNKNOWN:
		p = _T("Proxy authentication not supported");
		break;
	case PROXYERROR_AUTHFAILED:
		p = _T("Proxy authentication failed");
		break;
	//case PROXYERROR_AUTHNOLOGON:
	//	p = _T("Proxy authentication required");
	//	break;
	case PROXYERROR_CANTRESOLVEHOST:
		p = _T("Proxy hostname not resolved");
		break;
	case PROXYSTATUS_LISTENSOCKETCREATED:
		p = _T("Proxy listen socket created");
		break;
	default:
		CString strError;
		strError.Format(_T("Proxy-Error: %i"), nErrorCode);
		return strError;
	}
	return CString(p);
}