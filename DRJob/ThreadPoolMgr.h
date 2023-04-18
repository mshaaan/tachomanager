#pragma once
#pragma warning(disable: 4251)

#include <vector>
#include <windows.h>

#include "workerthread.h"

#define		TP_DEF_POOL_SIZE			5 

typedef std::vector<CWorkerThread*>		thread_pool;

class  CThreadPoolMgr
{
public:
	CThreadPoolMgr();
	CThreadPoolMgr(int p_iPoolSize);
	virtual ~CThreadPoolMgr();

	bool SubmitJob(CJobAbst* p_pJob, LPVOID pData);
	int	 GetWorkingJobCount();
	void SetThreadPriority(int nPriority);

private:
	bool InitThreadPool();
	bool ClearThreadPool();

	int					m_iThreadPoolSize;
	thread_pool			m_obThreadPool;
	bool				m_bIsPoolValid;	
	CRITICAL_SECTION	m_csCritSec;

};
//////////////////////////////////////////////////////////////////////
