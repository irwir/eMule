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

#include "StdAfx.h"
#include "UPnPImplWrapper.h"
#include "UPnPImpl.h"
#include "UPnPImplWinServ.h"
#include "UPnPImplMiniLib.h"
#include "Preferences.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CUPnPImplWrapper::CUPnPImplWrapper()
{
	if (!thePrefs.IsWinServUPnPImplDisabled())
		m_liAvailable.AddTail(new CUPnPImplWinServ());
	if (!thePrefs.IsMinilibUPnPImplDisabled())
		m_liAvailable.AddTail(new CUPnPImplMiniLib());
	if (m_liAvailable.IsEmpty())
		m_liAvailable.AddTail(new CUPnPImplNone());
	Init();
}
CUPnPImplWrapper::~CUPnPImplWrapper()
{
	while (!m_liAvailable.IsEmpty())
		delete m_liAvailable.RemoveHead();
	while (!m_liUsed.IsEmpty())
		delete m_liUsed.RemoveHead();
	m_pActiveImpl = NULL;
}
void CUPnPImplWrapper::Init()
{
	ASSERT(!m_liAvailable.IsEmpty());
	m_pActiveImpl = NULL;

	for (POSITION pos = m_liAvailable.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		CUPnPImpl *tmp = m_liAvailable.GetNext(pos);
		if (tmp->GetImplementationID() == thePrefs.GetLastWorkingUPnPImpl()) {
			m_pActiveImpl = tmp;
			m_liAvailable.RemoveAt(pos2);
			break;
		}
	}

	if (m_pActiveImpl == NULL)
		m_pActiveImpl = m_liAvailable.RemoveHead();
	m_liUsed.AddTail(m_pActiveImpl);
}

void CUPnPImplWrapper::Reset()
{
	while (!m_liUsed.IsEmpty())
		m_liAvailable.AddTail(m_liUsed.RemoveHead());
	Init();
}

bool CUPnPImplWrapper::SwitchImplentation()
{
	if (m_liAvailable.IsEmpty())
		return false;

	m_pActiveImpl = m_liAvailable.RemoveHead();
	m_liUsed.AddTail(m_pActiveImpl);
	return true;
}