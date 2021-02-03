//this file is part of eMule
//Copyright (C)2002-2008 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
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
#include "stdafx.h"
#include "emule.h"
#include "emuledlg.h"
#include "DropTarget.h"
#include "OtherFunctions.h"
#include <intshcut.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define FILEEXT_INETSHRTCUTA		"url"						// ANSI string
#define FILEEXT_INETSHRTCUTW		L"url"						// Unicode string
#define FILEEXT_INETSHRTCUT			_T(FILEEXT_INETSHRTCUTA)
#define FILEEXTDOT_INETSHRTCUTA		"." FILEEXT_INETSHRTCUTA	// ANSI string
#define FILEEXTDOT_INETSHRTCUTW		L"." FILEEXT_INETSHRTCUTW	// Unicode string
#define FILEEXTDOT_INETSHRTCUT		_T(".") FILEEXT_INETSHRTCUT
//#define FILETYPE_INETSHRTCUT		_T("Internet Shortcut File")
//#define FILEFLT_INETSHRTCUT		FILETYPE_INETSHRTCUT _T("s (*") FILEEXTDOT_INETSHRTCUT _T(")|*") FILEEXTDOT_INETSHRTCUT _T("|")

BOOL IsUrlSchemeSupportedW(LPCWSTR pszUrl)
{
	static const struct SCHEME
	{
		LPCWSTR pszPrefix;
		int iLen;
	} _aSchemes[] =
	{
#define SCHEME_ENTRY(prefix)	{ prefix, _countof(prefix)-1 }
		SCHEME_ENTRY(L"ed2k://"),
		SCHEME_ENTRY(L"magnet:?")
#undef SCHEME_ENTRY
	};

	for (unsigned i = 0; i < _countof(_aSchemes); ++i)
		if (wcsncmp(pszUrl, _aSchemes[i].pszPrefix, _aSchemes[i].iLen) == 0)
			return TRUE;
	return FALSE;
}

// GetFileExtA -- ANSI version
//
// This function is thought to be used only for filenames which have been
// validated by 'GetFullPathName' or similar functions.
LPCSTR GetFileExtA(LPCSTR pszPathA, int iLen /*= -1*/)
{
	// Just search the last '.'-character which comes after an optionally
	// available last '\'-char.
	int iPos = iLen >= 0 ? iLen : (int)strlen(pszPathA);
	while (iPos-- > 0) {
		if (pszPathA[iPos] == '.')
			return &pszPathA[iPos];
		if (pszPathA[iPos] == '\\')
			break;
	}

	return NULL;
}

// GetFileExtW -- Unicode version
//
// This function is thought to be used only for filenames which have been
// validated by 'GetFullPathName' or similar functions.
LPCWSTR GetFileExtW(LPCWSTR pszPathW, int iLen /*= -1*/)
{
	// Just search the last '.'-character which comes after an optionally
	// available last '\'-char.
	int iPos = iLen >= 0 ? iLen : (int)wcslen(pszPathW);
	while (iPos-- > 0) {
		if (pszPathW[iPos] == L'.')
			return &pszPathW[iPos];
		if (pszPathW[iPos] == L'\\')
			break;
	}

	return NULL;
}


//////////////////////////////////////////////////////////////////////////////
// PASTEURLDATA

struct PASTEURLDATA
{
	PASTEURLDATA()
		: m_eType(InvalidType)
		, m_dwFlags()
	{
	}

	explicit PASTEURLDATA(BSTR bstrText, DWORD dwFlags = 0)
		: m_eType(HTMLText)
		, m_dwFlags(dwFlags)
		, m_bstrURLs(bstrText)
	{
	}

	explicit PASTEURLDATA(IDispatch *pIDispatch, DWORD dwFlags = 0)
		: m_eType(Document)
		, m_dwFlags(dwFlags)
		, m_pIDispDoc(pIDispatch)
	{
	}

	enum DataType
	{
		InvalidType = -1,
		Text,
		HTMLText,
		Document
	} m_eType;

	DWORD m_dwFlags;

	union
	{
		BSTR m_bstrURLs;
		IDispatch *m_pIDispDoc;
	};
};


//////////////////////////////////////////////////////////////////////////////
// CMainFrameDropTarget

CMainFrameDropTarget::CMainFrameDropTarget()
	: m_bDropDataValid()
	, m_cfHTML((CLIPFORMAT)RegisterClipboardFormat(_T("HTML Format")))
	, m_cfShellURL((CLIPFORMAT)RegisterClipboardFormat(CFSTR_SHELLURL))
{
	ASSERT(m_cfHTML != 0);
	ASSERT(m_cfShellURL != 0);
}

HRESULT CMainFrameDropTarget::PasteHTMLDocument(IHTMLDocument2 *doc, PASTEURLDATA* /*pPaste*/)
{
	HRESULT hrPasteResult = S_FALSE; // default: nothing was pasted
	int iURLElements = 0;

	// get_links		HREF	all <LINK> and <AREA> elements -> that's *wrong* it also contains all <A> elements!
	// get_anchors		HREF	all <A> elements which have a NAME or ID value!

	//
	// Links
	//
	CComPtr<IHTMLElementCollection> links;
	if (doc->get_links(&links) == S_OK) {
		long lLinks;
		if (links->get_length(&lLinks) == S_OK && lLinks > 0) {
			iURLElements += lLinks;
			CComVariant vaIndex(0L);
			CComVariant vaNull(0L);
			for (long i = 0; i < lLinks; ++i) {
				vaIndex.lVal = i;
				CComPtr<IDispatch> item;
				if (links->item(vaIndex, vaNull, &item) == S_OK) {
					CComPtr<IHTMLAnchorElement> anchor;
					if (SUCCEEDED(item->QueryInterface(&anchor))) {
						CComBSTR bstrHref;
						if (anchor->get_href(&bstrHref) == S_OK && bstrHref.Length() > 0 && IsUrlSchemeSupportedW(bstrHref)) {
							theApp.emuledlg->ProcessED2KLink(CString(bstrHref));
							hrPasteResult = S_OK;
						}
						anchor.Release(); // conserve memory
					}
				}
			}
		}
		links.Release(); // conserve memory
	}

	//
	// Text
	//
	// The explicit handling of text is needed, if we're looking at contents which were copied
	// to the clipboard in HTML format -- although it is simple raw text!! This situation applies,
	// if the user opens the "View Partial Source" HTML window for some selected HTML contents,
	// and copies some text (e.g. a URL) to the clipboard. In that case we'll get the raw text
	// as HTML contents!!!
	//
	// PROBLEM: We can *not* always process the HTML elements (anchors, ...) *and* the inner text.
	// The following example (a rather *usual* one) would lead to the adding of the same URL twice
	// because the URL is noted as a HREF *and* as the inner text.
	//
	// <P><A href="http://www.domain.com/image.gif">http://www.domain.com/image.gif</A></P>
	//
	// So, in practice, the examination of the 'innerText' is only done, if there were no other
	// HTML elements in the document.
	//
	if (iURLElements == 0) {
		CComPtr<IHTMLElement> el;
		if (doc->get_body(&el) == S_OK) {
			CComBSTR bstr;
			if (el->get_innerText(&bstr) == S_OK && bstr.Length() > 0) {
				LPCWSTR pwsz = bstr;
				while (*pwsz != L'\0' && iswspace(*pwsz)) // Skip white spaces
					++pwsz;

				// PROBLEM: The 'innerText' does not contain any HTML tags, but it *MAY* contain
				// HTML comments like "<!--StartFragment-->...<!--EndFragment-->". Those
				// tags have to be explicitly parsed to get the real raw text contents.
				// Those Start- and End-tags are available if the text is copied into the clipboard
				// from a HTML window which was open with "View Partial Source"!
				static const WCHAR _wszStartFrag[] = L"<!--StartFragment-->";
				if (wcsncmp(pwsz, _wszStartFrag, _countof(_wszStartFrag) - 1) == 0) {
					pwsz += _countof(_wszStartFrag) - 1;

					// If there's a Start-tag, search for an End-tag.
					static const WCHAR _wszEndFrag[] = L"<!--EndFragment-->";
					LPWSTR pwszEnd = (LPWSTR)bstr + bstr.Length();
					pwszEnd -= _countof(_wszEndFrag) - 1;
					if (pwszEnd >= pwsz) {
						if (wcsncmp(pwszEnd, _wszEndFrag, _countof(_wszEndFrag) - 1) == 0)
							*pwszEnd = L'\0'; // Ugly but efficient, terminate the BSTR!
					}
				}

				// Search all white-space terminated strings and check for a valid URL-scheme
				while (*pwsz != L'\0') {
					while (*pwsz != L'\0' && iswspace(*pwsz)) // Skip white spaces
						++pwsz;

					if (IsUrlSchemeSupportedW(pwsz)) {
						LPCWSTR pwszEnd = pwsz;
						while (*pwszEnd != L'\0' && !iswspace(*pwszEnd)) // Search next white space (end of current string)
							++pwszEnd;
						int iLen = (int)(pwszEnd - pwsz);
						if (iLen > 0) {
							CString strURL(pwsz, iLen);
							theApp.emuledlg->ProcessED2KLink(strURL);
							hrPasteResult = S_OK;
							pwsz += iLen;
						}
					} else {
						while (*pwsz != L'\0' && !iswspace(*pwsz)) // Search next white space (end of current string)
							++pwsz;
					}

					while (*pwsz != L'\0' && iswspace(*pwsz)) // Skip white spaces
						++pwsz;
				}
			}
		}
	}

	return hrPasteResult;
}

HRESULT CMainFrameDropTarget::PasteHTML(PASTEURLDATA *pPaste)
{
	HRESULT hrPasteResult = S_FALSE; // default: nothing was pasted
	if (pPaste->m_bstrURLs[0] != L'\0') {
		CComPtr<IHTMLDocument2> doc;
		if (SUCCEEDED(doc.CoCreateInstance(CLSID_HTMLDocument, NULL))) {
			SAFEARRAY *psfHtmlLines = SafeArrayCreateVector(VT_VARIANT, 0, 1);
			if (psfHtmlLines != NULL) {
				VARIANT *pva;
				if (SafeArrayAccessData(psfHtmlLines, (void**)&pva) == S_OK) {
					pva->vt = VT_BSTR;
					pva->bstrVal = pPaste->m_bstrURLs;
					VERIFY(SafeArrayUnaccessData(psfHtmlLines) == S_OK);

					// Build the HTML document
					//
					// NOTE: 'bstrHTML' may contain a complete HTML document (see CF_HTML) or
					// just a fragment (without <HTML>, <BODY>, ... tags).
					//
					// WOW! We even can pump partially (but well defined) HTML stuff into the
					// document (e.g. contents without <HTML>, <BODY>...) *and* we are capable
					// of accessing the HTML object model (can use 'get_links'...)
					if (doc->write(psfHtmlLines) == S_OK)
						hrPasteResult = PasteHTMLDocument(doc, pPaste);
					else
						hrPasteResult = E_FAIL;
				} else
					hrPasteResult = E_OUTOFMEMORY;

				// Destroy the array *and* all of the data (BSTRs!)
				if (SafeArrayAccessData(psfHtmlLines, (void**)&pva) == S_OK) {
					// 'Remove' the BSTR which was specified before, to *NOT* have it deleted by 'SafeArrayDestroy'
					pva->vt = VT_NULL;
					pva->bstrVal = NULL;
					VERIFY(SafeArrayUnaccessData(psfHtmlLines) == S_OK);
				}
				VERIFY(SafeArrayDestroy(psfHtmlLines) == S_OK);
			} else
				hrPasteResult = E_OUTOFMEMORY;
		} else
			hrPasteResult = E_FAIL;
	}
	return hrPasteResult;
}

HRESULT CMainFrameDropTarget::PasteHTML(COleDataObject &data)
{
	HRESULT hrPasteResult = E_FAIL;
	HGLOBAL hMem = data.GetGlobalData(m_cfHTML);
	if (hMem != NULL) {
		LPCSTR pszClipboard = (LPCSTR)GlobalLock(hMem);
		if (pszClipboard != NULL) {
			hrPasteResult = S_FALSE; // default: nothing was pasted
			LPCSTR pszHTML = strchr(pszClipboard, '<');
			if (pszHTML != NULL) {
				CComBSTR bstrHTMLText(pszHTML);
				PASTEURLDATA Paste(bstrHTMLText);
				hrPasteResult = PasteHTML(&Paste);
			}
			GlobalUnlock(hMem);
		}
		GlobalFree(hMem);
	}
	return hrPasteResult;
}

HRESULT CMainFrameDropTarget::PasteText(CLIPFORMAT cfData, COleDataObject &data)
{
	HRESULT hrPasteResult = E_FAIL;
	HANDLE hMem = data.GetGlobalData(cfData);
	if (hMem != NULL) {
		LPCSTR pszUrlA = (LPCSTR)GlobalLock(hMem);
		if (pszUrlA != NULL) {
			// skip white space
			while (isspace(*pszUrlA))
				++pszUrlA;

			hrPasteResult = S_FALSE; // default: nothing was pasted
			if (_strnicmp(pszUrlA, "ed2k://|", 8) == 0 || _strnicmp(pszUrlA, "magnet:?", 8) == 0) {
				const CString strData(pszUrlA);
				for (int iPos = 0; iPos >= 0;) {
					const CString &sLink(strData.Tokenize(_T("\r\n"), iPos));
					if (!sLink.IsEmpty()) {
						theApp.emuledlg->ProcessED2KLink(sLink);
						hrPasteResult = S_OK;
					}
				}
			}

			GlobalUnlock(hMem);
		}
		GlobalFree(hMem);
	}
	return hrPasteResult;
}

HRESULT CMainFrameDropTarget::AddUrlFileContents(LPCTSTR pszFileName)
{
	HRESULT hrResult = S_FALSE;

	if (ExtensionIs(pszFileName, FILEEXTDOT_INETSHRTCUT)) {
		CComPtr<IUniformResourceLocatorW> pIUrl;
		hrResult = CoCreateInstance(CLSID_InternetShortcut, NULL, CLSCTX_INPROC_SERVER, IID_IUniformResourceLocatorW, (void**)&pIUrl);
		if (SUCCEEDED(hrResult)) {
			CComPtr<IPersistFile> pIFile;
			hrResult = pIUrl.QueryInterface(&pIFile);
			if (SUCCEEDED(hrResult)) {
				hrResult = pIFile->Load(CComBSTR(pszFileName), STGM_READ | STGM_SHARE_DENY_WRITE);
				if (SUCCEEDED(hrResult)) {
					LPWSTR pwszUrl;
					hrResult = pIUrl->GetURL(&pwszUrl);
					if (hrResult == S_OK) {
						if (pwszUrl != NULL && pwszUrl[0] != L'\0' && IsUrlSchemeSupportedW(pwszUrl))
							theApp.emuledlg->ProcessED2KLink(pwszUrl);
						else
							hrResult = S_FALSE;
						CoTaskMemFree(pwszUrl);
					}
				}
			}
		}
	}

	return hrResult;
}

HRESULT CMainFrameDropTarget::PasteHDROP(COleDataObject &data)
{
	HRESULT hrPasteResult = E_FAIL;
	HANDLE hMem = data.GetGlobalData(CF_HDROP);
	if (hMem != NULL) {
		LPDROPFILES lpDrop = (LPDROPFILES)GlobalLock(hMem);
		if (lpDrop != NULL) {
			if (lpDrop->fWide) {
				LPCWSTR pszFileNameW = (LPCWSTR)((LPBYTE)lpDrop + lpDrop->pFiles);
				while (*pszFileNameW != L'\0') {
					if (FAILED(AddUrlFileContents(pszFileNameW)))
						break;
					hrPasteResult = S_OK;
					pszFileNameW += wcslen(pszFileNameW) + 1;
				}
			} else {
				LPCSTR pszFileNameA = (LPCSTR)((LPBYTE)lpDrop + lpDrop->pFiles);
				while (*pszFileNameA != '\0') {
					if (FAILED(AddUrlFileContents(CString(pszFileNameA))))
						break;
					hrPasteResult = S_OK;
					pszFileNameA += strlen(pszFileNameA) + 1;
				}
			}
			GlobalUnlock(hMem);
		}
		GlobalFree(hMem);
	}
	return hrPasteResult;
}

BOOL CMainFrameDropTarget::IsSupportedDropData(COleDataObject *pDataObject)
{
	BOOL bResult;

	//************************************************************************
	//*** THIS FUNCTION HAS TO BE AS FAST AS POSSIBLE!!!
	//************************************************************************

	if (m_cfHTML && pDataObject->IsDataAvailable(m_cfHTML)) {
		// If the data is in 'HTML Format', there is no need to check the contents.
		bResult = TRUE;
	} else if (m_cfShellURL && pDataObject->IsDataAvailable(m_cfShellURL)) {
		// If the data is in 'UniformResourceLocator', there is no need to check the contents.
		bResult = TRUE;
	} else if (pDataObject->IsDataAvailable(CF_UNICODETEXT)) {
		//
		// Check text data
		//
		bResult = FALSE;
		HANDLE hMem = pDataObject->GetGlobalData(CF_UNICODETEXT);
		if (hMem != NULL) {
			LPCWSTR lpszUrl = (LPCWSTR)GlobalLock(hMem);
			if (lpszUrl != NULL) {
				// skip white space
				while (isspace(*lpszUrl))
					++lpszUrl;
				bResult = IsUrlSchemeSupportedW(lpszUrl);
				GlobalUnlock(hMem);
			}
			GlobalFree(hMem);
		}
	} else if (pDataObject->IsDataAvailable(CF_HDROP)) {
		//
		// Check HDROP data
		//
		bResult = FALSE;

		HANDLE hMem = pDataObject->GetGlobalData(CF_HDROP);
		if (hMem != NULL) {
			LPDROPFILES lpDrop = (LPDROPFILES)GlobalLock(hMem);
			if (lpDrop != NULL) {
				// Just check, if there's at least one file we can import
				if (lpDrop->fWide) {
					LPCWSTR pszFileW = (LPCWSTR)((LPBYTE)lpDrop + lpDrop->pFiles);
					while (*pszFileW != L'\0') {
						int iLen = (int)wcslen(pszFileW);
						LPCWSTR pszExtW = GetFileExtW(pszFileW, iLen);
						if (pszExtW != NULL && _wcsicmp(pszExtW, FILEEXTDOT_INETSHRTCUTW) == 0) {
							bResult = TRUE;
							break;
						}
						pszFileW += iLen + 1;
					}
				} else {
					LPCSTR pszFileA = (LPCSTR)((LPBYTE)lpDrop + lpDrop->pFiles);
					while (*pszFileA != '\0') {
						int iLen = (int)strlen(pszFileA);
						LPCSTR pszExtA = GetFileExtA(pszFileA, iLen);
						if (pszExtA != NULL && _stricmp(pszExtA, FILEEXTDOT_INETSHRTCUTA) == 0) {
							bResult = TRUE;
							break;
						}
						pszFileA += iLen + 1;
					}
				}
				GlobalUnlock(hMem);
			}
			GlobalFree(hMem);
		}
	} else {
		// Unknown data format
		bResult = FALSE;
	}

	return bResult;
}

DROPEFFECT CMainFrameDropTarget::OnDragEnter(CWnd*, COleDataObject *pDataObject, DWORD, CPoint)
{
	m_bDropDataValid = IsSupportedDropData(pDataObject);
	return m_bDropDataValid ? DROPEFFECT_COPY : DROPEFFECT_NONE;
}

DROPEFFECT CMainFrameDropTarget::OnDragOver(CWnd*, COleDataObject*, DWORD, CPoint)
{
	return m_bDropDataValid ? DROPEFFECT_COPY : DROPEFFECT_NONE;
}

BOOL CMainFrameDropTarget::OnDrop(CWnd*, COleDataObject *pDataObject, DROPEFFECT /*dropEffect*/, CPoint /*point*/)
{
	if (m_bDropDataValid) {
		if (m_cfHTML && pDataObject->IsDataAvailable(m_cfHTML))
			PasteHTML(*pDataObject);
		else if (m_cfShellURL && pDataObject->IsDataAvailable(m_cfShellURL))
			PasteText(m_cfShellURL, *pDataObject);
		else if (pDataObject->IsDataAvailable(CF_TEXT))
			PasteText(CF_TEXT, *pDataObject);
		else if (pDataObject->IsDataAvailable(CF_HDROP))
			return PasteHDROP(*pDataObject) == S_OK;
		return TRUE;
	}
	return FALSE;
}

void CMainFrameDropTarget::OnDragLeave(CWnd*)
{
	// Do *NOT* set m_bDropDataValid to FALSE!
	// 'OnDragLeave' may be called from MFC when scrolling!
	// In that case it's not really a "leave".
	//m_bDropDataValid = FALSE;
}