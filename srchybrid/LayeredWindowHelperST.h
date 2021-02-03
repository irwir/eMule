//
//	Class:		CLayeredWindowHelperST
//
//	Compiler:	Visual C++
//	Tested on:	Visual C++ 6.0
//
//	Version:	See GetVersionC() or GetVersionI()
//
//	Created:	17/January/2002
//	Updated:	15/February/2002
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

class CLayeredWindowHelperST
{
public:
	CLayeredWindowHelperST();
	virtual	~CLayeredWindowHelperST() = default;

	static LONG_PTR AddLayeredStyle(HWND hWnd);
	static LONG_PTR RemoveLayeredStyle(HWND hWnd);
	BOOL SetLayeredWindowAttributes(HWND hWnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);
	BOOL SetTransparentPercentage(HWND hWnd, UINT byPercentage);

	static short GetVersionI()		{ return 11; }
	static LPCTSTR GetVersionC()	{ return (LPCTSTR)_T("1.1"); }

private:
	typedef BOOL(WINAPI *lpfnSetLayeredWindowAttributes)(HWND hWnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);

	HMODULE		m_hDll;
};