#include "stdafx.h"
#include "Quantize.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
CQuantizer::CQuantizer(UINT nMaxColors, UINT nColorBits)
	: m_pTree()
	, m_pReducibleNodes()
	, m_nLeafCount()
	, m_nMaxColors(nMaxColors)
	, m_nColorBits(nColorBits < 8 ? nColorBits : 8)
{
}
/////////////////////////////////////////////////////////////////////////////
CQuantizer::~CQuantizer()
{
	if (m_pTree != NULL)
		DeleteTree(&m_pTree);
}
/////////////////////////////////////////////////////////////////////////////
BOOL CQuantizer::ProcessImage(HANDLE hImage)
{
	BITMAPINFOHEADER ds;
	memcpy(&ds, hImage, sizeof ds);
	int effwdt = ((((ds.biBitCount * ds.biWidth) + 31) / 32) * 4);

	int	nPad = effwdt - (((ds.biWidth *	ds.biBitCount) + 7) / 8);

	BYTE *pbBits = (BYTE*)hImage + *(LPDWORD)hImage;

	switch (ds.biBitCount) {

	case 1:	// 1-bit DIB
	case 4:	// 4-bit DIB
	case 8:	// 8-bit DIB
		for (int i = 0; i < ds.biHeight; ++i) {
			for (int j = 0; j < ds.biWidth; ++j) {
				BYTE idx = GetPixelIndex(j, i, ds.biBitCount, effwdt, pbBits);
				BYTE *pal = (BYTE*)(hImage)+sizeof(BITMAPINFOHEADER);
				long ldx = idx * sizeof(RGBQUAD);
				BYTE b = pal[ldx++];
				BYTE g = pal[ldx++];
				BYTE r = pal[ldx];
				AddColor(&m_pTree, r, g, b, m_nColorBits, 0, &m_nLeafCount,
					m_pReducibleNodes);
				while (m_nLeafCount > m_nMaxColors)
					ReduceTree(m_nColorBits, &m_nLeafCount,
						m_pReducibleNodes);
			}
		}
		return TRUE;
	case 24: //	24-bit DIB
		for (int i = 0; i < ds.biHeight; ++i) {
			for (int j = 0; j < ds.biWidth; ++j) {
				BYTE b = *pbBits++;
				BYTE g = *pbBits++;
				BYTE r = *pbBits++;
				AddColor(&m_pTree, r, g, b, m_nColorBits, 0, &m_nLeafCount,
					m_pReducibleNodes);
				while (m_nLeafCount > m_nMaxColors)
					ReduceTree(m_nColorBits, &m_nLeafCount, m_pReducibleNodes);
			}
			pbBits += nPad;
		}
		return TRUE;
	}

	return FALSE; //Unrecognized color format
}
/////////////////////////////////////////////////////////////////////////////
void CQuantizer::AddColor(NODE **ppNode, BYTE r, BYTE g, BYTE b,
	UINT nColorBits, UINT nLevel, UINT *pLeafCount, NODE **pReducibleNodes)
{
	static BYTE	mask[8] = {0x80, 0x40,	0x20, 0x10,	0x08, 0x04,	0x02, 0x01};

	// If the node doesn't exist, create it.
	if (*ppNode == NULL)
		*ppNode = (NODE*)CreateNode(nLevel, nColorBits, pLeafCount, pReducibleNodes);

	// Update color	information	if it's	a leaf node.
	if ((*ppNode)->bIsLeaf) {
		(*ppNode)->nPixelCount++;
		(*ppNode)->nRedSum += r;
		(*ppNode)->nGreenSum += g;
		(*ppNode)->nBlueSum += b;
	} else {	// Recurse a level deeper if the node is not a leaf.
		int	shift = 7 - nLevel;
		int	nIndex = (((r & mask[nLevel]) >> shift) << 2) |
			(((g & mask[nLevel]) >> shift) << 1) |
			((b & mask[nLevel]) >> shift);
		AddColor(&((*ppNode)->pChild[nIndex]), r, g, b, nColorBits,
			nLevel + 1, pLeafCount, pReducibleNodes);
	}
}
/////////////////////////////////////////////////////////////////////////////
void* CQuantizer::CreateNode(UINT nLevel, UINT	nColorBits, UINT *pLeafCount,
	NODE **pReducibleNodes)
{
	NODE *pNode = (NODE*)malloc(sizeof(NODE));

	if (pNode == NULL)
		return NULL;

	pNode->bIsLeaf = static_cast<BOOL>(nLevel == nColorBits);
	if (pNode->bIsLeaf)
		++(*pLeafCount);
	else {
		pNode->pNext = pReducibleNodes[nLevel];
		pReducibleNodes[nLevel] = pNode;
	}
	return pNode;
}
/////////////////////////////////////////////////////////////////////////////
void CQuantizer::ReduceTree(UINT nColorBits, UINT *pLeafCount,
	NODE **pReducibleNodes)
{
	// Find	the	deepest	level containing at	least one reducible	node.
	int i;
	for (i = nColorBits - 1; (i > 0) && (pReducibleNodes[i] == NULL); i--);

	// Reduce the node most	recently added to the list at level	i.
	NODE *pNode = pReducibleNodes[i];
	pReducibleNodes[i] = pNode->pNext;

	UINT nRedSum = 0;
	UINT nGreenSum = 0;
	UINT nBlueSum = 0;
	UINT nChildren = 0;

	for (i = 0; i < 8; ++i) {
		if (pNode->pChild[i] != NULL) {
			nRedSum += pNode->pChild[i]->nRedSum;
			nGreenSum += pNode->pChild[i]->nGreenSum;
			nBlueSum += pNode->pChild[i]->nBlueSum;
			pNode->nPixelCount += pNode->pChild[i]->nPixelCount;
			free(pNode->pChild[i]);
			pNode->pChild[i] = NULL;
			++nChildren;
		}
	}

	pNode->bIsLeaf = TRUE;
	pNode->nRedSum = nRedSum;
	pNode->nGreenSum = nGreenSum;
	pNode->nBlueSum = nBlueSum;
	*pLeafCount -= (nChildren - 1);
}
/////////////////////////////////////////////////////////////////////////////
void CQuantizer::DeleteTree(NODE**	ppNode)
{
	for (int i = 0; i < 8; ++i)
		if ((*ppNode)->pChild[i] != NULL)
			DeleteTree(&((*ppNode)->pChild[i]));

	free(*ppNode);
	*ppNode = NULL;
}
/////////////////////////////////////////////////////////////////////////////
void CQuantizer::GetPaletteColors(NODE *pTree, RGBQUAD *prgb, UINT *pIndex)
{
	if (pTree)
		if (pTree->bIsLeaf) {
			prgb[*pIndex].rgbRed = (BYTE)((pTree->nRedSum) / (pTree->nPixelCount));
			prgb[*pIndex].rgbGreen = (BYTE)((pTree->nGreenSum) / (pTree->nPixelCount));
			prgb[*pIndex].rgbBlue = (BYTE)((pTree->nBlueSum) / (pTree->nPixelCount));
			prgb[*pIndex].rgbReserved = 0;
			++(*pIndex);
		} else
			for (int i = 0; i < 8; ++i)
				if (pTree->pChild[i] != NULL)
					GetPaletteColors(pTree->pChild[i], prgb, pIndex);
}
/////////////////////////////////////////////////////////////////////////////
UINT CQuantizer::GetColorCount() const
{
	return m_nLeafCount;
}
/////////////////////////////////////////////////////////////////////////////
void CQuantizer::SetColorTable(RGBQUAD *prgb)
{
	UINT nIndex = 0;
	GetPaletteColors(m_pTree, prgb, &nIndex);
}
/////////////////////////////////////////////////////////////////////////////
BYTE CQuantizer::GetPixelIndex(long x, long y, int nbit, long effwdt, BYTE *pimage)
{
	if (nbit == 8)
		return pimage[y*effwdt + x];

	BYTE iDst = pimage[y*effwdt + (x*nbit >> 3)];
	if (nbit == 4) {
		BYTE pos = (BYTE)(4 * (1 - x % 2));
		iDst &= (0x0F << pos);
		return iDst >> pos;
	}
	if (nbit == 1) {
		BYTE pos = (BYTE)(7 - x % 8);
		iDst &= (0x01 << pos);
		return iDst >> pos;
	}
	return 0;
}