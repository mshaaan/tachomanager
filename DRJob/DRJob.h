#pragma once

#include "workerthread.h"
#include "..\\drlib\\msnetclient.h"
#include "..\\drlib\\DRnetdata.h"
#include "..\\drlib\\DRCriteriaData.h"
#include "EBCardJob.h"
#if !defined(SEPUNG)
#include "..\\drlib\\DRAllocFile.h"
#endif

#define DRJOB_INVALIDERECNO					-1

#define DRJOBSTATE_START					0
#define DRJOBSTATE_PROGRESS					1
#define DRJOBSTATE_END						2
#define DRJOBSTATE_LOG						3
#define DRJOBSTATE_COMPLETED				4

#define DRLOGMSG_ERROR_OK					0
#define DRLOGMSG_ERROR_OPENDRFILE			1
#define DRLOGMSG_ERROR_COPYFILE				2
#define DRLOGMSG_ERROR_FINDDRFILE			3
#define DRLOGMSG_ERROR_FINDSERVICEPROGRAM	4
#define DRLOGMSG_ERROR_CONNECTSERVER		5
#define DRLOGMSG_ERROR_MEMORY				6
#define DRLOGMSG_ERROR_SENDING				7
#define DRLOGMSG_ERROR_ANALYSIS				8
#define DRLOGMSG_ERROR_CREATEFOLDER			9
#define DRLOGMSG_ERROR_TABLEOPEN			10

class CDRJob;

typedef enum _DRJOBTYPE
{
	DRJobCopyFromUSB,
	DRJobSend,
	DRJobAnalysis,
	DRJobStandBySend,
	DRJobFTP,
	DRJobEBCard
} DRJOBTYPE;

typedef struct _DRJOBCOPYFROMUSBDATA
{
	CWnd *			pWnd;
	int				nIdxPort;
	CString			szFile;
	CString			szSrcPath;
	CString			szDestPath;
} DRJOBCOPYFROMUSBDATA;

typedef struct _DRSENDMSGDATA
{
	DWORD			dwMsg;
	BOOL			bEnable;
	HWND			hWnd;
	CString			szName;
}DRSENDMSGDATA;

typedef struct _DRSENDNETWORKDATA
{
	BOOL			bEnable;
	CString			szAddr;
	CString			szPort;
}DRSENDNETWORKDATA;

struct DRJOBSENDDATA
{
	int					nRec;
	CString				szPath;
	DRSENDMSGDATA		msgData;
	DRSENDNETWORKDATA	netData;
};

struct DRJOBSENDFTPDATA
{
	BOOL				bEnable;
	UINT				uiPort;
	CString				szPath;
	CString				szAddr;
	CString				szDestFolder;
	CString				szUsername;
	CString				szPassword;
	CString				szPort;
};

struct DRJOBANALYSISDATA
{
	BOOL					bAlloc;
	CString					szPath;
	CString					szCarNo;
	CString					szCRName;
	CString					szOffice;
	CString					szLine;

	DRJOBSENDFTPDATA		Ftp;
	CEBCardJob			*	pJobEbCard;
#if !defined(SEPUNG)
	ALLOCSTOPPLACEDATALIST *pSPList;
#endif
};

struct DRJOBSTANDBYSENDDATA
{
	CString				szPath;
	CString				szDest;
	CString				szAddr;
	UINT				uiPort;
};

typedef struct _COPYFILENOTIFYDATA
{
	DRJOBCOPYFROMUSBDATA *	pParam;
	LONGLONG				nTotalFileSize;
	LONGLONG				nTotalTransfered;
	LONGLONG				nCurFileSize;
	LONGLONG				nCurTransfered;
	LPCTSTR					szDestFile;
	BOOL					bSucess;
}COPYFILENOTIFYDATA;

typedef struct _DRJOBNOTIFYDATA
{
	DRJOBTYPE	nType;
	LPVOID		pParam;
	BOOL		bSucess;
} DRJOBNOTIFYDATA;

typedef struct _DRLOGNOTIFYDATA
{
	DRJOBTYPE	nType;
	BOOL		bSuccess;
	int			nMsg;
	CString		szDetail;
} DRLOGNOTIFYDATA;

class CDRJob : public CJobAbst
{
	char					m_szIDNet[CRNET_ID_LEN+1];
	char					m_szIDMsg[CRNET_ID_LEN+1];

	DRJOBTYPE				m_nJobType;

	void					MakeSafePathname(CString & szPath);
	BOOL					Send(CMSNetClient * pNet, LPBYTE pSendBuf, int nLen, DRJOBSENDDATA * pData, BOOL bData);
	void					NotifyLog(DRJOBTYPE, int nMsg, LPCTSTR szDetail, BOOL bResult);
	LPCTSTR					MakeDestFilename(CString & szDst);
	CString					TrimRight(LPCTSTR szPath);
	void					MakeDataBuf(BYTE * pBuf, CString & szData, int * pPos, BYTE bToken = CRNET_TOKEN_HSEP); 
	void					MakeDataBuf(BYTE * pBuf, char * szData, int * pPos, BYTE bToken = CRNET_TOKEN_HSEP); 

	DWORD					JobCopyFromUSB(DRJOBCOPYFROMUSBDATA * pData);
	DWORD					JobSend(DRJOBSENDDATA * param);
	DWORD					JobAnalysis(DRJOBANALYSISDATA * pData);
	DWORD					JobStandBySend(DRJOBSTANDBYSENDDATA *  pData);
	DWORD					JobSendFtp(DRJOBSENDFTPDATA * pData);

public:
	DWORD					m_dwRetVal;
	HANDLE					m_hEventWait;
	CDRNetDataClient		m_netData;

	FNDRJOBPROCESSNOTIFY	m_fnNotifyProcess;

	CDRJob(DRJOBTYPE nType);
	~CDRJob(void);

	DRJOBTYPE				GetType() { return m_nJobType; }
	DWORD					Process(void * param);

	virtual void			Delete()				{	delete (this);		}


	static DWORD CALLBACK	CopyProgressRoutine(LARGE_INTEGER TotalFileSize, LARGE_INTEGER TotalBytesTransferred,
						LARGE_INTEGER StreamSize,  LARGE_INTEGER StreamBytesTransferred,  DWORD dwStreamNumber,
						DWORD dwCallbackReason,  HANDLE hSourceFile,  HANDLE hDestinationFile,  LPVOID lpData);

	static void				OnDataReceived(LPCTSTR szAddr, const LPBYTE pData, DWORD dwLen, const LPVOID lpPass);

	inline void				ChangeExtention(CString & szPath, LPCTSTR szExt)
	{
		if (szPath.GetLength() > 3)
		{
			szPath = szPath.Left(szPath.GetLength() - 3);
			szPath += szExt;
		}
	}

	inline void				MakeTailBuf(LPBYTE pSendBuf, int nPos, BOOL isVer2)
	{
		pSendBuf[nPos] = CRNET_TOKEN_SEP;
		pSendBuf[nPos+1] = CRNET_TOKEN_ETX;
		CDRNetData::MAKECHECKSUMBUF(pSendBuf + (nPos + 2), CDRNetData::CHECKSUM(pSendBuf+1, nPos));
//		pSendBuf[nPos+4] = isVer2 ? CRNET_TOKEN_CR : CRNET_TOKEN_FF;
		pSendBuf[nPos+4] = CRNET_TOKEN_FF;
	}
};
