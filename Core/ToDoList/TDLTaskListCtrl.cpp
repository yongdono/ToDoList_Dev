// TDCTreeListCtrl.cpp: implementation of the CTDCListListCtrl class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#include "TDlTaskListCtrl.h"
#include "todoctrldata.h"
#include "tdcstatic.h"
#include "tdcmsg.h"
#include "tdcimagelist.h"

#include "..\shared\graphicsmisc.h"
#include "..\shared\autoflag.h"
#include "..\shared\holdredraw.h"
#include "..\shared\timehelper.h"
#include "..\shared\misc.h"
#include "..\shared\themed.h"
#include "..\shared\osversion.h"

#include "..\Interfaces\Preferences.h"

#include "..\3rdparty\colordef.h"

#include <math.h>

/////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////

const int LV_COLPADDING			= 3;
const int TITLE_BORDER_OFFSET	= 2;

const int CLIENTCOLWIDTH		= GraphicsMisc::ScaleByDPIFactor(1000);
const UINT TIMER_EDITLABEL		= 42; // List ctrl's internal timer ID for label edits

const COLORREF COMMENTSCOLOR	= RGB(98, 98, 98);
const COLORREF ALTCOMMENTSCOLOR = RGB(164, 164, 164);

//////////////////////////////////////////////////////////////////////

#ifndef CDRF_SKIPPOSTPAINT
#	define CDRF_SKIPPOSTPAINT 0x00000100
#endif

#ifndef LVS_EX_LABELTIP
#	define LVS_EX_LABELTIP 0x00004000
#endif

//////////////////////////////////////////////////////////////////////

enum LCH_CHECK 
{ 
	LCHC_NONE		= INDEXTOSTATEIMAGEMASK(1), 
	LCHC_UNCHECKED	= INDEXTOSTATEIMAGEMASK(2), 
	LCHC_CHECKED	= INDEXTOSTATEIMAGEMASK(3), 
	LCHC_MIXED		= INDEXTOSTATEIMAGEMASK(4), 
};

//////////////////////////////////////////////////////////////////////

enum
{
	IDC_TITLES = 101,		
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CTDLTaskListCtrl, CTDLTaskCtrlBase)

//////////////////////////////////////////////////////////////////////

CTDLTaskListCtrl::CTDLTaskListCtrl(const CTDCImageList& ilIcons,
								   const CToDoCtrlData& data, 
								   const CToDoCtrlFind& find,
								   const CTDCStyleMap& styles,
								   const TDCAUTOLISTDATA& tld,
								   const CTDCColumnIDMap& mapVisibleCols,
								   const CTDCCustomAttribDefinitionArray& aCustAttribDefs) 
	: 
	CTDLTaskCtrlBase(TRUE, ilIcons, data, find, styles, tld, mapVisibleCols, aCustAttribDefs)
{
}

CTDLTaskListCtrl::~CTDLTaskListCtrl()
{
}

///////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CTDLTaskListCtrl, CTDLTaskCtrlBase)
//{{AFX_MSG_MAP(CTDCListListCtrl)
//}}AFX_MSG_MAP
ON_WM_SETCURSOR()
END_MESSAGE_MAP()

///////////////////////////////////////////////////////////////////////////

DWORD CTDLTaskListCtrl::GetHelpID() const
{
	static LPCTSTR IDC_LISTVIEW_CTRL = _T("IDC_LISTVIEW_CTRL");
	return (DWORD)IDC_LISTVIEW_CTRL;
}

BOOL CTDLTaskListCtrl::CreateTasksWnd(CWnd* pParentWnd, const CRect& rect, BOOL bVisible)
{
	DWORD dwStyle = (WS_CHILD | (bVisible ? WS_VISIBLE : 0));
	
	if (m_lcTasks.Create((dwStyle | 
							WS_TABSTOP | 
							LVS_REPORT | 
							LVS_NOCOLUMNHEADER | 
							LVS_EDITLABELS | 
							WS_CLIPSIBLINGS),
							rect, 
							pParentWnd, 
							IDC_TITLES))
	{
		ListView_SetExtendedListViewStyleEx(m_lcTasks, (LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP), (LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP));
		ListView_SetCallbackMask(m_lcTasks, LVIS_STATEIMAGEMASK);

		return TRUE;	
	}

	return FALSE;
}

void CTDLTaskListCtrl::SetTasksImageList(HIMAGELIST hil, BOOL bState, BOOL bOn) 
{
	ListView_SetImageList(m_lcTasks, (bOn ? hil : NULL), (bState ? LVSIL_STATE : LVSIL_SMALL));

	// Hack to resize the listview if hiding the task imagelist 
	if (!bState && !bOn)
	{
		m_lcTasks.ModifyStyle(LVS_REPORT, LVS_SMALLICON);
		m_lcTasks.ModifyStyle(LVS_SMALLICON, LVS_REPORT);
	}
}

BOOL CTDLTaskListCtrl::IsListItemSelected(HWND hwnd, int nItem) const
{
	return (ListView_GetItemState(hwnd, nItem, LVIS_SELECTED) & LVIS_SELECTED);
}

void CTDLTaskListCtrl::OnStylesUpdated(const CTDCStyleMap& styles, BOOL bAllowResort)
{
	CTDLTaskCtrlBase::OnStylesUpdated(styles, bAllowResort);

	// Our extra handling
	if (styles.HasStyle(TDCS_SHOWINFOTIPS))
	{
		DWORD dwLabelTips = (styles.IsStyleEnabled(TDCS_SHOWINFOTIPS) ? 0 : LVS_EX_LABELTIP);
		ListView_SetExtendedListViewStyleEx(Tasks(), LVS_EX_LABELTIP, dwLabelTips);
	}
}

BOOL CTDLTaskListCtrl::BuildColumns()
{
	if (!CTDLTaskCtrlBase::BuildColumns())
		return FALSE;
	
	// add client column
	const TDCCOLUMN* pClient = GetColumn(TDCC_CLIENT);
	ASSERT(pClient);
	
	m_lcTasks.InsertColumn(0, CEnString(pClient->nIDName), pClient->nTextAlignment, CLIENTCOLWIDTH);

	return TRUE;
}

DWORD CTDLTaskListCtrl::HitTestTasksTask(const CPoint& ptScreen) const
{
	// see if we hit a task in the tree
	CPoint ptClient(ptScreen);
	m_lcTasks.ScreenToClient(&ptClient);
	
	int nItem = m_lcTasks.HitTest(ptClient);

	if (nItem != -1)
		return GetTaskID(nItem);

	// all else
	return 0;
}

void CTDLTaskListCtrl::Release() 
{ 
	if (::IsWindow(m_hdrTasks))
		m_hdrTasks.UnsubclassWindow();

	CTDLTaskCtrlBase::Release();
}

void CTDLTaskListCtrl::DeleteAll()
{
	CTLSHoldResync hr(*this);
	
	m_lcTasks.DeleteAllItems();
	m_lcColumns.DeleteAllItems();
}

void CTDLTaskListCtrl::RemoveDeletedItems()
{
	int nItem = m_lcTasks.GetItemCount();

	if (nItem == 0)
		return;
	
	CTLSHoldResync hr(*this);

	while (nItem--)
	{
		if (!HasTask(nItem))
		{
			m_lcTasks.DeleteItem(nItem);
			m_lcColumns.DeleteItem(nItem);
		}
	}

	// fix up item data of columns
	nItem = m_lcColumns.GetItemCount();
	
	while (nItem--)
		m_lcColumns.SetItemData(nItem, nItem);
}

LRESULT CTDLTaskListCtrl::OnListCustomDraw(NMLVCUSTOMDRAW* pLVCD)
{
	HWND hwndList = pLVCD->nmcd.hdr.hwndFrom;
	int nItem = (int)pLVCD->nmcd.dwItemSpec;
	DWORD dwTaskID = pLVCD->nmcd.lItemlParam;

	if (hwndList == m_lcColumns)
	{
		// columns handled by base class
		return CTDLTaskCtrlBase::OnListCustomDraw(pLVCD);
	}

	DWORD dwRes = CDRF_DODEFAULT;

	switch (pLVCD->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		dwRes = CDRF_NOTIFYITEMDRAW;
		break;
								
	case CDDS_ITEMPREPAINT:
		{
			BOOL bFillRow = !OsIsXP();
			dwRes = OnPrePaintTaskTitle(pLVCD->nmcd, bFillRow, pLVCD->clrText, pLVCD->clrTextBk);

			if (bFillRow)
 				ListView_SetBkColor(m_lcTasks, pLVCD->clrTextBk);

			return dwRes;
		}
		break;

	case CDDS_ITEMPOSTPAINT:
		{
			dwRes = OnPostPaintTaskTitle(pLVCD->nmcd);
			
			// restore default back colour set in CDDS_ITEMPREPAINT
			ListView_SetBkColor(m_lcTasks, GetSysColor(COLOR_WINDOW));
		}
		break;
	}
	
	return dwRes;
}

void CTDLTaskListCtrl::OnNotifySplitterChange(int nSplitPos)
{
	CTDLTaskCtrlBase::OnNotifySplitterChange(nSplitPos);

	// if split width exceeds client column width
	// extend column width to suit
	if (IsRight(m_lcTasks))
	{
		CRect rClient;
		CWnd::GetClientRect(rClient);

		nSplitPos = (rClient.Width() - nSplitPos);
	}

	if (nSplitPos > CLIENTCOLWIDTH)
	{
		m_lcTasks.SetColumnWidth(0, (nSplitPos + 4));

		if (IsSplitting())
			PostResize();
	}
	else if (m_lcTasks.GetColumnWidth(0) > CLIENTCOLWIDTH)
	{
		m_lcTasks.SetColumnWidth(0, CLIENTCOLWIDTH);
	}
}

int CTDLTaskListCtrl::InsertItem(DWORD dwTaskID, int nPos)
{
	ASSERT(FindTaskItem(dwTaskID) == -1);

	// omit task references from list
	if (m_data.IsTaskReference(dwTaskID))
		return -1;

	// else
	if (nPos == -1)
		nPos = GetItemCount();

	return m_lcTasks.InsertItem(LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM | LVIF_STATE, 
								nPos, 
								LPSTR_TEXTCALLBACK, 
								0,
								LVIS_STATEIMAGEMASK,
								I_IMAGECALLBACK, 
								dwTaskID);
}

LRESULT CTDLTaskListCtrl::OnListGetDispInfo(NMLVDISPINFO* pLVDI)
{
	if (pLVDI->hdr.hwndFrom == m_lcTasks)
	{
		if (pLVDI->item.mask & LVIF_TEXT) // for tooltips
		{
			DWORD dwTaskID = pLVDI->item.lParam;
			
			const TODOITEM* pTDI = m_data.GetTrueTask(dwTaskID);
			ASSERT(pTDI);

			if (pTDI)
			{
				pLVDI->item.pszText = (LPTSTR)(LPCTSTR)pTDI->sTitle;

				// Hack to get tooltip delays consistent across all task views
				HWND hwndTooltip = (HWND)m_lcTasks.SendMessage(LVM_GETTOOLTIPS);

				if (hwndTooltip)
				{
					::SendMessage(hwndTooltip, TTM_SETDELAYTIME, TTDT_INITIAL, MAKELPARAM(50, 0));
					::SendMessage(hwndTooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, MAKELPARAM(10000, 0));
				}
			}
		}

		if (pLVDI->item.mask & LVIF_IMAGE)
		{
			DWORD dwTaskID = pLVDI->item.lParam;
			
			const TODOITEM* pTDI = NULL;
			const TODOSTRUCTURE* pTDS = NULL;
			
			if (m_data.GetTrueTask(dwTaskID, pTDI, pTDS))
			{
				// user icon
				if (!IsColumnShowing(TDCC_ICON))
				{
					int nImage = -1;
					
					if (!pTDI->sIcon.IsEmpty())
					{
						nImage = m_ilTaskIcons.GetImageIndex(pTDI->sIcon);
					}
					else if (HasStyle(TDCS_SHOWPARENTSASFOLDERS) && pTDS->HasSubTasks())
					{
						nImage = 0;
					}
					
					pLVDI->item.iImage = nImage;
				}

				// checkbox icon
				if (!IsColumnShowing(TDCC_DONE) && HasStyle(TDCS_ALLOWTREEITEMCHECKBOX))
				{
					pLVDI->item.mask |= LVIF_STATE;
					pLVDI->item.stateMask = LVIS_STATEIMAGEMASK | LVIS_SELECTED;

					if (pTDI->IsDone())
					{
						pLVDI->item.state = LCHC_CHECKED;
					}
					else if (m_data.TaskHasCompletedSubtasks(pTDS))
					{
						pLVDI->item.state = LCHC_MIXED;
					}
					else 
					{
						pLVDI->item.state = LCHC_UNCHECKED;
					}
				}
			}
		}
	}
	
	return 0L;
}

LRESULT CTDLTaskListCtrl::WindowProc(HWND hRealWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	if (!IsResyncEnabled())
		return CTDLTaskCtrlBase::WindowProc(hRealWnd, msg, wp, lp);
	
	ASSERT(hRealWnd == GetHwnd());
	
	switch (msg)
	{
	case WM_NOTIFY:
		{
			LPNMHDR pNMHDR = (LPNMHDR)lp;
			HWND hwnd = pNMHDR->hwndFrom;
			
			if (hwnd == m_lcTasks)
			{
				switch (pNMHDR->code)
				{
 				case LVN_BEGINLABELEDIT:
					// always eat this because we handle it ourselves
 					return 1L;
					
				case LVN_GETDISPINFO:
					return OnListGetDispInfo((NMLVDISPINFO*)pNMHDR);

// 				case LVN_GETINFOTIP:
// 					{
// 						NMLVGETINFOTIP* pLVGIT = (NMLVGETINFOTIP*)pNMHDR;
// 						DWORD dwTaskID = GetTaskID(pLVGIT->iItem);
// 						
// 						if (dwTaskID)
// 						{
// 							CString sInfoTip(FormatInfoTip(dwTaskID, (pLVGIT->cchTextMax - 1)));
// 							lstrcpyn(pLVGIT->pszText, sInfoTip, (pLVGIT->cchTextMax - 1));
// 
// 							return 0L; // eat
// 						}
// 					}
// 					break;
				}
			}
		}
		break;
	}
	
	return CTDLTaskCtrlBase::WindowProc(hRealWnd, msg, wp, lp);
}

void CTDLTaskListCtrl::NotifyParentSelChange(SELCHANGE_ACTION nAction)
{
	if (nAction == SC_BYMOUSE)
		UpdateAll();

	NMLISTVIEW nmlv = { 0 };
	
	nmlv.hdr.code = LVN_ITEMCHANGED;
	nmlv.iItem = nmlv.iSubItem = -1;
	
	RepackageAndSendToParent(WM_NOTIFY, 0, (LPARAM)&nmlv);
}

LRESULT CTDLTaskListCtrl::ScWindowProc(HWND hRealWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	if (!IsResyncEnabled())
		return CTDLTaskCtrlBase::ScWindowProc(hRealWnd, msg, wp, lp);
	
	switch (msg)
	{
#ifdef _DEBUG
/*
	case WM_PAINT:
		if (hRealWnd == m_lcTasks)
		{
			DWORD dwTick = GetTickCount();
			LRESULT lr = CTDLTaskCtrlBase::ScWindowProc(hRealWnd, msg, wp, lp);
			TRACE(_T("WM_PAINT(ListView - Client Column) took %d ms)\n"), (GetTickCount() - dwTick));
			return lr;
		}
		else if (hRealWnd == m_lcColumns)
		{
			DWORD dwTick = GetTickCount();
			LRESULT lr = CTDLTaskCtrlBase::ScWindowProc(hRealWnd, msg, wp, lp);
			TRACE(_T("WM_PAINT(ListView - Attribute Columns) took %d ms)\n"), (GetTickCount() - dwTick));
			return lr;
		}
		break;
*/
#endif

	case WM_NOTIFY:
		{
			LPNMHDR pNMHDR = (LPNMHDR)lp;
			HWND hwnd = pNMHDR->hwndFrom;
			
			switch (pNMHDR->code)
			{
			case HDN_ITEMCLICK:
				if (hwnd == m_hdrTasks)
				{
					NMHEADER* pNMH = (NMHEADER*)pNMHDR;
					
					// forward on to our parent
					if ((pNMH->iButton == 0) && m_hdrColumns.IsItemVisible(pNMH->iItem))
					{
						HDITEM hdi = { HDI_LPARAM, 0 };
						
						pNMH->pitem = &hdi;
						pNMH->pitem->lParam = TDCC_CLIENT;
						
						RepackageAndSendToParent(msg, wp, lp);
					}
					return 0L;
				}
				break;
				
			case TTN_NEEDTEXT:
				if (hwnd == m_hdrTasks)
				{
					// this has a nasty habit of redrawing the current item label
					// even when a tooltip is not displayed which caused any
					// trailing comment text to disappear, so we always invalidate
					// the entire item if it has comments
					CPoint pt(GetMessagePos());
					m_lcTasks.ScreenToClient(&pt);

					int nItem = m_lcTasks.HitTest(pt);

					if (nItem != -1)
					{
						const TODOITEM* pTDI = GetTask(nItem);
						ASSERT(pTDI);
						
						if (!pTDI->sComments.IsEmpty())
						{
							CRect rItem;
							VERIFY (m_lcTasks.GetItemRect(nItem, rItem, LVIR_BOUNDS));

							m_lcTasks.InvalidateRect(rItem, FALSE);
						}
					}
				}
				break;

			case TTN_SHOW:
				if (hRealWnd == m_lcTasks)
				{
					// Set the font to bold for top-level item tooltips
					// but not for info tips
					if (!HasStyle(TDCS_SHOWINFOTIPS))
					{
						CPoint pt(GetMessagePos());
						m_lcTasks.ScreenToClient(&pt);
					
						int nItem = m_lcTasks.HitTest(pt);
					
						if (nItem != -1)
						{
							const TODOSTRUCTURE* pTDS = m_data.LocateTask(GetTaskID(nItem));
							ASSERT(pTDS);

							BOOL bTopLevel = (pTDS && pTDS->ParentIsRoot());
							::SendMessage(hwnd, WM_SETFONT, (WPARAM)m_fonts.GetHFont(bTopLevel ? GMFS_BOLD : 0), TRUE);
						}
					}
				}
				break;
			}
		}
		break;
		
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
		if (hRealWnd == m_lcTasks)
		{
			KillTimer(TIMER_EDITLABEL);

			// let parent handle any focus changes first
			m_lcTasks.SetFocus();

			// don't let the selection to be set to -1
			// when clicking below the last item
			UINT nHitFlags = 0;

			if (m_lcTasks.HitTest(lp, &nHitFlags) == -1)
				return 0; // eat it

			if (msg == WM_LBUTTONDBLCLK)
			{
				if (CTreeListSyncer::HasStyle(hRealWnd, LVS_EDITLABELS, FALSE) &&
					(nHitFlags & (LVHT_ONITEMLABEL | LVHT_TORIGHT)))
				{
					NotifyParentOfColumnEditClick(TDCC_CLIENT, GetSelectedTaskID());
					return 0L; // eat it
				}
			}
		}
		break;

	case WM_RBUTTONDOWN:
		// Don't let the selection to be set to -1 when clicking below the last item
		// BUT NOT ON Linux because it interferes with context menu handling
		if ((hRealWnd == m_lcTasks) && (COSVersion() != OSV_LINUX))
		{
			// let parent handle any focus changes first
			m_lcTasks.SetFocus();

			if (m_lcTasks.HitTest(lp) == -1)
				return 0L; // eat it
		}
		break;

	case WM_LBUTTONDOWN:
		if (hRealWnd == m_lcTasks)
		{
			BOOL bHadFocus = HasFocus();

			// let parent handle any focus changes first
			m_lcTasks.SetFocus();

			UINT nFlags = 0;
			int nHit = m_lcTasks.HitTest(lp, &nFlags);

			if (nHit != -1)
			{
				if (Misc::ModKeysArePressed(0))
				{
					// if the item is not selected we must first deal
					// with that before processing the click
					BOOL bHitSelected = IsListItemSelected(m_lcTasks, nHit);
					BOOL bSelChange = FALSE;
					
					if (!bHitSelected)
					{
						bSelChange = SelectItem(nHit);
						NotifyParentSelChange(SC_BYMOUSE);
					}

					// If multiple items are selected and no edit took place
					// Clear the selection to just the hit item
					if (!HandleClientColumnClick(lp, FALSE) && (GetSelectedCount() > 1))
						bSelChange |= SelectItem(nHit);
				
					// Eat the msg to prevent a label edit if we changed the selection
					// Or if the item was already selected but not focused, 
					// to be consistent with CTDLTaskTreeCtrl
					if (bSelChange || !bHadFocus)
						return 0L;
				}
				else if (Misc::IsKeyPressed(VK_SHIFT)) // bulk-selection
				{
					{
						CTLSHoldResync hr(*this);
						
						int nAnchor = m_lcTasks.GetSelectionMark();
						m_lcColumns.SetSelectionMark(nAnchor);

						if (!Misc::IsKeyPressed(VK_CONTROL))
							DeselectAll();				

						// prevent resyncing
						// Add new items to tree and list
						int nHit = m_lcTasks.HitTest(lp);

						int nFrom = (nAnchor < nHit) ? nAnchor : nHit;
						int nTo = (nAnchor < nHit) ? nHit : nAnchor;

						for (int nItem = nFrom; nItem <= nTo; nItem++)
						{
							m_lcTasks.SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);				
							m_lcColumns.SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);				
						}
					}

					NotifyParentSelChange(SC_BYMOUSE);
					return 0; // eat it
				}
				else if (CTDLTaskCtrlBase::HandleListLBtnDown(m_lcTasks, lp))
				{
					return 0L;
				}
			}
			else if (CTDLTaskCtrlBase::HandleListLBtnDown(m_lcTasks, lp))
			{
				return 0L;
			}
		}
		else
		{
			ASSERT(hRealWnd == m_lcColumns);

			// Selecting or de-selecting a lot of items can be slow
			// because OnListSelectionChange is called once for each.
			// Base class handles simple click de-selection so we
			// handle bulk selection here
			if (Misc::IsKeyPressed(VK_SHIFT)) // bulk-selection
			{
				// Scope the resync to re-enable before notifying
				// the parent else redraw issues abound
				{
					CTLSHoldResync hr(*this);
					
					int nAnchor = m_lcColumns.GetSelectionMark();
					m_lcTasks.SetSelectionMark(nAnchor);

					if (!Misc::IsKeyPressed(VK_CONTROL))
						DeselectAll();

					// Add new items to tree and list
					TDC_COLUMN nColID = TDCC_NONE;
					int nHit = HitTestColumnsItem(lp, TRUE, nColID);

					int nFrom = (nAnchor < nHit) ? nAnchor : nHit;
					int nTo = (nAnchor < nHit) ? nHit : nAnchor;

					for (int nItem = nFrom; nItem <= nTo; nItem++)
					{
						m_lcTasks.SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);				
						m_lcColumns.SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);				
					}

				}
				
				NotifyParentSelChange(SC_BYMOUSE);
				return 0; // eat it
			}
			
			if (CTDLTaskCtrlBase::HandleListLBtnDown(m_lcColumns, lp))
				return 0L;
		}
		break;

	case WM_KEYDOWN:
		if (Misc::IsKeyPressed(VK_SHIFT)) // bulk-selection
		{
			int nAnchor = ::SendMessage(hRealWnd, LVM_GETSELECTIONMARK, 0, 0);
			::SendMessage(OtherWnd(hRealWnd), LVM_SETSELECTIONMARK, 0, nAnchor);

			int nFrom = -1, nTo = -1;

			switch (wp)
			{
			case VK_NEXT:
				nFrom = nAnchor;
				nTo = (m_lcTasks.GetTopIndex() + m_lcTasks.GetCountPerPage());
				break;

			case VK_PRIOR: 
				nFrom = m_lcTasks.GetTopIndex();
				nTo = nAnchor;
				break;

			case VK_HOME:
				nFrom = 0;
				nTo = nAnchor;
				break;

			case VK_END:
				nFrom = nAnchor;
				nTo = (m_lcTasks.GetItemCount() - 1);
				break;
			}

			if ((nFrom != -1) && (nTo != -1))
			{
				CTLSHoldResync hr(*this);
				
				if (!Misc::IsKeyPressed(VK_CONTROL))
					DeselectAll();

				for (int nItem = nFrom; nItem <= nTo; nItem++)
				{
					m_lcTasks.SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);				
					m_lcColumns.SetItemState(nItem, LVIS_SELECTED, LVIS_SELECTED);				
				}

				NotifyParentSelChange(SC_BYKEYBOARD);
			}
		}
		break;

	case WM_KEYUP:
		{
			switch (wp)
			{
			case VK_NEXT:  
			case VK_DOWN:
			case VK_UP:
			case VK_PRIOR: 
			case VK_HOME:
			case VK_END:
				if (hRealWnd == m_lcTasks)
					NotifyParentSelChange(SC_BYKEYBOARD);
				break;

			case VK_LEFT:
			case VK_RIGHT:
				// Windows only invalidates the item labels but
				// we need the whole row because we render the 
				// comments after the task text
				if (hRealWnd == m_lcTasks)
					m_lcTasks.Invalidate(FALSE);
				break;
			}
		}
		break;

	case WM_TIMER:
		// make sure the mouse is still over the item label because
		// with LVS_EX_FULLROWSELECT turned on the whole 
		if (wp == TIMER_EDITLABEL)
		{
			if (CTreeListSyncer::HasStyle(hRealWnd, LVS_EDITLABELS, FALSE))
			{
				const TODOITEM* pTDI = NULL;
				const TODOSTRUCTURE* pTDS = NULL;
				
				int nItem = GetSelectedItem();
				DWORD dwTaskID = GetTaskID(nItem);
				
				if (m_data.GetTrueTask(dwTaskID, pTDI, pTDS))
				{
					CClientDC dc(&m_lcTasks);
					CFont* pOldFont = dc.SelectObject(GetTaskFont(pTDI, pTDS, FALSE));
					
					CRect rLabel;
					
					if (GetItemTitleRect(nItem, TDCTR_TEXT, rLabel, &dc, pTDI->sTitle))
					{
						// Add a bit ie. 'near enough is good enough'
						rLabel.right += 10;

						CPoint pt(GetMessagePos());
						m_lcTasks.ScreenToClient(&pt);
						
						if (rLabel.PtInRect(pt))
							NotifyParentOfColumnEditClick(TDCC_CLIENT, GetSelectedTaskID());
					}
					
					// cleanup
					dc.SelectObject(pOldFont);
				}
			}
			
			::KillTimer(hRealWnd, wp);
			return 0L; // eat
		}
		break;
	}
	
	return CTDLTaskCtrlBase::ScWindowProc(hRealWnd, msg, wp, lp);
}

BOOL CTDLTaskListCtrl::OnListSelectionChange(NMLISTVIEW* /*pNMLV*/)
{
	// only called for the list that currently has the focus
	
	// Don't notify if the up/down cursor key is still pressed
	if (Misc::IsCursorKeyPressed(MKC_UPDOWN))
	{
		return FALSE;
	}

	// Or if the mouse button is down, over an item, 
	// the selection is empty, and the CTRL
	// key is not pressed. ie. the user is in 
	// the middle of selecting a new item
	if (Misc::IsKeyPressed(VK_LBUTTON) &&
			HitTestTask(GetMessagePos(), false) &&
			!Misc::IsKeyPressed(VK_CONTROL) &&
			!GetSelectedCount())
	{
		return FALSE;
	}

	// Or we are bounds selecting
	if (IsBoundSelecting())
		return FALSE;

	// Or more than 2 tasks are selected
	if (m_lcColumns.GetSelectedCount() > 2)
		return FALSE;

	NotifyParentSelChange();
	return TRUE;
}

BOOL CTDLTaskListCtrl::HasHitTestFlag(UINT nFlags, UINT nFlag)
{
	return (Misc::HasFlag(nFlags, nFlag) &&
			(!OsIsXP() || !Misc::HasFlag(nFlags, LVHT_ONITEM)));
}

BOOL CTDLTaskListCtrl::HandleClientColumnClick(const CPoint& pt, BOOL bDblClk)
{
	if (!IsReadOnly() && Misc::ModKeysArePressed(0))
	{
		UINT nFlags = 0;
		int nItem = m_lcTasks.HitTest(pt, &nFlags);
		
		if (nItem != -1)
		{
			ASSERT(IsListItemSelected(m_lcTasks, nItem)); 

			DWORD dwTaskID = GetColumnItemTaskID(nItem);
			TDC_COLUMN nClickCol = TDCC_NONE;

			if (!bDblClk)
			{
				if (!IsColumnShowing(TDCC_DONE) && HasHitTestFlag(nFlags, LVHT_ONITEMSTATEICON))
				{
					nClickCol = TDCC_DONE;
				}
				else if (!IsColumnShowing(TDCC_ICON) && HasHitTestFlag(nFlags, LVHT_ONITEMICON))
				{
					nClickCol = TDCC_ICON;
				}
			}
			else
			{
				CRect rItem, rClient;

				m_lcTasks.GetItemRect(nItem, rItem, LVIR_LABEL);
				m_lcTasks.GetClientRect(rClient);

				rItem.right = rClient.right;

				if (rItem.PtInRect(pt))
				{
					nClickCol = TDCC_CLIENT;
				}
			}

			if ((nClickCol != TDCC_NONE) && !SelectionHasLocked(FALSE))
			{
				// forward the click
				NotifyParentOfColumnEditClick(nClickCol, dwTaskID);

				return TRUE;
			}
		}
	}

	// not handled
	return FALSE;
}

BOOL CTDLTaskListCtrl::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) 
{
	if ((pWnd == &m_lcTasks) && !IsReadOnly() && !IsColumnShowing(TDCC_ICON))
	{
		UINT nHitFlags = 0;
		CPoint ptClient(::GetMessagePos());
		m_lcTasks.ScreenToClient(&ptClient);

		int nHit = m_lcTasks.HitTest(ptClient, &nHitFlags);
	
		if (nHit != -1)
		{
			if (m_calculator.IsTaskLocked(GetTaskID(nHit)))
			{
				return GraphicsMisc::SetAppCursor(_T("Locked"), _T("Resources\\Cursors"));
			}
			else if (HasHitTestFlag(nHitFlags, LVHT_ONITEMICON))
			{
				return GraphicsMisc::SetHandCursor();
			}
		}
	}
	
	// else
	return CTDLTaskCtrlBase::OnSetCursor(pWnd, nHitTest, message);
}

void CTDLTaskListCtrl::GetWindowRect(CRect& rWindow, BOOL bWithHeader) const
{
	// always
	GetWindowRect(rWindow);

	if (!bWithHeader)
	{
		CRect rTree;
		m_lcTasks.GetWindowRect(rTree);

		rWindow.top = rTree.top;
	}
}

BOOL CTDLTaskListCtrl::GetItemTitleRect(const NMCUSTOMDRAW& nmcd, TDC_LABELRECT nArea, CRect& rect, CDC* pDC, LPCTSTR szTitle) const
{
	return GetItemTitleRect((int)nmcd.dwItemSpec, nArea, rect, pDC, szTitle);
}

BOOL CTDLTaskListCtrl::GetItemTitleRect(int nItem, TDC_LABELRECT nArea, CRect& rect, CDC* pDC, LPCTSTR szTitle) const
{
	// basic title rect
	const_cast<CListCtrl*>(&m_lcTasks)->GetItemRect(nItem, rect, LVIR_LABEL);
	int nHdrWidth = m_hdrTasks.GetItemWidth(0);
	
	switch (nArea)
	{
	case TDCTR_BKGND:
		if (pDC && szTitle)
		{
			rect.right = (rect.left + pDC->GetTextExtent(szTitle).cx);
			rect.right = min(rect.right + 3, nHdrWidth);
		}
		else
		{
			ASSERT(!pDC && !szTitle);
			rect.right = nHdrWidth;
		}
		return TRUE;
		
	case TDCTR_TEXT:
		if (GetItemTitleRect(nItem, TDCTR_BKGND, rect, pDC, szTitle)) // recursive call
		{
			rect.left += TITLE_BORDER_OFFSET;
			return TRUE;
		}
		break;
		
	case TDCTR_EDIT:
		if (GetItemTitleRect(nItem, TDCTR_BKGND, rect)) // recursive call
		{
			rect.top--;
			
			// return in screen coords
			m_lcTasks.ClientToScreen(rect);
			return TRUE;
		}
		break;
	}

	return FALSE;
}

BOOL CTDLTaskListCtrl::GetSelectionBoundingRect(CRect& rSelection) const
{
	rSelection.SetRectEmpty();
	
	POSITION pos = m_lcTasks.GetFirstSelectedItemPosition();
	
	// initialize to first item
	if (pos)
		VERIFY(GetItemTitleRect(m_lcTasks.GetNextSelectedItem(pos), TDCTR_TEXT, rSelection));
	
	// rest of selection
	while (pos)
	{
		CRect rItem;
		VERIFY(GetItemTitleRect(m_lcTasks.GetNextSelectedItem(pos), TDCTR_TEXT, rItem));
		
		rSelection |= rItem;
	}
	
	m_lcTasks.ClientToScreen(rSelection);
	ScreenToClient(rSelection);
	
	return !rSelection.IsRectEmpty();
}

BOOL CTDLTaskListCtrl::GetLabelEditRect(CRect& rLabel) const
{
	return GetItemTitleRect(GetSelectedItem(), TDCTR_EDIT, rLabel);
}

BOOL CTDLTaskListCtrl::InvalidateSelection(BOOL bUpdate)
{
	if (CTDLTaskCtrlBase::InvalidateColumnSelection(bUpdate))
	{
		VERIFY(CTDLTaskCtrlBase::InvalidateSelection(m_lcTasks, bUpdate));
		return TRUE;
	}

	return FALSE;
}

BOOL CTDLTaskListCtrl::InvalidateTask(DWORD dwTaskID, BOOL bUpdate)
{
	if ((dwTaskID == 0) || !m_lcTasks.IsWindowVisible())
		return TRUE; // nothing to do
	
	int nItem = FindListItem(m_lcTasks, dwTaskID);
	
	return InvalidateItem(nItem, bUpdate);
}

BOOL CTDLTaskListCtrl::InvalidateItem(int nItem, BOOL bUpdate)
{
	if (InvalidateColumnItem(nItem, bUpdate))
	{
		CRect rItem;
		VERIFY (m_lcTasks.GetItemRect(nItem, rItem, LVIR_BOUNDS));
			
		m_lcTasks.InvalidateRect(rItem);
		
		if (bUpdate)
			m_lcTasks.UpdateWindow();

		return TRUE;
	}

	return FALSE;
}

int CTDLTaskListCtrl::GetSelectedItem() const
{
	POSITION pos = m_lcTasks.GetFirstSelectedItemPosition();

	return m_lcTasks.GetNextSelectedItem(pos);
}

BOOL CTDLTaskListCtrl::IsItemSelected(int nItem) const
{
	return CTreeListSyncer::IsListItemSelected(m_lcTasks, nItem);
}

BOOL CTDLTaskListCtrl::SelectItem(int nItem)
{
	m_lcTasks.SetSelectionMark(nItem);
	m_lcColumns.SetSelectionMark(nItem);
	
	// avoid unnecessary selections
	if ((GetSelectedCount() == 1) && (GetSelectedItem() == nItem))
		return TRUE;

	CTLSHoldResync hold(*this);

	DeselectAll();

	m_lcTasks.SetItemState(nItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	m_lcColumns.SetItemState(nItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

	return TRUE;
}

BOOL CTDLTaskListCtrl::EnsureSelectionVisible()
{
	if (GetSelectedCount())
	{
		OSVERSION nOSVer = COSVersion();
		
		if ((nOSVer == OSV_LINUX) || (nOSVer < OSV_VISTA))
		{
			m_lcTasks.PostMessage(LVM_ENSUREVISIBLE, GetSelectedItem());
		}
		else
		{
			m_lcTasks.EnsureVisible(GetSelectedItem(), FALSE);
		}

		return TRUE;
	}
	
	// else
	return FALSE;
}

BOOL CTDLTaskListCtrl::IsTaskSelected(DWORD dwTaskID, BOOL bSingly) const
{
	if (bSingly && (m_lcTasks.GetSelectedCount() != 1))
		return FALSE;

	POSITION pos = m_lcTasks.GetFirstSelectedItemPosition();
	
	while (pos)
	{
		int nItem = m_lcTasks.GetNextSelectedItem(pos);
		DWORD dwItemID = GetTaskID(nItem);

		if (dwItemID == dwTaskID)
			return TRUE;
	}

	// else
	return FALSE;
}

DWORD CTDLTaskListCtrl::GetTaskID(int nItem) const 
{ 
	if ((nItem == -1) || (nItem >= m_lcTasks.GetItemCount()))
		return 0;

	return m_lcTasks.GetItemData(nItem); 
}

DWORD CTDLTaskListCtrl::GetSelectedTaskID() const 
{ 
	return GetTaskID(GetSelectedItem()); 
}

DWORD CTDLTaskListCtrl::GetTrueTaskID(int nItem) const 
{ 
	DWORD dwTaskID = GetTaskID(nItem);

	if (dwTaskID == 0)
		return 0;

	return m_data.GetTrueTaskID(dwTaskID); 
}

int CTDLTaskListCtrl::GetSelectedTaskIDs(CDWordArray& aTaskIDs, BOOL bTrue) const
{
	DWORD dwFocusID;
	int nNumIDs = GetSelectedTaskIDs(aTaskIDs, dwFocusID);

	// extra processing
	if (nNumIDs && bTrue)
		m_data.GetTrueTaskIDs(aTaskIDs);

	return nNumIDs;
}

int CTDLTaskListCtrl::GetSelectedTaskIDs(CDWordArray& aTaskIDs, DWORD& dwFocusedTaskID) const
{
	aTaskIDs.RemoveAll();
	dwFocusedTaskID = 0;

	if (m_lcTasks.GetSelectedCount())
	{
		aTaskIDs.SetSize(m_lcTasks.GetSelectedCount());
	
		int nCount = 0;
		POSITION pos = m_lcTasks.GetFirstSelectedItemPosition();
	
		while (pos)
		{
			int nItem = m_lcTasks.GetNextSelectedItem(pos);
		
			aTaskIDs[nCount] = m_lcTasks.GetItemData(nItem);
			nCount++;
		}
	
		dwFocusedTaskID = GetFocusedListTaskID();
	}
	ASSERT((!aTaskIDs.GetSize() && (dwFocusedTaskID == 0)) || Misc::HasT(dwFocusedTaskID, aTaskIDs));
	
	return aTaskIDs.GetSize();
}

BOOL CTDLTaskListCtrl::SelectTasks(const CDWordArray& aTaskIDs, BOOL bTrue)
{
	ASSERT(aTaskIDs.GetSize());

	if (!aTaskIDs.GetSize())
		return FALSE;

	if (!bTrue)
	{
		SetSelectedTasks(aTaskIDs, aTaskIDs[0]);
	}
	else
	{
		CDWordArray aTrueTaskIDs;
		
		aTrueTaskIDs.Copy(aTaskIDs);
		m_data.GetTrueTaskIDs(aTrueTaskIDs);
		
		SetSelectedTasks(aTrueTaskIDs, aTrueTaskIDs[0]);
	}

	return TRUE;
}

BOOL CTDLTaskListCtrl::SelectTask(DWORD dwTaskID, BOOL bTrue)
{
	if (dwTaskID == 0)
	{
		ASSERT(0);
		return FALSE;
	}

	CDWordArray aTaskIDs;
	aTaskIDs.Add(dwTaskID);

	SetSelectedTasks(aTaskIDs, bTrue);
	return TRUE;
}

void CTDLTaskListCtrl::SetSelectedTasks(const CDWordArray& aTaskIDs, DWORD dwFocusedTaskID)
{
	// Prevent re-entrancy
	CTLSHoldResync hr(*this);

	DeselectAll();

	int nID = aTaskIDs.GetSize();

	while (nID--)
	{
		DWORD dwTaskID = aTaskIDs[nID];
		int nItem = FindTaskItem(dwTaskID);

		if (nItem != -1)
		{
			BOOL bAnchor = (dwTaskID == dwFocusedTaskID);
			DWORD dwState = LVIS_SELECTED;

			if (bAnchor)
			{
				dwState |= LVIS_FOCUSED;

				m_lcTasks.SetSelectionMark(nItem);
				m_lcColumns.SetSelectionMark(nItem);
			}

			m_lcTasks.SetItemState(nItem, dwState, (LVIS_SELECTED | LVIS_FOCUSED));
			m_lcColumns.SetItemState(nItem, dwState, (LVIS_SELECTED | LVIS_FOCUSED));
		}
	}

	EnsureSelectionVisible();
}

int CTDLTaskListCtrl::CacheSelection(TDCSELECTIONCACHE& cache, BOOL bIncBreadcrumbs) const
{
	if (GetSelectedTaskIDs(cache.aSelTaskIDs, cache.dwFocusedTaskID) > 0)
	{
		cache.dwFirstVisibleTaskID = GetTaskID(m_lcTasks.GetTopIndex());
	
		// breadcrumbs
		cache.aBreadcrumbs.RemoveAll();
	
		if (bIncBreadcrumbs)
		{
			// cache the preceding and following 10 tasks
			int nFocus = GetFocusedListItem(), nItem;
			int nMin = 0, nMax = m_lcTasks.GetItemCount() - 1;
		
			nMin = max(nMin, nFocus - 11);
			nMax = min(nMax, nFocus + 11);
		
			// following tasks first
			for (nItem = (nFocus + 1); nItem <= nMax; nItem++)
				cache.aBreadcrumbs.InsertAt(0, m_lcTasks.GetItemData(nItem));
		
			for (nItem = (nFocus - 1); nItem >= nMin; nItem--)
				cache.aBreadcrumbs.InsertAt(0, m_lcTasks.GetItemData(nItem));
		}
	}	

	return cache.aSelTaskIDs.GetSize();
}

int CTDLTaskListCtrl::RestoreSelection(const TDCSELECTIONCACHE& cache, BOOL bEnsureSelection)
{
	if ((GetItemCount() == 0) || cache.IsEmpty())
	{
		DeselectAll();
		return 0;
	}

	DWORD dwFocusedTaskID = cache.dwFocusedTaskID;
	ASSERT(dwFocusedTaskID);
	
	if (FindTaskItem(dwFocusedTaskID) == -1)
	{
		dwFocusedTaskID = 0;

		if (bEnsureSelection)
		{
			int nID = cache.aBreadcrumbs.GetSize();
		
			while (nID--)
			{
				dwFocusedTaskID = cache.aBreadcrumbs[nID];
			
				if (FindTaskItem(dwFocusedTaskID) != -1)
					break;
				else
					dwFocusedTaskID = 0;
			}
		}
	}
	
	// add focused task if it isn't already
	if (dwFocusedTaskID)
	{
		CDWordArray aTaskIDs;
		aTaskIDs.Copy(cache.aSelTaskIDs);
		
		if (!Misc::HasT(dwFocusedTaskID, aTaskIDs))
			aTaskIDs.Add(dwFocusedTaskID);
			
		SetSelectedTasks(aTaskIDs, dwFocusedTaskID);
		
		// restore pos
		if (cache.dwFirstVisibleTaskID)
			SetTopIndex(FindTaskItem(cache.dwFirstVisibleTaskID));
	}
	else if (bEnsureSelection)
	{
		SelectItem(0);
	}
	else
	{
		DeselectAll();
	}

	return m_lcTasks.GetSelectedCount();
}

void CTDLTaskListCtrl::DeselectAll() 
{ 
	// prevent resyncing
	CTLSHoldResync hr(*this);

	m_lcTasks.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
	m_lcColumns.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
}

BOOL CTDLTaskListCtrl::SelectAll() 
{ 
	// prevent resyncing
	CTLSHoldResync hr(*this);

	m_lcTasks.SetItemState(-1, LVIS_SELECTED, LVIS_SELECTED);
	m_lcColumns.SetItemState(-1, LVIS_SELECTED, LVIS_SELECTED);

	return TRUE;
}

int CTDLTaskListCtrl::GetTopIndex() const
{
	if (m_lcTasks.GetItemCount() == 0)
		return -1;
	
	return m_lcTasks.GetTopIndex();
}

BOOL CTDLTaskListCtrl::SetTopIndex(int nIndex)
{
	if ((nIndex >= 0) && (nIndex < m_lcTasks.GetItemCount()))
	{
		int nCurTop = GetTopIndex();
		ASSERT(nCurTop != -1);
		
		if (nIndex == nCurTop)
			return TRUE;
		
		// else calculate offset and scroll
		CRect rItem;
		
		if (m_lcTasks.GetItemRect(nCurTop, rItem, LVIR_BOUNDS))
		{
			int nItemHeight = rItem.Height();
			m_lcTasks.Scroll(CSize(0, (nIndex - nCurTop) * nItemHeight));
			
			return TRUE;
		}
	}
	
	// else
	return FALSE;
}

DWORD CTDLTaskListCtrl::GetFocusedListTaskID() const
{
	int nItem = GetFocusedListItem();
	
	if (nItem == -1)
		nItem = GetSelectedItem();

	if (nItem != -1)
		return m_lcTasks.GetItemData(nItem);
	
	// else
	return 0;
}

int CTDLTaskListCtrl::GetFocusedListItem() const
{
	return m_lcTasks.GetNextItem(-1, LVNI_FOCUSED | LVNI_SELECTED);
}

int CTDLTaskListCtrl::FindTaskItem(DWORD dwTaskID) const
{
	return CTreeListSyncer::FindListItem(m_lcTasks, dwTaskID);
}

BOOL CTDLTaskListCtrl::IsAlternateTitleLine(const NMCUSTOMDRAW& nmcd) const
{
	return IsAlternateColumnLine((int)nmcd.dwItemSpec);
}

GM_ITEMSTATE CTDLTaskListCtrl::GetItemTitleState(const NMCUSTOMDRAW& nmcd) const
{
	return GetListItemState((int)nmcd.dwItemSpec);
}

GM_ITEMSTATE CTDLTaskListCtrl::GetListItemState(int nItem) const
{
	if (!m_bSavingToImage)
	{
		if (m_lcTasks.GetItemState(nItem, LVIS_DROPHILITED) & LVIS_DROPHILITED)
		{
			return GMIS_DROPHILITED;
		}
		else if (m_lcTasks.GetItemState(nItem, LVIS_SELECTED) & LVIS_SELECTED)
		{
			DWORD dwTaskID = GetTaskID(nItem);

			if (HasFocus() || (dwTaskID == m_dwEditTitleTaskID))
				return GMIS_SELECTED;

			// else 
			return GMIS_SELECTEDNOTFOCUSED;
		}
	}
	
	return GMIS_NONE;
}

GM_ITEMSTATE CTDLTaskListCtrl::GetColumnItemState(int nItem) const
{
	return GetListItemState(nItem);
}

BOOL CTDLTaskListCtrl::DoSaveToImage(CBitmap& bmImage, COLORREF crDivider)
{
	// Resize the title column to the actual width of the title text
	int nColWidth = m_lcTasks.GetColumnWidth(0);

	int nReqWidth = CalcRequiredTitleColumnWidthForImage();
	m_lcTasks.SetColumnWidth(0, nReqWidth);

	BOOL bRes = CTDLTaskCtrlBase::DoSaveToImage(bmImage, crDivider);

	// Restore title column width
	m_lcTasks.SetColumnWidth(0, nColWidth);

	return bRes;
}

int CTDLTaskListCtrl::CalcRequiredTitleColumnWidthForImage()
{
	int nItem = m_lcTasks.GetItemCount();
	CString sLongestTopLevel, sLongestChild;

	while (nItem--)
	{
		DWORD dwTaskID = GetTaskID(nItem);

		const TODOITEM* pTDI = m_data.GetTrueTask(dwTaskID);
		const TODOSTRUCTURE* pTDS = m_data.LocateTask(dwTaskID);

		int nTitleLen = pTDI->sTitle.GetLength();

		if (pTDS->ParentIsRoot())
		{
			if (nTitleLen > sLongestTopLevel.GetLength())
				sLongestTopLevel = pTDI->sTitle;
		}
		else
		{
			if (nTitleLen > sLongestChild.GetLength())
				sLongestChild = pTDI->sTitle;
		}
	}

	CClientDC dc(this);

	int nMaxTopLevelWidth = GraphicsMisc::GetAverageMaxStringWidth(sLongestTopLevel, &dc, m_fonts.GetFont(GMFS_BOLD));
	int nMaxChildWidth = GraphicsMisc::GetAverageMaxStringWidth(sLongestChild, &dc, m_fonts.GetFont());

	int nMaxTextWidth = max(nMaxTopLevelWidth, nMaxChildWidth);

	// Add to this the left margin
	CRect rLabel;
	m_lcTasks.GetItemRect(0, rLabel, LVIR_LABEL);

	return (rLabel.left + nMaxTextWidth + (LV_COLPADDING * 2));
}