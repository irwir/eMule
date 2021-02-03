#pragma once

class CListCtrlItemWalk
{
public:
	explicit CListCtrlItemWalk(CListCtrl *pListCtrl)	{ m_pListCtrl = pListCtrl; }

	virtual CObject* GetNextSelectableItem();
	virtual CObject* GetPrevSelectableItem();

	CListCtrl* GetListCtrl() const						{ return m_pListCtrl; }

protected:
	virtual	~CListCtrlItemWalk() = default; //just in case...
	CListCtrl *m_pListCtrl;
};