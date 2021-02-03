// TitleMenu.h: interface for the CTitleMenu class.
// Based on the code of Per Fikse(1999/06/16) on codeguru.earthweb.com
// Author: Arthur Westerman
// Bug reports by : Brian Pearson
//////////////////////////////////////////////////////////////////////
#pragma once

class CTitleMenu : public CMenu
{
public:
	CTitleMenu();
	virtual	~CTitleMenu();
	CTitleMenu(const CTitleMenu&) = delete;
	CTitleMenu& operator=(const CTitleMenu&) = delete;

	BOOL CreateMenu();
	BOOL DestroyMenu();
	void DeleteIcons();

	void EnableIcons();
	void AddMenuTitle(LPCTSTR lpszTitle, bool bIsIconMenu = false);
	BOOL AppendMenu(UINT nFlags, UINT_PTR nIDNewItem = 0, LPCTSTR lpszNewItem = NULL, LPCTSTR lpszIconName = NULL);
	BOOL InsertMenu(UINT nPosition, UINT nFlags, UINT_PTR nIDNewItem = 0, LPCTSTR lpszNewItem = NULL, LPCTSTR lpszIconName = NULL);
	BOOL RenameMenu(UINT_PTR nIDNewItem, UINT nFlags, LPCTSTR lpszNewItem, LPCTSTR lpszIconName = NULL);

	long GetColor()											{ return m_clLeft; }
	void SetColor(long cl)									{ m_clLeft = cl; }

	long GetGradientColor()									{ return m_clRight; }
	void SetGradientColor(long cl)							{ m_clRight = cl; }

	long GetTextColor()										{ return m_clText; }
	void SetTextColor(long cl)								{ m_clText = cl; }

	long GetEdge()											{ return m_uEdgeFlags; }
	void SetEdge(bool shown, UINT remove = 0, UINT add = 0)	{ m_bDrawEdge = shown; (m_uEdgeFlags ^= remove) |= add; }

	bool HasEnabledItems() const;

	virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMIS);
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDIS);

protected:
	CString m_strTitle;
	CImageList m_ImageList;
	CMap<int, int, int, int> m_mapMenuIdToIconIdx;
	CMapStringToPtr m_mapIconNameToIconIdx;
	CMapStringToPtr m_mapIconNameToBitmap;

	long m_clRight;
	long m_clLeft;
	long m_clText;
	UINT m_uEdgeFlags;
	bool m_bDrawEdge;
	bool m_bIconMenu;

	void DrawMonoIcon(int nIconPos, CPoint nDrawPos, CDC *dc);
	void SetMenuBitmap(UINT nFlags, UINT nIDNewItem, LPCTSTR lpszNewItem, LPCTSTR lpszIconName);
};