#include "stdafx.h"
#include "TDLStatusBar.h"
#include "ToDoItem.h"
#include "resource.h"
#include "FilteredToDoCtrl.h"

#include "..\shared\Localizer.h"
#include "..\shared\Misc.h"
#include "..\shared\GraphicsMisc.h"

#include "..\interfaces\UIThemeFile.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////

CTDLStatusBarProgressProxy::CTDLStatusBarProgressProxy(CTDLStatusBar& statusBar, const CString& sPrompt)
	:
	CStatusBarProgressProxy(&statusBar.m_progress, statusBar.GetSafeHwnd(), sPrompt, 0, TRUE)
{
}

CTDLStatusBarProgressProxy::~CTDLStatusBarProgressProxy()
{
}

/////////////////////////////////////////////////////////////////////////////

static SBACTPANEINFO SB_PANES[] =
{
	{ ID_SB_SELTASKTITLE,	MAKEINTRESOURCE(IDS_SB_SELTASKTITLE_TIP), SBACTF_STRETCHY | SBACTF_RESOURCETIP },
	{ ID_SB_TASKCOUNT,		MAKEINTRESOURCE(IDS_SB_TASKCOUNT_TIP), SBACTF_AUTOFIT | SBACTF_RESOURCETIP },
	//{ ID_SB_SPACER }, 
	{ ID_SB_SELTIMEEST,		MAKEINTRESOURCE(IDS_SB_SELTIMEEST_TIP), SBACTF_AUTOFIT | SBACTF_RESOURCETIP },
	{ ID_SB_SELTIMESPENT,	MAKEINTRESOURCE(IDS_SB_SELTIMESPENT_TIP), SBACTF_AUTOFIT | SBACTF_RESOURCETIP },
	{ ID_SB_SELCOST,		MAKEINTRESOURCE(IDS_SB_SELCOST_TIP), SBACTF_AUTOFIT | SBACTF_RESOURCETIP },
	//{ ID_SB_SPACER }, 
	{ ID_SB_FOCUS,			MAKEINTRESOURCE(IDS_SB_FOCUS_TIP), SBACTF_AUTOFIT | SBACTF_RESOURCETIP },
};

static int SB_PANECOUNT = sizeof(SB_PANES) / sizeof(SBACTPANEINFO);

/////////////////////////////////////////////////////////////////////////////
// CTDLStatusBar 

CTDLStatusBar::CTDLStatusBar(const TODOITEM& tdiDefault)
	:
	m_tdiDefault(tdiDefault),
	m_iSelTaskIcon(-1),
	m_hilTaskIcons(NULL)
{
}


CTDLStatusBar::~CTDLStatusBar()
{
}

/////////////////////////////////////////////////////////////////////////////
// CWelcomeWizard

BEGIN_MESSAGE_MAP(CTDLStatusBar, CStatusBarACTEx)
	//{{AFX_MSG_MAP(CWelcomeWizard)
	//}}AFX_MSG_MAP
	ON_WM_CREATE()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWelcomeWizard message handlers

int CTDLStatusBar::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CStatusBarACTEx::OnCreate(lpCreateStruct) == -1)
		return -1;

	// prevent translation because we handle it manually
	CLocalizer::EnableTranslation(*this, FALSE);

	for (int nPane = 0; nPane < SB_PANECOUNT; nPane++)
		VERIFY(CString().LoadString(SB_PANES[nPane].nID));

	if (!SetPanes(SB_PANES, SB_PANECOUNT))
		return -1;

	// Translate tooltips
	if (CLocalizer::IsInitialized())
	{
		int nPane = SB_PANECOUNT;

		while (nPane--)
			SetPaneTooltip(SB_PANES[nPane].nID, CEnString((UINT)SB_PANES[nPane].lpszTip));
	}

	return 0;
}

void CTDLStatusBar::SetUITheme(const CUIThemeFile& theme)
{
	SetUIColors(theme.crStatusBarLight,
				theme.crStatusBarDark,
				theme.crStatusBarText,
				theme.HasGradient(),
				theme.HasGlass());
}

BOOL CTDLStatusBar::DrawPaneText(CDC* pDC, int nPane, int nOffset)
{
	if ((nPane == 0) && m_hilTaskIcons && (m_iSelTaskIcon != -1) && !m_progress.IsActive())
	{
		ImageList_Draw(m_hilTaskIcons, m_iSelTaskIcon, *pDC, nOffset, 1, ILD_TRANSPARENT);
	
		nOffset += GraphicsMisc::ScaleByDPIFactor(16 + 4);
	}

	return CStatusBarACTEx::DrawPaneText(pDC, nPane, nOffset);
}

void CTDLStatusBar::UpdateTaskTotals(const CFilteredToDoCtrl& tdc)
{
	if (m_progress.IsActive())
		return;

	UINT nVisibleCount = 0;
	UINT nTotalCount = tdc.GetTaskCount(&nVisibleCount);

	SetPaneTextAndTooltip(ID_SB_TASKCOUNT, 
						  0, 
						  Misc::Format(_T("%ld / %ld"), nVisibleCount, nTotalCount), 
						  IDS_SB_TASKCOUNT_TIP);
}

void CTDLStatusBar::UpdateTaskSelection(const CFilteredToDoCtrl& tdc, const  CTDCAttributeMap& mapAttrib)
{
	if (m_progress.IsActive())
		return;

	// Task path
	CString sTextValue, sTipValue;
	UINT nIDTextFormat = 0, nIDTipFormat = 0;

	int nSelCount = tdc.GetSelectedCount();

	if (WantUpdateAttribute(TDCA_TASKNAME, mapAttrib))
	{
		switch (nSelCount)
		{
		case 0:
			nIDTextFormat = ID_SB_NOSELTASK;
			break;

		case 1:
			sTextValue = Misc::Format(_T("%s (%ld)"), tdc.GetSelectedTaskPath(TRUE), tdc.GetSelectedTaskID());
			nIDTipFormat = IDS_SB_SELTASKTITLE_TIP;
			break;

		default: // > 1
			nIDTextFormat = ID_SB_MULTISELTASK;
			sTipValue = tdc.FormatSelectedTaskTitles(TRUE, '\n', 10);

			if (nSelCount > 10)
			{
				sTipValue += _T("\n+ ");
				sTipValue += CEnString(ID_SB_MULTISELTASKMORE_TIP, (nSelCount - 10));
			}
			break;
		}
		SetPaneTextAndTooltip(ID_SB_SELTASKTITLE, nIDTextFormat, sTextValue, nIDTipFormat, sTipValue);
	}

	if (WantUpdateAttribute(TDCA_ICON, mapAttrib))
	{
		int nIcon = -1;

		if (nSelCount == 1)
			nIcon = tdc.GetTaskIconIndex(tdc.GetSelectedTaskID());

		if (nIcon != m_iSelTaskIcon)
		{
			m_iSelTaskIcon = nIcon;
			Invalidate();
		}

		if (m_iSelTaskIcon != -1)
			m_hilTaskIcons = tdc.GetTaskIconImageList();
		else
			m_hilTaskIcons = NULL;
	}

	// Cost
	if (WantUpdateAttribute(TDCA_COST, mapAttrib))
	{
		if (nSelCount && tdc.IsColumnShowing(TDCC_COST))
		{
			double dValue = tdc.CalcSelectedTaskCost();
			sTextValue = Misc::Format(dValue, 2);

			nIDTextFormat = ID_SB_SELCOST;
			nIDTipFormat = IDS_SB_SELCOST_TIP;
		}
		else
		{
			sTextValue.Empty();
			nIDTextFormat = nIDTipFormat = 0;
		}

		SetPaneTextAndTooltip(ID_SB_SELCOST, nIDTextFormat, sTextValue, nIDTipFormat);
	}

	// Time estimate
	if (WantUpdateAttribute(TDCA_TIMEESTIMATE, mapAttrib))
	{
		if (tdc.IsColumnShowing(TDCC_TIMEESTIMATE) && nSelCount)
		{
			double dValue = tdc.CalcSelectedTaskTimeEstimate(m_tdiDefault.timeEstimate.nUnits);
			sTextValue = FormatTime(dValue, m_tdiDefault.timeEstimate.nUnits, tdc.HasStyle(TDCS_DISPLAYHMSTIMEFORMAT));

			nIDTextFormat = ID_SB_SELTIMEEST;
			nIDTipFormat = IDS_SB_SELTIMEEST_TIP;
		}
		else
		{
			sTextValue.Empty();
			nIDTextFormat = nIDTipFormat = 0;
		}

		SetPaneTextAndTooltip(ID_SB_SELTIMEEST, nIDTextFormat, sTextValue, nIDTipFormat);
	}

	// Time spent
	if (WantUpdateAttribute(TDCA_TIMESPENT, mapAttrib))
	{
		if (tdc.IsColumnShowing(TDCC_TIMESPENT) && nSelCount)
		{
			double dValue = tdc.CalcSelectedTaskTimeSpent(m_tdiDefault.timeSpent.nUnits);
			sTextValue = FormatTime(dValue, m_tdiDefault.timeSpent.nUnits, tdc.HasStyle(TDCS_DISPLAYHMSTIMEFORMAT));

			nIDTextFormat = ID_SB_SELTIMESPENT;
			nIDTipFormat = IDS_SB_SELTIMESPENT_TIP;
		}
		else
		{
			sTextValue.Empty();
			nIDTextFormat = nIDTipFormat = 0;
		}

		SetPaneTextAndTooltip(ID_SB_SELTIMESPENT, nIDTextFormat, sTextValue, nIDTipFormat);
	}
}

BOOL CTDLStatusBar::WantUpdateAttribute(TDC_ATTRIBUTE nAttribID, const CTDCAttributeMap& mapAttrib)
{
	return (mapAttrib.Has(TDCA_ALL) || mapAttrib.Has(nAttribID));
}

void CTDLStatusBar::UpdateFocusedControl(const CString& sFocus)
{
	if (m_progress.IsActive())
		return;

	SetPaneTextAndTooltip(ID_SB_FOCUS, 0, sFocus, IDS_SB_FOCUS_TIP);
}

CString CTDLStatusBar::FormatTime(double dAmount, TDC_UNITS nUnits, BOOL bHMS)
{
	TH_UNITS nTHUnits = TDC::MapUnitsToTHUnits(nUnits);

	if (bHMS)
		return CTimeHelper().FormatTimeHMS(dAmount, nTHUnits);

	// else
	return CTimeHelper().FormatTime(dAmount, nTHUnits, 2);
}

void CTDLStatusBar::SetPaneTextAndTooltip(UINT nIDPane, UINT nIDTextFormat, const CString& sTextValue, UINT nIDTipFormat, LPCTSTR szTipValue)
{
	CEnString sText, sTooltip;

	if (nIDTextFormat)
		sText.Format(nIDTextFormat, sTextValue);
	else
		sText = sTextValue;

	if (nIDTipFormat)
	{
		if (szTipValue)
			sTooltip.Format(nIDTipFormat, szTipValue);
		else
			sTooltip.LoadString(nIDTipFormat);
	}
	else if (!Misc::IsEmpty(szTipValue))
	{
		sTooltip = szTipValue;
	}

	int nPane = CommandToIndex(nIDPane);
	ASSERT(nPane != -1);

	SetPaneText(nPane, sText);
	SetPaneTooltipIndex(nPane, sTooltip);
}