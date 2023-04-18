#include "StdAfx.h"
#include "shlwapi.h"
#include "DRJob.h"
#include "..\\drlib\\tachofile.h"
#include "..\\drlib\\tachofiled20.h"
#include "..\\drlib\\xtachofile.h"
#include <afxinet.h>

#define RECORDSPERSEND			30
#define MAX_DRHEADERITEM		8
#define SENDWAIT_MSEC			30000
#define DATAVERSIONIS2(a)		(_wtoi(a) >= 2)

static char * g_pHeader[MAX_DRHEADERITEM] = { TACHOFILEDATAVER, TACHOFILEVERSION, TACHOFILECARNO, TACHOFILEDRIVERCODE,
				TACHOFILEMILEAGE, TACHOFILERECORDTIME, TACHOFILEDATATYPE, TACHOFILEENVRONMENT };

typedef struct _DRCOPYPROCDATA
{
	FNDRJOBPROCESSNOTIFY	fnNotifyProcess;
	COPYFILENOTIFYDATA		copyData;
} DRCOPYPROCDATA;

CDRJob::CDRJob(DRJOBTYPE nType)
{
	m_dwRetVal	= 0;
	m_nJobType	= nType;
	m_hEventWait = NULL;
}

CDRJob::~CDRJob(void)
{
	if (m_hEventWait)
	{
		::CloseHandle(m_hEventWait);
		m_hEventWait = NULL;
	}	
}

void CDRJob::MakeSafePathname(CString & szPath)
{
	int i=1;
	CString szNewPath(szPath);
	CFileStatus fStatus;

	while(i<366 && CFile::GetStatus(szNewPath, fStatus))
		szNewPath.Format(_T("%s_%d.%s"), szPath.Left(szPath.ReverseFind('.')), i++, szPath.Right(3));

	szPath = szNewPath;
}

CString	CDRJob::TrimRight(LPCTSTR szPath)
{
	CString szInput = szPath;

	if (szInput.ReverseFind('\\') >= 0)
		szInput = szInput.Right(szInput.GetLength() - szInput.ReverseFind('\\') - 1);

	if (szInput.ReverseFind('.') >= 0)
		szInput = szInput.Left(szInput.ReverseFind('.'));

	return CString(szInput);
}

BOOL CDRJob::Send(CMSNetClient * pNet, LPBYTE pSendBuf, int nLen, DRJOBSENDDATA * pData, BOOL bData)
{
	DWORD dwRet;
//	COPYDATASTRUCT cData;

	if (pData->netData.bEnable)
	{
		::ResetEvent(m_hEventWait);
		
		if (pNet->Send(pSendBuf, nLen))
		{
			dwRet = ::WaitForSingleObject(m_hEventWait, SENDWAIT_MSEC);

			if (dwRet == WAIT_TIMEOUT || dwRet == WAIT_FAILED || m_dwRetVal != CRNET_CMD_ACK)
			{
				if (dwRet == WAIT_TIMEOUT)
					OutputDebugString(_T("Tacho Data Send Response Wait Time out\n"));
				else if (dwRet == WAIT_FAILED)
					OutputDebugString(_T("Tacho Data Send Response Wait Fail\n"));
				else
					OutputDebugString(_T("Tacho Data Send Fail Response From Server\n"));
				return FALSE;
			}
			else if (strlen(m_szIDNet) == 0)
				strcpy(m_szIDNet, m_netData.GetID());
		}else
			return FALSE;
	}

/*
	if (pData->msgData.bEnable)
	{
		if (::FindWindow(NULL, pData->msgData.szName))
		{
			if (bData)
				memcpy(pSendBuf+3, m_szIDMsg, CRNET_ID_LEN);

			cData.dwData = pData->msgData.dwMsg;
			cData.cbData = nLen;
			cData.lpData = pSendBuf;

			::SendMessage(::FindWindow(NULL, pData->msgData.szName), WM_COPYDATA, (WPARAM)pData->msgData.hWnd, (LPARAM)&cData);
			dwRet = ::WaitForSingleObject(m_hEventWait, SENDWAIT_MSEC);

			if (dwRet == WAIT_TIMEOUT || dwRet == WAIT_FAILED || m_dwRetVal != CRNET_CMD_ACK)
				return FALSE;
			else
				strcpy(m_szIDMsg, m_netData.GetID());
		}else
			return FALSE;
	}
*/
	return TRUE;
}

void CDRJob::NotifyLog(DRJOBTYPE dJob, int nMsg, LPCTSTR szDetail, BOOL bResult)
{
	DRLOGNOTIFYDATA logData;

	logData.bSuccess = bResult;
	logData.nType = dJob;
	logData.nMsg = nMsg;
	logData.szDetail = szDetail;

	m_fnNotifyProcess((LPVOID)(LPVOID)DRJOBSTATE_LOG, &logData);
}

DWORD CDRJob::Process(void * param)
{
	DWORD dwRet = 0;

	switch(m_nJobType)
	{
	case DRJobCopyFromUSB:	dwRet = JobCopyFromUSB((DRJOBCOPYFROMUSBDATA *) param); 		break;
	case DRJobSend:			dwRet = JobSend((DRJOBSENDDATA *) param);						break;
	case DRJobAnalysis:		dwRet = JobAnalysis((DRJOBANALYSISDATA *) param);				break;
	case DRJobStandBySend:	dwRet = JobStandBySend((DRJOBSTANDBYSENDDATA *) param);			break;
	case DRJobFTP :			dwRet = JobSendFtp((DRJOBSENDFTPDATA *)param);					break;
	}

	return dwRet;
}

DWORD CDRJob::JobCopyFromUSB(DRJOBCOPYFROMUSBDATA * pData)
{
	int i=0;
	BOOL bCanceled = FALSE;
	BOOL bFind = TRUE;
	BOOL bDRFind = FALSE;
	WIN32_FIND_DATA wFind;
	CString szFindFolder = (LPCTSTR)pData->szSrcPath;
	CString szBuf, szFilename, szFileSrc, szFileDest, szPath;
	DRCOPYPROCDATA cfpInfo;
	CFileStatus fStatus;
	DRLOGNOTIFYDATA logData;
	CStringList listCopy;
	CString szVersion;
	BOOL bValid = FALSE;
//	CTachoFile tFile;
	CTachoFileAbst * ptFile;

	szFindFolder += "\\*.*";
	HANDLE hFind = FindFirstFile((LPCTSTR)szFindFolder, &wFind);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		DWORD dwBase = GetTickCount();

		while (hFind == INVALID_HANDLE_VALUE)
		{
			hFind = FindFirstFile((LPCTSTR)szFindFolder, &wFind);

			if (GetTickCount() - dwBase > 2000)
				break;
		}
	}

	cfpInfo.fnNotifyProcess = m_fnNotifyProcess;
	cfpInfo.copyData.pParam = pData;
	cfpInfo.copyData.szDestFile = NULL;
	cfpInfo.copyData.bSucess = TRUE;
	logData.bSuccess = TRUE;
	logData.nType = GetType();

	while (hFind != INVALID_HANDLE_VALUE && bFind)
	{
		if(*wFind.cFileName != '.')
		{
			szFilename = wFind.cFileName;

			if (!(wFind.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				szBuf = szFilename.Mid(szFilename.ReverseFind('.') + 1, szFilename.GetLength());
				
				if (szBuf.CompareNoCase(TACHOFILEEXT_D10) == 0 || szBuf.CompareNoCase(TACHOFILEEXT_D20) == 0)
				{
					bDRFind = TRUE;

					szFileSrc = pData->szSrcPath;
					szFileSrc += '\\';
					szFileSrc += szFilename;

					for (i=0; i<listCopy.GetCount(); i++)
					{
						szPath = listCopy.GetAt(listCopy.FindIndex(i));

						if (szFileSrc.CompareNoCase(szPath) < 0)
						{
							listCopy.InsertBefore(listCopy.FindIndex(i), szFileSrc);
							break;
						}
					}

					if (i>=listCopy.GetCount())
						listCopy.AddTail(szFileSrc);
				}
			}
		}

		bFind = FindNextFile(hFind, &wFind);
	}	
	
	if (hFind != INVALID_HANDLE_VALUE)
		FindClose(hFind);

	for (i=0; i<listCopy.GetCount(); i++)
	{
		szFileSrc = listCopy.GetAt(listCopy.FindIndex(i));
		
		if (szFileSrc.Right(3).CompareNoCase(TACHOFILEEXT_D10) == 0)
			ptFile = (CTachoFileAbst *) new CTachoFile;
		else
			ptFile = (CTachoFileAbst *) new CTachoFileD20;

		if (ptFile && ptFile->Open(szFileSrc))
		{
			szVersion = ptFile->GetHeaderValue(TACHOFILECARNO);
			
			if (szVersion.GetLength() > 0)
				bValid = TRUE;

			ptFile->Close();
		}else
		{
			logData.nMsg = DRLOGMSG_ERROR_OPENDRFILE;
			logData.szDetail =  TrimRight(szFileSrc);
		}

		if (ptFile) delete ptFile;

		if (bValid)
		{
			cfpInfo.copyData.nTotalTransfered = 0;
			cfpInfo.copyData.nCurTransfered = 0;

			if (CFile::GetStatus(szFileSrc, fStatus))
			{
				cfpInfo.copyData.nTotalFileSize = fStatus.m_size;
				cfpInfo.copyData.nCurFileSize = fStatus.m_size;
				cfpInfo.copyData.szDestFile = szFileDest;
				szBuf = szFileSrc;

				szFilename = szBuf.Right(szBuf.GetLength() - szBuf.ReverseFind('\\') - 1);
				szFileDest = pData->szDestPath;
				szFileDest += '\\';
				szFileDest += szFilename;

				m_fnNotifyProcess((LPVOID)DRJOBSTATE_START, &cfpInfo.copyData);

				if (szFileSrc.Right(3).CompareNoCase(TACHOFILEEXT_D10) == 0)
				{
					if (CopyFileEx(szFileSrc, szFileDest, CopyProgressRoutine, &cfpInfo, &bCanceled, 0))
					{
						DeleteFile(szFileSrc);		
						pData->szFile = szFilename;
					}
					else
						bValid = FALSE;
				}else
				{
					CTachoFile d10File;
					CTachoFileD20 d20File;
					TACHOREBUFFER tBuffer;
					CString szDrvCodeOri, szDrvCode;

					szFileDest = szFileDest.Left(szFileDest.ReverseFind('.') + 1);
					szFileDest += TACHOFILEEXT_D10;
					DeleteFile(szFileDest);
					
					if (d20File.Open(szFileSrc) && d10File.CreateFile(szFileDest))
					{
						szBuf = d20File.GetHeaderValue(TACHOFILEDATAVER);		d10File.AddHeader(TACHOFILEDATAVER, szBuf);
						szBuf = d20File.GetHeaderValue(TACHOFILECARNO);			d10File.AddHeader(TACHOFILECARNO, szBuf);
						szBuf = d20File.GetHeaderValue(TACHOFILEDRIVERCODE);	d10File.AddHeader(TACHOFILEDRIVERCODE, szBuf);
						szBuf = d20File.GetHeaderValue(TACHOFILEDATATYPE);		d10File.AddHeader(TACHOFILEDATATYPE, szBuf);
						szBuf = d20File.GetHeaderValue(TACHOFILEVIN);			d10File.AddHeader(TACHOFILEVIN, szBuf);
						szBuf = d20File.GetHeaderValue(TACHOFILECARTYPE);		d10File.AddHeader(TACHOFILECARTYPE, szBuf);
						szBuf = d20File.GetHeaderValue(TACHOFILESEIALNO);		d10File.AddHeader(TACHOFILESEIALNO, szBuf);
						szBuf = d20File.GetHeaderValue(TACHOFILEBIZNO);			d10File.AddHeader(TACHOFILEBIZNO, szBuf);
						d10File.WriteHeader();
						cfpInfo.copyData.nCurFileSize = d20File.GetRecordCount();
						cfpInfo.copyData.nCurTransfered = 0;
						szDrvCodeOri = d20File.GetHeaderValue(TACHOFILEDRIVERCODE);

						while(d20File.GetNextData(&tBuffer))
						{
							szDrvCode = d20File.GetHeaderValue(TACHOFILEDRIVERCODE);

							if (szDrvCode != szDrvCodeOri)
							{
								SYSTEMTIME st;

								d10File.Close();
								
								pData->szFile = szFilename.Left(szFilename.ReverseFind('.') + 1);
								pData->szFile += TACHOFILEEXT_D10;
								m_fnNotifyProcess((LPVOID)(LPVOID)DRJOBSTATE_END, &cfpInfo.copyData);
								
								d20File.SetHeaderValue(TACHOFILEDRIVERCODE, szDrvCode);
															
								GetLocalTime(&st);
								szFileDest = szFileDest.Left(szFileDest.ReverseFind('\\') + 1);
								szFilename.Format(_T("%02d%02d%02d%02d%02d%02d_%s.%s"), st.wYear-2000, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
											d20File.GetHeaderValue(TACHOFILECARNO), TACHOFILEEXT_D10);
								szFileDest += szFilename;

								if (d10File.CreateFile(szFileDest))
								{
									szBuf = d20File.GetHeaderValue(TACHOFILEDATAVER);		d10File.AddHeader(TACHOFILEDATAVER, szBuf);
									szBuf = d20File.GetHeaderValue(TACHOFILECARNO);			d10File.AddHeader(TACHOFILECARNO, szBuf);
									szBuf = d20File.GetHeaderValue(TACHOFILEDRIVERCODE);	d10File.AddHeader(TACHOFILEDRIVERCODE, szBuf);	
									szBuf = d20File.GetHeaderValue(TACHOFILEDATATYPE);		d10File.AddHeader(TACHOFILEDATATYPE, szBuf);
									szBuf = d20File.GetHeaderValue(TACHOFILEVIN);			d10File.AddHeader(TACHOFILEVIN, szBuf);
									szBuf = d20File.GetHeaderValue(TACHOFILECARTYPE);		d10File.AddHeader(TACHOFILECARTYPE, szBuf);
									szBuf = d20File.GetHeaderValue(TACHOFILESEIALNO);		d10File.AddHeader(TACHOFILESEIALNO, szBuf);
									szBuf = d20File.GetHeaderValue(TACHOFILEBIZNO);			d10File.AddHeader(TACHOFILEBIZNO, szBuf);
									d10File.WriteHeader();
								}else
								{
									bValid = FALSE;
									break;
								}

								szDrvCodeOri = szDrvCode;
							}

							d10File.WriteData(&tBuffer);
							cfpInfo.copyData.nCurTransfered++; 

							if (cfpInfo.copyData.nCurTransfered % 300 == 0)
								m_fnNotifyProcess((LPVOID)DRJOBSTATE_PROGRESS, &cfpInfo.copyData);
						}

						d20File.Close();
						d10File.Close();

						if (bValid)
						{
							pData->szFile = szFileDest.Right(szFileDest.GetLength() - szFileDest.ReverseFind('\\') - 1);;
							DeleteFile(szFileSrc);
						}
					}else
						bValid = FALSE;

					cfpInfo.copyData.nTotalTransfered += fStatus.m_size;
				}

				if (!bValid)
				{
					logData.nMsg = DRLOGMSG_ERROR_COPYFILE;
					logData.szDetail = TrimRight(szFileSrc);
				}
			}
		}

		if (bValid)
		{
			logData.nMsg = DRLOGMSG_ERROR_OK;
			logData.szDetail = szFilename;
		}
		else
		{
			logData.bSuccess = FALSE;
			cfpInfo.copyData.bSucess = FALSE;
		}

		m_fnNotifyProcess((LPVOID)(LPVOID)DRJOBSTATE_LOG, &logData);
		m_fnNotifyProcess((LPVOID)(LPVOID)DRJOBSTATE_END, &cfpInfo.copyData);
	}

	if (!bDRFind)
	{
		logData.bSuccess = FALSE;
		logData.nMsg = DRLOGMSG_ERROR_FINDDRFILE;
		logData.szDetail = szFindFolder;
		cfpInfo.copyData.nTotalFileSize = 0;
		cfpInfo.copyData.bSucess = FALSE;

		m_fnNotifyProcess((LPVOID)(LPVOID)DRJOBSTATE_LOG, &logData);
	}

	m_fnNotifyProcess((LPVOID)(LPVOID)DRJOBSTATE_COMPLETED, &cfpInfo.copyData); 

	return bDRFind;
}

void CDRJob::MakeDataBuf(BYTE * pDataBuf, CString & szData, int * pPos, BYTE bToken)
{
	char * pBuf	= new char[200];

	szData.Replace(bToken, ' ');

#ifdef UNICODE
	WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)szData, -1, (LPSTR)pBuf, 200, NULL, NULL );
#else
	strcpy(pBuf, szData);
#endif

	memcpy(pDataBuf + *pPos, pBuf, strlen(pBuf));
	*pPos += (int)strlen(pBuf);
	pDataBuf[(*pPos)++] = bToken;

	delete [] pBuf;
}

void CDRJob::MakeDataBuf(BYTE * pDataBuf, char * szData, int * pPos, BYTE bToken)
{
	for (int i=0; i<(int)strlen(szData); i++)
		if (szData[i] == bToken)
			szData[i] = ' ';

	memcpy(pDataBuf + *pPos, szData, (int)strlen(szData));
	*pPos += (int)strlen(szData);				
	pDataBuf[(*pPos)++] = bToken;
}

DWORD CDRJob::JobSend(DRJOBSENDDATA * pData)
{
	BOOL bSendOk = FALSE;
	BOOL bComplete = FALSE;
	CTachoFile tFile;
	CMSNetClient netClient;
	DRJOBNOTIFYDATA notifyData;
	DRLOGNOTIFYDATA logData;
	CString szFile, szExtTemp(TACHOFILEEXT_TEMPER);
	BOOL	bTemperFile;
	int	 nBufLen;

#ifdef _DEBUG
	OutputDebugString(_T("CDRJob::JobSend Start\n"));
#endif

	memset(m_szIDNet, 0, sizeof(m_szIDNet));
	m_netData.Reset();
	notifyData.nType = GetType();
	notifyData.pParam = this;
	notifyData.bSucess = FALSE;
	logData.nType = GetType();

	if (!pData->szPath.Right((int)wcslen(TACHOFILEEXT_TEMPER)).CompareNoCase(szExtTemp))
	{
		notifyData.bSucess = TRUE;
		pData->nRec = DRJOB_INVALIDERECNO;
		m_fnNotifyProcess((LPVOID)DRJOBSTATE_END, &notifyData);
		DeleteFile(pData->szPath);
		m_fnNotifyProcess((LPVOID)DRJOBSTATE_COMPLETED, &notifyData);		
		return 0;
	}

	szFile = pData->szPath.Right(pData->szPath.GetLength() - pData->szPath.ReverseFind('\\') - 1);

	if (pData->netData.bEnable)
	{
#ifdef UNICODE
		if (netClient.Connect(pData->netData.szAddr, _wtoi(pData->netData.szPort)))
#else
		if ( netClient.Connect(pData->netData.szAddr, atoi(pData->netData.szPort)))
#endif
		{
			bSendOk = TRUE;
			netClient.RegReceiveProc(OnDataReceived, this);
		}else
			NotifyLog(GetType(), DRLOGMSG_ERROR_CONNECTSERVER, szFile, FALSE);
	}

	if (!bSendOk)	
	{
		m_fnNotifyProcess((LPVOID)DRJOBSTATE_END, &notifyData);
		return 0;
	}

	m_hEventWait = ::CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!m_hEventWait)
	{
		NotifyLog(GetType(), DRLOGMSG_ERROR_MEMORY, szFile, FALSE);
		m_fnNotifyProcess((LPVOID)DRJOBSTATE_END, &notifyData);
		return FALSE;		
	}

	if (pData->szPath.Right((int)wcslen(TACHOFILEEXT_TEMPER)).CompareNoCase(szExtTemp))
	{
		nBufLen = (int)sizeof(TACHOREBUFFER);
		bTemperFile = FALSE;
	}
	else
	{
		nBufLen = 44;
		bTemperFile = TRUE;
	}

	if (tFile.Open(pData->szPath))
	{
		int i, nCount, nPos;
		BYTE * pSendBuf = new BYTE[3000];
		CString szBuf, szVer;
		TACHOREBUFFER tData;
		
		if (pSendBuf)
		{
			nPos = 3;
			szVer = tFile.GetHeaderValue(TACHOFILEDATAVER);

			pSendBuf[0] = CRNET_TOKEN_STX;
			pSendBuf[1] = DATAVERSIONIS2(szVer) ? CRNET_CMD_SOH : CRNET_CMD_ACK_GDR;
			pSendBuf[2] = CRNET_TOKEN_SEP;

			if (DATAVERSIONIS2(szVer))
			{
				MakeDataBuf(pSendBuf, szVer, &nPos);
				MakeDataBuf(pSendBuf, CRNET_DATALOGTYPE_TACHO, &nPos);
				MakeDataBuf(pSendBuf, tFile.GetHeaderValue(TACHOFILECARNO), &nPos);
				MakeDataBuf(pSendBuf, tFile.GetHeaderValue(TACHOFILEDRIVERCODE), &nPos);

				if (_wtoi(szVer) < 3)
					MakeDataBuf(pSendBuf, tFile.GetHeaderValue(TACHOFILEMILEAGE), &nPos);
				else
				{
					MakeDataBuf(pSendBuf, tFile.GetHeaderValue(TACHOFILEVIN), &nPos);
					MakeDataBuf(pSendBuf, tFile.GetHeaderValue(TACHOFILECARTYPE), &nPos);
					MakeDataBuf(pSendBuf, tFile.GetHeaderValue(TACHOFILESEIALNO), &nPos);
				}
			}else
			{
				for (i=0; i<MAX_DRHEADERITEM; i++)
					MakeDataBuf(pSendBuf, tFile.GetHeaderValue(g_pHeader[i]), &nPos);
			}

			MakeTailBuf(pSendBuf, nPos, DATAVERSIONIS2(szVer));
		
			while(1)
			{
				if (!Send(&netClient, pSendBuf, nPos + 5, pData, FALSE))
					break;

				nCount = 0;
				nPos = 3;
				pSendBuf[1] = CRNET_CMD_DAT_TXT;

				if (DATAVERSIONIS2(szVer))
					MakeDataBuf(pSendBuf, CRNET_DATALOGTYPE_TACHO, &nPos, CRNET_TOKEN_SEP);

				MakeDataBuf(pSendBuf, m_szIDNet, &nPos, CRNET_TOKEN_SEP);
				bComplete = TRUE;

				tFile.Move(pData->nRec);


				while(1)
				{
					if (!tFile.GetNextData(&tData))
						break;

					memcpy(pSendBuf+nPos, &tData, nBufLen);

					if (DATAVERSIONIS2(szVer))
					{
						nPos += nBufLen;
						pSendBuf[nPos++] = CRNET_TOKEN_HSEP;
					}
					else
					{
						pSendBuf[nBufLen+nPos] = 0x0D;
						pSendBuf[nBufLen+nPos+1] = 0x0A;
						nPos += (nBufLen + 2);
					}

					if (++nCount == RECORDSPERSEND)
					{
						nCount = 0;
						MakeTailBuf(pSendBuf, nPos, DATAVERSIONIS2(szVer));

						if (!Send(&netClient, pSendBuf, nPos + 5, pData, TRUE))
						{
							NotifyLog(GetType(), DRLOGMSG_ERROR_SENDING, szFile, FALSE);
							m_fnNotifyProcess((LPVOID)DRJOBSTATE_LOG, &logData);

							bComplete = FALSE;
							break;
						}

						if (DATAVERSIONIS2(szVer))
							nPos = (int)strlen(CRNET_DATATYPE_TACHO) + (int)strlen(m_szIDNet) + 5;
						else
							nPos = (int)strlen(m_szIDNet)+4;

						pData->nRec += RECORDSPERSEND;
					}
				}			

				if (bComplete)
				{
					if (nCount != 0)
					{
						MakeTailBuf(pSendBuf, nPos, DATAVERSIONIS2(szVer));
						Send(&netClient, pSendBuf, nPos + 5, pData, TRUE);
					}

					pSendBuf[1] = CRNET_CMD_EOF;
					nPos = (int)strlen(m_szIDNet)+4;
					MakeTailBuf(pSendBuf, nPos, DATAVERSIONIS2(szVer));
					Send(&netClient, pSendBuf, nPos + 5, pData, FALSE);

					notifyData.bSucess = TRUE;
					pData->nRec = DRJOB_INVALIDERECNO;
				}

				break;
			}
		}else
			NotifyLog(GetType(), DRLOGMSG_ERROR_MEMORY, szFile, FALSE);

		if (pSendBuf) delete [] pSendBuf;
		tFile.Close();
	}else
		NotifyLog(GetType(), DRLOGMSG_ERROR_OPENDRFILE, szFile, FALSE);

	netClient.Disconnect();

	if (m_hEventWait)
	{
		::CloseHandle(m_hEventWait);
		m_hEventWait = NULL;
	}

	m_fnNotifyProcess((LPVOID)DRJOBSTATE_END, &notifyData);

	if (bComplete)
	{
		DeleteFile(pData->szPath);
		NotifyLog(GetType(), DRLOGMSG_ERROR_OK, szFile, TRUE);
		m_fnNotifyProcess((LPVOID)DRJOBSTATE_COMPLETED, &notifyData);		
	}
	
#ifdef _DEBUG
	OutputDebugString(_T("CDRJob::JobSend End\n"));
#endif

	return 0;
}

DWORD CDRJob::JobAnalysis(DRJOBANALYSISDATA * pData)
{
	int nMsg = DRLOGMSG_ERROR_OK;
	BOOL bRet = FALSE;
	CString szBuf, szFile, szAnalPath;
	
	CTachoFile tFile;
	DRJOBNOTIFYDATA notifyData;
	DRLOGNOTIFYDATA logData;
	TRANALYSISDATA analData;
	CDRCriteriaData criData(_T(""), MakePath(SUBDIR_DATA_SYSTEM), FILE_CRITERIA);

	notifyData.nType = GetType();
	notifyData.pParam = this;
	notifyData.bSucess = FALSE;
	logData.nType = GetType();
	szFile = pData->szPath.Right(pData->szPath.GetLength() - pData->szPath.ReverseFind('\\') - 1);
	criData.Load();
	criData.SelectCriteria(pData->szCRName);

	if (tFile.Open(pData->szPath))
	{
		if (tFile.GetRecordCount() <= 0)
		{
			tFile.Close();
			DeleteFile(pData->szPath);
		
			notifyData.bSucess = TRUE;
			m_fnNotifyProcess((LPVOID)DRJOBSTATE_END, &notifyData);
			m_fnNotifyProcess((LPVOID)DRJOBSTATE_COMPLETED, &notifyData);

			return 0;
		}

		szAnalPath = MakePath(SUBDIR_DATA_UPLOAD);
		szAnalPath += '\\';
		szAnalPath += pData->szCarNo;
		szAnalPath += '.';
		szAnalPath += TACHOFILEEXT_ANALYSIS;	

		MakeSafePathname(szAnalPath);
		analData.Dest.szDest = szAnalPath;
		analData.nType = TRANALYSISTYPE_FILE;
		analData.pCriteria = &criData;

		bRet = tFile.MakeAnalysisFile(&analData);
		tFile.Close();

		if (bRet)
		{
#if !defined(SEPUNG)

			if (criData.IsAllocation())
			{
				CDRAllocFile alcFile;

				szBuf = szAnalPath.Left(szAnalPath.GetLength() - 3);
				szBuf += ACRALLOCFILE_EXT;
				MakeSafePathname(szBuf);

				alcFile.MakeAnalData(pData->pSPList, pData->szPath, szBuf);
			}

			if (pData->Ftp.bEnable)
			{
				CXTachoFile xtFile;

				szBuf = szBuf = MakePath(SUBDIR_DATA_UPLOAD);
				szBuf += '\\';
				szBuf += analData.szFile;
				szBuf = szBuf.Left(szBuf.GetLength() - 3);
				szBuf += XTACHOFILEEXT_D10;
	
				if (xtFile.MakeFile(szAnalPath, pData->szPath, szBuf))
					pData->Ftp.szPath = szBuf;
			}
#endif
			if (nMsg == DRLOGMSG_ERROR_OK)
			{
				szBuf = criData.GetDataFolder();
				szBuf += '\\';
				szBuf += pData->szCarNo;
				szBuf += '\\';
				szBuf += analData.szFile;
				
				if (MoveFileEx(pData->szPath, szBuf, MOVEFILE_REPLACE_EXISTING))
				{
					notifyData.bSucess = TRUE;

					if (pData->pJobEbCard->Is())
					{
						CString szDateTime(analData.szFile);

						pData->pJobEbCard->Do(pData->szCarNo, szBuf, szDateTime.Left(8));
					}

					m_fnNotifyProcess((LPVOID)DRJOBSTATE_END, &notifyData);
					m_fnNotifyProcess((LPVOID)DRJOBSTATE_COMPLETED, &notifyData);
					return 0;
				}else
				{
					nMsg = DRLOGMSG_ERROR_COPYFILE;
				}
			}
		}else
			nMsg = DRLOGMSG_ERROR_ANALYSIS;

	}else
		nMsg = DRLOGMSG_ERROR_OPENDRFILE;

	NotifyLog(GetType(), nMsg, szFile, FALSE);
	m_fnNotifyProcess((LPVOID)DRJOBSTATE_END, &notifyData);

	return 0;
}

DWORD CDRJob::JobStandBySend(DRJOBSTANDBYSENDDATA *  pData)
{
	CFileStatus fs;
	CFile tFile;
	DRJOBNOTIFYDATA notifyData;

#ifdef _DEBUG
	OutputDebugString(_T("CDRJob::JobStandBySend Start\n"));
#endif

	notifyData.nType = GetType();
	notifyData.pParam = this;
	notifyData.bSucess = TRUE;

	if (CFile::GetStatus(pData->szDest, fs))
	{
		if (tFile.Open(pData->szDest, CFile::modeWrite))
		{
			CTachoFile oFile;
			TACHOREBUFFER tBuffer;

			if (oFile.Open(pData->szPath))
			{
				tFile.SeekToEnd();

				while (oFile.GetNextData(&tBuffer))
				{
					tFile.Write(&tBuffer, sizeof(tBuffer));
					tFile.Write("\r\n", 2);
				}

				oFile.Close();
				DeleteFile(pData->szPath);
			}else
				notifyData.bSucess = FALSE;

			tFile.Close();
		}else
			notifyData.bSucess = FALSE;
	}else
	{
		if (!MoveFile(pData->szPath, pData->szDest))
			notifyData.bSucess = FALSE;
	}

	m_fnNotifyProcess((LPVOID)DRJOBSTATE_END, &notifyData);
	
	if (notifyData.bSucess)
		m_fnNotifyProcess((LPVOID)DRJOBSTATE_COMPLETED, &notifyData);

#ifdef _DEBUG
	OutputDebugString(_T("CDRJob::JobStandBySend End\n"));
#endif

	return 0;
}

DWORD CDRJob::JobSendFtp(DRJOBSENDFTPDATA * pData)
{
	CInternetSession *	pISession;
	CFtpConnection *	pFtpConnection = NULL;
	DRJOBNOTIFYDATA notifyData;

	notifyData.nType = GetType();
	notifyData.pParam = this;
	notifyData.bSucess = FALSE;

	pISession = new CInternetSession(_T("JobSendFtp"), INTERNET_OPEN_TYPE_PRECONFIG);

	if (pISession)
	{
		try
		{
			pFtpConnection = pISession->GetFtpConnection(pData->szAddr, pData->szUsername, pData->szPassword, pData->uiPort, TRUE);		
		}catch (CInternetException* pEx) 
		{
			pEx->Delete();
		}

		if (pFtpConnection)
		{
			CString szDest, szFile;

			szFile = pData->szPath.Right(pData->szPath.GetLength() - pData->szPath.ReverseFind('\\') - 1);

			if (pData->szDestFolder.GetLength() > 0)
			{
				szDest = pData->szDestFolder;
				szDest += '/';
				szDest += szFile;
			}else
				szDest = szFile;

			notifyData.bSucess = pFtpConnection->PutFile(pData->szPath, szDest);
			
			if (notifyData.bSucess)
				DeleteFile(pData->szPath);

			try
			{
				pFtpConnection->Close();
			}catch(...)
			{
			}

			delete pFtpConnection;

		}

		pISession->Close();
		delete pISession;
	}

	m_fnNotifyProcess((LPVOID)DRJOBSTATE_END, &notifyData);
	
	if (notifyData.bSucess)
		m_fnNotifyProcess((LPVOID)DRJOBSTATE_COMPLETED, &notifyData);

	return 0;
}

DWORD CALLBACK CDRJob::CopyProgressRoutine(LARGE_INTEGER TotalFileSize, LARGE_INTEGER TotalBytesTransferred,
						LARGE_INTEGER StreamSize,  LARGE_INTEGER StreamBytesTransferred,  DWORD dwStreamNumber,
						DWORD dwCallbackReason,  HANDLE hSourceFile,  HANDLE hDestinationFile,  LPVOID lpData)
{
	DRCOPYPROCDATA *pInfo = (DRCOPYPROCDATA *)lpData;
	

	if (pInfo && pInfo->fnNotifyProcess)
	{
		if (TotalFileSize.QuadPart == TotalBytesTransferred.QuadPart)
			pInfo->copyData.nTotalTransfered += TotalFileSize.QuadPart;

		pInfo->copyData.nCurFileSize = TotalFileSize.QuadPart;
		pInfo->copyData.nCurTransfered = TotalBytesTransferred.QuadPart;
		pInfo->fnNotifyProcess((LPVOID)DRJOBSTATE_PROGRESS, &pInfo->copyData);
	}

	return PROGRESS_CONTINUE;
}

void CDRJob::OnDataReceived(LPCTSTR szAddr, const LPBYTE pData, DWORD dwLen, const LPVOID lpPass)
{
	CDRJob * pJob = (CDRJob *)lpPass;
	
	if (pJob->m_hEventWait)
	{
		pJob->m_dwRetVal = pJob->m_netData.OnReceived(pData, dwLen);

		if (pJob->m_dwRetVal != CRNET_CMD_DUMMY)
			::SetEvent(pJob->m_hEventWait);
	}
}
