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
#include "UPnPImpl.h"

struct UPNPUrls;
struct IGDdatas;

class CUPnPImplMiniLib : public CUPnPImpl
{
public:
	CUPnPImplMiniLib();
	virtual	~CUPnPImplMiniLib();

	virtual void StartDiscovery(uint16 nTCPPort, uint16 nUDPPort, uint16 nTCPWebPort);
	virtual bool CheckAndRefresh();
	virtual void StopAsyncFind();
	virtual void DeletePorts();
	virtual bool IsReady();
	virtual int GetImplementationID()				{ return UPNP_IMPL_MINIUPNPLIB; }

	class CStartDiscoveryThread : public CWinThread
	{
		DECLARE_DYNCREATE(CStartDiscoveryThread)
	protected:
		CStartDiscoveryThread();
		bool OpenPort(uint16 nPort, bool bTCP, char *pachLANIP, bool bCheckAndRefresh);

	public:
		virtual BOOL InitInstance();
		virtual int Run();
		void SetValues(CUPnPImplMiniLib *pOwner)	{ m_pOwner = pOwner; }

	private:
		CUPnPImplMiniLib *m_pOwner;
	};

protected:
	void DeletePorts(bool bSkipLock);

private:
	void Cleanup();
	void DeletePort(uint16 port, LPCTSTR prot);
	void GetOldPorts();
	void StartThread();
	bool m_bSucceededOnce;
	char m_achLanIP[16];

	UPNPUrls *m_pURLs;
	IGDdatas *m_pIGDData;
	HANDLE m_hThreadHandle;

	static CMutex m_mutBusy;
	volatile bool m_bAbortDiscovery;
};