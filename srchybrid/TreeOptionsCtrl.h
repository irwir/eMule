/*
Module : TreeOptionsCtrl.h
Purpose: Defines the interface for an MFC class to implement a tree options control
		 similar to the advanced tab as seen on the "Internet options" dialog in
		 Internet Explorer 4 and later
Created: PJN / 31-03-1999


Copyright (c) 1999 - 2003 by PJ Naughter.  (Web: www.naughter.com, Email: pjna@naughter.com)

All rights reserved.

Copyright / Usage Details:

You are allowed to include the source code in any product (commercial, shareware, freeware or otherwise)
when your product is released in binary form. You are allowed to modify the source code in any way you want
except you cannot modify the copyright details at the top of each module. If you want to distribute source
code with your application, then you are only allowed to distribute versions released by the author. This is
to maintain a single distribution point for the source code.

*/


/////////////////////////////// Defines ///////////////////////////////////////
#ifndef __TREEOPTIONSCTRL_H__
#define __TREEOPTIONSCTRL_H__



/////////////////////////////// Includes //////////////////////////////////////

#ifndef __AFXDTCTL_H__
#pragma message("To avoid this message please put afxdtctl.h in your PCH (normally stdafx.h)")
#include <afxdtctl.h>
#endif



/////////////////////////////// Classes ///////////////////////////////////////

//forward declaration
class CTreeOptionsCtrl;
class CTreeOptionsBrowseButton;

//Class which represents a combo box used by the tree options class
class CTreeOptionsCombo : public CComboBox
{
	friend class CTreeOptionsCtrl;

public:
	//Constructors / Destructors
	CTreeOptionsCombo();
	virtual	~CTreeOptionsCombo() = default;

protected:
	//Misc methods
	void SetButtonBuddy(CTreeOptionsBrowseButton *pButton)	{ m_pButtonCtrl = pButton; };
	void SetTreeBuddy(CTreeOptionsCtrl *pTreeCtrl)			{ m_pTreeCtrl = pTreeCtrl; };
	void SetTreeItem(HTREEITEM hItem)						{ m_hTreeCtrlItem = hItem; };
	virtual DWORD GetWindowStyle();
	virtual int GetDropDownHeight();
	BOOL IsRelatedWnd(CWnd *pChild);

	//{{AFX_VIRTUAL(CTreeOptionsCombo)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CTreeOptionsCombo)
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg UINT OnGetDlgCode();
	afx_msg void OnKillFocus(CWnd *pNewWnd);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
	DECLARE_DYNCREATE(CTreeOptionsCombo)

	//Member variables
	CTreeOptionsCtrl *m_pTreeCtrl;
	CTreeOptionsBrowseButton *m_pButtonCtrl;
	HTREEITEM m_hTreeCtrlItem;
};



//Class which represents a combo box which allows a Font Name to be specified
class CTreeOptionsFontNameCombo : public CTreeOptionsCombo
{
public:
	//Constructors / Destructors
	CTreeOptionsFontNameCombo() = default;
	virtual	~CTreeOptionsFontNameCombo() = default;

protected:
	//{{AFX_VIRTUAL(CTreeOptionsFontNameCombo)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CTreeOptionsFontNameCombo)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
	DECLARE_DYNCREATE(CTreeOptionsFontNameCombo)

	//Misc Methods
	virtual DWORD GetWindowStyle();
	int EnumFontProc(CONST LOGFONT *lplf, CONST TEXTMETRIC* /*lptm*/, DWORD /*dwType*/);
	static int CALLBACK _EnumFontProc(CONST LOGFONT *lplf, CONST TEXTMETRIC *lptm, DWORD dwType, LPARAM lpData) noexcept;
};


//Class which represents a combo box which allows a True / False value to be specified
class CTreeOptionsBooleanCombo : public CTreeOptionsCombo
{
public:
	//Constructors / Destructors
	CTreeOptionsBooleanCombo();
	virtual	~CTreeOptionsBooleanCombo();

protected:
	//{{AFX_VIRTUAL(CTreeOptionsBooleanCombo)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CTreeOptionsBooleanCombo)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
	DECLARE_DYNCREATE(CTreeOptionsBooleanCombo)
};


//forward declaration
class CTreeOptionsBrowseButton;

//Class which represents an edit box used by the tree options class
class CTreeOptionsEdit : public CEdit
{
	friend class CTreeOptionsCtrl;
	friend class CTreeOptionsBrowseButton;

public:
	//Constructors / Destructors
	CTreeOptionsEdit();
	virtual	~CTreeOptionsEdit();

protected:
	//Misc methods
	void SetTreeBuddy(CTreeOptionsCtrl *pTreeCtrl)				{ m_pTreeCtrl = pTreeCtrl; };
	void SetButtonBuddy(CTreeOptionsBrowseButton *pButtonCtrl)	{ m_pButtonCtrl = pButtonCtrl; };
	void SetTreeItem(HTREEITEM hItem)							{ m_hTreeCtrlItem = hItem; };
	virtual DWORD GetWindowStyle();
	virtual int GetHeight(int nItemHeight);
	virtual void BrowseForFolder(const CString &sInitialFolder);
	virtual void BrowseForFile(const CString &sInitialFile);
	virtual CString GetBrowseForFolderCaption();
	virtual CString GetBrowseForFileCaption();
	virtual CString GetFileExtensionFilter();

	//{{AFX_VIRTUAL(CTreeOptionsEdit)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CTreeOptionsEdit)
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg UINT OnGetDlgCode();
	afx_msg void OnKillFocus(CWnd *pNewWnd);
	//}}AFX_MSG

	static int CALLBACK SHBrowseSetSelProc(HWND hWnd, UINT uMsg, LPARAM, LPARAM lpData);

	DECLARE_MESSAGE_MAP()
	DECLARE_DYNCREATE(CTreeOptionsEdit)

	//Member variables
	CTreeOptionsCtrl *m_pTreeCtrl;
	CTreeOptionsBrowseButton *m_pButtonCtrl;
	HTREEITEM m_hTreeCtrlItem;
	BOOL m_bDoNotDestroyUponLoseFocus;
};



//Class which represents the spin control which can be used in association with an edit box by the tree options class
class CTreeOptionsSpinCtrl : public CSpinButtonCtrl
{
	friend class CTreeOptionsCtrl;

public:
	//Constructors / Destructors
	CTreeOptionsSpinCtrl();
	virtual	~CTreeOptionsSpinCtrl() = default;

protected:
	//Misc methods
	void SetTreeBuddy(CTreeOptionsCtrl *pTreeCtrl);
	void SetEditBuddy(CTreeOptionsEdit *pEdit);
	void SetTreeItem(HTREEITEM hItem)						{ m_hTreeCtrlItem = hItem; };
	virtual DWORD GetWindowStyle();
	virtual void GetDefaultRange(int &lower, int &upper);

	//{{AFX_VIRTUAL(CTreeOptionsSpinCtrl)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CTreeOptionsSpinCtrl)
	afx_msg void OnKillFocus(CWnd *pNewWnd);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
	DECLARE_DYNCREATE(CTreeOptionsSpinCtrl)

	//Member variables
	CTreeOptionsCtrl *m_pTreeCtrl;
	HTREEITEM m_hTreeCtrlItem;
	CTreeOptionsEdit *m_pEdit;
};


//Class which represents the browse button which can be used in association with an edit box by the tree options class
class CTreeOptionsBrowseButton : public CButton
{
	friend class CTreeOptionsCtrl;

public:
	//Constructors / Destructors
	CTreeOptionsBrowseButton();
	virtual	~CTreeOptionsBrowseButton();

protected:
	//Misc methods
	void            SetTreeBuddy(CTreeOptionsCtrl *pTreeCtrl);
	void            SetTreeItem(HTREEITEM hItem)			{ m_hTreeCtrlItem = hItem; };
	void            SetEditBuddy(CTreeOptionsEdit *pEdit);
	void            SetComboBuddy(CTreeOptionsCombo *pCombo);
	virtual DWORD   GetWindowStyle();
	virtual int     GetWidth();
	virtual CString GetCaption();
	COLORREF        GetColor() const						{ return m_Color; };
	void            SetColor(COLORREF color);
	void            GetFontItem(LOGFONT *pLogFont);
	void            SetFontItem(const LOGFONT *pLogFont);
	virtual void    BrowseForColor();
	virtual void    BrowseForFont();
	virtual void    BrowseForOpaque();

	//{{AFX_VIRTUAL(CTreeOptionsBrowseButton)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CTreeOptionsBrowseButton)
	afx_msg void OnKillFocus(CWnd *pNewWnd);
	afx_msg void OnClicked();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
	DECLARE_DYNCREATE(CTreeOptionsBrowseButton)

	//Member variables
	COLORREF m_Color;
	LOGFONT m_Font;
	CTreeOptionsCtrl *m_pTreeCtrl;
	CTreeOptionsEdit *m_pEdit;
	CTreeOptionsCombo *m_pCombo;
	HTREEITEM m_hTreeCtrlItem;
};



//Class which is used for browsing for filenames
class CTreeOptionsFileDialog : public CFileDialog
{
public:
	//Constructors / Destructors
	explicit CTreeOptionsFileDialog(BOOL bOpenFileDialog, LPCTSTR lpszDefExt = NULL, LPCTSTR lpszFileName = NULL
		, DWORD dwFlags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, LPCTSTR lpszFilter = NULL, CWnd *pParentWnd = NULL);

protected:
	DECLARE_DYNAMIC(CTreeOptionsFileDialog)

	virtual void OnInitDone();

	//{{AFX_MSG(CTreeOptionsFileDialog)
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};



//Class which represents a date / time control used by the list options class
class CTreeOptionsDateCtrl : public CDateTimeCtrl
{
	friend class CTreeOptionsCtrl;

public:
	//Constructors / Destructors
	CTreeOptionsDateCtrl();
	virtual	~CTreeOptionsDateCtrl();

	//Methods
	virtual CString GetDisplayText(const SYSTEMTIME &st);

protected:
	//Misc methods
	void SetTreeBuddy(CTreeOptionsCtrl *pTreeCtrl)		{ m_pTreeCtrl = pTreeCtrl; };
	void SetTreeItem(HTREEITEM hItem)					{ m_hTreeCtrlItem = hItem; };
	virtual DWORD GetWindowStyle();
	virtual BOOL IsRelatedWnd(CWnd *pChild);
	void GetDateTime(SYSTEMTIME &st) const				{ memcpy(&st, &m_SystemTime, sizeof(SYSTEMTIME)); };
	void SetDateTime(const SYSTEMTIME &st)				{ memcpy(&m_SystemTime, &st, sizeof(SYSTEMTIME)); };

	//{{AFX_VIRTUAL(CTreeOptionsDateCtrl)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CTreeOptionsDateCtrl)
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg UINT OnGetDlgCode();
	afx_msg void OnKillFocus(CWnd *pNewWnd);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
	DECLARE_DYNCREATE(CTreeOptionsDateCtrl)

	//Member variables
	CTreeOptionsCtrl *m_pTreeCtrl;
	HTREEITEM m_hTreeCtrlItem;
	BOOL m_bDoNotDestroyUponLoseFocus;
	SYSTEMTIME m_SystemTime;
};



//Class which represents a time control used by the list options class
class CTreeOptionsTimeCtrl : public CTreeOptionsDateCtrl
{
public:
	//Constructors / Destructors
	CTreeOptionsTimeCtrl();
	virtual	~CTreeOptionsTimeCtrl();

	//methods
	virtual CString GetDisplayText(const SYSTEMTIME &st);

protected:
	virtual DWORD GetWindowStyle();

	//{{AFX_VIRTUAL(CTreeOptionsTimeCtrl)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CTreeOptionsTimeCtrl)
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

	DECLARE_DYNCREATE(CTreeOptionsTimeCtrl)
};



//Class which represents IP Address control used by the list options class
class CTreeOptionsIPAddressCtrl : public CIPAddressCtrl
{
	friend class CTreeOptionsCtrl;
public:
	//Constructors / Destructors
	CTreeOptionsIPAddressCtrl();
	virtual	~CTreeOptionsIPAddressCtrl();

	//methods
	virtual CString GetDisplayText(DWORD dwAddress);

protected:
	//Misc methods
	void SetTreeBuddy(CTreeOptionsCtrl *pTreeCtrl)			{ m_pTreeCtrl = pTreeCtrl; };
	void SetTreeItem(HTREEITEM hItem)						{ m_hTreeCtrlItem = hItem; };
	virtual DWORD GetWindowStyle();
	DWORD GetIPAddress() const								{ return m_dwAddress; };
	void SetIPAddress(DWORD dwAddress)						{ m_dwAddress = dwAddress; };
	virtual BOOL IsRelatedWnd(CWnd *pChild);

	//{{AFX_VIRTUAL(CTreeOptionsIPAddressCtrl)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CTreeOptionsIPAddressCtrl)
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg UINT OnGetDlgCode();
	afx_msg void OnKillFocus(CWnd *pNewWnd);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
	DECLARE_DYNCREATE(CTreeOptionsIPAddressCtrl)

	//Member variables
	CTreeOptionsCtrl *m_pTreeCtrl;
	HTREEITEM m_hTreeCtrlItem;
	BOOL	m_bDoNotDestroyUponLoseFocus;
	DWORD	m_dwAddress;
};



//Class which is stored in the tree options item data
class CTreeOptionsItemData
{
public:
	//Enums
	enum ControlType
	{
		Unknown,
		Normal,
		Spin,
		FileBrowser,
		FolderBrowser,
		ColorBrowser,
		FontBrowser,
		CheckBox,
		RadioButton,
		ComboBox,
		EditBox,
		DateTimeCtrl,
		IPAddressCtrl,
		OpaqueBrowser,
	};

	//Data
	CRuntimeClass	*m_pRuntimeClass1;
	CRuntimeClass	*m_pRuntimeClass2;
	ControlType		m_Type;
	DWORD			m_dwItemData;
	COLORREF		m_Color;
	LOGFONT			m_Font;
	SYSTEMTIME		m_DateTime;
	DWORD			m_dwIPAddress;
	BOOL			m_bDrawColorForIcon;


	//Methods
	CTreeOptionsItemData()
		: m_pRuntimeClass1()
		, m_pRuntimeClass2()
		, m_Type(Unknown)
		, m_dwItemData(_UI32_MAX)
		, m_Color(RGB(255, 0, 0))
		, m_Font()
		, m_DateTime()
		, m_dwIPAddress()
		, m_bDrawColorForIcon(TRUE)
	{
	}
};



//The actual tree options control class
class CTreeOptionsCtrl : public CTreeCtrl
{
	friend class CTreeOptionsEdit;
	friend class CTreeOptionsStatic;
	friend class CTreeOptionsCombo;
	friend class CTreeOptionsSpinCtrl;
	friend class CTreeOptionsBrowseButton;
	friend class CTreeOptionsDateCtrl;
	friend class CTreeOptionsIPAddressCtrl;

public:
	//Constructors / Destructors
	CTreeOptionsCtrl();
	virtual	~CTreeOptionsCtrl();

	//Misc
	void    SetAutoSelect(BOOL bAutoSelect)					{ m_bAutoSelect = bAutoSelect; };
	BOOL    GetAutoSelect() const							{ return m_bAutoSelect; };
	void    SetImageListResourceIDToUse(UINT nResourceID)	{ m_nilID = nResourceID; };
	UINT    GetImageListResourceIDToUse() const				{ return m_nilID; };
	void    SetToggleOverIconOnly(BOOL bToggle)				{ m_bToggleOverIconOnly = bToggle; };
	BOOL    GetToggleOverIconOnly() const					{ return m_bToggleOverIconOnly; };
	DWORD   GetUserItemData(HTREEITEM hItem) const;
	BOOL    SetUserItemData(HTREEITEM hItem, DWORD dwData);
	void    SetTextSeparator(const CString &sSeparator)		{ m_sSeparator = sSeparator; };
	const CString& GetTextSeparator() const					{ return m_sSeparator; };
	void    Clear();
	virtual BOOL DeleteAllItems();

	//Inserting items into the control
	HTREEITEM InsertGroup(LPCTSTR lpszItem, int nImage, HTREEITEM hParent = TVI_ROOT, HTREEITEM hAfter = TVI_LAST, DWORD dwItemData = _UI32_MAX);
	HTREEITEM InsertCheckBox(LPCTSTR lpszItem, HTREEITEM hParent, BOOL bCheck = TRUE, HTREEITEM hAfter = TVI_LAST, DWORD dwItemData = _UI32_MAX);
	HTREEITEM InsertRadioButton(LPCTSTR lpszItem, HTREEITEM hParent, BOOL bCheck = TRUE, HTREEITEM hAfter = TVI_LAST, DWORD dwItemData = _UI32_MAX);

	//Validation methods
	BOOL IsGroup(HTREEITEM hItem) const;
	BOOL IsCheckBox(HTREEITEM hItem) const;
	BOOL IsRadioButton(HTREEITEM hItem) const;
	BOOL IsEditBox(HTREEITEM hItem) const;
	BOOL IsFileItem(HTREEITEM hItem) const;
	BOOL IsFolderItem(HTREEITEM hItem) const;
	BOOL IsColorItem(HTREEITEM hItem) const;
	BOOL IsFontItem(HTREEITEM hItem) const;
	BOOL IsDateTimeItem(HTREEITEM hItem) const;
	BOOL IsIPAddressItem(HTREEITEM hItem) const;
	BOOL IsOpaqueItem(HTREEITEM hItem) const;

	//Setting / Getting combo states
	BOOL SetCheckBox(HTREEITEM hItem, BOOL bCheck);
	BOOL GetCheckBox(HTREEITEM hItem, BOOL &bCheck) const;

	//Setting / Getting radio states
	virtual BOOL SetRadioButton(HTREEITEM hParent, int nIndex);
	virtual BOOL SetRadioButton(HTREEITEM hItem);
	BOOL GetRadioButton(HTREEITEM hParent, int &nIndex, HTREEITEM &hCheckItem) const;
	BOOL GetRadioButton(HTREEITEM hItem, BOOL &bCheck) const;

	//Enable / Disable items
	BOOL SetGroupEnable(HTREEITEM hItem, BOOL bEnable);
	BOOL SetCheckBoxEnable(HTREEITEM hItem, BOOL bEnable);
	BOOL SetRadioButtonEnable(HTREEITEM hItem, BOOL bEnable);
	BOOL GetRadioButtonEnable(HTREEITEM hItem, BOOL &bEnable) const;
	BOOL GetCheckBoxEnable(HTREEITEM hItem, BOOL &bEnable) const;

	//Adding a combo box to an item
	BOOL    AddComboBox(HTREEITEM hItem, CRuntimeClass *pRuntimeClass, DWORD dwItemData = _UI32_MAX);
	CString GetComboText(HTREEITEM hItem) const;
	void    SetComboText(HTREEITEM hItem, const CString &sComboText);

	//Adding an edit box (and a spin control) to an item
	BOOL    AddEditBox(HTREEITEM hItem, CRuntimeClass *pRuntimeClassEditCtrl, DWORD dwItemData = _UI32_MAX);
	BOOL    AddEditBox(HTREEITEM hItem, CRuntimeClass *pRuntimeClassEditCtrl, CRuntimeClass *pRuntimeClassSpinCtrl, DWORD dwItemData = _UI32_MAX);
	CString GetEditText(HTREEITEM hItem) const;
	void    SetEditText(HTREEITEM hItem, const CString &sEditText);

	//Adding a file / Folder edit box (and a browse button) to an item
	BOOL    AddFileEditBox(HTREEITEM hItem, CRuntimeClass *pRuntimeClassEditCtrl, CRuntimeClass *pRuntimeClassButton, DWORD dwItemData = _UI32_MAX);
	CString GetFileEditText(HTREEITEM hItem) const;
	void    SetFileEditText(HTREEITEM hItem, const CString &sEditText);
	BOOL    AddFolderEditBox(HTREEITEM hItem, CRuntimeClass *pRuntimeClassEditCtrl, CRuntimeClass *pRuntimeClassButton, DWORD dwItemData = _UI32_MAX);
	CString GetFolderEditText(HTREEITEM hItem) const;
	void    SetFolderEditText(HTREEITEM hItem, const CString &sEditText);

	//Adding a Color selector to an item
	BOOL     AddColorSelector(HTREEITEM hItem, CRuntimeClass *pRuntimeClassButton, DWORD dwItemData = _UI32_MAX, BOOL bDrawColorForIcon = TRUE);
	COLORREF GetColor(HTREEITEM hItem) const;
	void     SetColor(HTREEITEM hItem, COLORREF color);

	//Adding a font name selector to an item
	BOOL     AddFontSelector(HTREEITEM hItem, CRuntimeClass *pRuntimeClassButton, DWORD dwItemData = _UI32_MAX);
	void     GetFontItem(HTREEITEM hItem, LOGFONT *pLogFont) const;
	void     SetFontItem(HTREEITEM hItem, const LOGFONT *pLogFont);

	//Adding a Date Time  selector to an item
	BOOL     AddDateTime(HTREEITEM hItem, CRuntimeClass *pRuntimeClassDateTime, DWORD dwItemData = _UI32_MAX);
	void     GetDateTime(HTREEITEM hItem, SYSTEMTIME &st) const;
	void     SetDateTime(HTREEITEM hItem, const SYSTEMTIME &st);

	//Adding an IP Address selector to an item
	BOOL     AddIPAddress(HTREEITEM hItem, CRuntimeClass *pRuntimeClassDateTime, DWORD dwItemData = _UI32_MAX);
	DWORD    GetIPAddress(HTREEITEM hItem) const;
	void     SetIPAddress(HTREEITEM hItem, DWORD dwAddress);

	//Adding an Opaque selector to an item
	BOOL     AddOpaque(HTREEITEM hItem, CRuntimeClass *pRuntimeClass1, CRuntimeClass *pRuntimeClass2, DWORD dwItemData = _UI32_MAX);
	DWORD    GetOpaque(HTREEITEM hItem) const;
	void     SetOpaque(HTREEITEM hItem, DWORD dwItemData);

	//Virtual methods
	virtual void OnCreateImageList();
	virtual HTREEITEM CopyItem(HTREEITEM hItem, HTREEITEM htiNewParent, HTREEITEM htiAfter = TVI_LAST);
	virtual HTREEITEM CopyBranch(HTREEITEM htiBranch, HTREEITEM htiNewParent, HTREEITEM htiAfter = TVI_LAST);

protected:
	//Variables
	CImageList					m_ilTree;
	CTreeOptionsCombo			*m_pCombo;
	CTreeOptionsEdit			*m_pEdit;
	CTreeOptionsSpinCtrl		*m_pSpin;
	CTreeOptionsBrowseButton	*m_pButton;
	CTreeOptionsDateCtrl		*m_pDateTime;
	CTreeOptionsIPAddressCtrl	*m_pIPAddress;
	HTREEITEM					m_hControlItem;
	CFont						m_Font;
	CString						m_sSeparator;
	UINT						m_nilID;
	BOOL						m_bToggleOverIconOnly;
	BOOL						m_bAutoSelect;
	BOOL						m_bBeingCleared;

	//Methods
	virtual void DestroyOldChildControl();
	virtual void RemoveChildControlText(HTREEITEM hItem);
	virtual void CreateNewChildControl(HTREEITEM hItem);
	virtual void CreateSpinCtrl(CRuntimeClass *pRuntimeClassSpinCtrl, const CRect &rItem, const CRect &rText, const CRect &rPrimaryControl);
	virtual void CreateBrowseButton(CRuntimeClass *pRuntimeClassBrowseButton, const CRect &rItem, const CRect &rText);
	virtual void UpdateTreeControlValueFromChildControl(HTREEITEM hItem);
	virtual void HandleChildControlLosingFocus();
	virtual void HandleCheckBox(HTREEITEM hItem, BOOL bCheck);
	virtual BOOL SetEnabledSemiCheckBox(HTREEITEM hItem, BOOL bSemi);
	virtual BOOL GetSemiCheckBox(HTREEITEM hItem, BOOL &bSemi) const;
	virtual int  GetIndentPostion(HTREEITEM hItem) const;
	virtual void MemDeleteAllItems(HTREEITEM hParent);

	//{{AFX_VIRTUAL(CTreeOptionsCtrl)
	virtual void PreSubclassWindow();
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CTreeOptionsCtrl)
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnDestroy();
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKillFocus(CWnd *pNewWnd);
	//}}AFX_MSG
	afx_msg BOOL OnNmClick(LPNMHDR, LRESULT *pResult);
	afx_msg BOOL OnSelchanged(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg BOOL OnDeleteItem(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg LRESULT OnSetFocusToChild(WPARAM, LPARAM);
	afx_msg LRESULT OnRepositionChild(WPARAM, LPARAM);
	afx_msg BOOL OnCustomDraw(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg BOOL OnItemExpanding(LPNMHDR, LRESULT *pResult);

	DECLARE_DYNAMIC(CTreeOptionsCtrl)

	DECLARE_MESSAGE_MAP()
};

//Dialog Data exchange support
void DDX_TreeCheck(CDataExchange *pDX, int nIDC, HTREEITEM hItem, BOOL &bCheck);
void DDX_TreeRadio(CDataExchange *pDX, int nIDC, HTREEITEM hParent, int &nIndex);
void DDX_TreeEdit(CDataExchange *pDX, int nIDC, HTREEITEM hItem, CString &sText);
void DDX_TreeEdit(CDataExchange *pDX, int nIDC, HTREEITEM hItem, int &nValue);
void DDX_TreeCombo(CDataExchange *pDX, int nIDC, HTREEITEM hItem, CString &sText);
void DDX_TreeFileEdit(CDataExchange *pDX, int nIDC, HTREEITEM hItem, CString &sText);
void DDX_TreeFolderEdit(CDataExchange *pDX, int nIDC, HTREEITEM hItem, CString &sText);
void DDX_TreeColor(CDataExchange *pDX, int nIDC, HTREEITEM hItem, COLORREF &color);
void DDX_TreeFont(CDataExchange *pDX, int nIDC, HTREEITEM hItem, LOGFONT *pLogFont);
void DDX_TreeBoolean(CDataExchange *pDX, int nIDC, HTREEITEM hItem, BOOL &bValue);
void DDX_TreeDateTime(CDataExchange *pDX, int nIDC, HTREEITEM hItem, SYSTEMTIME &st);
void DDX_TreeIPAddress(CDataExchange *pDX, int nIDC, HTREEITEM hItem, DWORD &dwAddress);


#endif //__TREEOPTIONSCTRL_H__