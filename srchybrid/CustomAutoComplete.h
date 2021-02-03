//--------------------------------------------------------------------------------------------
//  Author:         Klaus H. Probst [kprobst@vbbox.com]
//
//--------------------------------------------------------------------------------------------
#pragma once
#include <initguid.h>
#include <shldisp.h>
#include <shlguid.h>

class CCustomAutoComplete :
	public IEnumString
{
private:
	CStringArray m_asList;
	CComPtr<IAutoComplete> m_pac;

	ULONG m_nCurrentElement;
	ULONG m_nRefCount;
	bool m_fBound;
	int m_iMaxItemCount;

	// Constructors/destructors
public:
	CCustomAutoComplete();
	explicit CCustomAutoComplete(const CStringArray &p_sItemList);
	~CCustomAutoComplete();

	// Implementation
public:
	bool Bind(HWND p_hWndEdit, DWORD p_dwOptions = 0, LPCTSTR p_lpszFormatString = NULL);
	void Unbind();
	bool IsBound() const				{ return m_fBound; }

	bool SetList(const CStringArray &p_sItemList);
	const CStringArray& GetList() const;
	int GetItemCount();

	bool AddItem(const CString &p_sItem, int iPos);
	bool RemoveItem(const CString &p_sItem);
	bool RemoveSelectedItem();
	CString GetItem(int pos);

	bool Clear();
	bool Disable();
	bool Enable();

	bool LoadList(LPCTSTR pszFileName);
	bool SaveList(LPCTSTR pszFileName);

public:
	STDMETHOD_(ULONG, AddRef)();
	STDMETHOD_(ULONG, Release)();
	STDMETHOD(QueryInterface)(REFIID riid, void **ppvObject);

public:
	STDMETHOD(Next)(ULONG celt, LPOLESTR *rgelt, ULONG *pceltFetched);
	STDMETHOD(Skip)(ULONG celt);
	STDMETHOD(Reset)(void);
	STDMETHOD(Clone)(IEnumString **ppenum);

	// Internal implementation
private:
	void InternalInit();
	HRESULT EnDisable(BOOL p_fEnable);
	int FindItem(const CString &rstr);
};