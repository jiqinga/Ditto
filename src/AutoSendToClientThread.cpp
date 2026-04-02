#include "stdafx.h"
#include "AutoSendToClientThread.h"
#include "Misc.h"
#include "Options.h"
#include "CP_Main.h"

CAutoSendToClientThread::CAutoSendToClientThread(void)
{
	m_waitTimeout = 30000;
	m_threadName = "CAutoSendToClientThread";
	for(int eventEnum = 0; eventEnum < ECAUTOSENDTOCLIENTTHREADEVENTS_COUNT; eventEnum++)
	{
		AddEvent(eventEnum);
	}
}

CAutoSendToClientThread::~CAutoSendToClientThread(void)
{
}

void CAutoSendToClientThread::FireSendToClient(CClipList *pClipList)
{
	Start();

	ATL::CCritSecLock csLock(m_cs.m_sect);
	if(m_threadRunning)
	{
		Log(_T("Adding clip to send to client in thread"));

		POSITION pos = pClipList->GetHeadPosition();
		while(pos)
		{
			CClip *pClip = pClipList->GetNext(pos);
		
			m_saveClips.AddTail(pClip);
		}

		pClipList->RemoveAll();
		
		FireEvent(SEND_TO_CLIENTS);
	}
	else
	{
		Log(_T("Error creating thread to send to clients"));
	}
}
void CAutoSendToClientThread::OnTimeOut(void *param)
{
	Stop(-1);
}

void CAutoSendToClientThread::OnEvent(int eventId, void *param)
{
	switch((eCAutoSendToClientThreadEvents)eventId)
	{
	case SEND_TO_CLIENTS:
		OnSendToClient();
		break;
	}
}

void CAutoSendToClientThread::OnSendToClient()
{
	CClipList *pLocalClips = new CClipList();

	//Save the clips locally
	{
		ATL::CCritSecLock csLock(m_cs.m_sect);

		POSITION pos;
		CClip* pClip;

		pos = m_saveClips.GetHeadPosition();
		while(pos)
		{
			pClip = m_saveClips.GetNext(pos);
			pLocalClips->AddTail(pClip);
		}

		//pLocalClips now own, the clips
		m_saveClips.RemoveAll();
	}

	SendToClient(pLocalClips);

	delete pLocalClips;
	pLocalClips = NULL;
}

bool CAutoSendToClientThread::SendToClient(CClipList *pClipList)
{
	if(pClipList == NULL)
	{
		LogSendRecieveInfo("ERROR if(pClipList == NULL)");
		return FALSE;
	}

	ISyncProvider *pSyncProvider = theApp.GetActiveSyncProvider();
	if(pSyncProvider == NULL)
	{
		LogSendRecieveInfo("ERROR - sync provider not initialized");
		return FALSE;
	}

	return pSyncProvider->SendClips(pClipList);
}
