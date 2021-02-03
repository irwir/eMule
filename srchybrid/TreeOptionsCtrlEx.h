#pragma once
#include "TreeOptionsCtrl.h"

typedef struct
{
	NMHDR nmhdr;
	HTREEITEM hItem;
} TREEOPTSCTRLNOTIFY;


// pre defined treeview image list indices
#define	TREEOPTSCTRLIMG_EDIT	11


///////////////////////////////////////////////////////////////////////////////
// CTreeOptionsCtrlEx

class CTreeOptionsCtrlEx : public CTreeOptionsCtrl
{
public:
	explicit CTreeOptionsCtrlEx(UINT uImageListColorFlags = ILC_COLOR);
	virtual	~CTreeOptionsCtrlEx() = default;

	void SetEditLabel(HTREEITEM hItem, const CString &rstrLabel);
	void UpdateCheckBoxGroup(HTREEITEM hItem);
	void SetImageListColorFlags(UINT uImageListColorFlags);

	virtual void OnCreateImageList();
	virtual void HandleChildControlLosingFocus();
	virtual BOOL SetRadioButton(HTREEITEM hParent, int nIndex);
	virtual BOOL SetRadioButton(HTREEITEM hItem);
	BOOL NotifyParent(UINT uCode, HTREEITEM hItem);

protected:
	UINT m_uImageListColorFlags;

	virtual void HandleCheckBox(HTREEITEM hItem, BOOL bCheck);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnDestroy();
};

//Dialog Data exchange support

void DDX_TreeCheck(CDataExchange *pDX, int nIDC, HTREEITEM hItem, bool &bCheck);
void DDX_Text(CDataExchange *pDX, int nIDC, HTREEITEM hItem, CString &sText);
void DDX_Text(CDataExchange *pDX, int nIDC, HTREEITEM hItem, int &value);
void DDX_Text(CDataExchange *pDX, int nIDC, HTREEITEM hItem, UINT &value);
void DDX_Text(CDataExchange *pDX, int nIDC, HTREEITEM hItem, long &value);
void DDX_Text(CDataExchange *pDX, int nIDC, HTREEITEM hItem, DWORD &value);
void DDX_Text(CDataExchange *pDX, int nIDC, HTREEITEM hItem, float &value);
void DDX_Text(CDataExchange *pDX, int nIDC, HTREEITEM hItem, double &value);


///////////////////////////////////////////////////////////////////////////////
// CNumTreeOptionsEdit

class CNumTreeOptionsEdit : public CTreeOptionsEdit
{
	DECLARE_DYNCREATE(CNumTreeOptionsEdit)

public:
	CNumTreeOptionsEdit()
		: m_bSelf(false)
	{
	}
	virtual	~CNumTreeOptionsEdit() = default;

	virtual DWORD GetWindowStyle()		{ return CTreeOptionsEdit::GetWindowStyle() | ES_NUMBER; }

protected:
	bool m_bSelf;

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnEnChange();

	DECLARE_MESSAGE_MAP()
};


///////////////////////////////////////////////////////////////////////////////
// CTreeOptionsEditEx

class CTreeOptionsEditEx : public CTreeOptionsEdit
{
	DECLARE_DYNCREATE(CTreeOptionsEditEx)

public:
	CTreeOptionsEditEx()
		: m_bSelf(false)
	{
	}
	virtual	~CTreeOptionsEditEx() = default;

protected:
	bool m_bSelf;

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnEnChange();

	DECLARE_MESSAGE_MAP()
};