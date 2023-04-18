#include "StdAfx.h"
#include "DRBkJob.h"

#define MAXCONCURRENTJOB		1

CDRBkJob * CDRBkJob::m_pDRBkJob;

CDRBkJob::CDRBkJob(void)
{
	m_nCurJob = 0;
	m_pThreadPool = NULL;
	m_pLog = NULL;

	CDRBkJob::m_pDRBkJob = this;
	::InitializeCriticalSection( &m_csCritSec ); 
}

CDRBkJob::~CDRBkJob(void)
{
	Stop();

	RemoveAll();
	::DeleteCriticalSection( &m_csCritSec ); 
}

BOOL CDRBkJob::IsWorking()
{
	ASSERT(m_pThreadPool);

	if (GetCount() > 0 && m_pThreadPool->GetWorkingJobCount() > 0)
		return TRUE;
	else
		return FALSE;

}

BOOL CDRBkJob::IsWorking(DRJOBTYPE drJob)
{
	if (IsWorking())
	{
		DRBKGNDJOBDATA * pData;

		for (int i=0; i<GetCount(); i++)
		{
			pData = m_listJob.GetAt(m_listJob.FindIndex(i));

			if (pData->pJob->GetType() == drJob && pData->nStatus == DRBkJobWorking)
				return TRUE;
		}
	}

	return FALSE;
}

BOOL CDRBkJob::IsWorkingFull(DRJOBTYPE nType)
{
	int nCount = 0;
	DRBKGNDJOBDATA * pData;

	for (int i=0; i<GetCount(); i++)
	{
		pData = m_listJob.GetAt(m_listJob.FindIndex(i));

		if (pData->pJob->GetType() == nType && pData->nStatus == DRBkJobWorking)
			nCount++;

		if (nCount == MAXCONCURRENTJOB)
			break;
	}

	if (nCount >= MAXCONCURRENTJOB)
		return TRUE;
	else
		return FALSE;
}

DRBKGNDJOBDATA * CDRBkJob::Find(DRJOBTYPE jType, LPVOID pValue)
{
	int i;
	BOOL bFind;
	DRBKGNDJOBDATA * pBkJobRet = NULL;
	DRBKGNDJOBDATA * pData;

	for (i=0; i<GetCount(); i++)
	{
		pData = m_listJob.GetAt(m_listJob.FindIndex(i));

		if (pData->pJob->GetType() == jType)
		{
			bFind = FALSE;

			if (jType == DRJobSend)
			{
				DRJOBSENDDATA * pSend = (DRJOBSENDDATA *)pData->pJobData;

				if (pSend->szPath.CompareNoCase((LPCTSTR)pValue) == 0)
				{
					pBkJobRet = pData;
					break;
				}
			}
		}
	}

	return pBkJobRet;
}

int CDRBkJob::Find(CDRJob *	pJob)
{
	int i;
	DRBKGNDJOBDATA * pData;

	for (i=0; i<GetCount(); i++)
	{
		pData = m_listJob.GetAt(m_listJob.FindIndex(i));

		if (pData->pJob == pJob)
			return i;
	}

	return -1;
}

BOOL CDRBkJob::Start()
{
	if (!m_pThreadPool)
		m_pThreadPool = new CThreadPoolMgr(4);

	if (m_pThreadPool)
	{
		m_pThreadPool->SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);
		return TRUE;
	}

	return FALSE;
}

void CDRBkJob::Stop()
{
	if (m_pThreadPool)
	{
		delete m_pThreadPool;
		m_pThreadPool = NULL;
	}
}

DRBKGNDJOBDATA * CDRBkJob::GetNextWaiting(DRJOBTYPE nType)
{
	int nIdx = nType == DRJobSend ? 0 : m_nCurJob;
	DRBKGNDJOBDATA * pData;

	for (int i=0; i<GetCount(); i++)
	{
		pData = m_listJob.GetAt(m_listJob.FindIndex(i));

		if (pData->pJob->GetType() == nType && pData->nStatus == DRBkJobWorking)
			return NULL;
	}		

	for (int i=0; i<GetCount(); i++)
	{
		pData = m_listJob.GetAt(m_listJob.FindIndex(nIdx));

		if (++nIdx == GetCount())
			nIdx = 0;

		if (pData->pJob->GetType() == nType && pData->nStatus == DRBkJobWaiting)
		{
			m_nCurJob = nIdx;
			return pData;
		}
	}

	return NULL;
}

void CDRBkJob::Process(DRJOBTYPE nType)
{
	if (m_pThreadPool)
	{
		DRBKGNDJOBDATA * pData = GetNextWaiting(nType);

		if (pData && !IsWorkingFull(nType))
		{
			pData->pJob->m_fnNotifyProcess = OnNotifyProcess;
			pData->pJob->m_fnNotifyEnd = OnNotifyComplete;
			m_pThreadPool->SubmitJob(pData->pJob, pData->pJobData);
			pData->nStatus = DRBkJobWorking;
		}
	}
}

BOOL CDRBkJob::Add(CDRJob * pJob, LPVOID pJobData)
{
	DRBKGNDJOBDATA * pData = new DRBKGNDJOBDATA;

#ifdef _DEBUG
	OutputDebugString(_T("CDRBkJob::Add(CDRJob * pJob, LPVOID pJobData) Start\n"));
#endif	
	if (pData)
	{
		::EnterCriticalSection( &m_csCritSec );
		pData->nStatus = DRBkJobWaiting;
		pData->pJob = pJob;
		pData->pJobData = pJobData;

		m_listJob.AddTail(pData);
		::LeaveCriticalSection( &m_csCritSec );
#ifdef _DEBUG
		OutputDebugString(_T("CDRBkJob::Add(CDRJob * pJob, LPVOID pJobData) End\n"));
#endif	
		return TRUE;
	}else

#ifdef _DEBUG
	OutputDebugString(_T("CDRBkJob::Add(CDRJob * pJob, LPVOID pJobData) End\n"));
#endif
	return FALSE;
}

BOOL CDRBkJob::Add(DRJOBSENDDATA * pSendData)
{
	CDRJob * pJobNew = new CDRJob(DRJobSend);
	DRJOBSENDDATA * pSendNew = new DRJOBSENDDATA;

#ifdef _DEBUG
	OutputDebugString(_T("CDRBkJob::Add(DRJOBSENDDATA * pSendData) Start\n"));
#endif	

	if (pJobNew && pSendNew)
	{
		*pSendNew = *pSendData;
		
		if (Add(pJobNew, pSendNew))
		{
#ifdef _DEBUG
			OutputDebugString(_T("CDRBkJob::Add(DRJOBSENDDATA * pSendData) End\n"));
#endif	
			return TRUE;
		}
	}

	if (pJobNew)	delete pJobNew;
	if (pSendNew)	delete pSendNew;

#ifdef _DEBUG
	OutputDebugString(_T("CDRBkJob::Add(DRJOBSENDDATA * pSendData) End\n"));
#endif	

	return FALSE;
}

BOOL CDRBkJob::Add(DRJOBSTANDBYSENDDATA * pJobData)
{
	CDRJob * pJobNew = new CDRJob(DRJobStandBySend);
	DRJOBSTANDBYSENDDATA * pStdByNew = new DRJOBSTANDBYSENDDATA;

#ifdef _DEBUG
	OutputDebugString(_T("CDRBkJob::Add(DRJOBSTANDBYSENDDATA * pJobData) Start\n"));
#endif	

	if (pJobNew && pStdByNew)
	{
		*pStdByNew = *pJobData;

		if (Add(pJobNew, pStdByNew))
		{
#ifdef _DEBUG
			OutputDebugString(_T("CDRBkJob::Add(DRJOBSTANDBYSENDDATA * pJobData) End\n"));
#endif	
			return TRUE;
		}
	}

	if (pJobNew)	delete pJobNew;
	if (pStdByNew)	delete pStdByNew;

#ifdef _DEBUG
			OutputDebugString(_T("CDRBkJob::Add(DRJOBSTANDBYSENDDATA * pJobData) End\n"));
#endif	
	return FALSE;
}


BOOL CDRBkJob::Add(DRJOBANALYSISDATA * pJobData)
{
	CDRJob * pJobNew = new CDRJob(DRJobAnalysis);
	DRJOBANALYSISDATA * pAnalNew = new DRJOBANALYSISDATA;

	if (pJobNew && pAnalNew)
	{
		*pAnalNew = *pJobData;
		
		if (Add(pJobNew, pAnalNew))
			return TRUE;
	}

	if (pJobNew)	delete pJobNew;
	if (pAnalNew)	delete pAnalNew;

	return FALSE;
}

BOOL CDRBkJob::Add(DRJOBSENDFTPDATA * pJobData)
{
	CDRJob * pJobNew = new CDRJob(DRJobFTP);
	DRJOBSENDFTPDATA * pFtpNew = new DRJOBSENDFTPDATA;

	if (pJobNew && pFtpNew)
	{
		*pFtpNew = *pJobData;
		
		if (Add(pJobNew, pFtpNew))
			return TRUE;
	}

	if (pJobNew)	delete pJobNew;
	if (pFtpNew)	delete pFtpNew;

	return FALSE;
}

BOOL CDRBkJob::Remove(DRBKGNDJOBDATA * pBkJobData)
{
	int i;
	CString szPath;
	DRBKGNDJOBDATA * pData;

#ifdef _DEBUG
	OutputDebugString(_T("CDRBkJob::Remove(DRBKGNDJOBDATA * pBkJobData) Start\n"));
#endif	
	::EnterCriticalSection( &m_csCritSec );

	for (i=0; i<GetCount(); i++)
	{
		pData = m_listJob.GetAt(m_listJob.FindIndex(i));

		if (pData == pBkJobData)
		{
			if (m_nCurJob != 0 && i < m_nCurJob)
				m_nCurJob--;

			m_listJob.RemoveAt(m_listJob.FindIndex(i));
			delete pData->pJob;
			delete pData->pJobData;
			delete pData;
			::LeaveCriticalSection( &m_csCritSec );
#ifdef _DEBUG
			OutputDebugString(_T("CDRBkJob::Remove(DRBKGNDJOBDATA * pBkJobData) End\n"));
#endif
			return TRUE;
		}
	}

	::LeaveCriticalSection( &m_csCritSec );
#ifdef _DEBUG
	OutputDebugString(_T("CDRBkJob::Remove(DRBKGNDJOBDATA * pBkJobData) End\n"));
#endif
	return FALSE;
}

BOOL CDRBkJob::Remove(CDRJob *	pJob)
{
	int nIdx;
	DRBKGNDJOBDATA * pData;

#ifdef _DEBUG
	OutputDebugString(_T("CDRBkJob::Remove(CDRJob *	pJob) Start\n"));
#endif	
	::EnterCriticalSection( &m_csCritSec );

	nIdx = Find(pJob);

	if (nIdx != -1)
	{
		pData = m_listJob.GetAt(m_listJob.FindIndex(nIdx));

		if (m_nCurJob != 0 && nIdx < m_nCurJob)
			m_nCurJob--;

		m_listJob.RemoveAt(m_listJob.FindIndex(nIdx));
		delete pData->pJob;
		delete pData->pJobData;
		delete pData;
		::LeaveCriticalSection( &m_csCritSec );
#ifdef _DEBUG
		OutputDebugString(_T("CDRBkJob::Remove(CDRJob *	pJob) End\n"));
#endif		
		return TRUE;
	}

	::LeaveCriticalSection( &m_csCritSec );
#ifdef _DEBUG
	OutputDebugString(_T("CDRBkJob::Remove(CDRJob *	pJob) End\n"));
#endif
	return FALSE;
}

void CDRBkJob::OnSendReady(CDRJob *	pJob)
{
	int nIdx;
	DRBKGNDJOBDATA * pData;
	DRJOBSTANDBYSENDDATA * pStdByData;

#ifdef _DEBUG
	OutputDebugString(_T("CDRBkJob::OnSendReady(CDRJob *	pJob) Start\n"));
#endif	
	nIdx = Find(pJob);

	if (nIdx != -1)
	{
		pData = m_listJob.GetAt(m_listJob.FindIndex(nIdx));
		pStdByData = (DRJOBSTANDBYSENDDATA *)pData->pJobData;

		if (!Find(DRJobSend, (LPVOID)(LPCTSTR)pStdByData->szDest))
		{
			DRJOBSENDDATA sData;

			sData.nRec = 0;
			sData.szPath = pStdByData->szDest;
			sData.msgData.bEnable = FALSE;
			sData.netData.bEnable = TRUE;
			sData.netData.szAddr = pStdByData->szAddr;
			sData.netData.szPort.Format(_T("%d"), pStdByData->uiPort);

			Add(&sData);
		}
	}

#ifdef _DEBUG
	OutputDebugString(_T("CDRBkJob::OnSendReady(CDRJob *	pJob) End\n"));
#endif	
}

void CDRBkJob::OnAnalysisEnd(CDRJob *	pJob)
{
	int nIdx;
	DRBKGNDJOBDATA * pData;

	nIdx = Find(pJob);

	if (nIdx != -1)
	{
		DRJOBANALYSISDATA * pAnalData;

		pData = m_listJob.GetAt(m_listJob.FindIndex(nIdx));
		pAnalData = (DRJOBANALYSISDATA *)pData->pJobData;

		if (pAnalData->Ftp.bEnable && pAnalData->Ftp.szPath.GetLength() > 0)
		{
			DRJOBSENDFTPDATA ftpData;
			
			ftpData.szAddr = pAnalData->Ftp.szAddr;
			ftpData.szDestFolder = pAnalData->Ftp.szDestFolder;
			ftpData.szPassword = pAnalData->Ftp.szPassword;
			ftpData.szPath = pAnalData->Ftp.szPath;
			ftpData.szUsername = pAnalData->Ftp.szUsername;

			Add(&ftpData);
		}
	}
}

BOOL CDRBkJob::SetState(CDRJob *	pJob, DRBKDNGJOBSTATUS bState)
{
	int i;
	DRBKGNDJOBDATA * pData;

	for (i=0; i<GetCount(); i++)
	{
		pData = m_listJob.GetAt(m_listJob.FindIndex(i));

		if (pData->pJob == pJob)
		{
			pData->nStatus = bState;
			return TRUE;
		}
	}

	return FALSE;
}

DRBKDNGJOBSTATUS CDRBkJob::GetState(CDRJob *	pJob)
{
	int i;
	DRBKGNDJOBDATA * pData;

	for (i=0; i<GetCount(); i++)
	{
		pData = m_listJob.GetAt(m_listJob.FindIndex(i));

		if (pData->pJob == pJob)
			return pData->nStatus;
	}

	return DRBkJobUnknown;
}

void CDRBkJob::OnCopyData(const LPBYTE pData, DWORD dwLen)
{
	int i;
	DRBKGNDJOBDATA * pBkData;

	for (i=0; i<GetCount(); i++)
	{
		pBkData = m_listJob.GetAt(m_listJob.FindIndex(i));

		if (pBkData->nStatus == DRBkJobWorking && pBkData->pJob->m_hEventWait)
		{
			pBkData->pJob->OnDataReceived(NULL, pData, dwLen, pBkData->pJob);
			return;
		}
	}
}

void CDRBkJob::RemoveAll()
{
	int nCount = GetCount();
	DRBKGNDJOBDATA * pData;

	m_nCurJob = 0;

	for (int i=0; i<nCount; i++)
	{
		pData = m_listJob.GetAt(m_listJob.FindIndex(0));

		if (pData)
		{
			m_listJob.RemoveAt(m_listJob.FindIndex(0));
			delete pData->pJob;
			delete pData->pJobData;
			delete pData;
		}
	}
}

void CDRBkJob::AddLog(DRJOBTYPE nType, int nMsg, LPCTSTR szDetail, BOOL bSuccess)
{
#ifdef _DEBUG
	OutputDebugString(_T("CDRBkJob::AddLog() Start\n"));
#endif
	if (m_pLog)
		m_pLog->AddCache((int)nType, nMsg, szDetail, bSuccess);
#ifdef _DEBUG
	OutputDebugString(_T("CDRBkJob::AddLog() End\n"));
#endif
}

BOOL CDRBkJob::Suspend(LPVOID pValue, DRJOBTYPE jType)
{
	BOOL bRet = TRUE;
	DRBKGNDJOBDATA * pData;

	::EnterCriticalSection( &m_csCritSec );
	pData = Find(jType, pValue);

	if (pData)
	{
		if (pData->nStatus == DRBkJobWorking)
			bRet = FALSE;
		else if (pData->nStatus == DRBkJobWaiting)
			pData->nStatus = DRBkJobSuspend;	
	}

	::LeaveCriticalSection( &m_csCritSec );

	return bRet;
}

void CDRBkJob::Resume(LPVOID pValue, DRJOBTYPE jType)
{
	DRBKGNDJOBDATA * pData;

	::EnterCriticalSection( &m_csCritSec );
	pData = Find(jType, pValue);

	if (pData->nStatus == DRBkJobSuspend)
			pData->nStatus = DRBkJobWaiting;

	::LeaveCriticalSection( &m_csCritSec );
}

void CDRBkJob::OnNotifyProcess(LPVOID nState, LPVOID pParam)
{
	switch((int)(DWORD)nState)
	{
	case DRJOBSTATE_LOG : 
		{
			DRLOGNOTIFYDATA * pData = (DRLOGNOTIFYDATA *)pParam;
		
			m_pDRBkJob->AddLog(pData->nType, pData->nMsg, pData->szDetail, pData->bSuccess);
		}
		break;
	case DRJOBSTATE_END :
		{
			DRJOBNOTIFYDATA * pData = (DRJOBNOTIFYDATA *)pParam;
			CDRJob * pJob = (CDRJob *)pData->pParam;
			
			m_pDRBkJob->SetState(pJob, pData->bSucess ? DRBkJobEnd : DRBkJobWaiting);
		
			if (pData->bSucess)
			{
				if (pJob->GetType() == DRJobStandBySend)
					m_pDRBkJob->OnSendReady(pJob);
//				else if (pJob->GetType() == DRJobAnalysis)
//					m_pDRBkJob->OnAnalysisEnd(pJob);
			}
		}
		break;
	}
}

void CDRBkJob::OnNotifyComplete(LPVOID pJob, LPVOID pParam)
{
	if (((CDRJob *)pJob)->GetType() == DRJobSend)
	{
		if (((DRJOBSENDDATA *)pParam)->nRec == DRJOB_INVALIDERECNO)
			m_pDRBkJob->Remove((CDRJob *)pJob);
	}else if (((CDRJob *)pJob)->GetType() == DRJobAnalysis || ((CDRJob *)pJob)->GetType() == DRJobStandBySend || ((CDRJob *)pJob)->GetType() == DRJobFTP)
		m_pDRBkJob->Remove((CDRJob *)pJob);
}

