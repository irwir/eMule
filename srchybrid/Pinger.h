//this file is part of eMule
//Copyright (C)2002-2008 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

/*---------------------------------------------------------------------
*
*  Some code in this file has been copied from a ping demo that was
*  created by Bob Quinn, 1997          http://www.sockets.com
*
* As some general documenation about how the ping is implemented,
* here is the description Bob Quinn wrote about the ping demo.
*
* Description:
*  Prototypes and typedefs Microsoft's ICMP.DLL functions & structs
*  for access to Internet Control Message Protocol.  This is capable
*  of doing "ping or "traceroute", although beware that Microsoft
*  discourages the use of these APIs.
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
* functions are used to create and destroy the context handle.</P>
*/
#pragma once

#define DEFAULT_TTL 64

#define IP_STATUS_BASE 11000
#define IP_SUCCESS 0
#define IP_BUF_TOO_SMALL (IP_STATUS_BASE + 1)
#define IP_DEST_NET_UNREACHABLE (IP_STATUS_BASE + 2)
#define IP_DEST_HOST_UNREACHABLE (IP_STATUS_BASE + 3)
#define IP_DEST_PROT_UNREACHABLE (IP_STATUS_BASE + 4)
#define IP_DEST_PORT_UNREACHABLE (IP_STATUS_BASE + 5)
#define IP_NO_RESOURCES (IP_STATUS_BASE + 6)
#define IP_BAD_OPTION (IP_STATUS_BASE + 7)
#define IP_HW_ERROR (IP_STATUS_BASE + 8)
#define IP_PACKET_TOO_BIG (IP_STATUS_BASE + 9)
#define IP_REQ_TIMED_OUT (IP_STATUS_BASE + 10)
#define IP_BAD_REQ (IP_STATUS_BASE + 11)
#define IP_BAD_ROUTE (IP_STATUS_BASE + 12)
#define IP_TTL_EXPIRED_TRANSIT (IP_STATUS_BASE + 13)
#define IP_TTL_EXPIRED_REASSEM (IP_STATUS_BASE + 14)
#define IP_PARAM_PROBLEM (IP_STATUS_BASE + 15)
#define IP_SOURCE_QUENCH (IP_STATUS_BASE + 16)
#define IP_OPTION_TOO_BIG (IP_STATUS_BASE + 17)
#define IP_BAD_DESTINATION (IP_STATUS_BASE + 18)
#define IP_ADDR_DELETED (IP_STATUS_BASE + 19)
#define IP_SPEC_MTU_CHANGE (IP_STATUS_BASE + 20)
#define IP_MTU_CHANGE (IP_STATUS_BASE + 21)
#define IP_UNLOAD (IP_STATUS_BASE + 22)
#define IP_GENERAL_FAILURE (IP_STATUS_BASE + 50)
#define MAX_IP_STATUS IP_GENERAL_FAILURE
#define IP_PENDING (IP_STATUS_BASE + 255)

typedef HANDLE WINAPI IcmpCreateFile(); /* INVALID_HANDLE_VALUE on error */
typedef BOOL WINAPI IcmpCloseHandle(HANDLE IcmpHandle); /* FALSE on error */

/* Note 2: For the most part, you can refer to RFC 791 for details
* on how to fill in values for the IP option information structure.
*/
//typedef struct ip_option_information {
//    u_char Ttl;		/* Time To Live (used for traceroute) */
//    u_char Tos;		/* Type Of Service (usually 0) */
//    u_char Flags;		/* IP header flags (usually 0) */
//    u_char OptionsSize; /* Size of options data (usually 0, max 40) */
//    u_char FAR *OptionsData;   /* Options data buffer */
//} IPINFO, *PIPINFO, FAR *LPIPINFO;

/* Note 1: The Reply Buffer will have an array of ICMP_ECHO_REPLY
* structures, followed by options and the data in ICMP echo reply
* datagram received. You must have room for at least one ICMP
* echo reply structure, plus 8 bytes for an ICMP header.
*/
//typedef struct icmp_echo_reply {
//    u_long Address;	/* source address */
//    u_long Status;	/* IP status value (see below) */
//    u_long RTTime;	/* Round Trip Time in milliseconds */
//    u_short DataSize;	/* reply data size */
//    u_short Reserved;	/* */
//    void FAR *Data;	/* reply data buffer */
//    struct ip_option_information Options; /* reply options */
//} ICMPECHO, *PICMPECHO, FAR *LPICMPECHO;

typedef DWORD WINAPI IcmpSendEcho(
	HANDLE IcmpHandle,	/* handle returned from IcmpCreateFile() */
	u_long DestAddress, /* destination IP address (in network order) */
	LPVOID RequestData, /* pointer to buffer to send */
	WORD RequestSize,	/* length of data in buffer */
	PIP_OPTION_INFORMATION RequestOptns,  /* see Note 2 */
	LPVOID ReplyBuffer, /* see Note 1 */
	DWORD ReplySize,	/* length of reply (must allow at least 1 reply) */
	DWORD Timeout		/* time in milliseconds to wait for reply */
);

struct PingStatus
{
	bool success;
	DWORD status;
	float delay;
	uint32 destinationAddress;
	uint32 ttl;
	DWORD error;
};

// UDPing - required constants and structures -->

// ICMP packet types
#define ICMP_T_ECHO_REPLY 0
#define ICMP_T_DEST_UNREACH 3
#define ICMP_T_TTL_EXPIRE 11
#define ICMP_T_ECHO_REQUEST 8

// Minimum ICMP packet size, in bytes
#define ICMP_MIN 8

#ifdef _MSC_VER
// The following two structures need to be packed tightly, but unlike
// Borland C++, Microsoft C++ does not do this by default.
#pragma pack(push, 1)
#endif

// The IP header
struct IPHeader
{
	BYTE h_len:4;		// Length of the header in dwords
	BYTE version:4;		// Version of IP
	BYTE tos;			// Type of service
	USHORT total_len;	// Length of the packet in dwords
	USHORT ident;		// unique identifier
	USHORT flags;		// Flags
	BYTE ttl;			// Time to live
	BYTE proto;			// Protocol number (TCP, UDP etc)
	USHORT checksum;	// IP checksum
	ULONG source_ip;
	ULONG dest_ip;
};

// ICMP header for DEST_UNREACH and TTL_EXPIRE replys
struct ICMPHeader
{
	BYTE type;			// ICMP packet type
	BYTE code;			// Type sub code
	USHORT checksum;
	BYTE unused[4];		// may be used for various data, we don't need it
	IPHeader hdrsent;	// original IP header
	union
	{
		BYTE data[8];	// data next to IP header (UDP header)
		struct
		{
			USHORT src_port;
			USHORT dest_port;
			USHORT length;
			USHORT checksum;
		} UDP;
	};
};

#ifdef _MSC_VER
#pragma pack(pop)
#endif

#define UDP_PORT 33434  // UDP/TCP traceroute port by iana.org - should not be filtered by routers/ISP

// UDPing - required constants and structures end <--


class Pinger
{
public:
	Pinger();
	~Pinger();
	Pinger(const Pinger&) = delete;
	Pinger& operator=(const Pinger&) = delete;

	PingStatus Ping(uint32 lAddr, uint32 ttl = DEFAULT_TTL, bool doLog = false, bool useUdp = false);

	static void PIcmpErr(DWORD nICMPErr);

private:
//    void DisplayErr(int nWSAErr);

	IcmpCreateFile *lpfnIcmpCreateFile;
	IcmpCloseHandle *lpfnIcmpCloseHandle;
	IcmpSendEcho *lpfnIcmpSendEcho;
	PingStatus PingUDP(uint32 lAddr, uint32 ttl, bool doLog);
	PingStatus PingICMP(uint32 lAddr, uint32 ttl, bool doLog);

	HANDLE hICMP;
	HMODULE hICMP_DLL; // PENDING: was HANDLE
	IP_OPTION_INFORMATION stIPInfo;

	SOCKET us;          // UDP socket to send requests
	SOCKET is;          // raw ICMP socket to catch responses

	bool udpStarted;
};