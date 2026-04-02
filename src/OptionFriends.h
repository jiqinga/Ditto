#if !defined(AFX_OPTIONFRIENDS_H__E44847C3_54CA_4053_9647_349405B64DF9__INCLUDED_)
#define AFX_OPTIONFRIENDS_H__E44847C3_54CA_4053_9647_349405B64DF9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// OptionFriends.h : header file
//
#include "OptionsSheet.h"
#include "Options.h"

class ISyncProvider;

/////////////////////////////////////////////////////////////////////////////
// COptionFriends dialog

class COptionFriends : public CPropertyPage
{
	DECLARE_DYNCREATE(COptionFriends)

// Construction
public:
	COptionFriends();
	~COptionFriends();

// Dialog Data
	//{{AFX_DATA(COptionFriends)
	enum { IDD = IDD_OPTIONS_FRIENDS };
	CButton	m_bDisableRecieve;
	CButton	m_SendRecieve;
	CButton	m_chkCloudConnectOnStartup;
	CButton	m_chkCloudAllowText;
	CButton	m_chkCloudAllowHtml;
	CButton	m_chkCloudAllowRtf;
	CComboBox	m_cbSyncMode;
	CListCtrl	m_List;
	CString	m_PlaceOnClipboard;
	CString	m_csPassword;
	CString	m_csAdditionalPasswords;
	CString	m_csCloudSupabaseUrl;
	CString	m_csCloudSupabaseAnonKey;
	CString	m_csCloudRoomCode;
	CString	m_csCloudDeviceName;
	int		m_nSyncMode;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COptionFriends)
	public:
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	void InitListCtrlCols();
	void InsertItems();
	BOOL EditItem(int nItem);
	void PopulateSyncModes();
	void UpdateModeControls();
	void ShowDirectControls(BOOL show);
	void ShowCloudControls(BOOL show);
	ISyncProvider *CreateSelectedProvider(CString &errorMessage);

	CString m_csTitle;
	COptionsSheet *m_pParent;
	
	// Generated message map functions
	//{{AFX_MSG(COptionFriends)
	virtual BOOL OnInitDialog();
	afx_msg void OnDblclkList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKeydownList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnCheckDisableFriends();
	afx_msg void OnSelchangeSyncMode();
	afx_msg void OnTestConnection();
	afx_msg void OnResetSyncState();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONFRIENDS_H__E44847C3_54CA_4053_9647_349405B64DF9__INCLUDED_)
