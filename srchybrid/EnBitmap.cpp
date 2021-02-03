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
#include "EnBitmap.h"
#include <atlimage.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


const int HIMETRIC_INCH = 2540;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

BOOL CEnBitmap::LoadImage(UINT uIDRes, LPCTSTR pszResourceType, HMODULE hInst, COLORREF crBack)
{
	return LoadImage(MAKEINTRESOURCE(uIDRes), pszResourceType, hInst, crBack);
}

BOOL CEnBitmap::LoadImage(LPCTSTR lpszResourceName, LPCTSTR szResourceType, HMODULE hInst, COLORREF crBack)
{
	ASSERT(m_hObject == NULL);      // only attach once, detach on destroy

	if (m_hObject != NULL)
		return FALSE;

	int nSize = 0;
	BOOL bResult = FALSE;

	// first call is to get buffer size
	if (GetResource(lpszResourceName, szResourceType, hInst, 0, nSize)) {
		if (nSize > 0) {
			BYTE *pBuff = new BYTE[nSize];

			// this loads it
			if (GetResource(lpszResourceName, szResourceType, hInst, pBuff, nSize)) {
				IPicture *pPicture = LoadFromBuffer(pBuff, nSize);

				if (pPicture) {
					bResult = Attach(pPicture, crBack);
					pPicture->Release();
				}
			}

			delete[] pBuff;
		}
	}

	return bResult;
}

BOOL CEnBitmap::LoadImage(LPCTSTR szImagePath, COLORREF crBack)
{
	ASSERT(m_hObject == NULL);      // only attach once, detach on destroy

	if (m_hObject != NULL)
		return FALSE;

	// If GDI+ is available, use that API because it supports more file formats and images with alpha channels.
	// That DLL is installed with WinXP is available as redistributable from Microsoft for Win98+. As this DLL
	// may not be available on some OS but we have to link statically to it, we have to take some special care.
	//
	extern bool g_bGdiPlusInstalled;
	if (g_bGdiPlusInstalled) {
		CImage img;
		if (SUCCEEDED(img.Load(szImagePath))) {
			CBitmap::Attach(img.Detach());
			return TRUE;
		}
	}

	BOOL bResult = FALSE;
	CFile			cFile;
	CFileException	e;

	if (cFile.Open(szImagePath, CFile::modeRead | CFile::typeBinary | CFile::shareDenyWrite, &e)) {
		int nSize = (int)cFile.GetLength();
		BYTE *pBuff = new BYTE[nSize];
		if (cFile.Read(pBuff, nSize) > 0) {
			IPicture *pPicture = LoadFromBuffer(pBuff, nSize);

			if (pPicture) {
				bResult = Attach(pPicture, crBack);
				pPicture->Release();
			}
		}
		delete[] pBuff;
	}

	return bResult;
}

IPicture* CEnBitmap::LoadFromBuffer(BYTE *pBuff, int nSize)
{
	IPicture *pPicture = NULL;

	HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, nSize);
	if (hGlobal != NULL) {
		void *pData = GlobalLock(hGlobal);
		if (pData != NULL) {
			memcpy(pData, pBuff, nSize);
			GlobalUnlock(hGlobal);

			IStream *pStream = NULL;
			if (CreateStreamOnHGlobal(hGlobal, TRUE/*fDeleteOnRelease*/, &pStream) == S_OK) {
				// Not sure what the 'KeepOriginalFormat' property is really used for. But if 'OleLoadPicture'
				// is invoked with 'fRunmode=FALSE' the function always creates a temporary file which even
				// does not get deleted when all COM pointers were released. It eventually gets deleted only
				// when process terminated. Using 'fRunmode=TRUE' does prevent this behaviour and does not
				// seem to have any other side effects.
				VERIFY(OleLoadPicture(pStream, nSize, TRUE/*FALSE*/, IID_IPicture, (LPVOID*)&pPicture) == S_OK);
				pStream->Release();
			} else
				GlobalFree(hGlobal);
		} else
			GlobalFree(hGlobal);
	}

	return pPicture; // caller releases
}

BOOL CEnBitmap::GetResource(LPCTSTR lpName, LPCTSTR lpType, HMODULE hInst, void *pResource, int &nBufSize)
{
	HRSRC		hResInfo;
	HANDLE		hRes;
	LPSTR		lpRes = NULL;
	bool		bResult = FALSE;

	// Find the resource
	hResInfo = FindResource(hInst, lpName, lpType);

	if (hResInfo == NULL)
		return false;

	// Load the resource
	hRes = LoadResource(hInst, hResInfo);

	if (hRes == NULL)
		return false;

	// Lock the resource
	lpRes = (char*)LockResource(hRes);

	if (lpRes != NULL) {
		if (pResource == NULL) {
			nBufSize = SizeofResource(hInst, hResInfo);
			bResult = true;
		} else {
			if (nBufSize >= (int)SizeofResource(hInst, hResInfo)) {
				memcpy(pResource, lpRes, nBufSize);
				bResult = true;
			}
		}

		UnlockResource(hRes);
	}

	// Free the resource
	FreeResource(hRes);

	return bResult;
}

BOOL CEnBitmap::Attach(IPicture *pPicture, COLORREF crBack)
{
	ASSERT(m_hObject == NULL);      // only attach once, detach on destroy

	if (m_hObject != NULL)
		return FALSE;

	ASSERT(pPicture);

	if (!pPicture)
		return FALSE;

	BOOL bResult = FALSE;

	CDC *pDC = CWnd::GetDesktopWindow()->GetDC();
	CDC dcMem;
	if (dcMem.CreateCompatibleDC(pDC)) {
		long hmWidth;
		long hmHeight;
		pPicture->get_Width(&hmWidth);
		pPicture->get_Height(&hmHeight);

		int nWidth = ::MulDiv(hmWidth, pDC->GetDeviceCaps(LOGPIXELSX), HIMETRIC_INCH);
		int nHeight = ::MulDiv(hmHeight, pDC->GetDeviceCaps(LOGPIXELSY), HIMETRIC_INCH);

		CBitmap bmMem;
		if (bmMem.CreateCompatibleBitmap(pDC, nWidth, nHeight)) {
			CBitmap *pOldBM = dcMem.SelectObject(&bmMem);

			if (crBack != CLR_NONE)
				dcMem.FillSolidRect(0, 0, nWidth, nHeight, crBack);

			HRESULT hr = pPicture->Render(dcMem, 0, 0, nWidth, nHeight, 0, hmHeight, hmWidth, -hmHeight, NULL);
			dcMem.SelectObject(pOldBM);

			bResult = (hr == S_OK) && CBitmap::Attach(bmMem.Detach());
		}
	}

	CWnd::GetDesktopWindow()->ReleaseDC(pDC);

	return bResult;
}