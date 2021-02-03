/*---------------------------------------------------------------------
*
*  Some code in this file has been copied from a ping demo that was
*  created by Bob Quinn, 1997          http://www.sockets.com
*
* As some general documentation about how the ping is implemented,
* here is the description Bob Quinn wrote about the ping demo.
*
* <--- Cut --->
*
* Description:
*  This is a ping program that uses Microsoft's ICMP.DLL functions
*  for access to Internet Control Message Protocol.  This is capable
*  of doing "ping or "traceroute", although beware that Microsoft
*  discourages the use of these APIs.
*
*  Tested with MSVC 5 compile with "cl ms_icmp.c /link ws2_32.lib"
*  from the console (if you've run VCVARS32.BAT batch file that
*  ships with MSVC to set the proper environment variables)
*
* NOTES:
* - With both "Don't Fragment" and "Router Alert" set, the
*    IP don't fragment bit is set, but not the router alert option.
* - The ICMP.DLL calls are not redirected to the TCP/IP service
*    provider (what's interesting about this is that the setsockopt()
*    IP_OPTION flag can't do Router Alert, but this API can... hmmm.
* - Although the IcmpSendEcho() docs say it can return multiple
*    responses, if I receive multiple responses (e.g. sending to
*    a limited or subnet broadcast address) IcmpSendEcho() only
*    returns one.  Interesting that NT4 and Win98 don't respond
*    to broadcast pings.
* - using winsock.h  WSOCK32.LIB and version 1.1 works as well as
*    using winsock2.h WS2_32.LIB  and version 2.2
*
* Some Background:
*
* The standard Berkeley Sockets SOCK_RAW socket type, is normally used
* to create ping (echo request/reply), and sometimes traceroute applications
* (the original traceroute application from Van Jacobson used UDP, rather
* than ICMP). Microsoft's WinSock version 2 implementations for NT4 and
* Windows 95 support raw sockets, but none of their WinSock version 1.1
* implementations (WFWG, NT3.x or standard Windows 95) did.
*
* Microsoft has their own API for an ICMP.DLL that their ping and tracert
* applications use (by the way, they are both non-GUI text-based console
* applications. This is a proprietary API, and all function calls that
* involve network functions operate in blocking mode. They still include
* it with WinSock 2 implementations.
*
* There is little documentation available (I first found it in the Win32
* SDK in \MSTOOLS\ICMP, and it exists on the MS Developers' Network
* CD-ROM now, also). Microsoft disclaims this API about as strongly as
* possible.  The README.TXT that accompanies it says:
*
* [DISCLAIMER]
*
* We have had requests in the past to expose the functions exported from
* icmp.dll. The files in this directory are provided for your convenience
* in building applications which make use of ICMPSendEcho(). Notice that
* the functions in icmp.dll are not considered part of the Win32 API and
* will not be supported in future releases. Once we have a more complete
* solution in the operating system, this DLL, and the functions it exports,
* will be dropped.
*
* [DOCUMENTATION]
*
* The ICMPSendEcho() function sends an ICMP echo request to the specified
* destination IP address and returns any replies received within the timeout
* specified. The API is synchronous, requiring the process to spawn a thread
* before calling the API to avoid blocking. An open IcmpHandle is required
* for the request to complete. IcmpCreateFile() and IcmpCloseHandle()
* functions are used to create and destroy the context handle.
*
* <--- End cut --->
*/

/*---------------------------------------------------------------------
* Reworked (tag UDPing) to use UDP packets and ICMP_TTL_EXPIRE responses
* to avoid problems with some ISPs disallowing or limiting ICMP_ECHO
* (ping) requests. This has downside on NT based systems - raw sockets
* can be used only by administrative users; thereby current code has to
* implement both approaches.
*
* Example code and ideas used from (credits go to articles authors):
* http://tangentsoft.net/wskfaq/examples/rawping.html
* http://www.cs.concordia.ca/~teaching/comp445/labs/webpage/ch16.htm
* http://www.networksorcery.com/enp/protocol/icmp.htm
*
* NB! Take care about winsock libraries - look below for IP_TTL!
*
* Partial copyright (c) 2003 by DonQ
*/

#include "stdafx.h"
#include "emule.h"
#include "TimeTick.h"
#include "Pinger.h"
#include "emuledlg.h"
#include "OtherFunctions.h"
#include "opcodes.h"

extern CString GetErrorMessage(DWORD dwError, DWORD dwFlags);

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#define BUFSIZE		8192
#define DEFAULT_LEN	0
#define TIMEOUT		SEC2MS(3)

/*---------------------------------------------------------
 * IcmpSendEcho() Error Strings
 *
 * The values in the status word returned in the ICMP Echo
 *  Reply buffer after calling IcmpSendEcho() all have a
 *  base value of 11000 (IP_STATUS_BASE).  At times,
 *  when IcmpSendEcho() fails outright, GetLastError() will
 *  subsequently return these error values also.
 *
 * Two Errors value defined in ms_icmp.h are missing from
 *  this string table (just to simplify use of the table):
 *    "IP_GENERAL_FAILURE (11050)"
 *    "IP_PENDING (11255)"
 */
 //#define MAX_ICMP_ERR_STRING  (IP_STATUS_BASE + 22) -- use _countof() instead
static LPCTSTR const aszSendEchoErr[] =
{
	_T("IP_STATUS_BASE (11000)"),
	_T("IP_BUF_TOO_SMALL (11001)"),
	_T("IP_DEST_NET_UNREACHABLE (11002)"),
	_T("IP_DEST_HOST_UNREACHABLE (11003)"),
	_T("IP_DEST_PROT_UNREACHABLE (11004)"),
	_T("IP_DEST_PORT_UNREACHABLE (11005)"),
	_T("IP_NO_RESOURCES (11006)"),
	_T("IP_BAD_OPTION (11007)"),
	_T("IP_HW_ERROR (11008)"),
	_T("IP_PACKET_TOO_BIG (11009)"),
	_T("IP_REQ_TIMED_OUT (11010)"),
	_T("IP_BAD_REQ (11011)"),
	_T("IP_BAD_ROUTE (11012)"),
	_T("IP_TTL_EXPIRED_TRANSIT (11013)"),
	_T("IP_TTL_EXPIRED_REASSEM (11014)"),
	_T("IP_PARAM_PROBLEM (11015)"),
	_T("IP_SOURCE_QUENCH (11016)"),
	_T("IP_OPTION_TOO_BIG (11017)"),
	_T("IP_BAD_DESTINATION (11018)"),
	_T("IP_ADDR_DELETED (11019)"),
	_T("IP_SPEC_MTU_CHANGE (11020)"),
	_T("IP_MTU_CHANGE (11021)"),
	_T("IP_UNLOAD (11022)")
};

Pinger::Pinger()
	: us(INVALID_SOCKET)
	, udpStarted()
{
	// udp start
	sockaddr_in sa;		// for UDP and raw sockets

	// ICMP must accept all responses
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = INADDR_ANY;
	sa.sin_port = 0;

	// attempt to initialize raw ICMP socket
	is = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (is != INVALID_SOCKET) {
		if (bind(is, (sockaddr*)&sa, sizeof sa) == SOCKET_ERROR) {
			//WSAGetLastError(); - not using this errpr code
			closesocket(is);		// ignore return value - error close anyway
		} else {
			// attempt to initialize ordinary UDP socket - why should this fail???
			// NB! no need to bind this at a moment - will be bound later, implicitly at sendto
			us = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (us == INVALID_SOCKET)
				closesocket(is);		// ignore return value - we need to close it anyway!
			else
				udpStarted = true;
		}
	}
	// udp end

	// Open ICMP.DLL
	hICMP_DLL = LoadLibrary(_T("ICMP.DLL"));
	if (hICMP_DLL == 0) {
		theApp.QueueDebugLogLine(false, _T("Pinger: LoadLibrary() failed: Unable to locate ICMP.DLL!"));
		return;
	}

	// Get pointers to ICMP.DLL functions
	lpfnIcmpCreateFile = (IcmpCreateFile*)GetProcAddress(hICMP_DLL, "IcmpCreateFile");
	lpfnIcmpCloseHandle = (IcmpCloseHandle*)GetProcAddress(hICMP_DLL, "IcmpCloseHandle");
	lpfnIcmpSendEcho = (IcmpSendEcho*)GetProcAddress(hICMP_DLL, "IcmpSendEcho");
	if (!lpfnIcmpCreateFile || !lpfnIcmpCloseHandle || !lpfnIcmpSendEcho) {
		theApp.QueueDebugLogLine(false, _T("Pinger: GetProcAddr() failed for at least one function."));
		return;
	}

	// Open the ping service
	hICMP = (HANDLE)lpfnIcmpCreateFile();
	if (hICMP == INVALID_HANDLE_VALUE) {
		DWORD nErr = ::GetLastError();
		theApp.QueueDebugLogLine(false, _T("Pinger: IcmpCreateFile() failed, err: %u"), nErr);
		PIcmpErr(nErr);
		return;
	}

	// Init IPInfo structure
	stIPInfo.Tos = 0;
	stIPInfo.Flags = 0;
	stIPInfo.OptionsSize = 0;
	stIPInfo.OptionsData = NULL;
}

Pinger::~Pinger()
{
// UDPing reworked cleanup -->
	if (udpStarted) {
		closesocket(is);		// close raw ICMP socket
		closesocket(us);		// close UDP socket
	}
// UDPing reworked cleanup end <--

	// Close the ICMP handle
	if (!lpfnIcmpCloseHandle(hICMP)) {
		DWORD nErr = ::GetLastError();
		theApp.QueueDebugLogLine(false, _T("Error closing ICMP handle, err: %u"), nErr);
		PIcmpErr(nErr);
	}

	// Shut down...
	FreeLibrary(hICMP_DLL);
}

PingStatus Pinger::Ping(uint32 lAddr, uint32 ttl, bool doLog, bool useUdp)
{
	return (useUdp && udpStarted) ? PingUDP(lAddr, ttl, doLog) : PingICMP(lAddr, ttl, doLog);
}

PingStatus Pinger::PingUDP(uint32 lAddr, uint32 ttl, bool doLog)
{
	// UDPing reworked ping sequence -->
	int nTTL = ttl;
	sockaddr_in sa;
	int nAddrLen = sizeof(sockaddr_in);
	char bufICMP[1500];				// allow full MTU

	// clear ICMP socket before sending UDP - not best solution, but may be needed to exclude late responses etc
	u_long bytes2read = 0;
	while (!ioctlsocket(is, FIONREAD, &bytes2read) && bytes2read) {
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = INADDR_ANY;
		sa.sin_port = 0;

		recvfrom(is				/* socket */
			, (LPSTR)bufICMP	/* buffer */
			, 1500				/* length */
			, 0					/* flags  */
			, (sockaddr*)&sa	/* source */
			, &nAddrLen);		/* addrlen*/
	}

	// set TTL value for UDP packet - should success with winsock 2
	// NB! take care about IP_TTL value - it's redefined in Ws2tcpip.h!
	// TODO: solve next problem correctly:
	// eMule is linking sockets functions using wsock32.lib (IP_TTL=7)
	// to use IP_TTL define, we must enforce linker to bind this function
	// to ws2_32.lib (IP_TTL=4) (linker options: ignore wsock32.lib)
	int nRet = setsockopt(us, IPPROTO_IP, IP_TTL, (char*)&nTTL, sizeof(int));
	if (nRet == SOCKET_ERROR) {
		PingStatus returnValue;
		returnValue.success = false;
		returnValue.delay = TIMEOUT;
		returnValue.error = WSAGetLastError();
		//if (toNowTimeOut < 3)
		//	++toNowTimeOut;
		//lastTimeOut = 3;
		return returnValue;
	}

	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = lAddr;
	sa.sin_port = htons(UDP_PORT);

	// send lonely UDP packet with almost minimal content (0 bytes is allowed too, but no data will be sent then)
	nRet = sendto(us, (LPSTR)&nTTL, 4, 0, (sockaddr*)&sa, sizeof sa);  // send four bytes - TTL :)
	CTimeTick m_time;
	m_time.Tick();
	if (nRet == SOCKET_ERROR) {
		PingStatus returnValue;
		returnValue.success = false;
		returnValue.error = WSAGetLastError();
		//if (toNowTimeOut < 3)
		//	++toNowTimeOut;
		//lastTimeOut = 3;
		return returnValue;
	}

	IPHeader *reply = reinterpret_cast<IPHeader*>(bufICMP);

	bytes2read = 0;
	int timeoutOpt = TIMEOUT;
	bool noRcvTimeOut = false;
	nRet = setsockopt(is, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeoutOpt, sizeof timeoutOpt);
	if (nRet == SOCKET_ERROR)
		noRcvTimeOut = true;

	float usResTime = 0.0f;
	while ((usResTime += m_time.Tick()) < TIMEOUT) {
		if (noRcvTimeOut) {
			nRet = ioctlsocket(is, FIONREAD, &bytes2read);
			if (nRet != 0) {
				PingStatus returnValue;
				returnValue.success = false;
				returnValue.delay = TIMEOUT;
				returnValue.error = WSAGetLastError();
				//if (toNowTimeOut < 3)
				//	++toNowTimeOut;
				//lastTimeOut = 3;
				return returnValue;
			}
			if (bytes2read <= 0) {
				::Sleep(1);		// share time with other threads
				continue;
			}
		}
		// read and filter incoming ICMP
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = INADDR_ANY;
		sa.sin_port = 0;
		nRet = recvfrom(is		/* socket */
			, bufICMP			/* buffer */
			, 1500				/* length */
			, 0					/* flags  */
			, (sockaddr*)&sa	/* source */
			, &nAddrLen);		/* addrlen*/

		usResTime += m_time.Tick();
		if (nRet == SOCKET_ERROR) {
			PingStatus returnValue;
			returnValue.success = false;
			returnValue.delay = TIMEOUT;
			returnValue.error = WSAGetLastError();
			//if (toNowTimeOut < 3)
			//	++toNowTimeOut;
			//lastTimeOut = 3;
			return returnValue;
		}

		unsigned short header_len = reply->h_len * 4;
		ICMPHeader *icmphdr = reinterpret_cast<ICMPHeader*>(bufICMP + header_len);
		IN_ADDR stDestAddr;

		stDestAddr.s_addr = reply->source_ip;

		if ((icmphdr->type == ICMP_T_TTL_EXPIRE || icmphdr->type == ICMP_T_DEST_UNREACH)
			&& icmphdr->UDP.dest_port == htons(UDP_PORT) && icmphdr->hdrsent.dest_ip == lAddr)
		{
			PingStatus returnValue;
			returnValue.success = true;
			returnValue.delay = usResTime;
			returnValue.destinationAddress = stDestAddr.s_addr;
			if (icmphdr->type == ICMP_T_TTL_EXPIRE) {
				returnValue.status = IP_TTL_EXPIRED_TRANSIT;
				returnValue.ttl = ttl;
			} else {
				returnValue.status = IP_DEST_HOST_UNREACHABLE;
				returnValue.ttl = 64 - (reply->ttl & 63);
			}

			if (doLog) {
				theApp.QueueDebugLogLine(false
					, _T("Reply (UDP-pinger) from %s: bytes=%d time=%3.2fms TTL=%i")
					, (LPCTSTR)ipstr(stDestAddr)
					, nRet
					, usResTime
					, returnValue.ttl);
			}
			return returnValue;
		}
		// verbose log filtered packets info (not seen yet...)
		//if (lastTimeOut)
		//	--lastTimeOut;
		//if (!lastTimeOut && toNowTimeOut) {
		//		--toNowTimeOut;
		//		if (toNowTimeOut)
		//			lastTimeOut = 3;
		//}
		if (doLog)
			theApp.QueueDebugLogLine(false
				, _T("Filtered reply (UDP-pinger) from %s: bytes=%d time=%3.2fms TTL=%i type=%i")
				, (LPCTSTR)ipstr(stDestAddr)
				, nRet
				, usResTime
				, 64 - (reply->ttl & 63)
				, icmphdr->type);
	}
	//if (usResTime >= TIMEOUT) {
	//	if (toNowTimeOut < 3)
	//		++toNowTimeOut;
	//	lastTimeOut = 3;
	//}
	// UDPing reworked ping sequence end <--

	PingStatus returnValue;
	returnValue.success = false;
	returnValue.delay = TIMEOUT;
	returnValue.error = IP_REQ_TIMED_OUT;

	return returnValue;
}

PingStatus Pinger::PingICMP(uint32 lAddr, uint32 ttl, bool doLog)
{
	PingStatus returnValue;

	IN_ADDR stDestAddr;
	char achRepData[sizeof(icmp_echo_reply) + BUFSIZE];

	// Address is assumed to be OK
	stDestAddr.s_addr = lAddr;
	stIPInfo.Ttl = (UCHAR)ttl;

	CTimeTick m_time;
	m_time.Tick();
	// Send the ICMP Echo Request and read the Reply
	DWORD dwReplyCount = lpfnIcmpSendEcho(hICMP
		, stDestAddr.s_addr
		, NULL // data buffer
		, 0 // DataLen, length of data buffer
		, &stIPInfo
		, achRepData
		, sizeof achRepData
		, TIMEOUT);
	float usResTime = m_time.Tick();
	if (dwReplyCount != 0) {
		const PICMP_ECHO_REPLY preply = reinterpret_cast<PICMP_ECHO_REPLY>(achRepData);
		long pingTime = preply->RoundTripTime;

		IN_ADDR stDestAddr1;

		stDestAddr1.s_addr = preply->Address;

		returnValue.success = true;
		returnValue.status = preply->Status;
		returnValue.delay = (m_time.isPerformanceCounter() && (pingTime <= 20 || pingTime % 10 == 0) && (pingTime + 10 > usResTime && usResTime + 10 > pingTime)) ? usResTime : pingTime;
		returnValue.destinationAddress = stDestAddr1.s_addr;
		returnValue.ttl = (returnValue.status != IP_SUCCESS) ? ttl : (preply->Options).Ttl;

		if (doLog) {
			theApp.QueueDebugLogLine(false, _T("Reply (ICMP-pinger) from %s: bytes=%hu time=%3.2fms (%3.2fms %lums) TTL=%u")
				, (LPCTSTR)ipstr(stDestAddr1)
				, preply->DataSize
				, returnValue.delay
				, m_time.isPerformanceCounter() ? usResTime : -1.0f
				, preply->RoundTripTime
				, returnValue.ttl);
		}
	} else {
		returnValue.success = false;
		returnValue.error = ::GetLastError();
		if (doLog)
			theApp.QueueDebugLogLine(false
				, _T("Error from %s: Error=%u")
				, (LPCTSTR)ipstr(stDestAddr)
				, returnValue.error);

	}

	return returnValue;
}


void Pinger::PIcmpErr(DWORD nICMPErr)
{
	if (nICMPErr >= IP_STATUS_BASE && nICMPErr < IP_STATUS_BASE + _countof(aszSendEchoErr)) {
		// Display ICMP Error String
		theApp.QueueDebugLogLine(false, _T("%s"), aszSendEchoErr[nICMPErr - IP_STATUS_BASE]);
	} else {
		// Error value is out of range, use system routine
		theApp.QueueDebugLogLine(false, _T("Pinger: %s"), (LPCTSTR)GetErrorMessage(nICMPErr, 1));
	}
}