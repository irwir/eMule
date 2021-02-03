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
#include "kademlia/routing/contact.h"
#include "KadContactHistogramCtrl.h"
#include "OtherFunctions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


BEGIN_MESSAGE_MAP(CKadContactHistogramCtrl, CWnd)
	ON_WM_PAINT()
END_MESSAGE_MAP()

CKadContactHistogramCtrl::CKadContactHistogramCtrl()
	: m_aHist()
	, m_iMaxLabelHeight(8)
	, m_iMaxNumLabelWidth(3 * 8)
	, m_bInitializedFontMetrics()
{
	ASSERT((1 << KAD_CONTACT_HIST_NEEDED_BITS) <= KAD_CONTACT_HIST_SIZE);

	m_penAxis.CreatePen(PS_SOLID, 1, RGB(128, 128, 128));
	m_penAux.CreatePen(PS_DOT, 1, RGB(192, 192, 192));
	m_penHist.CreatePen(PS_SOLID, 1, RGB(255, 32, 32));
}

void CKadContactHistogramCtrl::Localize()
{
	m_strXaxis = GetResString(IDS_KADEMLIA) + _T(' ') + GetResString(IDS_NETWORK);
	m_strYaxis = GetResString(IDS_KADCONTACTLAB);
	if (m_hWnd)
		Invalidate();
}

inline UINT GetHistSlot(const Kademlia::CUInt128 &KadUint128)
{
//	byte aucUInt128[16];
//	KadUint128.ToByteArray(aucUInt128);
//	CryptoPP::Integer d(aucUInt128, sizeof aucUInt128);
//	CryptoPP::Integer h;
//	h = d >> (128 - KAD_CONTACT_HIST_NEEDED_BITS);
//	UINT uHistSlot = h.ConvertToLong();

	// same as above but quite faster
	DWORD dwHighestWord = *(DWORD*)KadUint128.GetData();
	UINT uHistSlot = dwHighestWord >> (128 - 3 * 32 - KAD_CONTACT_HIST_NEEDED_BITS);
	ASSERT(uHistSlot < KAD_CONTACT_HIST_SIZE);
	return uHistSlot;
}

bool CKadContactHistogramCtrl::ContactAdd(const Kademlia::CContact *contact)
{
	Kademlia::CUInt128 distance;
	contact->GetClientID(&distance);
	UINT uHistSlot = GetHistSlot(distance);
	m_aHist[uHistSlot]++;
	Invalidate();
	return true;
}

void CKadContactHistogramCtrl::ContactRem(const Kademlia::CContact *contact)
{
	Kademlia::CUInt128 distance;
	contact->GetClientID(&distance);
	UINT uHistSlot = GetHistSlot(distance);
	ASSERT(m_aHist[uHistSlot] > 0);
	if (m_aHist[uHistSlot] > 0) {
		m_aHist[uHistSlot]--;
		Invalidate();
	}
}

void CKadContactHistogramCtrl::OnPaint()
{
	CPaintDC dc(this);

	CRect rcClient;
	GetClientRect(&rcClient);
	if (rcClient.IsRectEmpty())
		return;
	dc.FillSolidRect(rcClient, ::GetSysColor(COLOR_WINDOW));
	COLORREF crOldTextColor = dc.SetTextColor(::GetSysColor(COLOR_WINDOWTEXT));

	CFont *pOldFont = dc.SelectObject(AfxGetMainWnd()->GetFont());
	if (!m_bInitializedFontMetrics) {
		TEXTMETRIC tm;
		dc.GetTextMetrics(&tm);
		// why is 'tm.tmMaxCharWidth' and 'tm.tmAveCharWidth' that wrong?
		CRect rcLabel;
		dc.DrawText(_T("888"), 3, &rcLabel, DT_CALCRECT);
		m_iMaxNumLabelWidth = rcLabel.Width();
		if (m_iMaxNumLabelWidth <= 0)
			m_iMaxNumLabelWidth = 3 * 8;
		m_iMaxLabelHeight = tm.tmHeight;
		if (m_iMaxLabelHeight <= 0)
			m_iMaxLabelHeight = 8;
		m_bInitializedFontMetrics = true;
	}

	int iLeftBorder = 1 + m_iMaxNumLabelWidth + 3;
	int iRightBorder = 8;
	int iTopBorder = m_iMaxLabelHeight;
	int iBottomBorder = m_iMaxLabelHeight;

	int iBaseLineX = iLeftBorder;
	int iBaseLineY = rcClient.bottom - iBottomBorder;
	UINT uHistWidth = rcClient.Width() - iLeftBorder - iRightBorder;
	if (uHistWidth > _countof(m_aHist))
		uHistWidth = _countof(m_aHist);
	else if (uHistWidth == 0) {
		dc.SelectObject(pOldFont);
		dc.SetTextColor(crOldTextColor);
		return;
	}
	UINT uHistHeight = rcClient.Height() - iTopBorder - iBottomBorder;
	if (uHistHeight == 0) {
		dc.SelectObject(pOldFont);
		dc.SetTextColor(crOldTextColor);
		return;
	}

	UINT uMax = 0;
	for (unsigned i = 0; i < _countof(m_aHist); ++i)
		if (m_aHist[i] > uMax)
			uMax = m_aHist[i];

	//Lets take the average. This will keep the cluster of closest contacts from
	//streching the graph too far.
	uMax /= _countof(m_aHist);
	if (uMax < 15)
		uMax = 15/*uHistHeight*/;

	UINT uLabels = uHistHeight / (m_iMaxLabelHeight + m_iMaxLabelHeight / 2);
	if (uLabels == 0) {
		dc.SelectObject(pOldFont);
		dc.SetTextColor(crOldTextColor);
		return;
	}
	UINT uStep = ((uMax / uLabels + 5) / 10) * 10;
	if (uStep < 5)
		uStep = 5;

	CPen *pOldPen = dc.SelectObject(&m_penAxis);

	dc.MoveTo(iBaseLineX, rcClient.top + iTopBorder);
	dc.LineTo(iBaseLineX, iBaseLineY);
	dc.LineTo(iBaseLineX + uHistWidth, iBaseLineY);

	dc.SelectObject(&m_penAux);
	CString sLabel;
	for (UINT s = 0; s <= uMax; s += uStep) {
		int y = iBaseLineY - (uHistHeight * s) / uMax;
		int iLabelY = y - m_iMaxLabelHeight / 2;
		RECT rcLabel = { 1, iLabelY, 1 + m_iMaxNumLabelWidth, iLabelY + m_iMaxLabelHeight };
		if (s > 0) {
			dc.MoveTo(iBaseLineX - 2, y);
			dc.LineTo(iBaseLineX + uHistWidth, y);
		}
		sLabel.Format(_T("%u"), s);
		dc.DrawText(sLabel, &rcLabel, DT_RIGHT /*| DT_NOCLIP*/);
	}

	RECT rcLabel(rcClient);
	rcLabel.left = iBaseLineX;
	rcLabel.bottom = m_iMaxLabelHeight;
	dc.DrawText(m_strYaxis, &rcLabel, DT_LEFT | DT_TOP | DT_NOCLIP);

	rcLabel = rcClient;
	rcLabel.top = rcClient.bottom - m_iMaxLabelHeight + 1;
	dc.DrawText(m_strXaxis, &rcLabel, DT_RIGHT | DT_BOTTOM | DT_NOCLIP);

	int iLastHx = -1;
	for (UINT x = 0; x < uHistWidth; ++x) {
		int hx = (x * _countof(m_aHist)) / uHistWidth;
		UINT hv = m_aHist[hx];
		while (++iLastHx < hx)
			hv += m_aHist[iLastHx];

		if (hv > uMax) {
			dc.SelectObject(&m_penAxis);
			hv = uMax;
		} else
			dc.SelectObject(&m_penHist);

		if (hv) {
			dc.MoveTo(iBaseLineX + x, iBaseLineY - 1);
			UINT uHistVal = (hv * uHistHeight) / uMax;
			dc.LineTo(iBaseLineX + x, iBaseLineY - 1 - uHistVal - 1);
		}
	}

	dc.SelectObject(pOldPen);
	dc.SelectObject(pOldFont);
	dc.SetTextColor(crOldTextColor);
}