#include "stdafx.h"
#include "GameShared.h"


#ifdef WIN32
#pragma comment(lib,"psapi.lib")
bool IsMumbleInstalled(std::wstring* pstrOut)
{
  HKEY hkeyMumble = NULL;
  RegOpenKey(HKEY_CLASSES_ROOT,L"mumble\\shell\\open\\command",&hkeyMumble);
  if(hkeyMumble)
  {
    TCHAR szPath[MAX_PATH];
    DWORD cbPath = sizeof(szPath);
    LONG lRet = RegQueryValueEx(hkeyMumble,L"",NULL,NULL,(LPBYTE)szPath,&cbPath);
    if(lRet == ERROR_SUCCESS)
    {
      wstring strReg = szPath;
      
      wstring::iterator i = (strReg.begin() + strReg.find(L"\"") -1);
      wstring strQuote(strReg.begin(),i);
      *pstrOut = strQuote;
      return true;
    }
  }
  return false;
}
void OpenMumbleInstallLink()
{
  ShellExecute(NULL,L"open",L"http://www.tourdegiro.com/mumble.php",NULL,NULL,SW_SHOW);
}
bool IsMumbleRunning()
{
  bool fRet = false;
#ifdef _MSC_VER
  DWORD aProcesses[1024], cbNeeded, cProcesses;
  unsigned int i;

  if ( !EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded ) )
  {
      return 0;
  }
  // Calculate how many process identifiers were returned.
  cProcesses = cbNeeded / sizeof(DWORD);
  // Print the name and process identifier for each process.
  for ( i = 0; i < cProcesses && !fRet; i++ )
  {
      if( aProcesses[i] != 0 )
      {
          HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, aProcesses[i]);
          if(hProcess)
          {
            TCHAR szModuleName[MAX_PATH];
            if(GetProcessImageFileName(hProcess,szModuleName,NUMCHARS(szModuleName)))
            {
              wstring strFilename = GetFilePath(szModuleName);
              if(strFilename.compare(L"mumble.exe") == 0)
              {
                fRet = true; 
              }
            }
            CloseHandle(hProcess);
          }
      }
  }
#endif
  return fRet;
}

void StartMumble(const wstring& str, string strAUsername, int iSubChannel)
{
  if(IsMumbleRunning()) return;

  wstring strWUsername(strAUsername.begin(),strAUsername.end());

  TCHAR szParams[MAX_PATH];
  _snwprintf(szParams,NUMCHARS(szParams),L"mumble://%s@198.74.57.166/%d?version=1.2.0",strWUsername.c_str(),iSubChannel);
  ShellExecute(NULL,L"open",str.c_str(),szParams,NULL,SW_SHOWMINIMIZED  );
}
#elif defined(__APPLE__)
bool IsMumbleInstalled(std::wstring* pstrOut)
{
  return false;
}
void OpenMumbleInstallLink()
{

}
bool IsMumbleRunning()
{
  bool fRet = false;
  return fRet;
}

void StartMumble(const wstring& str, string strAUsername, int iSubChannel)
{

}
#endif