#include "stdafx.h"
#include "WorkerThread.h"

void JobCompleteDummy(void * pJob, void * pParam1)
{
//	if (pJob)
//		((CJobAbst *)pJob)->Delete();
}

void JobProcessDummy(LPVOID, LPVOID)
{
}

CJobAbst::CJobAbst()
{
	m_dwRetVal = 0;
	m_fnNotifyEnd = JobCompleteDummy;
	m_fnOnProcess = JobProcessDummy;
}

CWorkerThread::CWorkerThread()
{
	m_dwThreadID	= 0;
	m_hThread		= 0;
	m_iNumJobs		= 0;
	
	m_hThread		= ::CreateThread(0,	0, ThreadFunc, this, 0,	&m_dwThreadID );
}

CWorkerThread::~CWorkerThread()
{
	if ( m_hThread ) 
	{
		::PostThreadMessage(m_dwThreadID, WM_QUIT, 0, 0 ); 
		::WaitForSingleObject( m_hThread, 0 );
		m_hThread = 0;

	}
}

void CWorkerThread::SetThreadPriority(int nPriority)
{
	if (m_hThread)
		::SetThreadPriority(m_hThread, nPriority);
}

bool CWorkerThread::SubmitJob( CJobAbst * p_pJob, LPVOID pData)
{
	bool bRetVal = true;

	::PostThreadMessage( m_dwThreadID, WM_USER, ( WPARAM )p_pJob, (LPARAM)pData);
	::InterlockedIncrement( &m_iNumJobs );

	return bRetVal;
}

DWORD CWorkerThread::ThreadFunc( LPVOID lpvThreadParam )
{
	
	DWORD dwRetVal = 0;
	CWorkerThread* pWorkerThread = static_cast<CWorkerThread*>( lpvThreadParam );

	if ( !pWorkerThread ) 
		dwRetVal ++;
	else 
	{
		MSG msg;

		while (::GetMessage( &msg, 0, 0, 0 ) ) 
		{
//			try 
			{
				CJobAbst* pJob = (CJobAbst*) msg.wParam;
				
				if ( pJob ) 
				{
					pJob->m_dwRetVal = pJob->Process((LPVOID)msg.lParam);
					pJob->m_fnNotifyEnd(pJob, (LPVOID)msg.lParam); 
				}

				::InterlockedDecrement( &( pWorkerThread->m_iNumJobs ) );

			} 
//			catch ( ... ) 
			{
			}
		}
	}
	
	return dwRetVal;
}
