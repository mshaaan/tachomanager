#pragma once

#include "ThreadPoolMgr.h"
#include "drjob.h"
#include "drlogdata.h"

typedef enum _DRBKDNGJOBSTATUS
{
	DRBkJobWaiting,
	DRBkJobWorking,
	DRBkJobEnd,
	DRBkJobSuspend,
	DRBkJobUnknown = 99
}DRBKDNGJOBSTATUS;

typedef struct _DRBKGNDJOBDATA
{
	DRBKDNGJOBSTATUS	nStatus;
	CDRJob *			pJob;
	LPVOID				pJobData;
}DRBKGNDJOBDATA;

class CDRBkJob
{
	int					m_nCurJob;
	CThreadPoolMgr  *	m_pThreadPool;

	CDRLogData *		m_pLog;
	CRITICAL_SECTION	m_csCritSec;

	CList <DRBKGNDJOBDATA *, DRBKGNDJOBDATA *> m_listJob;

	DRBKGNDJOBDATA *	GetNextWaiting(DRJOBTYPE nType);
	BOOL				Add(CDRJob * pJob, LPVOID pJobData);
	BOOL				IsWorkingFull(DRJOBTYPE nType);
	DRBKGNDJOBDATA *	Find(DRJOBTYPE jType, LPVOID pValue);
	int					Find(CDRJob *	pJob);

public:
	CDRBkJob(void);
	~CDRBkJob(void);

	BOOL				Add(DRJOBSENDDATA * pJobData);
	BOOL				Add(DRJOBANALYSISDATA * pJobData);
	BOOL				Add(DRJOBSTANDBYSENDDATA * pJobData);
	BOOL				Add(DRJOBSENDFTPDATA * pJobData);

	BOOL				Remove(DRBKGNDJOBDATA * pBkJobData);
	BOOL				Remove(CDRJob *	pJob);
	BOOL				SetState(CDRJob *	pJob, DRBKDNGJOBSTATUS bState);
	DRBKDNGJOBSTATUS	GetState(CDRJob *	pJob);
	int					GetCount() { return (int)m_listJob.GetCount(); }
	void				RemoveAll();
	BOOL				IsWorking();
	BOOL				IsWorking(DRJOBTYPE drJob);
	void				AddLog(DRJOBTYPE nType, int nMsg, LPCTSTR szDetail, BOOL bSuccess);
	void				OnCopyData(const LPBYTE pData, DWORD dwLen);
	void				OnSendReady(CDRJob *	pJob);
	void				OnAnalysisEnd(CDRJob *	pJob);
	BOOL				Suspend(LPVOID pValue, DRJOBTYPE jType = DRJobSend);
	void				Resume(LPVOID pValue, DRJOBTYPE jType = DRJobSend);

	BOOL				Start();
	void				Stop();
	void				Process(DRJOBTYPE	nType);
	void				SetLogData(CDRLogData * pLog) { m_pLog = pLog; } 

	static CDRBkJob *	m_pDRBkJob;
	static void			OnNotifyProcess(LPVOID, LPVOID);
	static void			OnNotifyComplete(LPVOID pJob, LPVOID pParam);
};
