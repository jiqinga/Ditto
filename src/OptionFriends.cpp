// OptionFriends.cpp : implementation file
//

#include "stdafx.h"
#include "cp_main.h"
#include "OptionFriends.h"
#include "FriendDetails.h"
#include "DimWnd.h"
#include "ISyncProvider.h"
#include "DirectFriendProvider.h"
#include "SupabaseRelayProvider.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define EMPTRY_STRING "----"

namespace
{
	class CCloudSyncOptionsSnapshot
	{
	public:
		long m_syncMode;
		CString m_url;
		CString m_anonKey;
		CString m_roomCode;
		CString m_deviceName;
		BOOL m_connectOnStartup;
		BOOL m_allowText;
		BOOL m_allowHtml;
		BOOL m_allowRtf;

		void Capture()
		{
			m_syncMode = CGetSetOptions::GetSyncMode();
			m_url = CGetSetOptions::GetCloudSyncSupabaseUrl();
			m_anonKey = CGetSetOptions::GetCloudSyncSupabaseAnonKey();
			m_roomCode = CGetSetOptions::GetCloudSyncRoomCode();
			m_deviceName = CGetSetOptions::GetCloudSyncDeviceName();
			m_connectOnStartup = CGetSetOptions::GetCloudSyncConnectOnStartup();
			m_allowText = CGetSetOptions::GetCloudSyncAllowText();
			m_allowHtml = CGetSetOptions::GetCloudSyncAllowHtml();
			m_allowRtf = CGetSetOptions::GetCloudSyncAllowRtf();
		}

		void Restore() const
		{
			CGetSetOptions::SetSyncMode(m_syncMode);
			CGetSetOptions::SetCloudSyncSupabaseUrl(m_url);
			CGetSetOptions::SetCloudSyncSupabaseAnonKey(m_anonKey);
			CGetSetOptions::SetCloudSyncRoomCode(m_roomCode);
			CGetSetOptions::SetCloudSyncDeviceName(m_deviceName);
			CGetSetOptions::SetCloudSyncConnectOnStartup(m_connectOnStartup);
			CGetSetOptions::SetCloudSyncAllowText(m_allowText);
			CGetSetOptions::SetCloudSyncAllowHtml(m_allowHtml);
			CGetSetOptions::SetCloudSyncAllowRtf(m_allowRtf);
		}
	};
}

/////////////////////////////////////////////////////////////////////////////
// COptionFriends property page

IMPLEMENT_DYNCREATE(COptionFriends, CPropertyPage)

COptionFriends::COptionFriends() : CPropertyPage(COptionFriends::IDD)
{
	m_csTitle = theApp.m_Language.GetString("FriendsTitle", "Friends");
	m_psp.pszTitle = m_csTitle;
	m_psp.dwFlags |= PSP_USETITLE; 

	//{{AFX_DATA_INIT(COptionFriends)
	m_PlaceOnClipboard = _T("");
	m_csPassword = _T("");
	m_csAdditionalPasswords = _T("");
	m_csCloudSupabaseUrl = _T("");
	m_csCloudSupabaseAnonKey = _T("");
	m_csCloudRoomCode = _T("");
	m_csCloudDeviceName = _T("");
	m_nSyncMode = DITTO_SYNC_MODE_DIRECT_FRIENDS;
	//}}AFX_DATA_INIT
}

COptionFriends::~COptionFriends()
{
}

void COptionFriends::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionFriends)
	DDX_Control(pDX, IDC_CHECK_DISABLE_FRIENDS, m_bDisableRecieve);
	DDX_Control(pDX, IDC_CHECK_LOG_SEND_RECIEVE, m_SendRecieve);
	DDX_Control(pDX, IDC_CHECK_CLOUD_CONNECT_STARTUP, m_chkCloudConnectOnStartup);
	DDX_Control(pDX, IDC_CHECK_CLOUD_ALLOW_TEXT, m_chkCloudAllowText);
	DDX_Control(pDX, IDC_CHECK_CLOUD_ALLOW_HTML, m_chkCloudAllowHtml);
	DDX_Control(pDX, IDC_CHECK_CLOUD_ALLOW_RTF, m_chkCloudAllowRtf);
	DDX_Control(pDX, IDC_COMBO_SYNC_MODE, m_cbSyncMode);
	DDX_Control(pDX, IDC_LIST, m_List);
	DDX_Text(pDX, IDC_EDIT_PLACE_ON_CLIPBOARD, m_PlaceOnClipboard);
	DDX_Text(pDX, IDC_EDIT_PASSWORD, m_csPassword);
	DDX_Text(pDX, IDC_EDIT_ADDITIONAL, m_csAdditionalPasswords);
	DDX_Text(pDX, IDC_EDIT_CLOUD_URL, m_csCloudSupabaseUrl);
	DDX_Text(pDX, IDC_EDIT_CLOUD_ANON_KEY, m_csCloudSupabaseAnonKey);
	DDX_Text(pDX, IDC_EDIT_CLOUD_ROOM_CODE, m_csCloudRoomCode);
	DDX_Text(pDX, IDC_EDIT_CLOUD_DEVICE_NAME, m_csCloudDeviceName);
	DDX_CBIndex(pDX, IDC_COMBO_SYNC_MODE, m_nSyncMode);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionFriends, CPropertyPage)
	//{{AFX_MSG_MAP(COptionFriends)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST, OnDblclkList)
	ON_NOTIFY(LVN_KEYDOWN, IDC_LIST, OnKeydownList)
	ON_BN_CLICKED(IDC_CHECK_DISABLE_FRIENDS, OnCheckDisableFriends)
	ON_CBN_SELCHANGE(IDC_COMBO_SYNC_MODE, OnSelchangeSyncMode)
	ON_BN_CLICKED(IDC_BUTTON_TEST_SYNC, OnTestConnection)
	ON_BN_CLICKED(IDC_BUTTON_RESET_SYNC_STATE, OnResetSyncState)
	//}}AFX_MSG_MAP 
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionFriends message handlers


BOOL COptionFriends::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();

	m_pParent = (COptionsSheet *)GetParent();
	
	m_List.SetExtendedStyle(LVS_EX_FULLROWSELECT);
	InitListCtrlCols();
	PopulateSyncModes();

	InsertItems();

	m_SendRecieve.SetCheck(CGetSetOptions::GetLogSendReceiveErrors());
	m_bDisableRecieve.SetCheck(CGetSetOptions::GetDisableRecieve());

	m_PlaceOnClipboard = CGetSetOptions::m_csIPListToPutOnClipboard;
	m_csPassword = CGetSetOptions::m_csPassword;
	m_csAdditionalPasswords = CGetSetOptions::GetExtraNetworkPassword(false);
	m_csCloudSupabaseUrl = CGetSetOptions::GetCloudSyncSupabaseUrl();
	m_csCloudSupabaseAnonKey = CGetSetOptions::GetCloudSyncSupabaseAnonKey();
	m_csCloudRoomCode = CGetSetOptions::GetCloudSyncRoomCode();
	m_csCloudDeviceName = CGetSetOptions::GetCloudSyncDeviceName();
	m_nSyncMode = (int)CGetSetOptions::GetSyncMode();

	m_chkCloudConnectOnStartup.SetCheck(CGetSetOptions::GetCloudSyncConnectOnStartup());
	m_chkCloudAllowText.SetCheck(CGetSetOptions::GetCloudSyncAllowText());
	m_chkCloudAllowHtml.SetCheck(CGetSetOptions::GetCloudSyncAllowHtml());
	m_chkCloudAllowRtf.SetCheck(CGetSetOptions::GetCloudSyncAllowRtf());

	if(CGetSetOptions::GetRequestFilesUsingIP())
	{
		::CheckDlgButton(m_hWnd, IDC_RADIO_USE_IP, BST_CHECKED);
	}
	else
	{
		::CheckDlgButton(m_hWnd, IDC_RADIO_USE_HOST_NAME, BST_CHECKED);
	}

	UpdateData(FALSE);
	UpdateModeControls();
	OnCheckDisableFriends();

	theApp.m_Language.UpdateOptionFriends(this);
		
	return FALSE;
}

BOOL COptionFriends::OnApply() 
{
	UpdateData();

	CSendClients client;
	for (int i = 0; i < MAX_SEND_CLIENTS; i++)
	{
		if(m_List.GetItemText(i, 1) == "X")
			client.bSendAll = TRUE;
		else
			client.bSendAll = FALSE;

		client.csIP = m_List.GetItemText(i, 2);
		if(client.csIP == EMPTRY_STRING)
			client.csIP = "";
		
		client.csDescription = m_List.GetItemText(i, 3);
		if(client.csDescription == EMPTRY_STRING)
			client.csDescription = "";

		CGetSetOptions::SetSendClients(client, i);
	}

	CGetSetOptions::SetSyncMode(m_nSyncMode);
	CGetSetOptions::SetCloudSyncSupabaseUrl(m_csCloudSupabaseUrl);
	CGetSetOptions::SetCloudSyncSupabaseAnonKey(m_csCloudSupabaseAnonKey);
	CGetSetOptions::SetCloudSyncRoomCode(m_csCloudRoomCode);
	CGetSetOptions::SetCloudSyncDeviceName(m_csCloudDeviceName);
	CGetSetOptions::SetCloudSyncConnectOnStartup(m_chkCloudConnectOnStartup.GetCheck());
	CGetSetOptions::SetCloudSyncAllowText(m_chkCloudAllowText.GetCheck());
	CGetSetOptions::SetCloudSyncAllowHtml(m_chkCloudAllowHtml.GetCheck());
	CGetSetOptions::SetCloudSyncAllowRtf(m_chkCloudAllowRtf.GetCheck());

	CGetSetOptions::SetNetworkPassword(m_csPassword);
	CGetSetOptions::SetLogSendReceiveErrors(m_SendRecieve.GetCheck());
	CGetSetOptions::SetDisableRecieve(m_bDisableRecieve.GetCheck());
	CGetSetOptions::SetListToPutOnClipboard(m_PlaceOnClipboard);
	CGetSetOptions::SetExtraNetworkPassword(m_csAdditionalPasswords);
	CGetSetOptions::GetExtraNetworkPassword(true);
	CGetSetOptions::GetClientSendCount();

	if(::IsDlgButtonChecked(m_hWnd, IDC_RADIO_USE_IP) == BST_CHECKED)
	{
		CGetSetOptions::SetRequestFilesUsingIP(TRUE);
	}
	else if(::IsDlgButtonChecked(m_hWnd, IDC_RADIO_USE_HOST_NAME) == BST_CHECKED)
	{
		CGetSetOptions::SetRequestFilesUsingIP(FALSE);
	}

	theApp.StartStopServerThread();
	
	return CPropertyPage::OnApply();
}


void COptionFriends::InitListCtrlCols()
{
	// Insert some columns
	m_List.InsertColumn(0, _T(""), LVCFMT_LEFT, 25);

	m_List.InsertColumn(1, theApp.m_Language.GetString("Send_All_Copies", "Send All Copies"), LVCFMT_LEFT, 90);
	m_List.InsertColumn(2, theApp.m_Language.GetString("IP_Name", "IP/Name"), LVCFMT_LEFT, 135);
	m_List.InsertColumn(3, theApp.m_Language.GetString("Descriptions", "Descriptions"), LVCFMT_LEFT, 157);
}

void COptionFriends::InsertItems()
{
	// Delete the current contents
	m_List.DeleteAllItems();

	// Use the LV_ITEM structure to insert the items
	LVITEM lvi;
	CString strItem;
	for (int i = 0; i < MAX_SEND_CLIENTS; i++)
	{
		// Insert the first item
		lvi.mask =  LVIF_TEXT;
	
		lvi.iItem = i;

//-------------------------------------------------------------------

		strItem.Format(_T("%d"), i+1);

		lvi.iSubItem = 0;
		lvi.pszText = (LPTSTR)(LPCTSTR)(strItem);
		m_List.InsertItem(&lvi);

//-------------------------------------------------------------------
		if(CGetSetOptions::m_SendClients[i].bSendAll)
			strItem = "X";
		else
			strItem = EMPTRY_STRING;

		m_List.SetItemText(i, 1, strItem);

//-------------------------------------------------------------------

		strItem = CGetSetOptions::m_SendClients[i].csIP;
		if(CGetSetOptions::m_SendClients[i].csIP.GetLength() <= 0)
		{
			strItem = EMPTRY_STRING;
		}

		m_List.SetItemText(i, 2, strItem);

//-------------------------------------------------------------------

		strItem = CGetSetOptions::m_SendClients[i].csDescription;
		if(CGetSetOptions::m_SendClients[i].csDescription.GetLength() <= 0)
		{
			strItem = EMPTRY_STRING;
		}
		
		m_List.SetItemText(i, 3, strItem);
	}
}

void COptionFriends::OnDblclkList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	POSITION pos = m_List.GetFirstSelectedItemPosition();
	if(pos)
	{
		int nItem = m_List.GetNextSelectedItem(pos);

		EditItem(nItem);
	}
	
	*pResult = 0;
}

BOOL COptionFriends::EditItem(int nItem)
{
	CDimWnd dim(this->GetParent());

	CFriendDetails dlg(this);

	if(m_List.GetItemText(nItem, 1) == "X")
		dlg.m_checkSendAll = TRUE;
	else
		dlg.m_checkSendAll = FALSE;

	dlg.m_csIP = m_List.GetItemText(nItem, 2);
	if(dlg.m_csIP == EMPTRY_STRING)
		dlg.m_csIP = "";
	dlg.m_csDescription = m_List.GetItemText(nItem, 3);
	if(dlg.m_csDescription == EMPTRY_STRING)
		dlg.m_csDescription = "";

	if(dlg.DoModal() == IDOK)
	{
		if(dlg.m_checkSendAll)
		{
			m_List.SetItemText(nItem, 1, _T("X"));
		}
		else
			m_List.SetItemText(nItem, 1, _T(""));

		m_List.SetItemText(nItem, 2, dlg.m_csIP);
		m_List.SetItemText(nItem, 3, dlg.m_csDescription);

		return TRUE;
	}

	return FALSE;
}

void COptionFriends::OnKeydownList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_KEYDOWN* pLVKeyDow = (LV_KEYDOWN*)pNMHDR;

	switch (pLVKeyDow->wVKey)
	{
	case VK_DELETE:
		{
			POSITION pos = m_List.GetFirstSelectedItemPosition();
			if(pos)
			{
				int nItem = m_List.GetNextSelectedItem(pos);
				m_List.SetItemText(nItem, 1, _T(EMPTRY_STRING));
				m_List.SetItemText(nItem, 2, _T(EMPTRY_STRING));
				m_List.SetItemText(nItem, 3, _T(EMPTRY_STRING));
			}
		}
		break;
	case VK_RETURN:
		{
			POSITION pos = m_List.GetFirstSelectedItemPosition();
			if(pos)
			{
				int nItem = m_List.GetNextSelectedItem(pos);
				EditItem(nItem);
			}
		}
		break;
	}
	
	
	*pResult = 0;
}

void COptionFriends::OnCheckDisableFriends() 
{
	if(m_bDisableRecieve.GetCheck() == BST_CHECKED)
		GetDlgItem(IDC_EDIT_PLACE_ON_CLIPBOARD)->EnableWindow(FALSE);
	else
		GetDlgItem(IDC_EDIT_PLACE_ON_CLIPBOARD)->EnableWindow(TRUE);
}

void COptionFriends::PopulateSyncModes()
{
	m_cbSyncMode.ResetContent();
	m_cbSyncMode.AddString(theApp.m_Language.GetString("SyncModeDirectFriends", "Direct Friends"));
	m_cbSyncMode.AddString(theApp.m_Language.GetString("SyncModeCloudRelay", "Cloud Relay"));
	m_cbSyncMode.AddString(theApp.m_Language.GetString("SyncModeDisabled", "Disabled"));

	if (m_nSyncMode < DITTO_SYNC_MODE_DIRECT_FRIENDS || m_nSyncMode > DITTO_SYNC_MODE_DISABLED)
	{
		m_nSyncMode = DITTO_SYNC_MODE_DIRECT_FRIENDS;
	}

	m_cbSyncMode.SetCurSel(m_nSyncMode);
}

void COptionFriends::ShowDirectControls(BOOL show)
{
	const int ids[] =
	{
		IDC_EDIT_PLACE_ON_CLIPBOARD,
		IDC_CHECK_DISABLE_FRIENDS,
		IDC_EDIT_PASSWORD,
		IDC_EDIT_ADDITIONAL,
		IDC_LIST,
		IDC_CHECK_LOG_SEND_RECIEVE,
		IDC_STATIC_1,
		IDC_STATIC_2,
		IDC_STATIC_3,
		IDC_STATIC_4,
		IDC_STATIC_5,
		IDC_STATIC_6,
		IDC_RADIO_USE_IP,
		IDC_RADIO_USE_HOST_NAME,
		IDC_STATIC_REMOTE_FILES
	};

	for (int i = 0; i < sizeof(ids) / sizeof(ids[0]); i++)
	{
		CWnd *pWnd = GetDlgItem(ids[i]);
		if (pWnd)
		{
			pWnd->ShowWindow(show ? SW_SHOW : SW_HIDE);
		}
	}
}

void COptionFriends::ShowCloudControls(BOOL show)
{
	const int ids[] =
	{
		IDC_STATIC_CLOUD_GROUP,
		IDC_STATIC_CLOUD_URL,
		IDC_EDIT_CLOUD_URL,
		IDC_STATIC_CLOUD_ANON_KEY,
		IDC_EDIT_CLOUD_ANON_KEY,
		IDC_STATIC_CLOUD_ROOM_CODE,
		IDC_EDIT_CLOUD_ROOM_CODE,
		IDC_STATIC_CLOUD_DEVICE_NAME,
		IDC_EDIT_CLOUD_DEVICE_NAME,
		IDC_CHECK_CLOUD_CONNECT_STARTUP,
		IDC_STATIC_CLOUD_CONTENT,
		IDC_CHECK_CLOUD_ALLOW_TEXT,
		IDC_CHECK_CLOUD_ALLOW_HTML,
		IDC_CHECK_CLOUD_ALLOW_RTF,
		IDC_BUTTON_TEST_SYNC,
		IDC_BUTTON_RESET_SYNC_STATE
	};

	for (int i = 0; i < sizeof(ids) / sizeof(ids[0]); i++)
	{
		CWnd *pWnd = GetDlgItem(ids[i]);
		if (pWnd)
		{
			pWnd->ShowWindow(show ? SW_SHOW : SW_HIDE);
		}
	}
}

void COptionFriends::UpdateModeControls()
{
	UpdateData(TRUE);

	BOOL showDirect = (m_nSyncMode == DITTO_SYNC_MODE_DIRECT_FRIENDS);
	BOOL showCloud = (m_nSyncMode == DITTO_SYNC_MODE_CLOUD_RELAY);

	ShowDirectControls(showDirect);
	ShowCloudControls(showCloud);

	CWnd *pTestButton = GetDlgItem(IDC_BUTTON_TEST_SYNC);
	if (pTestButton)
	{
		pTestButton->EnableWindow(showCloud);
	}

	CWnd *pResetButton = GetDlgItem(IDC_BUTTON_RESET_SYNC_STATE);
	if (pResetButton)
	{
		pResetButton->EnableWindow(showCloud);
	}

	if (showDirect)
	{
		OnCheckDisableFriends();
	}
}

ISyncProvider *COptionFriends::CreateSelectedProvider(CString &errorMessage)
{
	UpdateData(TRUE);

	CCloudSyncOptionsSnapshot previousOptions;
	previousOptions.Capture();

	CGetSetOptions::SetSyncMode(m_nSyncMode);
	CGetSetOptions::SetCloudSyncSupabaseUrl(m_csCloudSupabaseUrl);
	CGetSetOptions::SetCloudSyncSupabaseAnonKey(m_csCloudSupabaseAnonKey);
	CGetSetOptions::SetCloudSyncRoomCode(m_csCloudRoomCode);
	CGetSetOptions::SetCloudSyncDeviceName(m_csCloudDeviceName);
	CGetSetOptions::SetCloudSyncConnectOnStartup(m_chkCloudConnectOnStartup.GetCheck());
	CGetSetOptions::SetCloudSyncAllowText(m_chkCloudAllowText.GetCheck());
	CGetSetOptions::SetCloudSyncAllowHtml(m_chkCloudAllowHtml.GetCheck());
	CGetSetOptions::SetCloudSyncAllowRtf(m_chkCloudAllowRtf.GetCheck());

	ISyncProvider *pProvider = NULL;
	if (m_nSyncMode == DITTO_SYNC_MODE_DIRECT_FRIENDS)
	{
		pProvider = new CDirectFriendProvider();
	}
	else if (m_nSyncMode == DITTO_SYNC_MODE_CLOUD_RELAY)
	{
		pProvider = new CSupabaseRelayProvider();
	}
	else
	{
		errorMessage = theApp.m_Language.GetString("SyncDisabled", "Sync is disabled.");
	}

	if (pProvider)
	{
		pProvider->InitializeFromOptions();
	}

	previousOptions.Restore();
	return pProvider;
}

void COptionFriends::OnSelchangeSyncMode() 
{
	m_nSyncMode = m_cbSyncMode.GetCurSel();
	UpdateModeControls();
}

void COptionFriends::OnTestConnection() 
{
	UpdateData(TRUE);

	if (m_nSyncMode != DITTO_SYNC_MODE_CLOUD_RELAY)
	{
		MessageBox(theApp.m_Language.GetString("CloudRelayTestOnly", "Connection testing is only available for Cloud Relay mode."), _T("Ditto"), MB_OK);
		return;
	}

	CString url = m_csCloudSupabaseUrl;
	url.Trim();
	CString anonKey = m_csCloudSupabaseAnonKey;
	anonKey.Trim();
	CString roomCode = m_csCloudRoomCode;
	roomCode.Trim();
	if (url.IsEmpty() || anonKey.IsEmpty() || roomCode.IsEmpty())
	{
		MessageBox(theApp.m_Language.GetString("CloudRelayTestRequiredFields", "Enter Supabase URL, Anon Key, and Room Code before testing the connection."), _T("Ditto"), MB_OK);
		return;
	}

	CString errorMessage;
	ISyncProvider *pProvider = CreateSelectedProvider(errorMessage);
	if (pProvider == NULL)
	{
		MessageBox(errorMessage, _T("Ditto"), MB_OK);
		return;
	}

	if (pProvider->SupportsConnectionTest() == false)
	{
		MessageBox(theApp.m_Language.GetString("SyncModeNoConnectionTest", "The selected sync mode does not support connection testing."), _T("Ditto"), MB_OK);
		delete pProvider;
		return;
	}

	if (pProvider->TestConnection(errorMessage))
	{
		MessageBox(theApp.m_Language.GetString("CloudRelayTestSucceeded", "Connection test succeeded."), _T("Ditto"), MB_OK);
	}
	else
	{
		MessageBox(errorMessage, _T("Ditto"), MB_OK);
	}

	delete pProvider;
}

void COptionFriends::OnResetSyncState() 
{
	UpdateData(TRUE);

	if (m_nSyncMode != DITTO_SYNC_MODE_CLOUD_RELAY)
	{
		MessageBox(theApp.m_Language.GetString("CloudRelayResetOnly", "Sync state reset is only available for Cloud Relay mode."), _T("Ditto"), MB_OK);
		return;
	}

	CString roomCode = m_csCloudRoomCode;
	roomCode.Trim();
	if (roomCode.IsEmpty())
	{
		MessageBox(theApp.m_Language.GetString("CloudRelayResetRequiredFields", "Enter Room Code before resetting cloud sync state."), _T("Ditto"), MB_OK);
		return;
	}

	CString errorMessage;
	ISyncProvider *pProvider = CreateSelectedProvider(errorMessage);
	if (pProvider == NULL)
	{
		MessageBox(errorMessage, _T("Ditto"), MB_OK);
		return;
	}

	if (pProvider->ResetSyncState(errorMessage))
	{
		MessageBox(theApp.m_Language.GetString("CloudRelayResetSucceeded", "Local sync state was reset."), _T("Ditto"), MB_OK);
	}
	else
	{
		MessageBox(errorMessage, _T("Ditto"), MB_OK);
	}

	delete pProvider;
}
