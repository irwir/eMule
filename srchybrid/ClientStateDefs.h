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

enum EUploadState : uint8
{
	US_UPLOADING,
	US_ONUPLOADQUEUE,
	US_CONNECTING,
	US_BANNED,
	US_NONE
};

enum EDownloadState : uint8
{
	DS_DOWNLOADING,
	DS_ONQUEUE,
	DS_CONNECTED,
	DS_CONNECTING,
	DS_WAITCALLBACK,
	DS_WAITCALLBACKKAD,
	DS_REQHASHSET,
	DS_NONEEDEDPARTS,
	DS_TOOMANYCONNS,
	DS_TOOMANYCONNSKAD,
	DS_LOWTOLOWIP,
	DS_BANNED,
	DS_ERROR,
	DS_NONE,
	DS_REMOTEQUEUEFULL  // not used yet, except in statistics
};

enum EPeerCacheDownState : uint8
{
	PCDS_NONE			= 0,
	PCDS_WAIT_CLIENT_REPLY,
	PCDS_WAIT_CACHE_REPLY,
	PCDS_DOWNLOADING
};

enum EPeerCacheUpState : uint8
{
	PCUS_NONE			= 0,
	PCUS_WAIT_CACHE_REPLY,
	PCUS_UPLOADING
};

enum EChatState : uint8
{
	MS_NONE,
	MS_CHATTING,
	MS_CONNECTING,
	MS_UNABLETOCONNECT
};

enum EKadState : uint8
{
	KS_NONE,
	KS_QUEUED_FWCHECK,
	KS_CONNECTING_FWCHECK,
	KS_CONNECTED_FWCHECK,
	KS_QUEUED_BUDDY,
	KS_INCOMING_BUDDY,
	KS_CONNECTING_BUDDY,
	KS_CONNECTED_BUDDY,
	KS_QUEUED_FWCHECK_UDP,
	KS_FWCHECK_UDP,
	KS_CONNECTING_FWCHECK_UDP
};

enum EClientSoftware : uint8
{
	SO_EMULE			= 0,	// default
	SO_CDONKEY			= 1,	// ET_COMPATIBLECLIENT
	SO_XMULE			= 2,	// ET_COMPATIBLECLIENT
	SO_AMULE			= 3,	// ET_COMPATIBLECLIENT
	SO_SHAREAZA			= 4,	// ET_COMPATIBLECLIENT
	SO_MLDONKEY			= 10,	// ET_COMPATIBLECLIENT
	SO_LPHANT			= 20,	// ET_COMPATIBLECLIENT
	// other client types which are not identified with ET_COMPATIBLECLIENT
	SO_EDONKEYHYBRID	= 50,
	SO_EDONKEY,
	SO_OLDEMULE,
	SO_URL,
	SO_UNKNOWN
};

enum ESecureIdentState : uint8
{
	IS_UNAVAILABLE		= 0,
	IS_ALLREQUESTSSEND	= 0,
	IS_SIGNATURENEEDED	= 1,
	IS_KEYANDSIGNEEDED	= 2,
};

enum EInfoPacketState : uint8
{
	IP_NONE				= 0,
	IP_EDONKEYPROTPACK	= 1,
	IP_EMULEPROTPACK	= 2,
	IP_BOTH				= 3,
};

enum ESourceFrom : uint8
{
	SF_SERVER			= 0,
	SF_KADEMLIA			= 1,
	SF_SOURCE_EXCHANGE	= 2,
	SF_PASSIVE			= 3,
	SF_LINK				= 4
};

enum EChatCaptchaState : uint8
{
	CA_NONE				= 0,
	CA_CHALLENGESENT,
	CA_CAPTCHASOLVED,
	CA_ACCEPTING,
	CA_CAPTCHARECV,
	CA_SOLUTIONSENT
};

enum EConnectingState : uint8
{
	CCS_NONE			= 0,
	CCS_DIRECTTCP,
	CCS_DIRECTCALLBACK,
	CCS_KADCALLBACK,
	CCS_SERVERCALLBACK,
	CCS_PRECONDITIONS
};