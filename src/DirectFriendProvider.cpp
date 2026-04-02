#include "stdafx.h"
#include "DirectFriendProvider.h"
#include "CP_Main.h"
#include "Client.h"
#include "Misc.h"
#include "Options.h"
#include "Server.h"

CDirectFriendProvider::CDirectFriendProvider()
{
}

CDirectFriendProvider::~CDirectFriendProvider()
{
}

DittoSyncProviderType CDirectFriendProvider::GetProviderType() const
{
	return DITTO_SYNC_PROVIDER_DIRECT_FRIENDS;
}

void CDirectFriendProvider::InitializeFromOptions()
{
}

bool CDirectFriendProvider::StartBackgroundSync(HWND mainHwnd)
{
	if (CGetSetOptions::GetDisableRecieve() == FALSE && CGetSetOptions::GetAllowFriends())
	{
		AfxBeginThread(MTServerThread, mainHwnd);
		return true;
	}

	theApp.m_bExitServerThread = true;
	closesocket(theApp.m_sSocket);
	return false;
}

void CDirectFriendProvider::StopBackgroundSync()
{
	theApp.m_bExitServerThread = true;
	closesocket(theApp.m_sSocket);
}

bool CDirectFriendProvider::SendClips(CClipList *pClipList)
{
	return SendToConfiguredClients(pClipList);
}

bool CDirectFriendProvider::SupportsConnectionTest() const
{
	return false;
}

bool CDirectFriendProvider::TestConnection(CString &errorMessage)
{
	errorMessage = _T("Direct friends mode does not support a generic connection test.");
	return false;
}

bool CDirectFriendProvider::ResetSyncState(CString &errorMessage)
{
	errorMessage = _T("Direct friends mode does not use cloud sync state.");
	return false;
}

bool CDirectFriendProvider::SendToConfiguredClients(CClipList *pClipList)
{
	LogSendRecieveInfo("@@@@@@@@@@@@@@@ - START OF SendClientThread - @@@@@@@@@@@@@@@");

	if(pClipList == NULL)
	{
		LogSendRecieveInfo("ERROR if(pClipList == NULL)");
		return FALSE;
	}

	INT_PTR lCount = pClipList->GetCount();

	LogSendRecieveInfo(StrF(_T("Start of Send ClientThread Count - %d"), lCount));

	for(int nClient = 0; nClient < MAX_SEND_CLIENTS; nClient++)
	{
		if(CGetSetOptions::m_SendClients[nClient].bSendAll &&
			CGetSetOptions::m_SendClients[nClient].csIP.GetLength() > 0)
		{
			CClient client;
			if(client.OpenConnection(CGetSetOptions::m_SendClients[nClient].csIP) == FALSE)
			{
				LogSendRecieveInfo(StrF(_T("ERROR opening connection to %s"), CGetSetOptions::m_SendClients[nClient].csIP));

				if(CGetSetOptions::m_SendClients[nClient].bShownFirstError == FALSE)
				{
					CString cs;
					cs.Format(_T("Error opening connection to %s"), CGetSetOptions::m_SendClients[nClient].csIP);
					::SendMessage(theApp.m_MainhWnd, WM_SEND_RECIEVE_ERROR, (WPARAM)cs.GetBuffer(cs.GetLength()), 0);
					cs.ReleaseBuffer();

					CGetSetOptions::m_SendClients[nClient].bShownFirstError = TRUE;
				}

				continue;
			}

			CGetSetOptions::m_SendClients[nClient].bShownFirstError = FALSE;

			CClip* pClip;
			POSITION pos;
			pos = pClipList->GetHeadPosition();
			while(pos)
			{
				pClip = pClipList->GetNext(pos);
				if(pClip == NULL)
				{
					ASSERT(FALSE);
					LogSendRecieveInfo("Error in GetNext");
					break;
				}

				LogSendRecieveInfo(StrF(_T("Sending clip to %s"), CGetSetOptions::m_SendClients[nClient].csIP));

				if(client.SendItem(pClip, false) == FALSE)
				{
					CString cs;
					cs.Format(_T("Error sending clip to %s"), CGetSetOptions::m_SendClients[nClient].csIP);
					::SendMessage(theApp.m_MainhWnd, WM_SEND_RECIEVE_ERROR, (WPARAM)cs.GetBuffer(cs.GetLength()), 0);
					cs.ReleaseBuffer();
					break;
				}
			}

			client.CloseConnection();
		}
	}

	LogSendRecieveInfo("@@@@@@@@@@@@@@@ - END OF SendClientThread - @@@@@@@@@@@@@@@");

	return TRUE;
}
