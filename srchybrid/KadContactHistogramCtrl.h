#pragma once

#define	KAD_CONTACT_HIST_NEEDED_BITS	12	// 8=256, 9=512, 10=1024, 11=2048, 12=4096, 13=8192, 14=16384, 15=32768, 16=65536
#define	KAD_CONTACT_HIST_SIZE			(1 << KAD_CONTACT_HIST_NEEDED_BITS)

class CKadContactHistogramCtrl : public CWnd
{
public:
	CKadContactHistogramCtrl();
	virtual	~CKadContactHistogramCtrl() = default;

	void Localize();

	bool ContactAdd(const Kademlia::CContact *contact);
	void ContactRem(const Kademlia::CContact *contact);

protected:
	CString m_strXaxis;
	CString m_strYaxis;
	CPen m_penAxis;
	CPen m_penAux;
	CPen m_penHist;
	UINT m_aHist[KAD_CONTACT_HIST_SIZE];
	int m_iMaxLabelHeight;
	int m_iMaxNumLabelWidth;
	bool m_bInitializedFontMetrics;

	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};