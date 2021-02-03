#pragma once
#include "ResizableLib/ResizablePage.h"
#include "ListCtrlX.h"
#include <list>

class CAbstractFile;
namespace Kademlia
{
	class CKadTag;
	typedef std::list<CKadTag*> TagList;
};

class CMetaDataDlg : public CResizablePage
{
	DECLARE_DYNAMIC(CMetaDataDlg)

	enum
	{
		IDD = IDD_META_DATA
	};
	CAbstractFile *m_pFile; //metadata was displayed for this file
public:
	CMetaDataDlg();
	virtual	~CMetaDataDlg();
	virtual BOOL OnInitDialog();

	void SetFiles(const CSimpleArray<CObject*> *paFiles)	{ m_paFiles = paFiles; m_bDataChanged = true; }
	void SetTagList(Kademlia::TagList *taglist);
	//CString GetTagNameByID(UINT id);
	void Localize();

protected:
	const CSimpleArray<CObject*> *m_paFiles;
	CListCtrlX m_tags;
	Kademlia::TagList *m_taglist;
	CMenu *m_pMenuTags;
	bool m_bDataChanged;

	void RefreshData();

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnSetActive();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnLvnKeydownTags(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnCopyTags();
	afx_msg void OnSelectAllTags();
	afx_msg void OnDestroy();
	afx_msg LRESULT OnDataChanged(WPARAM, LPARAM);
};