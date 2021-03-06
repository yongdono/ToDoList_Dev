// TDLShowReminderDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "TDLShowReminderDlg.h"
#include "filteredtodoctrl.h"
#include "todoctrlreminders.h"

#include "..\Shared\enstring.h"
#include "..\Shared\graphicsmisc.h"
#include "..\Shared\localizer.h"
#include "..\Shared\themed.h"
#include "..\Shared\autoflag.h"
#include "..\Shared\dlgunits.h"

#include "..\Interfaces\Preferences.h"

#pragma warning(push)
#pragma warning(disable: 4201)
#include <Mmsystem.h>
#pragma warning(pop)

/////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////

#ifndef LVS_EX_DOUBLEBUFFER
#define LVS_EX_DOUBLEBUFFER 0x00010000
#endif

/////////////////////////////////////////////////////////////////////////////

// for PlaySound
#pragma comment(lib, "winmm.lib")

///////////////////////////////////////////////////////////////////////////
// CTDLShowReminderDlg dialog

double ONE_DAY_IN_MINS = (24.0 * 60);

enum
{
	TASK_COL = 0,
	TASKPARENT_COL,
	TASKLIST_COL,
	WHEN_COL,
	NUM_COLS,
};

/////////////////////////////////////////////////////////////////////////////

CTDLShowReminderDlg::CTDLShowReminderDlg(CWnd* pParent /*=NULL*/)
	: 
	CTDLDialog(CTDLShowReminderDlg::IDD, _T("ShowReminders"), pParent),
	m_dwNextReminderID(1),
	m_dtSnoozeUntil(COleDateTime::GetCurrentTime()),
	m_bChangingReminders(FALSE)
{
	//{{AFX_DATA_INIT(CTDLShowReminderDlg)
	m_bSnoozeUntil = FALSE;
	//}}AFX_DATA_INIT

	// init snooze value
	m_nSnoozeMins = CPreferences().GetProfileInt(m_sPrefsKey, _T("Snooze"), 5);
}


void CTDLShowReminderDlg::DoDataExchange(CDataExchange* pDX)
{
	CTDLDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTDLShowReminderDlg)
	DDX_Control(pDX, IDC_REMINDERS, m_lcReminders);
	DDX_Radio(pDX, IDC_SNOOZEOPTIONFOR, m_bSnoozeUntil);
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_SNOOZEFOR, m_cbSnoozeFor);
	DDX_Control(pDX, IDC_SNOOZEUNTILDATE, m_dtcSnoozeDate);
	DDX_Control(pDX, IDC_SNOOZEUNTILTIME, m_cbSnoozeTime);

	ASSERT(m_cbSnoozeFor.GetCount());

	if (pDX->m_bSaveAndValidate)
	{
		m_nSnoozeMins = m_cbSnoozeFor.GetSelectedPeriod();

		COleDateTime date;
		m_dtcSnoozeDate.GetTime(date);

		m_dtSnoozeUntil = (CDateHelper::GetDateOnly(date).m_dt + m_cbSnoozeTime.GetOleTime());
	}
	else
	{
		m_cbSnoozeFor.SetSelectedPeriod(m_nSnoozeMins);

		m_dtcSnoozeDate.SetTime(CDateHelper::GetDateOnly(m_dtSnoozeUntil));
		m_cbSnoozeTime.Set24HourTime(CDateHelper::GetTimeOnly(m_dtSnoozeUntil) * 24);
	}
}


BEGIN_MESSAGE_MAP(CTDLShowReminderDlg, CTDLDialog)
	//{{AFX_MSG_MAP(CTDLShowReminderDlg)
	ON_BN_CLICKED(IDC_SNOOZE, OnSnooze)
	ON_BN_CLICKED(IDC_DISMISS, OnDismiss)
	ON_BN_CLICKED(IDC_DISMISSANDGOTOTASK, OnDismissAndGototask)
	ON_BN_CLICKED(IDC_SNOOZEOPTIONFOR, OnSnoozeFor)
	ON_BN_CLICKED(IDC_SNOOZEOPTIONUNTIL, OnSnoozeUntil)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_REMINDERS, OnItemchangedReminders)
	ON_BN_CLICKED(IDC_SNOOZEALL, OnSnoozeAll)
	//}}AFX_MSG_MAP
	ON_NOTIFY(NM_DBLCLK, IDC_REMINDERS, OnDblClkReminders)
	ON_WM_CLOSE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTDLShowReminderDlg message handlers

BOOL CTDLShowReminderDlg::Create(CWnd* pParent, BOOL bVisible)
{
	if (CTDLDialog::Create(IDD_SHOWREMINDER_DIALOG, pParent))
	{
		if (bVisible)
			ShowWindow(SW_SHOW);

		return TRUE;
	}

	return FALSE;
}

BOOL CTDLShowReminderDlg::OnInitDialog()
{
	CTDLDialog::OnInitDialog();

	if (m_icon.Load(IDR_MAINFRAME_STD))
		SetIcon(m_icon, FALSE);

	CDlgUnits dlu(this);
	m_cbSnoozeTime.SetItemHeight(-1, dlu.ToPixelsY(10));

	// create list columns
	m_lcReminders.InsertColumn(TASK_COL, CEnString(IDS_REMINDER_TASKCOL), LVCFMT_LEFT, GraphicsMisc::ScaleByDPIFactor(200));
	m_lcReminders.InsertColumn(TASKPARENT_COL, CEnString(IDS_REMINDER_TASKPARENTCOL), LVCFMT_LEFT, GraphicsMisc::ScaleByDPIFactor(75));
	m_lcReminders.InsertColumn(TASKLIST_COL, CEnString(IDS_REMINDER_TASKLISTCOL), LVCFMT_LEFT, GraphicsMisc::ScaleByDPIFactor(75));
	m_lcReminders.InsertColumn(WHEN_COL, CEnString(IDS_REMINDER_WHENCOL), LVCFMT_LEFT, GraphicsMisc::ScaleByDPIFactor(150));

	ListView_SetExtendedListViewStyleEx(m_lcReminders, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
	ListView_SetExtendedListViewStyleEx(m_lcReminders, LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);

	CThemed::SetWindowTheme(&m_lcReminders, _T("Explorer"));

	EnableControls();
	UpdateColumnWidths();

	return TRUE;
}

void CTDLShowReminderDlg::OnDestroy()
{
	RemoveAllListReminders();

	CTDLDialog::OnDestroy();
}

int CTDLShowReminderDlg::GetListReminderCount() const
{
	ASSERT(m_mapReminders.GetCount() == m_lcReminders.GetItemCount());

	return (m_mapReminders.GetCount());
}

BOOL CTDLShowReminderDlg::AddListReminder(const TDCREMINDER& rem)
{
	ASSERT(GetSafeHwnd());

	if (!GetSafeHwnd())
		return FALSE;
	
	int nItem = FindListReminder(rem);
	BOOL bNewReminder = (nItem == -1);

	if (bNewReminder)
	{
		CString sTask(rem.GetTaskTitle()), sParent(rem.GetParentTitle());
		sTask.Replace(_T("&"), _T("&&"));
		sParent.Replace(_T("&"), _T("&&"));
		
		nItem = m_lcReminders.InsertItem(0, sTask);
		ASSERT(nItem != -1);

		if (nItem == -1)
			return FALSE;

		m_lcReminders.SetItemText(nItem, TASKPARENT_COL, sParent);
		m_lcReminders.SetItemText(nItem, TASKLIST_COL, rem.GetTaskListName());
		m_lcReminders.SetItemData(nItem, m_dwNextReminderID);

		m_mapReminders[m_dwNextReminderID] = rem;
		m_dwNextReminderID++;

		// do we need to play a sound?
		if (!rem.sSoundFile.IsEmpty())
			PlaySound(rem.sSoundFile, NULL, (SND_FILENAME | SND_ASYNC));
		
		if ((m_lcReminders.GetItemCount() == 1) || (m_lcReminders.GetSelectedCount() == 0))
			m_lcReminders.SetItemState(nItem, (LVIS_SELECTED | LVIS_FOCUSED), (LVIS_SELECTED | LVIS_FOCUSED));
		
		m_lcReminders.SetFocus();
		UpdateTitleText();
	}

	m_lcReminders.SetItemText(nItem, WHEN_COL, rem.FormatWhenString()); // always
	
	return bNewReminder;
}

BOOL CTDLShowReminderDlg::UpdateListReminder(const TDCREMINDER& rem)
{
	int nItem = FindListReminder(rem);

	if (nItem == -1)
		return FALSE;

	m_lcReminders.SetItemText(nItem, TASKLIST_COL, rem.GetTaskListName());
	m_lcReminders.SetItemText(nItem, WHEN_COL, rem.FormatWhenString());

	return FALSE;
}

void CTDLShowReminderDlg::UpdateTitleText()
{
	SetWindowText(CEnString(IDS_TASKREMINDERDLG_TITLE, m_lcReminders.GetItemCount()));
}

void CTDLShowReminderDlg::RemoveListReminder(const TDCREMINDER& rem)
{
	int nItem = FindListReminder(rem);
	
	if (nItem != -1)
	{
		DWORD dwRemID = m_lcReminders.GetItemData(nItem);

		m_mapReminders.RemoveKey(dwRemID);
		m_lcReminders.DeleteItem(nItem);

		// Hide dialog when it becomes empty
		if (m_lcReminders.GetItemCount() == 0)
			HideWindow();
		
		UpdateTitleText();
	}
}

void CTDLShowReminderDlg::RemoveListReminders(const CFilteredToDoCtrl& tdc)
{
	ASSERT(m_mapReminders.GetCount() == m_lcReminders.GetItemCount());
	
	int nItem = m_lcReminders.GetItemCount();

	while (nItem--)
	{
		DWORD dwRemID = m_lcReminders.GetItemData(nItem);
		TDCREMINDER rem;

		if (m_mapReminders.Lookup(dwRemID, rem) && (rem.pTDC == &tdc))
		{
			m_mapReminders.RemoveKey(dwRemID);
			m_lcReminders.DeleteItem(nItem);
		}
	}

	// Hide dialog when it becomes empty
	if (m_lcReminders.GetItemCount() == 0)
		HideWindow();
	
	UpdateTitleText();
}

void CTDLShowReminderDlg::RemoveAllListReminders()
{
	ASSERT(m_mapReminders.GetCount() == m_lcReminders.GetItemCount());
	
	m_lcReminders.DeleteAllItems();
	m_mapReminders.RemoveAll();
}

int CTDLShowReminderDlg::FindListReminder(const TDCREMINDER& rem)
{
	ASSERT(m_mapReminders.GetCount() == m_lcReminders.GetItemCount());
	
	int nItem = m_lcReminders.GetItemCount();

	while (nItem--)
	{
		DWORD dwRemID = m_lcReminders.GetItemData(nItem);
		TDCREMINDER remItem;

		if (m_mapReminders.Lookup(dwRemID, remItem) && remItem.Matches(rem.pTDC, rem.dwTaskID))
			return nItem;
	}

	// not found
	return -1;
}

void CTDLShowReminderDlg::OnSnooze() 
{
	ASSERT(m_lcReminders.GetSelectedCount());

	SnoozeReminders(FALSE);
}

void CTDLShowReminderDlg::SnoozeReminders(BOOL bAll)
{
	UpdateData();

	// save snooze value for next time
	if (!m_bSnoozeUntil)
		CPreferences().WriteProfileInt(m_sPrefsKey, _T("Snooze"), GetSnoozeMinutes());

	CTDCReminderArray aRem;

	if ((bAll && GetVisibleReminders(aRem)) || (!bAll && GetSelectedReminders(aRem)))
	{
		CAutoFlag af(m_bChangingReminders, TRUE);

		for (int nRem = 0; nRem < aRem.GetSize(); nRem++)
			DoSnoozeReminder(aRem[nRem]);
	}

	UpdateControls();
}


void CTDLShowReminderDlg::OnSnoozeAll() 
{
	ASSERT(m_lcReminders.GetItemCount());
	
	SnoozeReminders(TRUE);
}

int CTDLShowReminderDlg::GetVisibleReminders(CTDCReminderArray& aRem) const
{
	int nRem = m_lcReminders.GetItemCount();
	aRem.SetSize(nRem);

	while (nRem--)
	{
		DWORD dwRemID = m_lcReminders.GetItemData(nRem);
		ASSERT(dwRemID);

		VERIFY(m_mapReminders.Lookup(dwRemID, aRem[nRem]));
	}

	return aRem.GetSize();
}

int CTDLShowReminderDlg::GetVisibleReminders(const CFilteredToDoCtrl& tdc, CTDCReminderArray& aRem) const
{
	int nNumRem = m_lcReminders.GetItemCount(), nItem = 0;
	aRem.SetSize(nNumRem); // max possible

	for (int nRem = 0; nRem < nNumRem; nRem++)
	{
		DWORD dwRemID = m_lcReminders.GetItemData(nRem);
		ASSERT(dwRemID);

		if (m_mapReminders.Lookup(dwRemID, aRem[nItem]) && (aRem[nItem].pTDC == &tdc))
			nItem++;
	}

	aRem.SetSize(nItem);

	return nItem;
}

int CTDLShowReminderDlg::GetSelectedReminder(TDCREMINDER& rem) const
{
	POSITION pos = m_lcReminders.GetFirstSelectedItemPosition();
	int nSel = m_lcReminders.GetNextSelectedItem(pos);

	if (nSel != -1)
	{
		DWORD dwRemID = m_lcReminders.GetItemData(nSel);
		ASSERT(dwRemID);

		if (m_mapReminders.Lookup(dwRemID, rem))
			return nSel;

		// else
		ASSERT(0);
	}

	return -1;
}

int CTDLShowReminderDlg::GetSelectedReminders(CTDCReminderArray& aRem) const
{
	aRem.SetSize(m_lcReminders.GetSelectedCount());
	int nRem = 0;
	
	POSITION pos = m_lcReminders.GetFirstSelectedItemPosition();

	while (pos)
	{
		int nSel = m_lcReminders.GetNextSelectedItem(pos);
		ASSERT(nSel != -1);

		DWORD dwRemID = m_lcReminders.GetItemData(nSel);
		ASSERT(dwRemID);

		if (m_mapReminders.Lookup(dwRemID, aRem[nRem]))
			nRem++;
	}

	return aRem.GetSize();
}

double CTDLShowReminderDlg::GetSnoozeDays() const
{
	return (GetSnoozeMinutes() / ONE_DAY_IN_MINS);
}

COleDateTime CTDLShowReminderDlg::GetSnoozeUntil() const
{
	ASSERT (m_bSnoozeUntil);
	return m_dtSnoozeUntil; 
}

void CTDLShowReminderDlg::OnDismissAndGototask() 
{
	ASSERT(m_lcReminders.GetSelectedCount() == 1);

	TDCREMINDER rem;
	int nSel = GetSelectedReminder(rem);

	if (nSel != -1)
		DoDismissReminder(rem, TRUE);	

	UpdateControls();
}

void CTDLShowReminderDlg::OnDismiss() 
{
	ASSERT(m_lcReminders.GetSelectedCount());

	CTDCReminderArray aRem;

	if (GetSelectedReminders(aRem))
	{
		CAutoFlag af(m_bChangingReminders, TRUE);

		for (int nRem = 0; nRem < aRem.GetSize(); nRem++)
			DoDismissReminder(aRem[nRem], FALSE);	
	}

	UpdateControls();
}

void CTDLShowReminderDlg::OnOK()
{
	// do nothing
}

void CTDLShowReminderDlg::OnCancel()
{
	HideWindow();
}

void CTDLShowReminderDlg::OnClose()
{
	HideWindow();
}

void CTDLShowReminderDlg::OnSnoozeFor() 
{
	EnableControls();
}

void CTDLShowReminderDlg::OnSnoozeUntil() 
{
	EnableControls();
}

void CTDLShowReminderDlg::EnableControls()
{
	UpdateData();
	
	int nNumRem = m_lcReminders.GetItemCount();
	int nNumSel = m_lcReminders.GetSelectedCount();

	GetDlgItem(IDC_SNOOZEFOR)->EnableWindow(nNumRem && !m_bSnoozeUntil);
	GetDlgItem(IDC_SNOOZEUNTILDATE)->EnableWindow(nNumRem && m_bSnoozeUntil);
	GetDlgItem(IDC_SNOOZEUNTILTIME)->EnableWindow(nNumRem && m_bSnoozeUntil);

	GetDlgItem(IDC_SNOOZEOPTIONFOR)->EnableWindow(nNumRem);
	GetDlgItem(IDC_SNOOZEOPTIONUNTIL)->EnableWindow(nNumRem);
	GetDlgItem(IDC_SNOOZEALL)->EnableWindow(nNumRem);

	GetDlgItem(IDC_SNOOZE)->EnableWindow(nNumSel);
	GetDlgItem(IDC_DISMISS)->EnableWindow(nNumSel);
	GetDlgItem(IDC_DISMISSANDGOTOTASK)->EnableWindow(nNumSel == 1);
}

void CTDLShowReminderDlg::UpdateControls()
{
	TDCREMINDER rem;

	if (GetSelectedReminder(rem) != -1)
	{
		UINT nSnooze = ((rem.nLastSnoozeMins > 0) ? rem.nLastSnoozeMins : 5);
		m_cbSnoozeFor.SetSelectedPeriod(nSnooze);
		
		m_dtSnoozeUntil = (COleDateTime::GetCurrentTime().m_dt + (nSnooze / ONE_DAY_IN_MINS));
		m_bSnoozeUntil = (rem.nLastSnoozeMins == 0);

		UpdateData(FALSE);
	}

	EnableControls();
}

void CTDLShowReminderDlg::OnItemchangedReminders(NMHDR* /*pNMHDR*/, LRESULT* pResult) 
{
	if (!m_bChangingReminders)
		UpdateControls();

	*pResult = 0;
}

void CTDLShowReminderDlg::OnDblClkReminders(NMHDR* /*pNMHDR*/, LRESULT* pResult) 
{
	ASSERT(m_lcReminders.GetSelectedCount() == 1);

	TDCREMINDER rem;
	int nSel = GetSelectedReminder(rem);

	if (nSel != -1)
		DoGotoTask(rem);

	*pResult = 0;
}

void CTDLShowReminderDlg::HideWindow()
{
	ShowWindow(SW_HIDE);
}

void CTDLShowReminderDlg::OnRepositionControls(int dx, int dy)
{
	CTDLDialog::OnRepositionControls(dx, dy);

	CDialogHelper::ResizeChild(&m_lcReminders, dx, dy);

	CDialogHelper::ResizeCtrl(this, IDC_DIVIDER, dx, 0);
	CDialogHelper::OffsetCtrl(this, IDC_DIVIDER, 0, dy);

	CDialogHelper::OffsetCtrl(this, IDC_SNOOZE, dx, dy);
	CDialogHelper::OffsetCtrl(this, IDC_SNOOZEALL, dx, dy);
	CDialogHelper::OffsetCtrl(this, IDC_DISMISS, dx, dy);
	CDialogHelper::OffsetCtrl(this, IDC_DISMISSANDGOTOTASK, dx, dy);

	CDialogHelper::OffsetCtrl(this, IDC_SNOOZEOPTIONFOR, 0, dy);
	CDialogHelper::OffsetCtrl(this, IDC_SNOOZEOPTIONUNTIL, 0, dy);
	CDialogHelper::OffsetCtrl(this, IDC_SNOOZEFOR, 0, dy);
	CDialogHelper::OffsetCtrl(this, IDC_SNOOZEUNTILDATE, 0, dy);
	CDialogHelper::OffsetCtrl(this, IDC_SNOOZEUNTILTIME, 0, dy);

	UpdateColumnWidths();
}

void CTDLShowReminderDlg::UpdateColumnWidths()
{
	CRect rAvail;
	m_lcReminders.GetClientRect(rAvail);

	int nCol = (NUM_COLS - 1), nTotalColWidth = 0;

	while (nCol--)
		nTotalColWidth += m_lcReminders.GetColumnWidth(nCol);

	// The 'when' column is essentially of fixed width so we leave it alone
	int nAvailWidth = (rAvail.Width() - m_lcReminders.GetColumnWidth(WHEN_COL));

	double dFactor = (double)nAvailWidth / nTotalColWidth;

	nCol = (NUM_COLS - 1);

	while (nCol--)
		m_lcReminders.SetColumnWidth(nCol, (int)(m_lcReminders.GetColumnWidth(nCol) * dFactor));
}
