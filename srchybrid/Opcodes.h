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
#pragma once

// MOD Note: Do not change this part - Merkur
#define	EMULE_PROTOCOL					0x01
// MOD Note: end
#define	EDONKEYVERSION					0x3C
#define KADEMLIA_VERSION1_46c			0x01 /*45b - 46c*/
#define KADEMLIA_VERSION2_47a			0x02 /*47a*/
#define KADEMLIA_VERSION3_47b			0x03 /*47b*/
#define KADEMLIA_VERSION4_47c			0x04 /*47c*/
#define KADEMLIA_VERSION5_48a			0x05 // -0.48a
#define KADEMLIA_VERSION6_49aBETA		0x06 // -0.49aBETA1, needs to support: OP_FWCHECKUDPREQ (!), obfuscation, direct callbacks, source type 6, UDP firewall check
#define KADEMLIA_VERSION7_49a			0x07 // -0.49a needs to support OP_KAD_FWTCPCHECK_ACK, KADEMLIA_FIREWALLED2_REQ
#define KADEMLIA_VERSION8_49b			0x08 // TAG_KADMISCOPTIONS, KADEMLIA2_HELLO_RES_ACK
#define KADEMLIA_VERSION9_50a			0x09 // handling AICH hashes on keyword storage
#define KADEMLIA_VERSION				0x0a // -Change CT_EMULE_MISCOPTIONS2 if Kad version becomes >= 15 (0x0F)
#define PREFFILE_VERSION				0x14 //<<-- last change: reduced .dat, by using .ini
#define PARTFILE_VERSION				0xe0
#define PARTFILE_SPLITTEDVERSION		0xe1
#define PARTFILE_VERSION_LARGEFILE		0xe2
#define SOURCEEXCHANGE2_VERSION			4	 // replaces the version sent in MISC_OPTIONS flag from SX1

#define CREDITFILE_VERSION		0x12
#define CREDITFILE_VERSION_29	0x11
#define COMPILE_DATE			__DATE__
#define COMPILE_TIME			__TIME__
#ifdef _DEBUG
#define EMULE_GUID				_T("EMULE-{4EADC6FC-516F-4b7c-9066-97D893649570}-DEBUG")
#else
#define EMULE_GUID				_T("EMULE-{4EADC6FC-516F-4b7c-9066-97D893649570}")
#endif

#define SEC(sec)		(sec)
#define	MIN2S(min)		((min)*60)
#define	HR2S(hr)		MIN2S((hr)*60)
#define DAY2S(day)		HR2S((day)*24)
#define	SEC2MS(sec)		((sec)*1000)
#define	MIN2MS(min)		SEC2MS(MIN2S(min))
#define	HR2MS(hr)		SEC2MS(HR2S(hr))
#define DAY2MS(day)		SEC2MS(DAY2S(day))

// MOD Note: Do not change this part - Merkur
#define UDPSEARCHSPEED			SEC2MS(1)	//1 sec - if this value is too low you will miss sources
#define MAX_RESULTS				100			// max global search results
#define	MAX_MORE_SEARCH_REQ		5			// this gives a max. total search results of (1+5)*201 = 1206 or (1+5)*300 = 1800
#define MAX_CLIENTCONNECTIONTRY	2
#define CONNECTION_TIMEOUT		SEC2MS(40)	//40 secs - set his lower if you want less connections at once, set it higher if you have enough sockets (edonkey has its own timeout too, so a very high value won't effect this)
#define	FILEREASKTIME			MIN2MS(29)	//29 mins
#define SERVERREASKTIME			MIN2MS(15)	//15 mins - don't set this too low, it wont speed up anything, but it could kill emule or your internet connection
#define UDPSERVERREASKTIME		MIN2MS(30)	//30 mins
#define	MAX_SERVERFAILCOUNT		10
#define SOURCECLIENTREASKS		MIN2MS(40)	//40 mins
#define SOURCECLIENTREASKF		MIN2MS(5)	//5 mins
#define KADEMLIAASKTIME			SEC2MS(1)	//1 second
#define KADEMLIATOTALFILE		5			//Total files to search sources for.
#define KADEMLIAREASKTIME		HR2MS(1)	//1 hour
#define KADEMLIAPUBLISHTIME		SEC(2)		//2 second
#define KADEMLIATOTALSTORENOTES	1			//Total hashes to store.
#define KADEMLIATOTALSTORESRC	4			//Total hashes to store.
#define KADEMLIATOTALSTOREKEY	3			//Total hashes to store.
#define KADEMLIAREPUBLISHTIMES	HR2S(5)		//5 hours
#define KADEMLIAREPUBLISHTIMEN	HR2S(24)	//24 hours
#define KADEMLIAREPUBLISHTIMEK	HR2S(24)	//24 hours
#define KADEMLIADISCONNECTDELAY	MIN2S(20)	//20 mins
#define	KADEMLIAMAXINDEX		50000		//Total keyword indexes.
#define	KADEMLIAMAXENTRIES		60000		//Total keyword entries.
#define KADEMLIAMAXSOURCEPERFILE 1000		//Max number of sources per file in index.
#define KADEMLIAMAXNOTESPERFILE	150			//Max number of notes per entry in index.
#define KADEMLIAFIREWALLCHECKS	4			//Firewall check requests at a time


#define ED2KREPUBLISHTIME		MIN2MS(1)	//1 min
#define MINCOMMONPENALTY		4
#define UDPSERVERSTATTIME		SEC2MS(5)	//5 secs
#define UDPSERVSTATREASKTIME	((time_t)HR2S(4.5))	//4.5 hours (A random time of up to one hour is reduced during runtime after each ping)
#define UDPSERVSTATMINREASKTIME	MIN2S(20)	//minimum time between two pings even when trying to force a premature ping for a new UDP key
#define	UDPSERVERPORT			4665		//default udp port
#define UDPMAXQUEUETIME			SEC2MS(30)	//30 Seconds
#define RSAKEYSIZE				384			//384 bits
#define	MAX_SOURCES_FILE_SOFT	750
#define	MAX_SOURCES_FILE_UDP	50u
#define SESSIONMAXTRANS			(PARTSIZE+20*1024) // "Try to send complete chunks" always sends this amount of data
#define SESSIONMAXTIME			HR2MS(1)	//1 hour
#define	MAXFILECOMMENTLEN		128
#define	PARTSIZE				9728000ui64
#define	MAX_EMULE_FILE_SIZE		0x4000000000ui64 // = 2^38 = 256GB
#define OLD_MAX_EMULE_FILE_SIZE	4290048000ui64	// (4294967295/PARTSIZE)*PARTSIZE = ~4GB
// MOD Note: end

#define CONFIGFOLDER			_T("config\\")
#define MAXCONPER5SEC			20
#define MAXCON5WIN9X			10
#define	UPLOAD_CLIENT_MAXDATARATE	(25*1024) // max. target upload speed per client
#define	MIN_UP_CLIENTS_ALLOWED	2			// min. clients allowed to download regardless of any other factors. Don't set this too high
#define	MAX_UP_CLIENTS_ALLOWED	100			// max. clients allowed regardless of any other factors; cannot be below MIN_UP_CLIENTS_ALLOWED+3
#define DOWNLOADTIMEOUT			SEC2MS(100)
#define CONSERVTIMEOUT			SEC2MS(25)	// age limit for pending connection attempts
#define RARE_FILE				50
#define BADCLIENTBAN			4
#define	MIN_REQUESTTIME			MIN2MS(10)
#define	MAX_PURGEQUEUETIME		HR2MS(1)
#define PURGESOURCESWAPSTOP		MIN2MS(15)	// 15 min - how long to forbid swapping a source to a certain file (NNP,...)
#define CONNECTION_LATENCY		22050		// latency for responses
#define MINWAIT_BEFORE_DLDISPLAY_WINDOWUPDATE   SEC2MS(1)
#define MINWAIT_BEFORE_ULDISPLAY_WINDOWUPDATE   SEC2MS(1)
#define CLIENTBANTIME			HR2MS(2)	// 2h
#define TRACKED_CLEANUP_TIME	HR2MS(1)	// 1 hour
#define KEEPTRACK_TIME			HR2MS(2)	// 2h -	how long to keep track of clients which were once in the upload queue
#define LOCALSERVERREQUESTS		SEC2MS(20)	// only one local src request during this timespan (WHERE IS THIS USED?)
#define DISKSPACERECHECKTIME	MIN2MS(15)
#define CLIENTLIST_CLEANUP_TIME	MIN2MS(34)	// 34 min
#define MAXPRIORITYCOLL_SIZE	(50*1024)	// max file size for collection file which are allowed to bypass the queue
#define SEARCH_SPAM_THRESHOLD	60
#define OLDFILES_PARTIALLYPURGE DAY2S(31)	// time after which some data about a know file in the known.met and known2.met is deleted

// you shouldn't change anything here if you are not really sure, or probably emule will not work
#define UDP_KAD_MAXFRAGMENT		1420		// based on a 1500 ethernet MTU, use a conservative value to leave enough room for IP/UDP headers, tunnel headers, Kad headers(16) and misconfigs
#define EMBLOCKSIZE				184320u
#define OP_EDONKEYHEADER		0xE3
#define OP_KADEMLIAHEADER		0xE4
#define OP_KADEMLIAPACKEDPROT	0xE5
#define OP_EDONKEYPROT			OP_EDONKEYHEADER
#define OP_PACKEDPROT			0xD4
#define OP_EMULEPROT			0xC5
#define OP_UDPRESERVEDPROT1		0xA3	// reserved for later UDP headers (important for EncryptedDatagramSocket)
#define OP_UDPRESERVEDPROT2		0xB2	// reserved for later UDP headers (important for EncryptedDatagramSocket)
#define OP_MLDONKEYPROT			0x00
#define	MET_HEADER				0x0E
#define	MET_HEADER_I64TAGS		0x0F

#define UNLIMITED				_UI32_MAX

//Proxytypes deadlake
#define PROXYTYPE_NOPROXY	0
#define PROXYTYPE_SOCKS4	1
#define PROXYTYPE_SOCKS4A	2
#define PROXYTYPE_SOCKS5	3
#define PROXYTYPE_HTTP10	4
#define PROXYTYPE_HTTP11	5

// client <-> server
#define OP_LOGINREQUEST			0x01	//<HASH 16><ID 4><PORT 2><1 Tag_set>
#define OP_REJECT				0x05	//(null)
#define OP_GETSERVERLIST		0x14	//(null)client->server
#define	OP_OFFERFILES			0x15	// <count 4>(<HASH 16><ID 4><PORT 2><1 Tag_set>)[count]
#define OP_SEARCHREQUEST		0x16	// <Query_Tree>
#define OP_DISCONNECT			0x18	// (not verified)
#define OP_GETSOURCES			0x19	// <HASH 16>
#define	OP_SEARCH_USER			0x1A	// <Query_Tree>
#define OP_CALLBACKREQUEST		0x1C	// <ID 4>
#define	OP_QUERY_CHATS			0x1D	// (deprecated not supported by server any longer)
#define OP_CHAT_MESSAGE			0x1E    // (deprecated not supported by server any longer)
#define OP_JOIN_ROOM            0x1F    // (deprecated not supported by server any longer)
#define OP_QUERY_MORE_RESULT    0x21    // (null)
#define OP_SERVERLIST			0x32	// <count 1>(<IP 4><PORT 2>)[count] server->client
#define OP_SEARCHRESULT			0x33	// <count 4>(<HASH 16><ID 4><PORT 2><1 Tag_set>)[count]
#define	OP_SERVERSTATUS			0x34	// <USER 4><FILES 4>
#define OP_CALLBACKREQUESTED	0x35	// <IP 4><PORT 2>
#define OP_CALLBACK_FAIL		0x36	// (null not verified)
#define OP_SERVERMESSAGE		0x38	// <len 2><Message len>
#define OP_CHAT_ROOM_REQUEST    0x39    // (deprecated not supported by server any longer)
#define OP_CHAT_BROADCAST       0x3A    // (deprecated not supported by server any longer)
#define OP_CHAT_USER_JOIN       0x3B    // (deprecated not supported by server any longer)
#define OP_CHAT_USER_LEAVE      0x3C    // (deprecated not supported by server any longer)
#define OP_CHAT_USER            0x3D    // (deprecated not supported by server any longer)
#define OP_IDCHANGE				0x40	// <NEW_ID 4><server_flags 4><primary_tcp_port 4 (unused)><client_IP_address 4>
#define OP_SERVERIDENT		    0x41	// <HASH 16><IP 4><PORT 2>{1 TAG_SET}
#define OP_FOUNDSOURCES			0x42	// <HASH 16><count 1>(<ID 4><PORT 2>)[count]
#define OP_USERS_LIST           0x43    // <count 4>(<HASH 16><ID 4><PORT 2><1 Tag_set>)[count]
#define OP_GETSOURCES_OBFU		0x23
#define OP_FOUNDSOURCES_OBFU	0x44    // <HASH 16><count 1>(<ID 4><PORT 2><obf settings 1>(UserHash16 if obf&0x08))[count]

//client <-> UDP server
#define	OP_GLOBSEARCHREQ3		0x90	// <1 tag set><search_tree>
#define	OP_GLOBSEARCHREQ2		0x92	// <search_tree>
#define OP_GLOBSERVSTATREQ		0x96	// (null)
#define	OP_GLOBSERVSTATRES		0x97	// <USER 4><FILES 4>
#define OP_GLOBSEARCHREQ		0x98	// <search_tree>
#define OP_GLOBSEARCHRES		0x99	//
#define OP_GLOBGETSOURCES		0x9A	// <HASH 16>
#define OP_GLOBGETSOURCES2		0x94	// <HASH 16><FILESIZE 4>
#define OP_GLOBFOUNDSOURCES		0x9B	//
#define OP_GLOBCALLBACKREQ		0x9C	// <IP 4><PORT 2><client_ID 4>
#define OP_INVALID_LOWID		0x9E	// <ID 4>
#define	OP_SERVER_LIST_REQ		0xA0	// <IP 4><PORT 2>
#define OP_SERVER_LIST_RES		0xA1	// <count 1> (<ip 4><port 2>)[count]
#define OP_SERVER_DESC_REQ		0xA2	// (null)
#define OP_SERVER_DESC_RES		0xA3	// <name_len 2><name name_len><desc_len 2 desc_en>
#define OP_SERVER_LIST_REQ2		0xA4	// (null)

#define INV_SERV_DESC_LEN		0xF0FF	// used as an 'invalid' string len for OP_SERVER_DESC_REQ/RES

// client <-> client
#define	OP_HELLO				0x01	// 0x10<HASH 16><ID 4><PORT 2><1 Tag_set>
#define OP_SENDINGPART			0x46	// <HASH 16><von 4><bis 4><Daten len:(von-bis)>
#define	OP_REQUESTPARTS			0x47	// <HASH 16><von[3] 4*3><bis[3] 4*3>
#define OP_FILEREQANSNOFIL		0x48	// <HASH 16>
#define OP_END_OF_DOWNLOAD		0x49    // <HASH 16>
#define OP_ASKSHAREDFILES		0x4A	// (null)
#define OP_ASKSHAREDFILESANSWER 0x4B	// <count 4>(<HASH 16><ID 4><PORT 2><1 Tag_set>)[count]
#define	OP_HELLOANSWER			0x4C	// <HASH 16><ID 4><PORT 2><1 Tag_set><SERVER_IP 4><SERVER_PORT 2>
#define OP_CHANGE_CLIENT_ID		0x4D	// <ID_old 4><ID_new 4>
#define	OP_MESSAGE				0x4E	// <len 2><Message len>
#define OP_SETREQFILEID			0x4F	// <HASH 16>
#define	OP_FILESTATUS			0x50	// <HASH 16><count 2><status(bit array) len:((count+7)/8)>
#define OP_HASHSETREQUEST		0x51	// *DEPRECATED* <HASH 16>
#define OP_HASHSETANSWER		0x52	// *DEPRECATED* <count 2><HASH[count] 16*count>
#define	OP_STARTUPLOADREQ		0x54	// <HASH 16>
#define	OP_ACCEPTUPLOADREQ		0x55	// (null)
#define	OP_CANCELTRANSFER		0x56	// (null)
#define OP_OUTOFPARTREQS		0x57	// (null)
#define OP_REQUESTFILENAME		0x58	// <HASH 16>	(more correctly file_name_request)
#define OP_REQFILENAMEANSWER	0x59	// <HASH 16><len 4><NAME len>
#define OP_CHANGE_SLOT			0x5B	// <HASH 16>
#define OP_QUEUERANK			0x5C	// <wert  4> (slot index of the request)
#define OP_ASKSHAREDDIRS        0x5D    // (null)
#define OP_ASKSHAREDFILESDIR    0x5E    // <len 2><Directory len>
#define OP_ASKSHAREDDIRSANS     0x5F    // <count 4>(<len 2><Directory len>)[count]
#define OP_ASKSHAREDFILESDIRANS 0x60    // <len 2><Directory len><count 4>(<HASH 16><ID 4><PORT 2><1 Tag_set>)[count]
#define OP_ASKSHAREDDENIEDANS   0x61    // (null)

// this 'identifier' is used for referencing shared part (incomplete) files with the OP_ASKSHAREDDIRS and related opcodes
// it was introduced with eDonkeyHybrid and is considered as part of the protocol.
#define OP_INCOMPLETE_SHARED_FILES "!Incomplete Files"
#define OP_OTHER_SHARED_FILES	   "!Other" // files which are not within a shared directory (single shared files)

// eDonkeyHybrid truncates every received client message to 200 bytes, although it allows to send messages of any(?) size.
#define	MAX_CLIENT_MSG_LEN		450		// using 200 is just too short
#define	MAX_IRC_MSG_LEN			450		// 450 = same as in mIRC

// extended prot client <-> extended prot client
#define	OP_EMULEINFO			0x01	//
#define	OP_EMULEINFOANSWER		0x02	//
#define OP_COMPRESSEDPART		0x40	// <HASH 16><von 4><size 4><Daten len:size>
#define OP_QUEUERANKING			0x60	// <RANG 2>
#define OP_FILEDESC				0x61	// <len 2><NAME len>
#define OP_REQUESTSOURCES		0x81	// *DEPRECATED* <HASH 16>
#define OP_ANSWERSOURCES		0x82	// *DEPRECATED*
#define OP_REQUESTSOURCES2		0x83	// <HASH 16><Version 1><Options 2>
#define OP_ANSWERSOURCES2		0x84	// <Version 1>[content]
#define OP_PUBLICKEY			0x85	// <len 1><pubkey len>
#define OP_SIGNATURE			0x86	// v1: <len 1><signature len>  v2:<len 1><signature len><sigIPused 1>
#define OP_SECIDENTSTATE		0x87	// <state 1><rndchallenge 4>
#define OP_REQUESTPREVIEW		0x90	// <HASH 16>
#define OP_PREVIEWANSWER		0x91	// <HASH 16><frames 1>{frames * <len 4><frame len>}
#define OP_MULTIPACKET			0x92	// *DEPRECATED*
#define OP_MULTIPACKETANSWER	0x93	// *DEPRECATED*
#define	OP_PEERCACHE_QUERY		0x94	// *DEPRECATED*
#define	OP_PEERCACHE_ANSWER		0x95	// *DEPRECATED*
#define	OP_PEERCACHE_ACK		0x96	// *DEPRECATED*
#define	OP_PUBLICIP_REQ			0x97
#define	OP_PUBLICIP_ANSWER		0x98
#define OP_CALLBACK				0x99	// <HASH 16><HASH 16><uint 16>
#define OP_REASKCALLBACKTCP		0x9A
#define OP_AICHREQUEST			0x9B	// <HASH 16><uint16><HASH aichhashlen>
#define OP_AICHANSWER			0x9C	// <HASH 16><uint16><HASH aichhashlen> <data>
#define OP_AICHFILEHASHANS		0x9D	// *DEPRECATED*
#define OP_AICHFILEHASHREQ		0x9E	// *DEPRECATED*
#define OP_BUDDYPING			0x9F
#define OP_BUDDYPONG			0xA0
#define OP_COMPRESSEDPART_I64	0xA1	// <HASH 16><von 8><size 4><Daten len:size>
#define OP_SENDINGPART_I64		0xA2	// <HASH 16><von 8><bis 8><Daten len:(von-bis)>
#define	OP_REQUESTPARTS_I64		0xA3	// <HASH 16><von[3] 8*3><bis[3] 8*3>
#define OP_MULTIPACKET_EXT		0xA4	// *DEPRECATED*
#define OP_CHATCAPTCHAREQ		0xA5	// <tags 1>[tags]<Captcha BITMAP>
#define OP_CHATCAPTCHARES		0xA6	// <status 1>
#define OP_FWCHECKUDPREQ		0xA7	// <Inter_Port 2><Extern_Port 2><KadUDPKey 4> *Support required for Kad version >= 6
#define OP_KAD_FWTCPCHECK_ACK	0xA8	// (null/reserved), replaces KADEMLIA_FIREWALLED_ACK_RES, *Support required for Kad version >= 7
#define OP_MULTIPACKET_EXT2		0xA9	// <FileIdentifier> ...
#define OP_MULTIPACKETANSWER_EXT2 0xB0	// <FileIdentifier> ...
#define OP_HASHSETREQUEST2		0xB1	// <FileIdentifier><Options 1>
#define OP_HASHSETANSWER2		0xB2	// <FileIdentifier><Options 1>[<HashSets> Options]
// extended prot client <-> extended prot client UDP
#define OP_REASKFILEPING		0x90	// <HASH 16>
#define OP_REASKACK				0x91	// <RANG 2>
#define OP_FILENOTFOUND			0x92	// (null)
#define OP_QUEUEFULL			0x93	// (null)
#define OP_REASKCALLBACKUDP		0x94
#define OP_DIRECTCALLBACKREQ	0x95	// <TCPPort 2><Userhash 16><ConnectionOptions 1>
#define OP_PORTTEST				0xFE	// Connection Test

// server.met
#define ST_SERVERNAME			0x01	// <string>
#define ST_DESCRIPTION			0x0B	// <string>
#define ST_PING					0x0C	// <uint32>
#define ST_FAIL					0x0D	// <uint32>
#define ST_PREFERENCE			0x0E	// <uint32>
#define	ST_PORT					0x0F	// <uint32>
#define	ST_IP					0x10	// <uint32>
#define	ST_DYNIP				0x85	// <string>
//#define ST_LASTPING			0x86	// <int> No longer used.
#define ST_MAXUSERS				0x87	// <uint32>
#define ST_SOFTFILES			0x88	// <uint32>
#define ST_HARDFILES			0x89	// <uint32>
#define ST_LASTPING				0x90	// <uint32>
#define	ST_VERSION				0x91	// <string>|<uint32>
#define	ST_UDPFLAGS				0x92	// <uint32>
#define	ST_AUXPORTSLIST			0x93	// <string>
#define	ST_LOWIDUSERS			0x94	// <uint32>
#define	ST_UDPKEY				0x95	// <uint32>
#define	ST_UDPKEYIP				0x96	// <uint32>
#define	ST_TCPPORTOBFUSCATION	0x97	// <uint16>
#define	ST_UDPPORTOBFUSCATION	0x98	// <uint16>

//file tags
#define  FT_FILENAME			 0x01	// <string>
#define TAG_FILENAME			"\x01"	// <string>
#define  FT_FILESIZE			 0x02	// <uint32> (or <uint64> when supported)
#define TAG_FILESIZE			"\x02"	// <uint32>
#define  FT_FILESIZE_HI			 0x3A	// <uint32>
#define TAG_FILESIZE_HI			"\x3A"	// <uint32>
#define  FT_FILETYPE			 0x03	// <string>
#define TAG_FILETYPE			"\x03"	// <string>
#define  FT_FILEFORMAT			 0x04	// <string>
#define TAG_FILEFORMAT			"\x04"	// <string>
#define  FT_LASTSEENCOMPLETE	 0x05	// <uint32>
#define TAG_COLLECTION			"\x05"
#define	TAG_PART_PATH			"\x06"	// <string>
#define	TAG_PART_HASH			"\x07"
#define  FT_TRANSFERRED			 0x08	// <uint32>
#define	TAG_TRANSFERRED			"\x08"	// <uint32>
#define  FT_GAPSTART			 0x09	// <uint32>
#define	TAG_GAPSTART			"\x09"	// <uint32>
#define  FT_GAPEND				 0x0A	// <uint32>
#define	TAG_GAPEND				"\x0A"	// <uint32>
#define  FT_DESCRIPTION			 0x0B	// <string>
#define	TAG_DESCRIPTION			"\x0B"	// <string>
#define	TAG_PING				"\x0C"
#define	TAG_FAIL				"\x0D"
#define	TAG_PREFERENCE			"\x0E"
#define TAG_PORT				"\x0F"
#define TAG_IP_ADDRESS			"\x10"
#define TAG_VERSION				"\x11"	// <string>
#define  FT_PARTFILENAME		 0x12	// <string>
#define TAG_PARTFILENAME		"\x12"	// <string>
//#define FT_PRIORITY			 0x13	// Not used any more
#define TAG_PRIORITY			"\x13"	// <uint32>
#define  FT_STATUS				 0x14	// <uint32>
#define TAG_STATUS				"\x14"	// <uint32>
#define  FT_SOURCES				 0x15	// <uint32>
#define TAG_SOURCES				"\x15"	// <uint32>
#define  FT_PERMISSIONS			 0x16	// <uint32>
#define TAG_PERMISSIONS			"\x16"
//#define FT_ULPRIORITY			 0x17	// Not used any more
#define TAG_PARTS				"\x17"
#define  FT_DLPRIORITY			 0x18	// Was 13
#define  FT_ULPRIORITY			 0x19	// Was 17
#define	 FT_COMPRESSION			 0x1A
#define	 FT_CORRUPTED			 0x1B
#define  FT_KADLASTPUBLISHKEY	 0x20	// <uint32>
#define  FT_KADLASTPUBLISHSRC	 0x21	// <uint32>
#define	 FT_FLAGS				 0x22	// <uint32>
#define	 FT_DL_ACTIVE_TIME		 0x23	// <uint32>
#define	 FT_CORRUPTEDPARTS		 0x24	// <string>
#define  FT_DL_PREVIEW			 0x25
#define  FT_KADLASTPUBLISHNOTES	 0x26	// <uint32>
#define  FT_AICH_HASH			 0x27
#define  FT_FILEHASH			 0x28
#define	 FT_COMPLETE_SOURCES	 0x30	// nr. of sources which share a complete version of the associated file (supported by eserver 16.46+)
#define TAG_COMPLETE_SOURCES	"\x30"
#define  FT_COLLECTIONAUTHOR	 0x31
#define  FT_COLLECTIONAUTHORKEY  0x32
#define  FT_PUBLISHINFO			 0x33	// <uint32>
#define TAG_PUBLISHINFO			"\x33"	// <uint32>
#define  FT_LASTSHARED			 0x34	// <uint32>
#define  FT_AICHHASHSET			 0x35	// <uint32>
#define	TAG_KADAICHHASHPUB		"\x36"	// <AICH Hash>
#define TAG_KADAICHHASHRESULT	"\x37"	// <Count 1>{<Publishers 1><AICH Hash> Count}
// statistic
#define  FT_ATTRANSFERRED		 0x50	// <uint32>
#define  FT_ATREQUESTED			 0x51	// <uint32>
#define  FT_ATACCEPTED			 0x52	// <uint32>
#define  FT_CATEGORY			 0x53	// <uint32>
#define	 FT_ATTRANSFERREDHI		 0x54	// <uint32>
#define	 FT_MAXSOURCES			 0x55	// <uint32>
#define	 FT_MEDIA_ARTIST		 0xD0	// <string>
#define	TAG_MEDIA_ARTIST		"\xD0"	// <string>
#define	 FT_MEDIA_ALBUM			 0xD1	// <string>
#define	TAG_MEDIA_ALBUM			"\xD1"	// <string>
#define	 FT_MEDIA_TITLE			 0xD2	// <string>
#define	TAG_MEDIA_TITLE			"\xD2"	// <string>
#define	 FT_MEDIA_LENGTH		 0xD3	// <uint32> !!!
#define	TAG_MEDIA_LENGTH		"\xD3"	// <uint32> !!!
#define	 FT_MEDIA_BITRATE		 0xD4	// <uint32>
#define	TAG_MEDIA_BITRATE		"\xD4"	// <uint32>
#define	 FT_MEDIA_CODEC			 0xD5	// <string>
#define	TAG_MEDIA_CODEC			"\xD5"	// <string>
#define TAG_KADMISCOPTIONS		"\xF2"	// <uint8>
#define TAG_ENCRYPTION			"\xF3"	// <uint8>
#define TAG_USER_COUNT			"\xF4"	// <uint32>
#define TAG_FILE_COUNT			"\xF5"	// <uint32>
#define  FT_FILECOMMENT			 0xF6	// <string>
#define TAG_FILECOMMENT			"\xF6"	// <string>
#define  FT_FILERATING			 0xF7	// <uint8>
#define TAG_FILERATING			"\xF7"	// <uint8>
#define TAG_BUDDYHASH			"\xF8"	// <string>
#define TAG_CLIENTLOWID			"\xF9"	// <uint32>
#define TAG_SERVERPORT			"\xFA"	// <uint16>
#define TAG_SERVERIP			"\xFB"	// <uint32>
#define TAG_SOURCEUPORT			"\xFC"	// <uint16>
#define TAG_SOURCEPORT			"\xFD"	// <uint16>
#define TAG_SOURCEIP			"\xFE"	// <uint32>
#define TAG_SOURCETYPE			"\xFF"	// <uint8>

#define	TAGTYPE_NONE			0x00
#define	TAGTYPE_HASH			0x01
#define	TAGTYPE_STRING			0x02
#define	TAGTYPE_UINT32			0x03
#define	TAGTYPE_FLOAT32			0x04
#define	TAGTYPE_BOOL			0x05
#define	TAGTYPE_BOOLARRAY		0x06
#define	TAGTYPE_BLOB			0x07
#define	TAGTYPE_UINT16			0x08
#define	TAGTYPE_UINT8			0x09
#define	TAGTYPE_BSOB			0x0A
#define	TAGTYPE_UINT64			0x0B
#define	TAGTYPE_UINT			0xFE //general uint: 8, 16, 32, 64 bits

#define TAGTYPE_STR1			0x11
#define TAGTYPE_STR2			0x12
#define TAGTYPE_STR3			0x13
#define TAGTYPE_STR4			0x14
#define TAGTYPE_STR5			0x15
#define TAGTYPE_STR6			0x16
#define TAGTYPE_STR7			0x17
#define TAGTYPE_STR8			0x18
#define TAGTYPE_STR9			0x19
#define TAGTYPE_STR10			0x1A
#define TAGTYPE_STR11			0x1B
#define TAGTYPE_STR12			0x1C
#define TAGTYPE_STR13			0x1D
#define TAGTYPE_STR14			0x1E
#define TAGTYPE_STR15			0x1F
#define TAGTYPE_STR16			0x20
#define TAGTYPE_STR17			0x21	// accepted by eMule 0.42f (02-Mai-2004) in receiving code only because of a flaw, those tags are handled correctly, but should not be handled at all
#define TAGTYPE_STR18			0x22	// accepted by eMule 0.42f (02-Mai-2004) in receiving code only because of a flaw, those tags are handled correctly, but should not be handled at all
#define TAGTYPE_STR19			0x23	// accepted by eMule 0.42f (02-Mai-2004) in receiving code only because of a flaw, those tags are handled correctly, but should not be handled at all
#define TAGTYPE_STR20			0x24	// accepted by eMule 0.42f (02-Mai-2004) in receiving code only because of a flaw, those tags are handled correctly, but should not be handled at all
#define TAGTYPE_STR21			0x25	// accepted by eMule 0.42f (02-Mai-2004) in receiving code only because of a flaw, those tags are handled correctly, but should not be handled at all
#define TAGTYPE_STR22			0x26	// accepted by eMule 0.42f (02-Mai-2004) in receiving code only because of a flaw, those tags are handled correctly, but should not be handled at all


#define	ED2KFTSTR_ANY			""		// Baked(!!) do not send in ED2K/Kad
#define	ED2KFTSTR_AUDIO			"Audio"	// value for eD2K tag FT_FILETYPE
#define	ED2KFTSTR_VIDEO			"Video"	// value for eD2K tag FT_FILETYPE
#define	ED2KFTSTR_IMAGE			"Image"	// value for eD2K tag FT_FILETYPE
#define	ED2KFTSTR_DOCUMENT		"Doc"	// value for eD2K tag FT_FILETYPE
#define	ED2KFTSTR_PROGRAM		"Pro"	// value for eD2K tag FT_FILETYPE
#define	ED2KFTSTR_ARCHIVE		"Arc"	// eMule internal use only
#define	ED2KFTSTR_CDIMAGE		"Iso"	// eMule internal use only
#define ED2KFTSTR_EMULECOLLECTION "EmuleCollection" // Value for eD2K tag FT_FILETYPE

// additional media meta data tags from eDonkeyHybrid (note also the upper/lower case)
#define	FT_ED2K_MEDIA_ARTIST	"Artist"	// <string>
#define	FT_ED2K_MEDIA_ALBUM		"Album"		// <string>
#define	FT_ED2K_MEDIA_TITLE		"Title"		// <string>
#define	FT_ED2K_MEDIA_LENGTH	"length"	// <string> !!!
#define	FT_ED2K_MEDIA_BITRATE	"bitrate"	// <uint32>
#define	FT_ED2K_MEDIA_CODEC		"codec"		// <string>
#define TAG_NSENT				"# Sent"
#define TAG_ONIP				"ip"
#define TAG_ONPORT				"port"

// ed2k search expression comparison operators
#define ED2K_SEARCH_OP_EQUAL         0 // eserver 16.45+
#define ED2K_SEARCH_OP_GREATER       1 // dserver
#define ED2K_SEARCH_OP_LESS          2 // dserver
#define ED2K_SEARCH_OP_GREATER_EQUAL 3 // eserver 16.45+
#define ED2K_SEARCH_OP_LESS_EQUAL    4 // eserver 16.45+
#define ED2K_SEARCH_OP_NOTEQUAL      5 // eserver 16.45+

// Kad search expression comparison operators
// 11-Sep-2005 []: Since eMule 0.47 Kad uses same operators as ED2K
//#define KAD_SEARCH_OP_EQUAL         0 // eMule 0.43+
//#define KAD_SEARCH_OP_GREATER_EQUAL 1 // eMule 0.40+; NOTE: this different than ED2K!
//#define KAD_SEARCH_OP_LESS_EQUAL    2 // eMule 0.40+; NOTE: this different than ED2K!
//#define KAD_SEARCH_OP_GREATER       3 // eMule 0.43+; NOTE: this different than ED2K!
//#define KAD_SEARCH_OP_LESS          4 // eMule 0.43+; NOTE: this different than ED2K!
//#define KAD_SEARCH_OP_NOTEQUAL      5 // eMule 0.43+

#define CT_NAME					0x01
#define	CT_PORT					0x0f
#define CT_VERSION				0x11
#define	CT_SERVER_FLAGS			0x20	// currently only used to inform a server about supported features
#define CT_MOD_VERSION			0x55
#define	CT_EMULECOMPAT_OPTIONS1	0xef
#define	CT_EMULE_RESERVED1		0xf0
#define	CT_EMULE_RESERVED2		0xf1
#define	CT_EMULE_RESERVED3		0xf2
#define	CT_EMULE_RESERVED4		0xf3
#define	CT_EMULE_RESERVED5		0xf4
#define	CT_EMULE_RESERVED6		0xf5
#define	CT_EMULE_RESERVED7		0xf6
#define	CT_EMULE_RESERVED8		0xf7
#define	CT_EMULE_RESERVED9		0xf8
#define	CT_EMULE_UDPPORTS		0xf9
#define	CT_EMULE_MISCOPTIONS1	0xfa
#define	CT_EMULE_VERSION		0xfb
#define CT_EMULE_BUDDYIP		0xfc
#define CT_EMULE_BUDDYUDP		0xfd
#define CT_EMULE_MISCOPTIONS2	0xfe
#define CT_EMULE_RESERVED13		0xff
#define CT_SERVER_UDPSEARCH_FLAGS 0x0E

// values for CT_SERVER_FLAGS (server capabilities)
#define SRVCAP_ZLIB				0x0001
#define SRVCAP_IP_IN_LOGIN		0x0002
#define SRVCAP_AUXPORT			0x0004
#define SRVCAP_NEWTAGS			0x0008
#define	SRVCAP_UNICODE			0x0010
#define	SRVCAP_LARGEFILES		0x0100
#define SRVCAP_SUPPORTCRYPT     0x0200
#define SRVCAP_REQUESTCRYPT     0x0400
#define SRVCAP_REQUIRECRYPT     0x0800

// values for CT_SERVER_UDPSEARCH_FLAGS
#define SRVCAP_UDP_NEWTAGS_LARGEFILES	0x01

// emule tagnames
#define ET_COMPRESSION			0x20
#define ET_UDPPORT				0x21
#define ET_UDPVER				0x22
#define ET_SOURCEEXCHANGE		0x23
#define ET_COMMENTS				0x24
#define ET_EXTENDEDREQUEST		0x25
#define ET_COMPATIBLECLIENT		0x26
#define ET_FEATURES				0x27
#define ET_MOD_VERSION			CT_MOD_VERSION


#define	PCPCK_VERSION			0x01

// PeerCache packet sub objcodes
#define	PCOP_NONE				0x00
#define	PCOP_REQ				0x01
#define PCOP_RES				0x02
#define	PCOP_ACK				0x03

// PeerCache tags (NOTE: those tags are using the new eD2K tags (short tags))
#define	PCTAG_CACHEIP			0x01
#define	PCTAG_CACHEPORT			0x02
#define	PCTAG_PUBLICIP			0x03
#define	PCTAG_PUBLICPORT		0x04
#define	PCTAG_PUSHID			0x05
#define	PCTAG_FILEID			0x06

// KADEMLIA (opcodes) (udp)
#define KADEMLIA_BOOTSTRAP_REQ_DEPRECATED		0x00	// <PEER (sender) [25]>
#define KADEMLIA2_BOOTSTRAP_REQ			0x01	//

#define KADEMLIA_BOOTSTRAP_RES_DEPRECATED		0x08	// <CNT [2]> <PEER [25]>*(CNT)
#define KADEMLIA2_BOOTSTRAP_RES			0x09	//

#define KADEMLIA_HELLO_REQ_DEPRECATED			0x10	// <PEER (sender) [25]>
#define KADEMLIA2_HELLO_REQ				0x11	//

#define KADEMLIA_HELLO_RES_DEPRECATED			0x18	// <PEER (receiver) [25]>
#define KADEMLIA2_HELLO_RES				0x19	//

#define KADEMLIA_REQ_DEPRECATED					0x20	// <TYPE [1]> <HASH (target) [16]> <HASH (receiver) 16>
#define KADEMLIA2_REQ					0x21	//

#define KADEMLIA2_HELLO_RES_ACK			0x22	// <NodeID><uint8 tags>

#define KADEMLIA_RES_DEPRECATED					0x28	// <HASH (target) [16]> <CNT> <PEER [25]>*(CNT)
#define KADEMLIA2_RES					0x29	//

#define KADEMLIA_SEARCH_REQ				0x30	// <HASH (key) [16]> <ext 0/1 [1]> <SEARCH_TREE>[ext]
//#define UNUSED						0x31	// Old Opcode, don't use.
#define KADEMLIA_SEARCH_NOTES_REQ		0x32	// <HASH (key) [16]>
#define KADEMLIA2_SEARCH_KEY_REQ		0x33	//
#define KADEMLIA2_SEARCH_SOURCE_REQ		0x34	//
#define KADEMLIA2_SEARCH_NOTES_REQ		0x35	//

#define KADEMLIA_SEARCH_RES				0x38	// <HASH (key) [16]> <CNT1 [2]> (<HASH (answer) [16]> <CNT2 [2]> <META>*(CNT2))*(CNT1)
//#define UNUSED						0x39	// Old Opcode, don't use.
#define KADEMLIA_SEARCH_NOTES_RES		0x3A	// <HASH (key) [16]> <CNT1 [2]> (<HASH (answer) [16]> <CNT2 [2]> <META>*(CNT2))*(CNT1)
#define KADEMLIA2_SEARCH_RES			0x3B	//

#define KADEMLIA_PUBLISH_REQ			0x40	// <HASH (key) [16]> <CNT1 [2]> (<HASH (target) [16]> <CNT2 [2]> <META>*(CNT2))*(CNT1)
//#define UNUSED						0x41	// Old Opcode, don't use.
#define KADEMLIA_PUBLISH_NOTES_REQ_DEPRECATED		0x42	// <HASH (key) [16]> <HASH (target) [16]> <CNT2 [2]> <META>*(CNT2))*(CNT1)
#define	KADEMLIA2_PUBLISH_KEY_REQ		0x43	//
#define	KADEMLIA2_PUBLISH_SOURCE_REQ	0x44	//
#define KADEMLIA2_PUBLISH_NOTES_REQ		0x45	//

#define KADEMLIA_PUBLISH_RES			0x48	// <HASH (key) [16]>
//#define UNUSED						0x49	// Old Opcode, don't use.
#define KADEMLIA_PUBLISH_NOTES_RES_DEPRECATED		0x4A	// <HASH (key) [16]>
#define	KADEMLIA2_PUBLISH_RES			0x4B	//
#define	KADEMLIA2_PUBLISH_RES_ACK		0x4C	// null

#define KADEMLIA_FIREWALLED_REQ			0x50	// <TCPPORT (sender) [2]>
#define KADEMLIA_FINDBUDDY_REQ			0x51	// <TCPPORT (sender) [2]>
#define KADEMLIA_CALLBACK_REQ			0x52	// <TCPPORT (sender) [2]>
#define KADEMLIA_FIREWALLED2_REQ		0x53	// <TCPPORT (sender) [2]><userhash><connectoptions 1>

#define KADEMLIA_FIREWALLED_RES			0x58	// <IP (sender) [4]>
#define KADEMLIA_FIREWALLED_ACK_RES		0x59	// (null)
#define KADEMLIA_FINDBUDDY_RES			0x5A	// <TCPPORT (sender) [2]>

#define KADEMLIA2_PING					0x60	// (null)
#define KADEMLIA2_PONG					0x61	// (null)

#define KADEMLIA2_FIREWALLUDP			0x62	// <errorcode [1]><UDPPort_Used [2]>

// KADEMLIA (parameter)
#define KADEMLIA_FIND_VALUE				0x02
#define KADEMLIA_STORE					0x04
#define KADEMLIA_FIND_NODE				0x0B
#define KADEMLIA_FIND_VALUE_MORE		KADEMLIA_FIND_NODE

// searchspam.met Tags
#define SP_FILEFULLNAME					0x01u
#define SP_FILEHASHSPAM					0x02u
#define SP_FILEHASHNOSPAM				0x03u
#define SP_FILESOURCEIP					0x04u
#define SP_FILESERVERIP					0x05u
#define SP_FILESIMILARNAME				0x06u
#define SP_FILESIZE						0x07u
#define SP_UDPSERVERSPAMRATIO			0x08u