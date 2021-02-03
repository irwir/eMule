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

#include "HttpClientReqSocket.h"

DWORD GetPeerCacheSocketUploadTimeout();
DWORD GetPeerCacheSocketDownloadTimeout();


///////////////////////////////////////////////////////////////////////////////
// CPeerCacheSocket

class CPeerCacheSocket : public CHttpClientReqSocket
{
	DECLARE_DYNCREATE(CPeerCacheSocket)

public:
	virtual CUpDownClient* GetClient() const				{ return m_client; }
	virtual void OnSend(int nErrorCode);
	virtual void OnReceive(int nErrorCode);
	virtual void Safe_Delete();

protected:
	explicit CPeerCacheSocket(CUpDownClient *pClient = NULL);
	virtual	~CPeerCacheSocket();
	virtual void DetachFromClient();

	virtual void OnError(int nErrorCode);

	virtual bool ProcessHttpResponse();
	virtual bool ProcessHttpResponseBody(const BYTE *pucData, UINT size);
	virtual bool ProcessHttpRequest();

	CUpDownClient *m_client;
};


///////////////////////////////////////////////////////////////////////////////
// CPeerCacheDownSocket

class CPeerCacheDownSocket : public CPeerCacheSocket
{
	DECLARE_DYNCREATE(CPeerCacheDownSocket)

public:
	explicit CPeerCacheDownSocket(CUpDownClient *pClient = NULL);

	virtual void OnClose(int nErrorCode);

protected:
	virtual	~CPeerCacheDownSocket();
	virtual void DetachFromClient();

	virtual bool ProcessHttpResponse();
	virtual bool ProcessHttpResponseBody(const BYTE *pucData, UINT uSize);
	virtual bool ProcessHttpRequest();
};


///////////////////////////////////////////////////////////////////////////////
// CPeerCacheUpSocket

class CPeerCacheUpSocket : public CPeerCacheSocket
{
	DECLARE_DYNCREATE(CPeerCacheUpSocket)

public:
	explicit CPeerCacheUpSocket(CUpDownClient *pClient = NULL);

	virtual void OnClose(int nErrorCode);
	virtual void OnSend(int nErrorCode);

protected:
	virtual	~CPeerCacheUpSocket();
	virtual void DetachFromClient();

	virtual bool ProcessHttpResponse();
	virtual bool ProcessHttpResponseBody(const BYTE *pucData, UINT size);
	virtual bool ProcessHttpRequest();
};
