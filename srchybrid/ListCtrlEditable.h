#pragma once

class CEditableListCtrl : public CListCtrl
{
public:
	CEditableListCtrl();

	CEdit* GetEditCtrl() const				{ return m_pctrlEdit; }
	void CommitEdit();
	void EndEdit();
	void HideEdit();

	virtual BOOL PreTranslateMessage(MSG *pMsg);

protected:
	CEdit *m_pctrlEdit;
	CComboBox *m_pctrlComboBox;
	int m_iRow;
	int m_iCol;
	int m_iEditRow;
	int m_iEditCol;

	void ShowEdit();
	void ShowComboBoxCtrl();
	void ResetTopPosition();
	void ResetBottomPosition();

	BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT *pResult);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnDestroy();
	afx_msg void OnEnKillFocus();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	//	afx_msg void OnKeydown(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLvnBeginScroll(LPNMHDR, LRESULT *pResult);
	afx_msg void OnLvnColumnClick(LPNMHDR, LRESULT *pResult);
	afx_msg void OnLvnEndScroll(LPNMHDR, LRESULT *pResult);
	afx_msg void OnNmCustomDraw(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnSetFocus(CWnd *pOldWnd);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
};