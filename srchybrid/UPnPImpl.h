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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#pragma once
#include <exception>

enum TRISTATE
{
	TRIS_FALSE,
	TRIS_UNKNOWN,
	TRIS_TRUE
};

enum UPNP_IMPLEMENTATION
{
	UPNP_IMPL_WINDOWSERVICE = 0,
	UPNP_IMPL_MINIUPNPLIB,
	UPNP_IMPL_NONE	/*last*/
};


class CUPnPImpl
{
public:
	CUPnPImpl();
	virtual	~CUPnPImpl() = default;
	struct UPnPError : public std::exception	{};
	enum
	{
		UPNP_OK,
		UPNP_FAILED,
		UPNP_TIMEOUT
	};

	virtual void StartDiscovery(uint16 nTCPPort, uint16 nUDPPort, uint16 nTCPWebPort) = 0;
	virtual bool CheckAndRefresh() = 0;
	virtual void StopAsyncFind() = 0;
	virtual void DeletePorts() = 0;
	virtual bool IsReady() = 0;
	virtual int GetImplementationID() = 0;

	void LateEnableWebServerPort(uint16 nPort);	// Add Web Server port to already existing port mapping

	void SetMessageOnResult(CWnd *cwnd, UINT nMessageID);
	TRISTATE ArePortsForwarded() const
	{
		return m_bUPnPPortsForwarded;
	}
	uint16 GetUsedTCPPort() const
	{
		return m_nTCPPort;
	}
	uint16 GetUsedUDPPort() const
	{
		return m_nUDPPort;
	}
	uint16 GetUsedTCPWebPort() const
	{
		return m_nTCPWebPort;
	}

// Implementation
protected:
	void SendResultMessage();
	volatile TRISTATE m_bUPnPPortsForwarded;
	uint16 m_nOldTCPPort;
	uint16 m_nOldTCPWebPort;
	uint16 m_nOldUDPPort;
	uint16 m_nTCPPort;
	uint16 m_nTCPWebPort;
	uint16 m_nUDPPort;
	bool m_bCheckAndRefresh;

private:
	CWnd *m_wndResultMessage;
	UINT m_nResultMessageID;
};

// Dummy Implementation to be used when no other implementation is available
class CUPnPImplNone : public CUPnPImpl
{
public:
	virtual void StartDiscovery(uint16, uint16, uint16)
	{
		ASSERT(0);
	}

	virtual bool CheckAndRefresh()
	{
		return false;
	}

	virtual void StopAsyncFind()
	{
	}

	virtual void DeletePorts()
	{
	}

	virtual bool IsReady()
	{
		return false;
	}

	virtual int GetImplementationID()
	{
		return UPNP_IMPL_NONE;
	}
};