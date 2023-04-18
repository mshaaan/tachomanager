#include "StdAfx.h"
#include "EBCardJob.h"
#include "..\\drlib\\tachofile.h"
#include "..\\drlib\\DRTableCar.h"
#include "..\\drlib\\TachoFileKorta.h"
#include <algorithm>
#include <Winnetwk.h>

#define		EBCARD_FOLDERNAME			_T("Data\\EB")
#define		EBCARD_FILESIGN				_T("U2120")


CEBCardJob::CEBCardJob()
{
	m_hCopyProc = NULL;
	m_bWorking = FALSE;
	m_bEnable = FALSE;
}

void CEBCardJob::Set(LPCTSTR szFolder0)
{
	int nStart = 0, nLen, nPos;
	CString szFolder;
	CString szFolderList(szFolder0);
	
	nLen = szFolderList.GetLength();

	while((nPos = szFolderList.Find(';', nStart)) > 0)
	{
		szFolder = szFolderList.Mid(nStart, nPos-nStart);
		m_listFolder.AddTail(szFolder);
		nStart = nPos+1;
	}

	szFolder = szFolderList.Mid(nStart, nLen-nStart);
	m_listFolder.AddTail(szFolder);
}

CEBCardJob::~CEBCardJob(void)
{
	if (m_hCopyProc)
		CloseHandle(m_hCopyProc);
}

CString CEBCardJob::Find(LPCTSTR szBizID)
{
	CString szFolder;
	POSITION pos;

	pos = m_listFolder.GetHeadPosition();

	while (pos)
	{
		szFolder = m_listFolder.GetNext(pos);

		if (szFolder.Left(szFolder.Find(',')).CompareNoCase(szBizID) == 0)
			return CString(szFolder.Right(szFolder.GetLength() - szFolder.Find(',') - 1));
	}

	return CString (_T(""));
}

CString CEBCardJob::MakeBackupFoldername()
{
	wchar_t szModulePath[_MAX_PATH];
	CString szFolder;
	CFileStatus fStatus;

	GetModuleFileName(AfxGetInstanceHandle(), szModulePath, sizeof(szModulePath));
	szFolder = szModulePath;
	szFolder = szFolder.Left(szFolder.ReverseFind('\\')+1);	

	szFolder += EBCARD_FOLDERNAME;

	return CString(szFolder);
}

void CEBCardJob::Init(LPCTSTR szPath0, LPCTSTR szDrive0, LPCTSTR szUsername, LPCTSTR szPassword)
{
	int nPos1, nPos2;
	BOOL bRemain = TRUE;
	CString szBuf;
	CString szFolder;
	CFileStatus fStatus;
    NETRESOURCE NetRes ;
	CString szPath_s(szPath0), szDrive_s(szDrive0);
	CString szPath, szDrive;
	wchar_t wcszPath[_MAX_PATH], wcszDrive[_MAX_PATH];

	memset(&NetRes,0,sizeof(NETRESOURCE));
    NetRes.dwType = RESOURCETYPE_DISK; 
    NetRes.lpProvider   = NULL; 

	while(bRemain)
	{
		nPos1 = szPath_s.Find(',', 0);
		szPath.Empty();
		szDrive.Empty();

		if (nPos1 > 0)
		{
			szPath = szPath_s.Left(nPos1);
			nPos2 = szDrive_s.Find(',', 0);
			szDrive = szDrive_s.Left(nPos2);
			szPath_s = szPath_s.Right(szPath_s.GetLength() - nPos1 - 1);
			szDrive_s = szDrive_s.Right(szDrive_s.GetLength() - nPos2 - 1);
		}else
		{
			szPath = szPath_s;
			szDrive = szDrive_s;
			bRemain = FALSE;
		}
		
		wcscpy(wcszPath, szPath);
		wcscpy(wcszDrive, szDrive);

		NetRes.lpLocalName = wcszDrive;
		NetRes.lpRemoteName = wcszPath;

		if (WNetAddConnection2(&NetRes, szPassword, szUsername, CONNECT_UPDATE_PROFILE) != NO_ERROR)
		{
			DWORD dwRet = GetLastError();

			szBuf.Format(_T("CEBCardJob::Init WNetAddConnection2 Error=%d, Drive = %s, Path = %s, User = %s, Pass = %s"),  dwRet, wcszDrive, wcszPath, szUsername, szPassword);
			OutputDebugString(szBuf);
		}
	}

	szFolder = MakeBackupFoldername();

	if (!CFile::GetStatus(szFolder, fStatus))
		CreateDirectory(szFolder, NULL);
	else
	{
		DWORD dwId;

		m_bClose = FALSE;
		m_hCopyProc = CreateThread(NULL, 0, CopyThreadProc, this, 0, &dwId);

		if (m_hCopyProc)
		{
			ResumeThread(m_hCopyProc);
			SetThreadPriority(m_hCopyProc,THREAD_PRIORITY_LOWEST);
		}		
	}
}


void CEBCardJob::Set(CDaoDatabase * pDB, LPCTSTR szBizLiNo, LPCTSTR szModelNo)
{
	EBCARDATAINFO ebCarInfo;
	CDRTableEBInfo tblEBInfo(pDB);
	CDRTableCar tblCar(pDB);

	m_szBizLiNo = szBizLiNo;
	m_szModelNo = szModelNo;

	m_listCarInfo.RemoveAll();

	if (tblEBInfo.OpenTable() && tblCar.OpenTable())
	{
		tblEBInfo.Seek(_T(""), _T(">="));

		while(!tblEBInfo.IsEOF())
		{
			tblEBInfo.GetField(DRFIELDNAME_REGNO, ebCarInfo.szCarNo);
			ebCarInfo.szBizID = tblEBInfo.GetBizID();
			ebCarInfo.szCarID = tblEBInfo.GetCarID();

			if (tblCar.Seek(ebCarInfo.szCarNo))
			{
				tblCar.GetField(DRFIELDNAME_TYPE, ebCarInfo.szType);
				tblCar.GetField(DRFIELDNAME_INDNO, ebCarInfo.szVin);
			}

			m_listCarInfo.AddTail(ebCarInfo);
			tblEBInfo.MoveNext();
		}

		tblEBInfo.CloseTable();
		tblCar.CloseTable();
	}
}

int CEBCardJob::Search(LPCTSTR szCarNo)
{
	int nMid;
	int nRet = -1;
	int nStart = 0;
	int nEnd = m_listCarInfo.GetCount() -1;
	EBCARDATAINFO ebCarInfo;

	while(nStart <= nEnd)
	{
		nMid = (nStart + nEnd) / 2;
		ebCarInfo = m_listCarInfo.GetAt(m_listCarInfo.FindIndex(nMid));
		nRet = ebCarInfo.Compare(szCarNo);

		if (nRet == 0)
		{
			Set(ebCarInfo.szBizID, ebCarInfo.szCarID, ebCarInfo.szType, ebCarInfo.szVin);
			return nMid;
		}
		else if (nRet > 0)
			nEnd = nMid - 1;
		else
			nStart = nMid + 1;
	}

	return -1;
}

void CEBCardJob::Set(LPCTSTR szBizID, LPCTSTR szCarID, LPCTSTR szType, LPCTSTR szVin)
{
	m_szBizID = szBizID;
	m_szCarID = szCarID;
	m_szType = szType;
	m_szVin = szVin;
}

BOOL CEBCardJob::Do(LPCTSTR szCarNo, LPCTSTR szPath, LPCTSTR szDateTime)
{
	CString szDestFolder, szDstPath;
	SYSTEMTIME st;
	CFileStatus fStatus;
	CTachoFile oriFile;
	CTachoFileKorta newFile;
	CString szMsg;

	if (Search(szCarNo) != -1)
	{
		TACHOREBUFFER rBuffer;

		GetLocalTime(&st);
		szDestFolder = Find(m_szBizID);

		if (!CFile::GetStatus(szDestFolder, fStatus))
			szDestFolder = MakeBackupFoldername();

		szDstPath.Format(_T("%s\\%s.%s.%s.%s000000.00000000.%4d%02d%02d"), szDestFolder, EBCARD_FILESIGN, m_szBizID, m_szCarID, szDateTime, 
			st.wYear, st.wMonth, st.wDay); 

		if (oriFile.Open(szPath) && oriFile.GetNextData(&rBuffer))
		{
			if (newFile.Open(szDstPath, CFile::modeWrite | CFile::modeCreate))
			{
				BYTE bufHeader[81];
				char cAzimuth[4];
				
				memset(cAzimuth, 0, sizeof(cAzimuth));
				memcpy(cAzimuth, rBuffer.Azimuth, sizeof(rBuffer.Azimuth));

				if (atoi(cAzimuth) > 359)
					memcpy(rBuffer.Azimuth, "000", sizeof(rBuffer.Azimuth));

				newFile.MakeHeaderValue(bufHeader, m_szModelNo, m_szVin, m_szType, szCarNo, m_szBizLiNo, oriFile.GetHeaderValue(TACHOFILEDRIVERCODE));
				newFile.Write(bufHeader, 81);
				newFile.Write(&rBuffer, sizeof(rBuffer));
				newFile.Write("\r\n", 2);

				while(oriFile.GetNextData(&rBuffer))
				{
					memcpy(cAzimuth, rBuffer.Azimuth, sizeof(rBuffer.Azimuth));

					if (atoi(cAzimuth) > 359)
						memcpy(rBuffer.Azimuth, "000", sizeof(rBuffer.Azimuth));

					newFile.Write(&rBuffer, sizeof(rBuffer));
					newFile.Write("\r\n", 2);
				}

				newFile.Close();

				{
					szMsg.Format(_T("EBCard Copy Sucess %s"), szDstPath); 
					OutputDebugString(szMsg);
				}
			}else
			{
				szMsg.Format(_T("EBCard Create Fail %s"), szDstPath); 
				OutputDebugString(szMsg);
			}

			oriFile.Close();
		}
	}else
	{
		CString szMsg;

		szMsg.Format(_T("EBCard %s Search Fail"), szCarNo); 
		OutputDebugString(szMsg);
	}

	return TRUE;
}

DWORD WINAPI CEBCardJob::CopyThreadProc(LPVOID lpParam)
{
	((CEBCardJob *)lpParam)->CopyProcess();

	return 0;
}

void CEBCardJob::CopyProcess()
{
	MSG msg;
	CString szFolder, szPath, szFile, szBizID, szDstPath;
	BOOL bFind = TRUE;
	HANDLE hFind;
	WIN32_FIND_DATA wFind;

	szFolder = MakeBackupFoldername();
	szPath = szFolder;
	szPath += _T("\\*.*");
	hFind = FindFirstFile((LPCTSTR)szPath, &wFind);
	m_bWorking = TRUE;


	while (!m_bClose && hFind && bFind)
	{
		if (*wFind.cFileName != '.')
		{
			szFile = wFind.cFileName;

			if (szFile.Left(wcslen(EBCARD_FILESIGN)).CompareNoCase(EBCARD_FILESIGN) == 0)
			{
				szPath = szFolder;
				szPath += '\\';
				szPath += szFile;

				szBizID = szFile.Mid(szFile.Find('.')+1, 7);
				szDstPath = Find(szBizID);

				szDstPath += '\\';
				szDstPath += szFile;

				if (CopyFile(szPath, szDstPath, FALSE))
					DeleteFile(szPath);
				else
				{
					DWORD dwRet = ::GetLastError();
					
					if (dwRet)
						dwRet = dwRet;

					CString szBuf = _T("CEBCardJob::COPY Error =&s");
					szBuf += szDstPath;
					OutputDebugString(szBuf);

				}
				
				while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
		}

		bFind = FindNextFile(hFind, &wFind);
	}

	if (hFind)
		FindClose(hFind);

	m_bWorking = FALSE;
}

