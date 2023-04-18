#include "stdafx.h"
#include "ThreadPoolMgr.h"

CThreadPoolMgr::CThreadPoolMgr()
		: m_iThreadPoolSize( TP_DEF_POOL_SIZE ) 
{
	::InitializeCriticalSection( &m_csCritSec ); 
	m_iThreadPoolSize = 1;
	InitThreadPool(); 

}

CThreadPoolMgr::CThreadPoolMgr( int p_iPoolSize )
		: m_iThreadPoolSize( p_iPoolSize ) 
{
	::InitializeCriticalSection( &m_csCritSec ); 
	InitThreadPool(); 

}

CThreadPoolMgr::~CThreadPoolMgr()
{
	ClearThreadPool(); 
	::DeleteCriticalSection( &m_csCritSec ); 

}

bool CThreadPoolMgr::InitThreadPool()
{
	bool bRetVal = true;

	m_bIsPoolValid = true;

	for ( int i = 0; i < m_iThreadPoolSize; i ++ ) 
	{
		CWorkerThread* pWorkerThread = new CWorkerThread;
		
		if ( pWorkerThread ) 
			m_obThreadPool.push_back( pWorkerThread );
		else 
		{
			bRetVal = false;
			ClearThreadPool(); 
		}

	}

	return bRetVal;

}

void CThreadPoolMgr::SetThreadPriority(int nPriority)
{
	CWorkerThread* pWorkerThread;

	for ( int i = 0; i < m_iThreadPoolSize; i ++ ) 
	{
		pWorkerThread = new CWorkerThread;
		
		if (pWorkerThread ) 
			pWorkerThread->SetThreadPriority(nPriority);
	}	
}

bool CThreadPoolMgr::ClearThreadPool()
{
	bool bRetVal = true;

	::EnterCriticalSection( &m_csCritSec );

	m_bIsPoolValid = false;

	for ( int i = 0; i < m_iThreadPoolSize; i ++ ) 
	{
		try 
		{
			delete m_obThreadPool[i]; 
			m_obThreadPool[i] = 0;

		} catch ( ... ) {
			bRetVal = false ;
		
		}

	}

	::LeaveCriticalSection( &m_csCritSec );

	return bRetVal;
}

bool CThreadPoolMgr::SubmitJob( CJobAbst* p_pJob, LPVOID pData)
{
	bool			bRetVal;
	CWorkerThread*	pWorkerThread;

	if ( !m_bIsPoolValid ) 
		bRetVal = false;	
	else 
	{
		bRetVal = true;

		::EnterCriticalSection( &m_csCritSec );

		if ( m_bIsPoolValid ) 
		{
			pWorkerThread = m_obThreadPool.front();

			for ( int i =0; i < m_iThreadPoolSize; i++ ) 
			{
				if (pWorkerThread->GetJobCount() > m_obThreadPool[i]->GetJobCount() ) 
					pWorkerThread = m_obThreadPool[i];
			}

			pWorkerThread->SubmitJob( p_pJob, pData); 
		}

		::LeaveCriticalSection( &m_csCritSec ); 

	}

	return bRetVal;
}

int	 CThreadPoolMgr::GetWorkingJobCount()
{
	int nCount = 0;

	for ( int i =0; i < m_iThreadPoolSize; i++ ) 
		nCount += m_obThreadPool[i]->GetJobCount();

	return nCount;
}

