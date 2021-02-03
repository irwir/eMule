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

class CIrcWnd;
class CIrcSocket;
class CIrcMain
{
public:
	CIrcMain();
	~CIrcMain() = default;

	void ParseMessage(const CString &sRawMessage);
	void PreParseMessage(const char *pszBufferA);
	void SendLogin();
	void Connect();
	void Disconnect(bool bIsShuttingDown = false);
	void SetConnectStatus(bool bConnected);
	void SetIRCWnd(CIrcWnd *pwndIRC);
	int SendString(const CString &sSend);
	void ParsePerform();
	static void ProcessLink(const CString &sED2KLink);
	uint32 SetVerify();
	CString GetNick() const;

protected:
	void PerformString(const CString &sPerform);
	CIrcSocket *m_pIRCSocket;
	CIrcWnd *m_pwndIRC;
	CStringA m_sPreParseBufferA;
	CString m_sUser;
	CString m_sNick;
	CString m_sVersion;
	uint32	m_uVerify;
	uint32	m_dwLastRequest;
};