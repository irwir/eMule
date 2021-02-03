/********************************************************************************************
* MOD-NAME      : QArray.h
* LONG-NAME     : QuickSort algorithm enabled CArray
*
* AUTHOR        : Martin Ziacek, Martin.Ziacek@swh.sk, http://www.swh.sk
* COPYRIGHT     : 1999 Martin Ziacek
* DEPARTMENT    : SWH s.r.o
* TELEPHONE     : +421 7 59684147
* CREATION-DATE : 1.5.1999 8:27:23
* SP-NO         :
* FUNCTION      : Implementation of QuickSort algorithm as template for array class
*                 and template for simple function QuickSort()
*
*********************************************************************************************/

#pragma once

//////////////////////////////////////////////////////////////////////////
// QuickSort functions
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// QuickSortRecursive - core of algorithm, do not call it, use QuickSort,
// see below
template <class T> void QuickSortRecursive(T *pArr, int d, int h, BOOL bAscending)
{
	int i, j;
	T str;

	i = h;
	j = d;

	str = pArr[((int)((d + h) / 2))];

	do {

		if (bAscending) {
			while (pArr[j] < str)
				++j;
			while (pArr[i] > str)
				--i;
		} else {
			while (pArr[j] > str)
				++j;
			while (pArr[i] < str)
				--i;
		}

		if (i >= j) {
			if (i != j) {
				T zal;

				zal = pArr[i];
				pArr[i] = pArr[j];
				pArr[j] = zal;
			}

			--i;
			++j;
		}
	} while (j <= i);

	if (d < i)
		QuickSortRecursive(pArr, d, i, bAscending);
	if (j < h)
		QuickSortRecursive(pArr, j, h, bAscending);
}

//////////////////////////////////////////////////////////////////////////
// QuickSort - entry to algorithm
//
// T *pArr			... pointer of array to sort
// int iSize		... size of array T *pArr
// BOOL bAscending	... if bAscending == TRUE, then sort ascending,
//						otherwise descending
//
// return TRUE if no error, error can be bad parameter, call ::GetLastError()
// if QuickSort returned FALSE
template <class T> BOOL QuickSort(T *pArr, int iSize, BOOL bAscending = TRUE)
{
	if (iSize > 1) {
		try {
			int	low = 0,
				high = iSize - 1;
			QuickSortRecursive(pArr, low, high, bAscending);
			return TRUE;
		} catch (...) {
		}
	}
	::SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
// CQArray
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CQArray declaration

template <class T, class PT> class CQArray : public CArray <T, PT>
{
public:
	void QuickSort(BOOL bAscending = TRUE);
};

//////////////////////////////////////////////////////////////////////////
// CQArray implementation

//////////////////////////////////////////////////////////////////////////
// QuickSort - entry to algorithm
//
// BOOL bAscending	... if bAscending == TRUE, then sort ascending,
//						otherwise descending
//
template <class T, class TP> void CQArray<T, TP>::QuickSort(BOOL bAscending/* = TRUE*/)
{
	if (GetSize() > 1)
		::QuickSort(GetData(), (int)GetSize(), bAscending);
}