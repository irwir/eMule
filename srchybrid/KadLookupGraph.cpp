//this file is part of eMule
//Copyright (C)2010 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
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

#include "StdAfx.h"
#include "kadlookupgraph.h"
#include "emule.h"
#include "otherfunctions.h"
#include "opcodes.h"
#include "kademlia/utils/LookupHistory.h"
#include "kademlia/kademlia/Search.h"
//#include "log.h"
#include <gdiplus.h>
#include "MemDC.h"
#include "ToolTipCtrlX.h"
#include "MenuCmds.h"
#include "VisualStylesXP.h"
#include "preferences.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define NODE_ENTRY_WIDTH		26
#define NODE_ENTRY_HEIGHT		20
#define NODE_X_BORDER			((NODE_ENTRY_WIDTH - 16) / 2)
#define NODE_Y_BORDER			((NODE_ENTRY_HEIGHT - 16) / 2)

using namespace Kademlia;

BEGIN_MESSAGE_MAP(CKadLookupGraph, CWnd)
	ON_WM_PAINT()
	ON_WM_MOUSEMOVE()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(MP_AUTOKADLOOKUPGRAPH, OnSwitchAutoLookup)
	ON_WM_SIZE()
END_MESSAGE_MAP()

CKadLookupGraph::CKadLookupGraph()
{
	m_penAxis.CreatePen(PS_SOLID, 1, RGB(128, 128, 128));
	m_penAux.CreatePen(PS_SOLID, 1, RGB(192, 192, 192));
	m_penRed.CreatePen(PS_SOLID, 1, RGB(255, 32, 32));

	m_iMaxNumLabelWidth = 3 * 8;
	m_iMaxLabelHeight = 8;
	m_bInitializedFontMetrics = false;
	m_pLookupHistory = NULL;
	m_iHotItemIdx = -1;
	m_bDbgLog = true;
	m_pToolTip = NULL;
}

CKadLookupGraph::~CKadLookupGraph()
{
	delete m_pToolTip;
	if (m_pLookupHistory != NULL)
		m_pLookupHistory->SetGUIDeleted();
	m_pLookupHistory = NULL;
}

void CKadLookupGraph::Localize()
{
	m_strXaxis = GetResString(IDS_TIME);
	m_strYaxis = GetResString(IDS_KADDISTANCE);
	if (m_hWnd)
		Invalidate();
}

void CKadLookupGraph::Init()
{
	m_iml.Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);
	m_iml.Add(CTempIconLoader(_T("Contact0")));
	m_iml.Add(CTempIconLoader(_T("Contact1")));
	m_iml.Add(CTempIconLoader(_T("Contact2")));
	m_iml.Add(CTempIconLoader(_T("Contact4")));
	m_iml.Add(CTempIconLoader(_T("Collection_Search")));
	m_iml.Add(CTempIconLoader(_T("KadStoreWord")));
	m_iml.Add(CTempIconLoader(_T("KadStoreFile")));
	m_iml.SetOverlayImage(m_iml.Add(CTempIconLoader(_T("ClientSecureOvl"))), 1);
	m_iml.SetOverlayImage(m_iml.Add(CTempIconLoader(_T("NoAccessFolderOvl"))), 2);
	m_pToolTip = new CToolTipCtrlX();
	m_pToolTip->Create(this);
	m_pToolTip->SetDelayTime(TTDT_AUTOPOP, 5000);
	m_pToolTip->SetDelayTime(TTDT_INITIAL, 3000);
	m_pToolTip->SetDelayTime(TTDT_RESHOW, 2000);
	EnableToolTips();
	m_pToolTip->AddTool(this);
	m_pToolTip->Activate(FALSE);
}

BOOL CKadLookupGraph::PreTranslateMessage(MSG *pMsg)
{
	if (NULL != m_pToolTip)
		m_pToolTip->RelayEvent(pMsg);

	return CWnd::PreTranslateMessage(pMsg);
}


void CKadLookupGraph::UpdateSearch(CLookupHistory *pLookupHistory)
{
	if (m_pLookupHistory == pLookupHistory && pLookupHistory != NULL) {
		m_bDbgLog = true;
		if (m_hWnd)
			Invalidate();
	}
}

void CKadLookupGraph::SetSearch(CLookupHistory *pLookupHistory)
{
	if (m_pLookupHistory != pLookupHistory) {
		if (m_pLookupHistory != NULL)
			m_pLookupHistory->SetGUIDeleted();
		if (pLookupHistory != NULL)
			pLookupHistory->SetUsedByGUI();
		m_pLookupHistory = pLookupHistory;
		if (m_hWnd)
			Invalidate();
	}
}

void CKadLookupGraph::OnPaint()
{
	m_aNodesDrawRects.RemoveAll();
	CPaintDC pdc(this);

	CRect rcClient;
	GetClientRect(&rcClient);
	if (rcClient.IsRectEmpty())
		return;

	CMemoryDC dc(&pdc, rcClient);
	CPen *pOldPen = dc.SelectObject(&m_penAxis);
	if (g_xpStyle.IsThemeActive() && g_xpStyle.IsAppThemed()) {
		HTHEME hTheme = g_xpStyle.OpenThemeData(NULL, L"ListView");
		if (hTheme) {
			g_xpStyle.DrawThemeBackground(hTheme, dc.m_hDC, 3, 0, &rcClient, NULL);
			g_xpStyle.CloseThemeData(hTheme);
		}
	} else
		dc.Rectangle(&rcClient);

	rcClient.DeflateRect(1, 1, 1, 1);
	dc.FillSolidRect(rcClient, ::GetSysColor(COLOR_WINDOW));
	rcClient.DeflateRect(1, 1, 1, 1);
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

	int iLeftBorder = 3;
	int iRightBorder = 8;
	int iTopBorder = m_iMaxLabelHeight;
	int iBottomBorder = m_iMaxLabelHeight;

	int iBaseLineX = iLeftBorder;
	int iBaseLineY = rcClient.bottom - iBottomBorder;
	UINT uHistWidth = rcClient.Width() - iLeftBorder - iRightBorder;
	UINT uHistHeight = rcClient.Height() - iTopBorder - iBottomBorder;
	if (uHistHeight == 0) {
		dc.SelectObject(pOldFont);
		dc.SetTextColor(crOldTextColor);
		return;
	}


	dc.MoveTo(iBaseLineX, rcClient.top + iTopBorder);
	dc.LineTo(iBaseLineX, iBaseLineY);
	dc.LineTo(iBaseLineX + uHistWidth, iBaseLineY);

	dc.SelectObject(&m_penAux);

	RECT rcLabel(rcClient);
	rcLabel.left = iBaseLineX;
	rcLabel.bottom = m_iMaxLabelHeight;
	dc.DrawText(m_strYaxis, &rcLabel, DT_LEFT | DT_TOP | DT_NOCLIP);

	rcLabel = rcClient;
	rcLabel.top = rcClient.bottom - m_iMaxLabelHeight + 1;
	dc.DrawText(m_strXaxis, &rcLabel, DT_RIGHT | DT_BOTTOM | DT_NOCLIP);

	if (m_pLookupHistory != NULL && m_pLookupHistory->GetHistoryEntries().GetCount() >= 1) {
		const CArray<Kademlia::CLookupHistory::SLookupHistoryEntry*> &he = m_pLookupHistory->GetHistoryEntries();
		const uint32 hecount = (uint32)he.GetCount();
		// How many nodes can we show without scrolling?
		uint32 uMaxNodes = uHistWidth / NODE_ENTRY_WIDTH;
		uint32 uNodeEntryWidth;
		if (hecount > uMaxNodes /*|| !m_pLookupHistory->IsSearchStopped()*/)
			uNodeEntryWidth = NODE_ENTRY_WIDTH; // While the search is running, use a fixed width
		else
			uNodeEntryWidth = uHistWidth / hecount; // when the search is finished, use all available screen space

		uint32 iVisibleNodes = min(uMaxNodes, hecount);

		// Set the scaling. 3 times the highest distance of the 1/3 closest nodes is the max distance
		CArray<CUInt128> aClosest;
		INT_PTR k = (INT_PTR)(iVisibleNodes / 3u);
		if (!k)
			k = 1;
		for (uint32 i = 1; i <= iVisibleNodes; ++i) {
			if (aClosest.GetCount() < k)
				aClosest.Add(he[hecount - i]->m_uDistance);
			else {
				int iReplace = -1;
				for (int j = (int)aClosest.GetCount(); --j >= 0;)
					if ((iReplace == -1 && aClosest[j] > he[hecount - i]->m_uDistance)
						|| (iReplace >= 0 && aClosest[j] > aClosest[iReplace]))
					{
						iReplace = j;
					}

				if (iReplace >= 0)
					aClosest[iReplace] = he[hecount - i]->m_uDistance;
			}
		}
		CUInt128 uTmpScalingDistance(0ul);
		for (INT_PTR j = aClosest.GetCount(); --j >= 0;)
			if (aClosest[j] > uTmpScalingDistance)
				uTmpScalingDistance = aClosest[j];

		// Convert it to uint64 by cutting of the less significant bits for easier and fast calculating
		uint64 uScalingDistance = 0;
		int iStartChunk;
		for (iStartChunk = 0; iStartChunk < 3; ++iStartChunk) {
			if (uTmpScalingDistance.Get32BitChunk(iStartChunk) > 0) {
				uScalingDistance = ((uint64)uTmpScalingDistance.Get32BitChunk(iStartChunk) << 32) + (uint64)uTmpScalingDistance.Get32BitChunk(iStartChunk + 1);
				break;
			}
		}
		if (uScalingDistance == 0) {
			iStartChunk = 2;
			uScalingDistance = uTmpScalingDistance.Get32BitChunk(3);
		}
		uScalingDistance /= (uHistHeight - NODE_ENTRY_HEIGHT);
		uScalingDistance *= 3;
		ASSERT(uScalingDistance > 0);
		if (uScalingDistance == 0)
			uScalingDistance = 1;


		//if (m_bDbgLog)
		//	AddDebugLogLine(false, _T("KadGraph: Considering %u of %u Nodes, 1/3 Max Distance found: %s"), iVisibleNodes, hecount, uTmpScalingDistance.ToHexString());

		CUInt128 uMaxScalingDistance(uTmpScalingDistance);
		uMaxScalingDistance.Add(uTmpScalingDistance);
		uMaxScalingDistance.Add(uTmpScalingDistance);

		// wow, what a mess, now lets collect drawing points
		for (uint32 i = 1; i <= iVisibleNodes; ++i) {
			CUInt128 uTmpDist = he[hecount - i]->m_uDistance;
			uint64 uDrawYPos;
			if (uTmpDist > uMaxScalingDistance)
				uDrawYPos = iTopBorder;
			else {
				uDrawYPos = (((uint64)uTmpDist.Get32BitChunk(iStartChunk) << 32) + (uint64)uTmpDist.Get32BitChunk(iStartChunk + 1));
				if (uDrawYPos > 0)
					uDrawYPos /= uScalingDistance;
				uDrawYPos = (iBaseLineY - NODE_ENTRY_HEIGHT) - uDrawYPos;
			}
			//if (m_bDbgLog)
			//	AddDebugLogLine(false, _T("KadGraph: Drawing Node %u of %u, Distance: %s, Y-Pos: %u"), (iVisibleNodes - i) + 1, iVisibleNodes, uTmpDist.ToHexString(), uDrawYPos);

			ASSERT(uDrawYPos <= (uHistHeight));
			uint32 nXOffset = 0;
			//if (uMaxNodes > iVisibleNodes && !m_pLookupHistory->IsSearchStopped()) // Fixed width for ongoing searches
			//	nXOffset = NODE_ENTRY_WIDTH * (uMaxNodes - iVisibleNodes);
			CPoint pointNode(uHistWidth - nXOffset - (i * uNodeEntryWidth), (uint32)uDrawYPos);
			m_aNodesDrawRects.Add(CRect(pointNode, CSize(NODE_ENTRY_WIDTH, NODE_ENTRY_HEIGHT)));
		}
		ASSERT(iVisibleNodes == (uint32)m_aNodesDrawRects.GetCount());

		// find HotItem (if any)
		m_iHotItemIdx = -1;
		CPoint ptCursor;
		if (::GetCursorPos(&ptCursor)) {
			CRect rcWnd;
			GetWindowRect(&rcWnd);
			if (rcWnd.PtInRect(ptCursor)) {
				ScreenToClient(&ptCursor);
				m_iHotItemIdx = CheckHotItem(ptCursor);
			}
		}
		m_pToolTip->Activate(static_cast<BOOL>(m_iHotItemIdx >= 0));
		UpdateToolTip();

		CArray<bool> abHotItemConnected;
		if (m_iHotItemIdx >= 0) {
			abHotItemConnected.SetSize(iVisibleNodes);
			for (INT_PTR i = (INT_PTR)iVisibleNodes; --i >= 0;)
				abHotItemConnected[i] = (m_iHotItemIdx == i);
		}
		// start drawing, beginning with the arrowClines connecting the nodes
		// if possible use GDI+ for Anti Aliasing
		extern bool g_bGdiPlusInstalled;
		ULONG_PTR gdiplusToken = 0;
		Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		if (g_bGdiPlusInstalled && Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) == Gdiplus::Ok) {
			{
				Gdiplus::Graphics gdipGraphic(dc);
				gdipGraphic.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
				Gdiplus::AdjustableArrowCap gdipArrow(6, 4);
				Gdiplus::Pen gdipPenGray(Gdiplus::Color(192, 192, 192), 0.8f);
				gdipPenGray.SetCustomEndCap(&gdipArrow);
				Gdiplus::Pen gdipPenDarkGray(Gdiplus::Color(100, 100, 100), 0.8f);
				gdipPenDarkGray.SetCustomEndCap(&gdipArrow);
				Gdiplus::Pen gdipPenRed(Gdiplus::Color(255, 32, 32), 0.8f);
				gdipPenRed.SetCustomEndCap(&gdipArrow);

				for (int i = 0; i < (int)iVisibleNodes; ++i) {
					const CLookupHistory::SLookupHistoryEntry *sEntry = he[hecount - (i + 1)];
					for (int j = 0; j < sEntry->m_liReceivedFromIdx.GetCount(); ++j) {
						int iIdx = sEntry->m_liReceivedFromIdx[j];
						if (iIdx >= (int)(hecount - iVisibleNodes)) {
							CPoint pFrom = m_aNodesDrawRects[hecount - (iIdx + 1)].CenterPoint();
							CPoint pointTo = m_aNodesDrawRects[i].CenterPoint();

							Gdiplus::Pen *pen;
							if ((int)hecount - (iIdx + 1) == m_iHotItemIdx) {
								abHotItemConnected[i] = true;
								pen = &gdipPenRed;
							} else if (i == m_iHotItemIdx) {
								abHotItemConnected[hecount - (iIdx + 1)] = true;
								pen = &gdipPenDarkGray;
							} else
								pen = &gdipPenGray;

							gdipGraphic.DrawLine(pen, pFrom.x, pFrom.y, pointTo.x, pointTo.y);
						}
					}
				}
			}
			Gdiplus::GdiplusShutdown(gdiplusToken);
		} else {
			for (int i = 0; i < (int)iVisibleNodes; ++i) {
				const CLookupHistory::SLookupHistoryEntry *sEntry = he[hecount - (i + 1)];
				for (int j = 0; j < sEntry->m_liReceivedFromIdx.GetCount(); ++j) {
					int iIdx = sEntry->m_liReceivedFromIdx[j];
					if (iIdx >= (int)(hecount - iVisibleNodes)) {

						CPoint pFrom = m_aNodesDrawRects[hecount - (iIdx + 1)].CenterPoint();
						CPoint pointTo = m_aNodesDrawRects[i].CenterPoint();

						if ((int)hecount - (iIdx + 1) == m_iHotItemIdx) {
							abHotItemConnected[i] = true;
							dc.SelectObject(&m_penRed);
						} else {
							if (i == m_iHotItemIdx)
								abHotItemConnected[hecount - (iIdx + 1)] = true;
							dc.SelectObject(&m_penAux);
						}

						POINT aptPoly[3];
						POINT pBase;
						float vecLine[2];
						float vecLeft[2];
						int nWidth = 4;

						// set to point
						aptPoly[0].x = pointTo.x;
						aptPoly[0].y = pointTo.y;

						// build the line vector
						vecLine[0] = (float)aptPoly[0].x - pFrom.x;
						vecLine[1] = (float)aptPoly[0].y - pFrom.y;

						// build the arrow base vector - normal to the line
						vecLeft[0] = -vecLine[1];
						vecLeft[1] = vecLine[0];

						// setup length parameters
						float fLength = (float)sqrt(vecLine[0] * vecLine[0] + vecLine[1] * vecLine[1]);
						float th = nWidth / (2.0f * fLength);
						float ta = nWidth / (2.0f * (tanf(0.3f) / 2.0f) * fLength);

						// find the base of the arrow
						pBase.x = (int)(aptPoly[0].x + -ta * vecLine[0]);
						pBase.y = (int)(aptPoly[0].y + -ta * vecLine[1]);

						// build the points on the sides of the arrow
						aptPoly[1].x = (int)(pBase.x + th * vecLeft[0]);
						aptPoly[1].y = (int)(pBase.y + th * vecLeft[1]);
						aptPoly[2].x = (int)(pBase.x + -th * vecLeft[0]);
						aptPoly[2].y = (int)(pBase.y + -th * vecLeft[1]);
						dc.MoveTo(pFrom);
						dc.LineTo(aptPoly[0].x, aptPoly[0].y);
						dc.Polygon(aptPoly, 3);
					}
				}
			}
		}

		// draw the nodes images
		for (int i = 0; i < (int)iVisibleNodes; ++i) {
			CPoint pointNode = m_aNodesDrawRects[i].CenterPoint();
			pointNode.x -= 8;
			pointNode.y -= 8;

			int iIconIdx;
			const CLookupHistory::SLookupHistoryEntry *sEntry = he[hecount - (i + 1)];
			if (sEntry->m_dwAskedContactsTime > 0) {
				if (sEntry->m_uRespondedContact > 0)
					iIconIdx = sEntry->m_bProvidedCloser ? 0 : 1; // green or blue
				else
					iIconIdx = (::GetTickCount() >= sEntry->m_dwAskedContactsTime + SEC2MS(3)) ? 3 : 2; // red or yellow
			} else if (sEntry->m_bForcedInteresting)
				iIconIdx = 2;
			else {
				iIconIdx = 0;
				ASSERT(0);
			}
			if (m_iHotItemIdx >= 0 && !abHotItemConnected[i])
				m_iml.DrawEx(&dc, iIconIdx, pointNode, CSize(), CLR_NONE, ::GetSysColor(COLOR_WINDOW), ILD_BLEND50);
			else
				m_iml.Draw(&dc, iIconIdx, pointNode, ILD_NORMAL);


			if (sEntry->m_dwAskedSearchItemTime > 0) {
				// Draw the Icon indicating that we asked this Node for results
				// enough space above? if not below
				CPoint pointIndicator = pointNode;
				if (pointIndicator.y - 16 - 4 >= iTopBorder)
					pointIndicator.y -= 20;
				else
					pointIndicator.y += 20;

				switch (m_pLookupHistory->GetType()) {
				case Kademlia::CSearch::FILE:
				case Kademlia::CSearch::KEYWORD:
				case Kademlia::CSearch::NOTES:
					{
						int nOverlayImage;
						if (sEntry->m_uRespondedSearchItem > 0)
							nOverlayImage = 1;
						else
							nOverlayImage = (::GetTickCount() >= sEntry->m_dwAskedSearchItemTime + SEC2MS(5)) ? 2 : 0;
						m_iml.Draw(&dc, 4, pointIndicator, ILD_NORMAL | INDEXTOOVERLAYMASK(nOverlayImage));
					}
					break;
				case Kademlia::CSearch::STOREFILE:
					m_iml.Draw(&dc, 6, pointIndicator, ILD_NORMAL);
					break;
				case Kademlia::CSearch::STOREKEYWORD:
				case Kademlia::CSearch::STORENOTES:
					m_iml.Draw(&dc, 5, pointIndicator, ILD_NORMAL);
				/* Nothing to show
				case CSearch::NODE:
				case CSearch::NODECOMPLETE:
				case CSearch::NODESPECIAL:
				case CSearch::NODEFWCHECKUDP:
				case CSearch::FINDBUDDY:
				default:
					break;*/
				}
			}
		}
	}
	m_bDbgLog = false;
	dc.SelectObject(pOldPen);
	dc.SelectObject(pOldFont);
	dc.SetTextColor(crOldTextColor);
}

void CKadLookupGraph::OnMouseMove(UINT nFlags, CPoint point)
{
	CWnd::OnMouseMove(nFlags, point);
	if (CheckHotItem(point) != m_iHotItemIdx && m_hWnd != NULL)
		Invalidate();
}

int CKadLookupGraph::CheckHotItem(const CPoint &point) const
{
	for (INT_PTR i = m_aNodesDrawRects.GetCount(); --i >= 0;)
		if (m_aNodesDrawRects[i].PtInRect(point))
			return (int)i;
	return -1;
}

void CKadLookupGraph::UpdateToolTip()
{
	CString strToolText;
	if (m_iHotItemIdx >= 0) {
		const CArray<Kademlia::CLookupHistory::SLookupHistoryEntry*> &he = m_pLookupHistory->GetHistoryEntries();
		const int hecount = (int)he.GetCount();
		int iHotItemRealIdx = hecount - (m_iHotItemIdx + 1);
		const CLookupHistory::SLookupHistoryEntry *sEntry = he[iHotItemRealIdx];
		CString strDiscovered;
		if (sEntry->m_liReceivedFromIdx.IsEmpty())
			strDiscovered = GetResString(IDS_ROUTINGTABLE);
		else
			strDiscovered.Format(_T("%i %s"), (int)sEntry->m_liReceivedFromIdx.GetCount(), (LPCTSTR)GetResString(IDS_NODES));

		CString strFoundNodes;
		if (sEntry->m_dwAskedContactsTime > 0) {
			if (sEntry->m_uRespondedContact == 0) {
				if (::GetTickCount() >= sEntry->m_dwAskedContactsTime + SEC2MS(3))
					strFoundNodes = GetResString(IDS_NORESPONSE);
				else
					strFoundNodes = GetResString(IDS_ASKING);
			} else {
				uint32 iUseful = 0;
				for (int i = 0; i < hecount; ++i)
					for (int j = 0; j < he[i]->m_liReceivedFromIdx.GetCount(); ++j)
						if (he[i]->m_liReceivedFromIdx[j] == iHotItemRealIdx)
							++iUseful;

				strFoundNodes.Format(_T("%u (%u)"), sEntry->m_uRespondedContact, iUseful);
			}
		} else
			strFoundNodes = GetResString(IDS_NOTASKED);

		CString strFoundResults;
		if (sEntry->m_dwAskedSearchItemTime > 0) {
			if (sEntry->m_uRespondedSearchItem == 0) {
				if (::GetTickCount() >= sEntry->m_dwAskedSearchItemTime + SEC2MS(5))
					strFoundResults = GetResString(IDS_BUDDYNONE);
				else
					strFoundResults = GetResString(IDS_ASKING);
			} else
				strFoundResults.Format(_T("%u"), sEntry->m_uRespondedSearchItem);
		} else
			strFoundResults = GetResString(IDS_NOTASKED);

		strToolText.Format(_T("%s\n")
			_T("%s: %s\n")
			_T("%s: %u\n<br>\n")
			_T("%s: %s\n")
			_T("%s: %s\n\n")
			_T("%s: %s\a")
			, (LPCTSTR)sEntry->m_uContactID.ToHexString()
			, (LPCTSTR)GetResString(IDS_KADDISTANCE), (LPCTSTR)sEntry->m_uDistance.ToHexString()
			, (LPCTSTR)GetResString(IDS_PROTOCOLVERSION), (unsigned)sEntry->m_byContactVersion
			, (LPCTSTR)GetResString(IDS_DISCOVEREDBY), (LPCTSTR)strDiscovered
			, (LPCTSTR)GetResString(IDS_FOUNDNODESUSEFUL), (LPCTSTR)strFoundNodes
			, (LPCTSTR)GetResString(IDS_SW_RESULT), (LPCTSTR)strFoundResults
		);
	}
	m_pToolTip->UpdateTipText(strToolText, this);
	m_pToolTip->Update();
}

CString CKadLookupGraph::GetCurrentLookupTitle() const
{
	if (m_pLookupHistory == NULL)
		return CString();

	if (!m_pLookupHistory->GetGUIName().IsEmpty())
		return _T('\"') + m_pLookupHistory->GetGUIName() + _T('\"');
	return CSearch::GetTypeName(m_pLookupHistory->GetType());
}

void CKadLookupGraph::OnContextMenu(CWnd*, CPoint point)
{
	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(MF_STRING, MP_AUTOKADLOOKUPGRAPH, GetResString(IDS_AUTOKADLOOKUPGRAPH));
	menu.CheckMenuItem(MP_AUTOKADLOOKUPGRAPH, thePrefs.GetAutoShowLookups() ? MF_CHECKED : MF_UNCHECKED);
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
}

bool CKadLookupGraph::HasActiveLookup() const
{
	return m_pLookupHistory != NULL && !m_pLookupHistory->IsSearchDeleted();
}

void CKadLookupGraph::OnSwitchAutoLookup()
{
	thePrefs.SetAutoShowLookups(!thePrefs.GetAutoShowLookups());
}

bool CKadLookupGraph::GetAutoShowLookups() const
{
	return thePrefs.GetAutoShowLookups();
}