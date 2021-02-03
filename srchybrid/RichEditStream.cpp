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
#include "stdafx.h"
#include "resource.h"
#include "MenuCmds.h"
#include "RichEditStream.h"
#include "OtherFunctions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CRichEditStream

BEGIN_MESSAGE_MAP(CRichEditStream, CRichEditCtrl)
END_MESSAGE_MAP()

CRichEditStream::CRichEditStream()
	: m_cfDef()
	, m_cfBold()
	, m_cfRed()
{
}

CRichEditStream& CRichEditStream::operator<<(LPCTSTR const psz)
{
	ReplaceSel(psz);
	return *this;
}

CRichEditStream& CRichEditStream::operator<<(const char *const psz)
{
	ReplaceSel(CString(psz));
	return *this;
}

CRichEditStream& CRichEditStream::operator<<(UINT uVal)
{
	CString strVal;
	strVal.Format(_T("%u"), uVal);
	ReplaceSel(strVal);
	return *this;
}

CRichEditStream& CRichEditStream::operator<<(int iVal)
{
	CString strVal;
	strVal.Format(_T("%d"), iVal);
	ReplaceSel(strVal);
	return *this;
}

CRichEditStream& CRichEditStream::operator<<(double fVal)
{
	CString strVal;
	strVal.Format(_T("%.3f"), fVal);
	ReplaceSel(strVal);
	return *this;
}

void CRichEditStream::InitColors()
{
	m_cfDef.cbSize = (UINT)sizeof m_cfDef;
	if (GetSelectionCharFormat(m_cfDef)) {
		m_cfBold = m_cfDef;
		m_cfBold.dwMask |= CFM_BOLD;
		m_cfBold.dwEffects |= CFE_BOLD;

		m_cfRed = m_cfDef;
		m_cfRed.dwMask |= CFM_COLOR;
		m_cfRed.dwEffects &= ~CFE_AUTOCOLOR;
		m_cfRed.crTextColor = RGB(255, 0, 0);
	}
}

DWORD CALLBACK CRichEditStream::StreamOutCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb)
{
	CStringA *pStreamBuffA = (CStringA*)dwCookie;
	pStreamBuffA->Append((LPCSTR)pbBuff, cb);
	*pcb = cb;
	return 0;
}

void CRichEditStream::GetRTFText(CStringA &rstrText)
{
	EDITSTREAM es = {};
	es.pfnCallback = StreamOutCallback;
	es.dwCookie = (DWORD_PTR)&rstrText;
	StreamOut(SF_RTF, es);
}
