// This is the main DLL file.
#include "stdafx.h"
#include "ArtTools.h"

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "jama/jama_lu.h"


using namespace std;
 
struct MemoryStruct {
  char *memory;
  size_t size;
};
 
 
size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  vector<char>* memBuf= (vector<char>*)userp;
 
  const char* pszContents = (const char*)contents;
  for(int x = 0;x < realsize; x++)
  {
    (*memBuf).push_back(pszContents[x]);
  }
  return realsize;
}

bool MultiPartPost(std::string strTarget, std::map<std::string,std::string> mapPostParams, std::string strFileToUpload)
{
  CURL *curl;
  CURLcode res;
 
  struct curl_httppost *formpost=NULL;
  struct curl_httppost *lastptr=NULL;
  struct curl_slist *headerlist=NULL;
  static const char buf[] = "Expect:";
 
  curl_global_init(CURL_GLOBAL_ALL);
 
  /* Fill in the file upload field */ 
  curl_formadd(&formpost,
               &lastptr,
               CURLFORM_COPYNAME, "tcx",
               CURLFORM_FILE, strFileToUpload.c_str(),
               CURLFORM_END);
 
  for(map<string,string>::const_iterator i = mapPostParams.begin(); i != mapPostParams.end(); i++)
  {
    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, i->first.c_str(),
                 CURLFORM_COPYCONTENTS, i->second.c_str(),
                 CURLFORM_END);

  }
 
  /* Fill in the submit field too, even if this is rarely needed */ 
  curl_formadd(&formpost,
               &lastptr,
               CURLFORM_COPYNAME, "submit",
               CURLFORM_COPYCONTENTS, "send",
               CURLFORM_END);
 

  std::vector<char> outBuf;

  curl = curl_easy_init();
  /* initalize custom header list (stating that Expect: 100-continue is not
     wanted */ 
  headerlist = curl_slist_append(headerlist, buf);
  if(curl) 
  {
    /* what URL that receives this POST */ 
    curl_easy_setopt(curl, CURLOPT_URL, strTarget.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&outBuf);

    curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
 
    /* Perform the request, res will get the return code */ 
    res = curl_easy_perform(curl);
    /* Check for errors */ 
    if(res != CURLE_OK)
    {
      fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(res));
    }
    else
    {
      string str(outBuf.begin(),outBuf.end());
      cout<<"Data upload complete!"<<endl;
    }
 
    /* always cleanup */ 
    curl_easy_cleanup(curl);
 
    /* then cleanup the formpost chain */ 
    curl_formfree(formpost);
    /* free slist */ 
    curl_slist_free_all (headerlist);
  }
  return 0;
}
HRESULT DownloadFileToMemory(wstring strURL, vector<char>& outBuf)
{
  
  string strURLA(strURL.begin(),strURL.end());

  CURL* curl = curl_easy_init();
 
  /* specify URL to get */ 
  curl_easy_setopt(curl, CURLOPT_URL, strURLA.c_str());
 
  /* send all data to this function  */ 
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
 
  /* we pass our 'chunk' struct to the callback function */ 
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&outBuf);
 
  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */ 
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
 
  /* get it! */ 
  curl_easy_perform(curl);

  curl_easy_cleanup(curl);

  curl_global_cleanup();

  return S_OK;
}

int ArtFopen(FILE** ppOut,const char* pszFilename,const char* pszArgs)
{
#ifdef _MSC_VER
  return fopen_s(ppOut,pszFilename,pszArgs);
#else
  FILE* pf = fopen(pszFilename,pszArgs);
  if(!pf)
  {
    return -1;
  }
  *ppOut = pf;
  return 0;
#endif
}
#ifdef __APPLE__
int ArtDeleteFile(LPCTSTR lpsz)
{
  wstring str(lpsz);
  string strA(str.begin(),str.end());
  return unlink(strA.c_str());
}
int ArtCreateDirectory(LPCTSTR lpsz, LPVOID null)
{
  wstring str(lpsz);
  string strA(str.begin(),str.end());
  return mkdir(strA.c_str(),0755);
}
#endif // __APPLE__
HRESULT DownloadFileTo(wstring strURL, wstring strTargetFile)
{
  HRESULT hr = 0;
  CURL* curl;
  CURLcode res;

  curl = curl_easy_init();
  if(curl)
  {
    string strURLA(strURL.begin(),strURL.end());
    string strFileA(strTargetFile.begin(),strTargetFile.end());
#ifdef _MSC_VER
    FILE* pOutFile = _wfopen(strTargetFile.c_str(),L"wb");
#else
    FILE* pOutFile = fopen(strFileA.c_str(),"wb");
#endif

    if(pOutFile)
    {
      curl_easy_setopt(curl,CURLOPT_URL, strURLA.c_str());
      curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1);
      curl_easy_setopt(curl,CURLOPT_USERAGENT,"Tour De Giro");
      curl_easy_setopt(curl,CURLOPT_FILE,pOutFile);

      res = curl_easy_perform(curl);

      fclose(pOutFile);
    }
  }

  curl_easy_cleanup(curl);

  curl_global_cleanup();
  return hr;
}

HRESULT HitURL(wstring strURL)
{
  HRESULT hr = 0;
#ifdef _WIN32
  
  CURL* curl;
  CURLcode res;

  curl = curl_easy_init();
  if(curl)
  {
    string strURLA(strURL.begin(),strURL.end());

    curl_easy_setopt(curl,CURLOPT_URL, strURLA.c_str());
    curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1);
    curl_easy_setopt(curl,CURLOPT_USERAGENT,"Tour De Giro");

    res = curl_easy_perform(curl);
  }

  curl_easy_cleanup(curl);

  curl_global_cleanup();
  /*HANDLE hTempFile =CreateFile(strTargetFile.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
  if(hTempFile == INVALID_HANDLE_VALUE) hr = HRESULT_FROM_WIN32(GetLastError());

  if(SUCCEEDED(hr))
  {
    HINTERNET hURL = InternetOpenUrl( hInternet,
                                      strURL.c_str(),
                                      NULL,
                                      0,
                                      INTERNET_FLAG_RELOAD,
                                      0);

    if(!hURL) hr = HRESULT_FROM_WIN32(GetLastError());
      
    if(SUCCEEDED(hr))
    {
      DWORD dwBytesTotal = 0;
      BOOL fDone = FALSE;
      while(!fDone)
      {
        BYTE pbBuffer[8192];
        DWORD dwBytesRead = 0;
        BOOL fRet = InternetReadFile(hURL,pbBuffer,1024,&dwBytesRead);
        if(fRet)
        {
          DWORD dwBytesWritten = 0;
          fRet = WriteFile(hTempFile,pbBuffer,dwBytesRead,&dwBytesWritten,NULL);
          if(fRet)
          {
            dwBytesTotal += dwBytesWritten;

            if(dwBytesWritten == 0)
            {
              hr = S_OK;
              break;
            }
          }
          else
          {
            hr = HRESULT_FROM_WIN32(GetLastError());
            break;
          }
        }
        else
        {
          hr = HRESULT_FROM_WIN32(GetLastError());
          break;
        }
      }

      InternetCloseHandle(hURL);
    }
  }

  if(hTempFile != INVALID_HANDLE_VALUE)
  {
    CloseHandle(hTempFile);
  }*/
#endif
  return hr;
}
unsigned int ArtGetTime()
{
  static bool fFirst = false;
  static boost::posix_time::ptime tmFirst;
  if(!fFirst)
  {
    tmFirst = boost::posix_time::microsec_clock::local_time();
    fFirst = true;
    return 0;
  }
  boost::posix_time::ptime pNow = boost::posix_time::microsec_clock::local_time();
  boost::posix_time::time_duration diff = (pNow - tmFirst);

  return diff.total_milliseconds();
}


#include "boost/filesystem.hpp" 
BOOL ArtFileExists(LPCTSTR szPath)
{
  wstring strW(szPath);
  string strA(strW.begin(),strW.end());
  return boost::filesystem::exists(strA);
}

bool IsEqualGUID(const boost::uuids::uuid& id1, const boost::uuids::uuid& id2)
{
  return id1 == id2;
}

double RandDouble()
{
  return ((double)(rand() % 10000)) / 10000.0;
};
int GetCheckSum(LPVOID pvData, const int cbData)
{
  int iRet = 0;
  char* pbData = (char*)pvData;
  for(int x = 0;x < cbData; x++)
  {
    iRet += pbData[x];
  }
  return iRet;
}

string UrlEncode(const char* str) 
{
  char szTemp[100];
  string result;
  result.reserve(strlen(str) + 1);
  for (char ch = *str; ch != '\0'; ch = *++str) {
    switch (ch) {
      case '%':
      case '=':
      case '&':
      case '\n':
      case ' ':
      case '\t':
        _snprintf(szTemp,sizeof(szTemp),"%%%02x",(unsigned char)ch);
        result.append(szTemp);
        break;
      default:
        result.push_back(ch);
        break;
    }
  }
  return result;
}
wstring UrlEncode(const TCHAR* str)
{
  TCHAR szTemp[100];
  wstring result;
  result.reserve(wcslen(str) + 1);
  for (TCHAR ch = *str; ch != '\0'; ch = *++str) {
    switch (ch) {
      case '%':
      case '=':
      case '&':
      case '\n':
      case ' ':
      case '\t':
        _snwprintf(szTemp,NUMCHARS(szTemp),L"%%%02x",static_cast<unsigned char>(ch));
        result.append(szTemp);
        break;
      default:
        result.push_back(ch);
        break;
    }
  }
  return result;
}
/*
BOOL DllMain(HINSTANCE hDLL, DWORD fdwReason, LPVOID lpReserved)
{
  volatile int i = 1;
  if(i)
  {
    return TRUE;
  }

  FormatTimeMinutesSecondsMs(0,0,0);
  return TRUE;
}*/

void FormatTimeMinutesSecondsMs(float flTimeInSeconds, char* lpszBuffer, int cchBuffer)
{
  int cMinutes = (int)(flTimeInSeconds / 60);
  int cSeconds = ((int)flTimeInSeconds) % 60;
  int cHundredths = (int)(100 * (flTimeInSeconds - ((int)flTimeInSeconds)));

  _snprintf(lpszBuffer, cchBuffer, "%02d:%02d.%02d",cMinutes,cSeconds,cHundredths);
}

std::string FormatDecimals(float fl, int digits)
{
  char sz[100];
  char szFormat[100];
  snprintf(szFormat,sizeof(szFormat),"%%%df",digits);
  snprintf(sz,sizeof(sz),szFormat,fl);
  return sz;
}

template<>
void TemplatedFunction<1>()
{
  Noop();
}

bool IsFileTimeEqual(const FILETIME& ft1, const FILETIME& ft2)
{
  return ft1.dwHighDateTime == ft2.dwHighDateTime && ft1.dwLowDateTime == ft2.dwLowDateTime;
}
boost::posix_time::ptime local_ptime_from_utc_time_t(std::time_t const t)
{
    using boost::date_time::c_local_adjustor;
    using boost::posix_time::from_time_t;
    using boost::posix_time::ptime;
    return c_local_adjustor<ptime>::utc_to_local(from_time_t(t));
}

SYSTEMTIME SecondsSince1970ToSYSTEMTIME(int cSeconds, bool fInLocalTime)
{
  boost::posix_time::ptime p = fInLocalTime ? 
                                  local_ptime_from_utc_time_t(cSeconds) :
                                  boost::posix_time::from_time_t(cSeconds);
  
  SYSTEMTIME st;
  st.wDay = p.date().day();
  st.wDayOfWeek = p.date().day_of_week();
  st.wHour = p.time_of_day().hours();
  st.wMilliseconds = 0;
  st.wMinute = p.time_of_day().minutes();
  st.wMonth = p.date().month();
  st.wSecond = p.time_of_day().seconds();
  st.wYear = p.date().year();
  return st;
}

void UnixTimeToFileTime(time_t t, FILETIME* pft)
{
  // Note that LONGLONG is a 64-bit value
  LONGLONG ll;
  
  ll = (LONGLONG)t * (LONGLONG)10000000 + 116444736000000000;
  pft->dwLowDateTime = (DWORD)ll;
  pft->dwHighDateTime = ll >> 32;
}
FILETIME SecondsSince1970ToFILETIME(int cSeconds)
{
  FILETIME ft;
  UnixTimeToFileTime(cSeconds,&ft);
  return ft;
}

DWORD GetSecondsSince1970GMT()
{
  time_t tm;
  time(&tm);
  return tm;
}
DWORD GetSecondsSince1970()
{
  return GetSecondsSince1970GMT();
}

wstring GetFilePath(LPCTSTR lpszFullPath)
{
  const int cch = wcslen(lpszFullPath);
  for(int x = cch-1; x >= 0; x--)
  {
    if(lpszFullPath[x] == '\\')
    {
      wstring ret = &lpszFullPath[x+1];
      return ret;
    }
  }
  return L"";
}

// a very simple integer conversion function.  Doesn't support negatives, but fails properly instead of returning 0
bool ArtAtoi(LPCSTR lpsz, int cch, int* pOut)
{
  int ixStart = 0;
  while((lpsz[ixStart] == ' ' || lpsz[ixStart] == '\t') && ixStart < cch) ixStart++;

  if(lpsz[ixStart] == '-')
  {
    // negative, that's fine
    ixStart++;
  }

  for(int x = ixStart;x < cch; x++)
  {
    if(lpsz[x] >= '0' && lpsz[x] <= '9')
    {
    }
    else
    {
      return false;
    }
  }
  *pOut = atoi(lpsz);
  return true;
}

LONGLONG GetFileSize(const std::wstring& path)
{
  string strA(path.begin(),path.end());
  return boost::filesystem::file_size(strA);
}

bool DoesFileExist(const std::wstring& path)
{
#ifdef _MSC_VER
  string strA(path.begin(),path.end());
  boost::filesystem3::path fs3path(strA);
  return boost::filesystem::exists(fs3path);
#else
  string strA(path.begin(),path.end());
  int ret = access(strA.c_str(),R_OK);
  return ret == 0;
#endif
}


#define LOWCASE(x) ((x)& (~0x20))

int nospacecompare(LPCTSTR lpsz1, LPCTSTR lpsz2)
{
  int ix1 = 0;
  int ix2 = 0;
  while(true)
  {
    if(lpsz1[ix1] == ' ')
    {
      ix1++; // skip the space
      continue;
    }
    if(lpsz2[ix2] == ' ') 
    {
      ix2++; // skip the space
      continue;
    }
    TCHAR c1 = LOWCASE(lpsz1[ix1]);
    TCHAR c2 = LOWCASE(lpsz2[ix2]);
    if(c1 != c2) 
      return 1;
    DASSERT(lpsz1[ix1] == lpsz2[ix2]); // they must match for this character...

    if(lpsz1[ix1] == 0) 
      return 0; // we found the end of the string.  we know that lpsz2[ix2] is the same, so this string matches!
    DASSERT(lpsz1[ix1] != 0 && lpsz2[ix2] != 0); // both these characters are nonzero
    ix1++;
    ix2++;
  }
  return 0;
}

void QuadEquation(float a, float b, float c, float* rgAnswers, int* pcAnswers)
{
  float flSqrtInput = b*b - 4*a*c;
  if(flSqrtInput < 0)
  {
    *pcAnswers = 0;
  }
  else if(flSqrtInput == 0)
  {
    *pcAnswers = 1;
    rgAnswers[0] = -b / 2*a;
  }
  else
  {
    *pcAnswers = 2;
    
    rgAnswers[0] = (-b + sqrt(flSqrtInput))/2*a;
    rgAnswers[1] = (-b - sqrt(flSqrtInput))/2*a;
  }
}

bool IsNaN(float f)
{
  return f!=f;
}

bool GetLastModifiedTime(const std::wstring& strFile, time_t* pTime)
{
  string strA(strFile.begin(),strFile.end());
  if(boost::filesystem::exists(strA))
  {
    *pTime = boost::filesystem::last_write_time(strA);
    return true;
  }
  else
  {
    return false;
  }
}

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN  /* We always want minimal includes */
#include <Windows.h>
#include <Shlobj.h>
#else
#include <stdlib.h>
#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#endif
std::wstring GetRoamingPath(std::wstring strPath)
{
#ifdef WIN32
  TCHAR szPath[MAX_PATH];
  
  if(SUCCEEDED(SHGetFolderPath(NULL,CSIDL_APPDATA,NULL,0,szPath)))
  {
    return std::wstring(szPath) + std::wstring(L"\\") + strPath;
  }
  return L"";
#elif defined(__APPLE__)
  for(int x = 0;x < strPath.length(); x++)
  {
    if(strPath[x] == '\\') strPath[x] = '/';
  }
  const char *homeDir = getenv("HOME");
  
  if (!homeDir) {
    struct passwd* pwd = getpwuid(getuid());
    if (pwd)
      homeDir = pwd->pw_dir;
  }
  string strHomeDir = homeDir;
  wstring strHomeDirW(strHomeDir.begin(),strHomeDir.end());
  return strHomeDirW + L"/Documents/" + strPath;
#endif
}

bool Regression(const vector<double>& rgX, const vector<double>& rgY, vector<double>& coefsOut)
{
  if(rgX.size() != rgY.size()) return false;
		
  const int nCoefs = coefsOut.size(); // how many coefficients I want to solve for.
  if(nCoefs <= 0) return false;

  vector<double> rgSquares;
  rgSquares.resize(nCoefs*2 + 1);
  for(unsigned int power = 0; power < rgSquares.size(); power++)
  {
    double dSum = 0;
    for(unsigned int x = 1;x < rgX.size(); x++) // note: skipping the first sample since it won't have an accurate power-loss number
    {
      dSum += pow(rgX[x], (double)power);
    }
    rgSquares[power] = dSum;
  }
		
  Array1D<double> known(nCoefs);
  for(int r = 0; r < nCoefs; r++)
  {
    double dSum = 0;
    for(unsigned int x = 1; x < rgX.size(); x++) // note: skipping the first sample since it won't have an accurate power-loss number
    {
      dSum += rgY[x]*pow(rgX[x], r);
    }
    known[r] = dSum;
  }

  TNT::Array2D<double> mainMatrix(nCoefs,nCoefs);
  for(int r = 0; r < nCoefs; r++)
  {
    for(int c = 0;c < nCoefs; c++)
    {
      mainMatrix[r][c] = rgSquares[r+c];
    }
  }
  JAMA::LU<double> aMatrixSolver(mainMatrix);
  Array1D<double> vSolution = aMatrixSolver.solve(known);
  if(vSolution.dim() > 0)
  {
    for(int x = 0; x < nCoefs; x++)
    {
      coefsOut[x] = vSolution[x];
    }
  
    return true;
  }
  else
  {
    return false;
  }
}


#ifdef WIN32
#include <CommDlg.h>
// shows the open file dialog, sticks result in szPath...
// true -> path is good
// false -> was cancelled
bool ArtGetOpenFileName(HWND hWndOwner, LPCTSTR lpszTitle, LPTSTR lpszPath, int cchPath, LPCTSTR lpszFilter)
{
  lpszPath[0] = '\0';
  OPENFILENAME ofn;
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hWndOwner;
  ofn.hInstance = NULL;
  ofn.lpstrFilter = lpszFilter;
  ofn.lpstrCustomFilter = NULL;
  ofn.nMaxCustFilter = 0;
  ofn.nFilterIndex = 0;
  ofn.lpstrFile = lpszPath;
  ofn.nMaxFile = cchPath;
  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.lpstrTitle = lpszTitle;
  ofn.Flags = OFN_NOCHANGEDIR;
  ofn.nFileOffset = 0;
  ofn.nFileExtension = 0;
  ofn.lpstrDefExt = NULL;
  ofn.lpfnHook = NULL;
  ofn.lpTemplateName = NULL;
  ofn.pvReserved = 0;
  ofn.dwReserved = 0;
  ofn.FlagsEx = 0;
  BOOL fSuccess =  GetOpenFileName(&ofn);
  return !!fSuccess;
}
bool ArtGetFolderSave(HWND hWndOwner, LPCTSTR lpszTitle, LPTSTR lpszPath, int cchPath)
{
  BROWSEINFO info;
  memset(&info,0,sizeof(info));
  info.hwndOwner = hWndOwner;
  info.pidlRoot = NULL;
  info.pszDisplayName = lpszPath;
  info.ulFlags = BIF_RETURNONLYFSDIRS;
  info.lpfn = NULL;
  info.lParam = NULL;
  info.iImage = 0;
  PIDLIST_ABSOLUTE pid = SHBrowseForFolder(&info);
  
  if ( pid && SHGetPathFromIDList ( pid, lpszPath ) )
  {
      wprintf( L"Selected Folder: %s\n", lpszPath );
  }
  // free memory used
  IMalloc * imalloc = 0;
  if ( SUCCEEDED( SHGetMalloc ( &imalloc )) )
  {
      imalloc->Free ( pid );
      imalloc->Release ( );
  }

  return pid != 0 && ::DoesFileExist(lpszPath);
}
BOOL ArtDirectoryExists(LPCTSTR szPath)
{
  DWORD dwAttrib = GetFileAttributes(szPath);

  return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
         (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}
#endif
/*
bool ArtGetSaveFileName(HWND hWndOwner, LPCTSTR lpszTitle, LPTSTR lpszPath, int cchPath, LPCTSTR lpszFilter)
{
  lpszPath[0] = '\0';
  OPENFILENAME ofn;
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hWndOwner;
  ofn.hInstance = NULL;
  ofn.lpstrFilter = lpszFilter;
  ofn.lpstrCustomFilter = NULL;
  ofn.nMaxCustFilter = 0;
  ofn.nFilterIndex = 0;
  ofn.lpstrFile = lpszPath;
  ofn.nMaxFile = cchPath;
  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.lpstrTitle = lpszTitle;
  ofn.Flags = 0;
  ofn.nFileOffset = 0;
  ofn.nFileExtension = 0;
  ofn.lpstrDefExt = NULL;
  ofn.lpfnHook = NULL;
  ofn.lpTemplateName = NULL;
  ofn.pvReserved = 0;
  ofn.dwReserved = 0;
  ofn.FlagsEx = 0;
  BOOL fSuccess =  GetSaveFileName(&ofn);
  return !!fSuccess;
}*/
// creates paths until the final one
bool ArtCreateDirectories(LPCTSTR lpszPath_)
{
  TCHAR szPath[MAX_PATH];
  wcscpy(szPath,lpszPath_);
  
  LPTSTR pathmark=0;
#ifdef _MSC_VER
  pathmark = L"\\";
#else
  pathmark = L"/";
#endif
  LPTSTR lpszSlash = wcsstr(szPath,pathmark);
  while(lpszSlash && lpszSlash[0] == pathmark[0])
  {
    TCHAR ch = lpszSlash[0];
    lpszSlash[0] = 0;
    if(!ArtDirectoryExists(szPath))
    {
      if(!ArtCreateDirectory(szPath,NULL))
      {
        return false;
      }
    }
    lpszSlash[0] = ch;
    lpszSlash = wcsstr(lpszSlash+1,pathmark);
  }
  return true;
}

bool g_fAssertsEnabled=true;
void DASSERTInternal(bool f)
{
#ifdef _MSC_VER
  if(!f && g_fAssertsEnabled && IsDebuggerPresent())
  {
    __asm int 3;
  }
#endif
}
void EnableAsserts(bool f)
{
  g_fAssertsEnabled = f;
}


#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
   DWORD dwType; // Must be 0x1000.
   LPCSTR szName; // Pointer to name (in user addr space).
   DWORD dwThreadID; // Thread ID (-1=caller thread).
   DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

void SetThreadName( DWORD dwThreadID, const char* threadName)
{
#ifdef _MSC_VER
   THREADNAME_INFO info;
   info.dwType = 0x1000;
   info.szName = threadName;
   info.dwThreadID = dwThreadID;
   info.dwFlags = 0;

   __try
  {
      const DWORD MS_VC_EXCEPTION=0x406D1388;
      RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
   }
   __except(EXCEPTION_EXECUTE_HANDLER)
   {
   }
#endif
}

std::string RandString()
{
  std::string ret = "12345678";
  for(unsigned int x = 0;x < ret.length(); x++)
  {
    int r = rand()%26;
    ret[x] = 'a' + r;
  }
  return ret;
}

#ifdef _MSC_VER

LONG ArtInterlockedExchangeAdd(LONG* plTarget, LONG lAddThis)
{
  return InterlockedExchangeAdd(plTarget,lAddThis);
}
LONG ArtInterlockedExchange(LONG* pl,LONG lNewValue)
{
  return InterlockedExchange(pl,lNewValue);
}
LONG ArtInterlockedIncrement(LONG* pl)
{
  return InterlockedIncrement(pl);
}
LONG ArtInterlockedDecrement(LONG* pl)
{
  return InterlockedDecrement(pl);
}

void ArtFileTimeToSystemTime(const FILETIME* pft,SYSTEMTIME* pst)
{
  FileTimeToSystemTime(pft,pst);
}

void OpenWofStream(ofstream& o, LPCTSTR lpszPath)
{
  wstring strPathW(lpszPath);
  string strPathA(strPathW.begin(),strPathW.end());
  o.open(strPathA.c_str());
}
void OpenWifStream(ifstream& i, LPCTSTR lpszPath)
{
  wstring strPathW(lpszPath);
  string strPathA(strPathW.begin(),strPathW.end());
  i.open(strPathA.c_str());
}
void ArtGetLocalTime(SYSTEMTIME* pst)
{
  GetLocalTime(pst);
}
#include "psapi.h"
DWORD ArtGetMemoryUsage(DWORD* pcHandles)
{
  DWORD dwRet = 0;
  DWORD procId = ::GetCurrentProcessId();
  HANDLE hProcess = OpenProcess(  PROCESS_QUERY_INFORMATION |
                                    PROCESS_VM_READ,
                                    FALSE, procId );
  if(hProcess)
  {
    PROCESS_MEMORY_COUNTERS pmc={0};
    pmc.cb = sizeof(pmc);
    if ( GetProcessMemoryInfo( hProcess, &pmc, sizeof(pmc)))
    {
      dwRet = pmc.WorkingSetSize;
    }

    if(pcHandles)
    {
      GetProcessHandleCount(hProcess,pcHandles);
    }

    CloseHandle(hProcess);
  }
  return dwRet;
}


bool IsVLCInstalled(std::wstring* pstrOut)
{
  LPCTSTR lpszTest = L"C:\\Program Files (x86)\\VideoLAN\\VLC\\vlc.exe";
  LPCTSTR lpszTest2 = L"C:\\Program Files\\VideoLAN\\VLC\\vlc.exe";
  if(::ArtFileExists(lpszTest))
  {
    *pstrOut = lpszTest;
    return true;
  }
  if(::ArtFileExists(lpszTest2))
  {
    *pstrOut = lpszTest2;
    return true;
  }
  return false;
}

// windows
#else
// OSX
DWORD ArtGetMemoryUsage(DWORD* pcHandles)
{
  return 0;
}
bool IsVLCInstalled(std::wstring* pstrOut)
{
  return false;
}

#include <libkern/OSAtomic.h> // OSAtomicAdd functions


bool IsDebuggerPresent()
{
  return false;
}

#include <carbon/carbon.h>
//#include <Navigation.h>
bool ArtGetFolderSave(HWND hWndOwner, LPCTSTR lpszTitle, LPTSTR lpszPath, int cchPath)
{
  char blah[5-sizeof(long)]; // longs had better be 4 bytes!
  char blah2[5-sizeof(int)]; // so had ints!
  OSStatus   err;
  NavDialogRef  openDialog;
  NavDialogCreationOptions dialogAttributes;
  
  err = NavGetDefaultDialogCreationOptions( &dialogAttributes );
  
  dialogAttributes.modality = kWindowModalityAppModal;
  
  NavCreateChooseFolderDialog (
                                              &dialogAttributes,
                                              NULL,
                                              NULL,
                                              NULL,
                                              &openDialog
                                              );
  
  err = NavDialogRun( openDialog );
  
  if ( err != noErr )
  {
    NavDialogDispose( openDialog );
  }
}
bool ArtGetOpenFileName(HWND hWndOwner, LPCTSTR lpszTitle, LPTSTR lpszPath, int cchPath, LPCTSTR lpszFilter)
{
  return false;
}

#include <sys/stat.h>
BOOL ArtDirectoryExists(LPCTSTR szPath)
{
  std::wstring strPathW(szPath);
  std::string strPathA(strPathW.begin(),strPathW.end());
  struct stat sb;
  if (stat(strPathA.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode))
  {
    return true;
  }
  return false;
}
void GetSystemTimeAsFileTime(FILETIME* pft)
{
  int time = GetSecondsSince1970GMT();
  UnixTimeToFileTime(time,pft);
}

LONG ArtInterlockedExchangeAdd(LONG* plTarget, LONG lAddThis)
{
  return OSAtomicAdd32(lAddThis,plTarget);
}
LONG ArtInterlockedExchange(LONG* pl,LONG lNewValue)
{
  return OSAtomicTestAndSet(lNewValue,pl);
}
LONG ArtInterlockedIncrement(LONG* pl)
{
  return OSAtomicIncrement32(pl);
}
LONG ArtInterlockedDecrement(LONG* pl)
{
  return OSAtomicDecrement32(pl);
}

#define WINDOWS_TICK 10000000
#define SEC_TO_UNIX_EPOCH 11644473600LL

unsigned int WindowsTickToUnixSeconds(long long windowsTicks)
{
  return (unsigned)(windowsTicks / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
}

void ArtGetLocalTime(SYSTEMTIME* pst)
{
  int time = GetSecondsSince1970GMT();
  *pst = SecondsSince1970ToSYSTEMTIME(time,true);
}
void ArtFileTimeToSystemTime(const FILETIME* pft,SYSTEMTIME* pst)
{
  long long* pll = (long long*)pft;
  unsigned int unix = WindowsTickToUnixSeconds(*pll);
  *pst = SecondsSince1970ToSYSTEMTIME(unix,true);
}

void OpenWofStream(ofstream& o, LPCTSTR lpszPath)
{
  wstring strPathW(lpszPath);
  string strPathA(strPathW.begin(),strPathW.end());
  o.open(strPathA.c_str());
}
void OpenWifStream(ifstream& i, LPCTSTR lpszPath)
{
  wstring strPathW(lpszPath);
  string strPathA(strPathW.begin(),strPathW.end());
  i.open(strPathA.c_str());  
}
#endif

void ArtGetTempPath(LPTSTR lpszPath, int cchPath)
{
#ifdef _MSC_VER
  GetTempPath(cchPath,lpszPath);
#else
  _snwprintf(lpszPath,cchPath,L"/tmp/");
#endif
}
