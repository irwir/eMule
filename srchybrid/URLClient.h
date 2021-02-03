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

#include "UpDownClient.h"

///////////////////////////////////////////////////////////////////////////////
// CUrlClient

class CUrlClient : public CUpDownClient
{
	DECLARE_DYNAMIC(CUrlClient)

public:
	CUrlClient();
	virtual	~CUrlClient() = default;

	bool SetUrl(LPCTSTR pszUrl, uint32 nIP = 0);

	virtual void SetRequestFile(CPartFile *pReqFile);
	virtual bool IsEd2kClient() const				{ return false; }

	virtual bool TryToConnect(bool bIgnoreMaxCon, bool bNoCallbacks = false, CRuntimeClass *pClassSocket = NULL);
	virtual void Connect();
	virtual void OnSocketConnected(int nErrorCode);
	virtual void ConnectionEstablished();
	virtual bool Disconnected(LPCTSTR pszReason, bool bFromSocket = false);

	virtual void SendHelloPacket();
	virtual void SendBlockRequests();
	virtual bool SendHttpBlockRequests();
	virtual void SendFileRequest();
	virtual void SendCancelTransfer();

	bool ProcessHttpDownResponse(const CStringAArray &astrHeaders);

	CStringA m_strHostA;
	int m_iRedirected;
};