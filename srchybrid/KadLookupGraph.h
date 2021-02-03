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

#pragma once
namespace Kademlia
{
	class CLookupHistory;
}
class CToolTipCtrlX;

class CKadLookupGraph : public CWnd
{
public:
	CKadLookupGraph();
	virtual	~CKadLookupGraph();

	void	Localize();
	void	Init();
	void	UpdateSearch(Kademlia::CLookupHistory *pLookupHistory);
	void	SetSearch(Kademlia::CLookupHistory *pLookupHistory);

	CString GetCurrentLookupTitle() const;
	bool	HasLookup() const							{ return m_pLookupHistory != NULL; }
	bool	HasActiveLookup() const;
	bool	GetAutoShowLookups() const;

protected:
	afx_msg void OnPaint();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd*, CPoint point);
	void		OnSwitchAutoLookup();
	virtual BOOL PreTranslateMessage(MSG *pMsg);

	DECLARE_MESSAGE_MAP()

	int		CheckHotItem(const CPoint &point) const;
	void	UpdateToolTip();

private:

	CPen m_penAxis;
	CPen m_penAux;
	CPen m_penRed;
	CString m_strXaxis;
	CString m_strYaxis;
	CImageList m_iml;
	CArray<CRect> m_aNodesDrawRects;
	Kademlia::CLookupHistory *m_pLookupHistory;
	CToolTipCtrlX *m_pToolTip;
	int m_iHotItemIdx;
	int m_iMaxLabelHeight;
	int m_iMaxNumLabelWidth;
	bool m_bInitializedFontMetrics;
	bool m_bDbgLog;
};