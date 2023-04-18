#pragma once

typedef void (* FNDRJOBPROCESSNOTIFY) (LPVOID, LPVOID);
typedef void (* FNDRJOBCOMPLETENOTIFY) (void *, LPVOID);


class CJobAbst
{
protected:
	FNDRJOBPROCESSNOTIFY	m_fnOnProcess;

public:
	CJobAbst(void);
	~CJobAbst(void) {};

	DWORD					m_dwRetVal;
	FNDRJOBCOMPLETENOTIFY	m_fnNotifyEnd;	

	virtual DWORD			Process(void * param) = 0;
	virtual void			Delete() = 0;
};

class CWorkerThread  
{
public:
	CWorkerThread();
	virtual ~CWorkerThread();

	bool SubmitJob( CJobAbst * p_pJob, LPVOID pData);
	long GetJobCount() { return m_iNumJobs; }
	void SetThreadPriority(int nPriority);

	static DWORD WINAPI ThreadFunc( LPVOID lpvThreadParam );

private:

	long	m_iNumJobs;
	HANDLE	m_hThread;
	DWORD   m_dwThreadID;
};

