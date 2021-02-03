#pragma once

class CComboBoxEx2 : public CComboBoxEx
{
	DECLARE_DYNAMIC(CComboBoxEx2)
public:
	CComboBoxEx2() = default;
	virtual	~CComboBoxEx2() = default;

	int AddItem(LPCTSTR pszText, int iImage);
	BOOL SelectItemDataString(LPCTSTR pszText);

	virtual BOOL PreTranslateMessage(MSG *pMsg);

protected:
	DECLARE_MESSAGE_MAP()
};

void UpdateHorzExtent(CComboBox &rctlComboBox, int iIconWidth);
HWND GetComboBoxEditCtrl(const CComboBox &cb);