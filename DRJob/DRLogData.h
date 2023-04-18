#pragma once
#pragma warning(disable: 4251)

#define SENDRESULT_UNKNOWN		0
#define SENDRESULT_OK			1
#define SENDRESULT_FAIL			2

typedef void (CALLBACK * FNDRLOGNOTIFY) (LPCTSTR, int, int, LPCTSTR, BOOL, LPVOID);

struct DRLOGINFODATA
{
	CString		szTime;
	int			nType;
	int			nMsg;
	CString		szDetail;
	BOOL		bResult;
};


class CDRLogData
{
	int				m_nSendResult;
	BOOL			m_bOpen;
	CFile			m_fileRead;
	void *			m_pNotifyData;
	FNDRLOGNOTIFY	m_pfnNotify;
	CList<DRLOGINFODATA *, DRLOGINFODATA *> m_listSaveCache;

	void ClearCache();

public:
	CDRLogData();
	~CDRLogData(void);

	int	 GetCachedDataCount() { return (int)m_listSaveCache.GetCount(); }
	BOOL GetCachedData(int nIdx, DRLOGINFODATA * pLogData);

	BOOL AddCache(int nType, int nMsg, LPCTSTR szDetail, BOOL bResult);
	BOOL SaveCacheData(LPCTSTR szFolder);

	int	GetSendResult()		{	return m_nSendResult;	}

	BOOL Open(LPCTSTR szFolder, int nYear, int nMonth, int nDay);
	void Close();
	BOOL Begin();
	BOOL GetNext(DRLOGINFODATA * pData);

	void SetNotity(FNDRLOGNOTIFY pfnNotify, void * pData) { m_pfnNotify = pfnNotify; m_pNotifyData = pData;}
};
