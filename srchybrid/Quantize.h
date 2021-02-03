/* === C R E D I T S  &  D I S C L A I M E R S ==============
 * Permission is given by the author to freely redistribute and include
 * this code in any program as long as this credit is given where due.
 *
 * CQuantizer (c)  1996-1997 Jeff Prosise
 *
 * COVERED CODE IS PROVIDED UNDER THIS LICENSE ON AN "AS IS" BASIS, WITHOUT WARRANTY
 * OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, WITHOUT LIMITATION, WARRANTIES
 * THAT THE COVERED CODE IS FREE OF DEFECTS, MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE
 * OR NON-INFRINGING. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE COVERED
 * CODE IS WITH YOU. SHOULD ANY COVERED CODE PROVE DEFECTIVE IN ANY RESPECT, YOU (NOT
 * THE INITIAL DEVELOPER OR ANY OTHER CONTRIBUTOR) ASSUME THE COST OF ANY NECESSARY
 * SERVICING, REPAIR OR CORRECTION. THIS DISCLAIMER OF WARRANTY CONSTITUTES AN ESSENTIAL
 * PART OF THIS LICENSE. NO USE OF ANY COVERED CODE IS AUTHORIZED HEREUNDER EXCEPT UNDER
 * THIS DISCLAIMER.
 *
 * Use at your own risk!
 * ==========================================================
 */
#pragma once

class CQuantizer
{
	typedef struct _NODE
	{
		BOOL bIsLeaf;               // TRUE if node has no children
		UINT nPixelCount;           // Number of pixels represented by this leaf
		UINT nRedSum;               // Sum of red components
		UINT nGreenSum;             // Sum of green components
		UINT nBlueSum;              // Sum of blue components
		struct _NODE *pChild[8];    // Pointers to child nodes
		struct _NODE *pNext;        // Pointer to next reducible node
	} NODE;
protected:
	NODE *m_pTree;
	NODE *m_pReducibleNodes[9];
	UINT m_nLeafCount;
	UINT m_nMaxColors;
	UINT m_nColorBits;

public:
	CQuantizer(UINT nMaxColors, UINT nColorBits);
	virtual	~CQuantizer();
	BOOL ProcessImage(HANDLE hImage);
	UINT GetColorCount() const;
	void SetColorTable(RGBQUAD *prgb);

protected:
	void AddColor(NODE **ppNode, BYTE r, BYTE g, BYTE b, UINT nColorBits
		, UINT nLevel, UINT *pLeafCount, NODE **pReducibleNodes);
	void* CreateNode(UINT nLevel, UINT nColorBits, UINT *pLeafCount
		, NODE **pReducibleNodes);
	void ReduceTree(UINT nColorBits, UINT *pLeafCount
		, NODE **pReducibleNodes);
	void DeleteTree(NODE **ppNode);
	static void GetPaletteColors(NODE *pTree, RGBQUAD *prgb, UINT *pIndex);
	static BYTE GetPixelIndex(long x, long y, int nbit, long effwdt, BYTE *pimage);
};