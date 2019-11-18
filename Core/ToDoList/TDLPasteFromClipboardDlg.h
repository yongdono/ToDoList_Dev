#if !defined(AFX_TDLPASTEFROMCLIPBOARDDLG_H__EAD04A7C_6F3E_400A_8837_C74897B84054__INCLUDED_)
#define AFX_TDLPASTEFROMCLIPBOARDDLG_H__EAD04A7C_6F3E_400A_8837_C74897B84054__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TDLPasteFromClipboardDlg.h : header file
//

#include "TDLDialog.h"

#include "..\Interfaces\ImportExportComboBox.h"

/////////////////////////////////////////////////////////////////////////////

class CTDCImportExportMgr;

/////////////////////////////////////////////////////////////////////////////
// CTDLPasteFromClipboardDlg dialog

class CTDLPasteFromClipboardDlg : public CTDLDialog
{
// Construction
public:
	CTDLPasteFromClipboardDlg(const CTDCImportExportMgr& mgr, CWnd* pParent = NULL);   // standard constructor

	CString GetClipboardText() const { return m_sClipboardText; }
	CString GetFormatTypeID() const { return m_sFormatTypeID; } 

protected:
// Dialog Data
	//{{AFX_DATA(CTDLPasteFromClipboardDlg)
	CImportExportComboBox	m_cbFormat;
	CString	m_sClipboardText;
	//}}AFX_DATA

	CString	m_sFormatTypeID;
	CFont m_fontMonospace;

protected:
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTDLPasteFromClipboardDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL
	virtual void OnOK();

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CTDLPasteFromClipboardDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnRefresh();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TDLPASTEFROMCLIPBOARDDLG_H__EAD04A7C_6F3E_400A_8837_C74897B84054__INCLUDED_)
