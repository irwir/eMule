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
#include "AsyncSocketEx.h"

class CIrcMain;
class CAsyncProxySocketLayer;

class CIrcSocket : public CAsyncSocketEx
{
public:
	explicit CIrcSocket(CIrcMain *pIrcMain);
	virtual	~CIrcSocket();

	BOOL Create(UINT uSocketPort = 0
			, int iSocketType = SOCK_STREAM
			, long lEvent = FD_DEFAULT
			, const CString &sSocketAddress = CString());
	void Connect();
	int SendString(const CString &sMessage);

	virtual void OnConnect(int iErrorCode);
	virtual void OnReceive(int iErrorCode);
	virtual void OnSend(int iErrorCode);
	virtual void OnClose(int iErrorCode);
	virtual void RemoveAllLayers();

protected:
	CAsyncProxySocketLayer *m_pProxyLayer;
	CIrcMain *m_pIrcMain;

	virtual int	OnLayerCallback(std::list<t_callbackMsg> &callbacks);
};
