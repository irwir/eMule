#pragma once
#include "MuleListCtrl.h"

class CServerList;
class CServer;
class CToolTipCtrlX;

class CServerListCtrl : public CMuleListCtrl
{
	DECLARE_DYNAMIC(CServerListCtrl)
public:
	CServerListCtrl();
	virtual	~CServerListCtrl();

	bool	Init();
	bool	AddServer(const CServer *pServer, bool bAddToList = true, bool bRandom = false);
	void	RemoveServer(const CServer *pServer);
	bool	AddServerMetToList(const CString &strFile);
	void	RefreshServer(const CServer *server);
	void	RemoveAllDeadServers();
	void	RemoveAllFilteredServers();
	void	Hide()									{ ShowWindow(SW_HIDE); }
	void	Visible()								{ ShowWindow(SW_SHOW); }
	void	Localize();
	void	ShowServerCount();
	bool	StaticServerFileAppend(CServer *server);
	bool	StaticServerFileRemove(CServer *server);

private:
	static int Undefined_at_bottom(const uint32 i1, const uint32 i2);
	static int Undefined_at_bottom(const CString &s1, const CString &s2);

protected:
	CToolTipCtrlX	*m_tooltip;

	CString CreateSelectedServersURLs();
	void DeleteSelectedServers();

	void SetSelectedServersPriority(UINT uPriority);
	void SetAllIcons();
	static int CALLBACK SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);

	virtual BOOL OnCommand(WPARAM wParam, LPARAM);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnContextMenu(CWnd*, CPoint point);
	afx_msg void OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnGetInfoTip(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnNmCustomDraw(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnNmDblClk(LPNMHDR, LRESULT*);
	afx_msg void OnSysColorChange();
};