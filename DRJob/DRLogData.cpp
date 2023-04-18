#include "StdAfx.h"
#include "DRLogData.h"
#include "..\\drjob\\drjob.h"

#define DRJOBSUCCEED			'1'
#define DRJOBFAILED				'0'
#define DRLOGFILERECORDEND		0x0A
#define DRLOGFILE_FORMAT		_T("%s\\%04d%02d%02d.log")

CDRLogData::CDRLogData()
{
	m_pfnNotify = NULL;
	m_pNotifyData = NULL;
	m_bOpen = FALSE;
	m_nSendResult = SENDRESULT_UNKNOWN;
}

CDRLogData::~CDRLogData(void)
{
	ClearCache();
	Close();
}

void CDRLogData::ClearCache()
{
	DRLOGINFODATA * pData; 

	while ((int)m_listSaveCache.GetCount())
	{
		pData = (DRLOGINFODATA *)m_listSaveCache.GetAt(m_listSaveCache.FindIndex(0));
		
		if (pData)
			delete pData;
		
		m_listSaveCache.RemoveHead();
	}
}

BOOL CDRLogData::AddCache(int nType, int nMsg, LPCTSTR szDetail, BOOL bResult)
{
	if (nMsg == DRLOGMSG_ERROR_CONNECTSERVER)
	{
		if (!bResult && m_nSendResult == SENDRESULT_FAIL)
			return TRUE;

		m_nSendResult = bResult? SENDRESULT_OK : SENDRESULT_FAIL;
	}

	if (nType == 1)
		m_nSendResult = bResult? SENDRESULT_OK : SENDRESULT_FAIL;

	DRLOGINFODATA * pData = (DRLOGINFODATA *) new DRLOGINFODATA; 

	if (pData)
	{
		SYSTEMTIME	st;
		GetLocalTime(&st);

		pData->szTime.Format(_T("%02d-%02d %02d:%02d:%02d"), st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
		pData->nType = nType;
		pData->nMsg = nMsg;
		pData->szDetail = szDetail;
		pData->bResult = bResult;
//		m_listSaveCache.AddTail(pData);

		if (m_pfnNotify)
			m_pfnNotify(pData->szTime, nType, nMsg, szDetail, bResult, m_pNotifyData);

		delete pData;
		return TRUE;
	}else
		return FALSE;
}

BOOL CDRLogData::SaveCacheData(LPCTSTR szFolder)
{
	CString		szFilename;
	SYSTEMTIME	stToday;
	CFile		logFile;
	int			nCount = (int)m_listSaveCache.GetCount();

	if (nCount == 0)
		return TRUE;

	GetSystemTime(&stToday);
	szFilename.Format(DRLOGFILE_FORMAT, szFolder, stToday.wYear, stToday.wMonth, stToday.wDay);
	
	if (logFile.Open(szFilename, CFile::modeCreate | CFile::modeNoTruncate | CFile::modeReadWrite))
	{
		int i;
		CString	szBuf;
		DRLOGINFODATA * pData; 
		char szLF[2];

		szLF[0] = DRLOGFILERECORDEND;
		szLF[1] = 0;

		logFile.SeekToEnd();

		for (i=0; i<nCount; i++)
		{
			pData = (DRLOGINFODATA *)m_listSaveCache.GetAt(m_listSaveCache.FindIndex(0));
			m_listSaveCache.RemoveHead();

			if (pData)
			{
				szBuf.Format(_T("%s,%d,%d,%s,%c"), pData->szTime,
						pData->nType, pData->nMsg, pData->szDetail, pData->bResult ? DRJOBSUCCEED : DRJOBFAILED);
#ifdef UNICODE
				char * pszBuf = new char[szBuf.GetLength()+1];

				if (pszBuf)
				{
					WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)szBuf, -1, (LPSTR)pszBuf, szBuf.GetLength()+1, NULL, NULL );
					logFile.Write(pszBuf, (int)strlen(pszBuf));
					delete [] pszBuf;
				}
#else
				logFile.Write(szBuf, szBuf.GetLength());
#endif
				logFile.Write(("\r\n"), 2);
				delete pData;
			}

		}
		
		logFile.Close();

		return TRUE;
	}else
		return FALSE;
}

BOOL CDRLogData::GetCachedData(int nIdx, DRLOGINFODATA * pLogData)
{
	DRLOGINFODATA * pData = (DRLOGINFODATA *)m_listSaveCache.GetAt(m_listSaveCache.FindIndex(nIdx)); 

	if (pData)
	{
		pLogData->szTime = pData->szTime;
		pLogData->nType = pData->nType;
		pLogData->nMsg = pData->nMsg;
		pLogData->szDetail = pData->szDetail;
		pLogData->bResult = pData->bResult;

		return TRUE;
	}else
		return FALSE;
}

BOOL CDRLogData::Open(LPCTSTR szFolder, int nYear, int nMonth, int nDay)
{
	if (!m_bOpen)
	{
		CString szFilename;

		szFilename.Format(DRLOGFILE_FORMAT, szFolder, nYear, nMonth, nDay);
		m_bOpen = m_fileRead.Open(szFilename, CFile::modeRead);
	}
	
	return m_bOpen;
}

void CDRLogData::Close()
{
	if (m_bOpen)
	{
		m_fileRead.Close();
		m_bOpen = FALSE;
	}
}

BOOL CDRLogData::Begin()
{
	if (m_bOpen)
	{
		m_fileRead.SeekToBegin();
		return TRUE;
	}else
		return FALSE;
}

BOOL CDRLogData::GetNext(DRLOGINFODATA * pData)
{ 
	int				nIdx = 0;
	int				nIdxHan = 0;
	unsigned char	szRead[2];
	unsigned char	szHan[3];
	CString			szBuf;

	szRead[1] = 0;
	szHan[2] = 0;

	while(1)
	{
		if (m_fileRead.Read(szRead, 1) == 1)
		{
			if (szRead[0] == ',')
			{
				switch(nIdx++)
				{
				case 0 : pData->szTime = szBuf; szBuf.Empty(); break;
#ifdef UNICODE
				case 1 : pData->nType = _wtoi(szBuf); szBuf.Empty(); break;
				case 2 : pData->nMsg = _wtoi(szBuf); szBuf.Empty(); break;
#else
				case 1 : pData->nType = atoi(szBuf); szBuf.Empty(); break;
				case 2 : pData->nMsg = atoi(szBuf); szBuf.Empty(); break;
#endif
				case 3 : pData->szDetail = szBuf; szBuf.Empty(); break;
				default : break;
				}
			}else if (szRead[0] == DRLOGFILERECORDEND)
			{
#ifdef UNICODE
				pData->bResult = _wtoi(szBuf);
#else
				pData->bResult = atoi(szBuf);
#endif
				break;
			}
			else 
			{
				if (szRead[0] > 127)
				{
					szHan[nIdxHan++] = szRead[0];

					if (nIdxHan == 2)
					{
						szBuf += (char *)szHan;
						nIdxHan = 0;
					}
				}else
					szBuf += (char *)szRead;
			}
		}else
			break;
	}

	if (nIdx == 4)
		return TRUE;
	else
		return FALSE;
}

