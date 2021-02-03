//
//	Class:		CListBoxST
//
//	Compiler:	Visual C++
//	Tested on:	Visual C++ 6.0
//
//	Version:	See GetVersionC() or GetVersionI()
//
//	Created:	13/March/2002
//	Updated:	29/July/2002
//
//	Author:		Davide Calabro'		davide_calabro@yahoo.com
//									http://www.softechsoftware.it
//
//	Disclaimer
//	----------
//	THIS SOFTWARE AND THE ACCOMPANYING FILES ARE DISTRIBUTED "AS IS" AND WITHOUT
//	ANY WARRANTIES WHETHER EXPRESSED OR IMPLIED. NO REPONSIBILITIES FOR POSSIBLE
//	DAMAGES OR EVEN FUNCTIONALITY CAN BE TAKEN. THE USER MUST ASSUME THE ENTIRE
//	RISK OF USING THIS SOFTWARE.
//
//	Terms of use
//	------------
//	THIS SOFTWARE IS FREE FOR PERSONAL USE OR FREEWARE APPLICATIONS.
//	IF YOU USE THIS SOFTWARE IN COMMERCIAL OR SHAREWARE APPLICATIONS YOU
//	ARE GENTLY ASKED TO DONATE 1$ (ONE U.S. DOLLAR) TO THE AUTHOR:
//
//		Davide Calabro'
//		P.O. Box 65
//		21019 Somma Lombardo (VA)
//		Italy
//
#pragma once

class CListBoxST : public CListBox
{
public:
	CListBoxST();
	virtual	~CListBoxST() = default;

	enum
	{
		ST_FULLROWSELECT = 0,	// Highlight full list box item (Default)
		ST_FULLTEXTSELECT,		// Highlight half list box item (Part containing text)
		ST_TEXTSELECT			// Highlight only list box text
	};

	int AddString(LPCTSTR lpszItem, int nImage = -1L);
	int InsertString(int nIndex, LPCTSTR lpszString, int nImage = -1L);
	int DeleteString(int nIndex);
	int ReplaceString(int nIndex, LPCTSTR lpszString, int nImage = -1L);
	void ResetContent();

	int SetItemData(int nIndex, DWORD_PTR dwItemData);
	DWORD_PTR GetItemData(int nIndex);
	int SetItemDataPtr(int nIndex, void *pData);
	void* GetItemDataPtr(int nIndex);

	void EnableItem(int nIndex, BOOL bEnable = TRUE, BOOL bRepaint = TRUE);
	BOOL IsItemEnabled(int nIndex);

	void SetRowSelect(BYTE byRowSelect = ST_FULLROWSELECT, BOOL bRepaint = TRUE);
	void SetImageList(CImageList *pImageList);
	void SetImage(int nIndex, int nImage, BOOL bRepaint = TRUE);
	void GetImage(int nIndex, LPINT lpnImage);

	int MoveUp(int nIndex, BOOL bSetCurSel = TRUE);
	int MoveDown(int nIndex, BOOL bSetCurSel = TRUE);
	int MoveTop(int nIndex, BOOL bSetCurSel = TRUE);
	int MoveBottom(int nIndex, BOOL bSetCurSel = TRUE);

	static short GetVersionI()		{ return 11; }
	static LPCTSTR GetVersionC()	{ return (LPCTSTR)_T("1.1"); }

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CListBoxST)
	//}}AFX_VIRTUAL
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDIStruct);

protected:
	virtual DWORD OnDrawTextBackground(int nIndex, CDC *pDC, const CRect *prcItem, const CRect *prcText, BOOL bIsDisabled, BOOL bIsSelected, COLORREF crSuggestedColor);
	//{{AFX_MSG(CListBoxST)
	afx_msg void OnDestroy();
	afx_msg BOOL OnReflectedDblclk();
	//}}AFX_MSG

	virtual DWORD OnDrawIconBackground(int nIndex, CDC *pDC, const CRect &prcItem, const CRect &prcIcon, BOOL bIsDisabled, BOOL bIsSelected, COLORREF crSuggestedColor);
	virtual DWORD OnDrawIcon(int /*nIndex*/, CDC *pDC, LPCRECT /*prcItem*/, LPCRECT prcIcon, int nImage, BOOL bIsDisabled, BOOL bIsSelected);

	BYTE		m_byRowSelect;
	CImageList	*m_pImageList;
	SIZE		m_szImage;		// Size of each image in image list

private:
	virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);

	int Move(int nOldIndex, int nNewIndex, BOOL bSetCurSel);
	void FreeResources();
	int ReplaceItemData(int nIndex, DWORD_PTR dwItemData, LPVOID pData, int nImage, DWORD dwFlags, BYTE byMask);
	void DeleteItemData(int nIndex);

#pragma pack(push, 1)
	typedef struct _STRUCT_LBDATA
	{
		DWORD_PTR	dwItemData;	// 32-bit value associated with an item in the list box
		LPVOID		pData;		// Pointer associated with an item in the list box
		int			nImage;		// Image associated with an item in the list box. -1 if no image.
		UINT		nFormat;	// Text alignment
		DWORD		dwFlags;	// Bit 0	- Item is disabled

		_STRUCT_LBDATA()
		{
			memset(this, 0, sizeof(_STRUCT_LBDATA));
		}
	} STRUCT_LBDATA;
#pragma pack(pop)

	DECLARE_MESSAGE_MAP()
};