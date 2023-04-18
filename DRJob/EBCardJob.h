#pragma once

#include "..\\drlib\\DRTableEBInfo.h"

struct EBCARDATAINFO
{
	CString		szCarNo;
	CString		szBizID;
	CString		szCarID;
	CString		szType;
	CString		szVin;
	int			Compare(LPCTSTR szCarNo0)
	{
		return	szCarNo.CompareNoCase(szCarNo0);
	}
};

class CEBCardJob
{
	BOOL			m_bEnable;
	BOOL			m_bWorking;
	BOOL			m_bClose;
	HANDLE			m_hCopyProc;
	CString			m_szBizID, m_szCarID, m_szBizLiNo, m_szModelNo, m_szType, m_szVin;
	CStringList		m_listFolder;
	CList<EBCARDATAINFO, EBCARDATAINFO &> m_listCarInfo;

	int				Search(LPCTSTR szCarNo);
	CString			Find(LPCTSTR szBizID);
	CString			MakeBackupFoldername();

public:
	CEBCardJob();
	~CEBCardJob(void);

	void			Init(LPCTSTR szPath, LPCTSTR szDrive, LPCTSTR szUsername, LPCTSTR szPassword);
	BOOL			Is()					{	return m_bEnable;		}
	void			Set(BOOL bEnable= TRUE)	{	m_bEnable = bEnable;	}
	void			Set(CDaoDatabase * pDB, LPCTSTR szBizLNo, LPCTSTR szModelNo);
	void			Set(LPCTSTR szFolder);
	void			Set(LPCTSTR szBizID, LPCTSTR szCarID, LPCTSTR szType, LPCTSTR szVin);
	BOOL			Do(LPCTSTR szCarNo, LPCTSTR szPath, LPCTSTR szDateTime);
	void			CopyProcess();
	void			Close()					{	m_bClose = TRUE;	}
	BOOL			IsWorking()				{	return m_bWorking;	}

	static DWORD WINAPI CopyThreadProc(LPVOID lpParam);
};

