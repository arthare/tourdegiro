#pragma once

#ifndef ARTTOOLS_H_
#define ARTTOOLS_H_

#include <boost/uuid/uuid.hpp>

#ifndef ArtMax
#define ArtMax(x,y) ((x) > (y) ? (x) : (y))
#endif
#ifndef ArtMin
#define ArtMin(x,y) ((x) > (y) ? (y) : (x))
#endif


#ifdef _MSC_VER
#define ARTOVERRIDE override
#define ARTTYPE
#define snprintf _snprintf
#else
#define ARTOVERRIDE
#define ARTTYPE typename
#endif


int ArtFopen(FILE** ppOut,const char* pszFilename,const char* pszArgs);

#ifndef BOOST_DISABLE_ASSERTS
#define BOOST_DISABLE_ASSERTS
#endif

#define NUMCHARS(x) ((sizeof(x)) / sizeof(*x))
#define NUMITEMS(x) ((sizeof(x)) / sizeof(*x))

#define IS_FLAG_SET(fdw,flag) (((fdw) & (flag)) != 0)


double RandDouble();

int GetCheckSum(void* pvData, const int cbData);

template<class c> c FLIPBITS(c v)
{
	char output[sizeof(c)];
	char* input = (char*)&v;
	for(int x = 0; x < sizeof(v); x++)
	{
		output[x] = input[sizeof(v)-x-1];
	}
	c result = *(c*)(output);
	return result;
}
#define FLIP(x) ((x) = FLIPBITS((x)))

struct FLOATRECT
{
  float left;
  float right;
  float top;
  float bottom;
};

#define KMH_TO_MS(x) ((x)/3.6)
#define MPH_TO_MS(x) ((x)*1.609/3.6)

#define RECT_WIDTH(prc) (((prc)->right)-((prc)->left))
#define RECT_HEIGHT(prc) (((prc)->bottom)-((prc)->top))

// replacements for windows types

#ifdef _MSC_VER
inline void Noop()
{
  __asm nop;
}
inline void Break()
{
   __asm int 3;
}
#else
inline void Noop()
{
}
inline void Break()
{
}
#endif

#ifdef _MSC_VER
// visual studio, just use WinAPI functions
#include <Windows.h>
#define ArtDeleteFile DeleteFile
#define ArtSleep(x) Sleep(x)
#define ArtGetCurrentThreadId GetCurrentThreadId
#define ArtCreateDirectory CreateDirectory
#else
// not on windows
// fuck you, OSX
#define WINAPI
typedef long unsigned int DWORD;
typedef void* LPVOID;
typedef void* HANDLE;
typedef long HRESULT;
#define _wfopen wfopen

#define ArtSleep(x) usleep((x)*1000)


#define ArtGetCurrentThreadId() 0
#define _snprintf snprintf
#define _snwprintf swprintf
bool IsDebuggerPresent();
struct SIZE
{
  unsigned short cx;
  unsigned short cy;
};
struct RECT
{
  int left;
  int top;
  int right;
  int bottom;
};
struct ARTFILETIME
{
  DWORD dwLowDateTime;
  DWORD dwHighDateTime;
};
typedef wchar_t TCHAR;
typedef TCHAR* LPTSTR;
typedef const TCHAR* const LPCTSTR;
typedef short WORD;
typedef long long LONGLONG;
typedef int BOOL;
typedef int HWND;
typedef short SHORT;
typedef unsigned short USHORT;
typedef int32_t LONG;
typedef DWORD HMODULE;

#define S_OK 0
#define FALSE 0
#define TRUE 1
#define MAX_PATH 260

#define SUCCEEDED(x) ((x) >= 0)

struct ARTSYSTEMTIME
{
  WORD wYear;
  WORD wMonth;
  WORD wDayOfWeek;
  WORD wDay;
  WORD wHour;
  WORD wMinute;
  WORD wSecond;
  WORD wMilliseconds;
};
#define SYSTEMTIME ARTSYSTEMTIME
#define FILETIME ARTFILETIME
typedef const char* LPCSTR;
typedef char* LPSTR;
int ArtDeleteFile(LPCTSTR lpsz);
int ArtCreateDirectory(LPCTSTR lpsz, LPVOID null);
void GetSystemTimeAsFileTime(FILETIME* pft);

#endif // end of OSX-only stuff

std::string UrlEncode(const char* str);
std::wstring UrlEncode(const wchar_t* str);
#define NUMELMS(x) (sizeof(x) / sizeof(*x))

template<int i> void TemplatedFunction();


void DASSERTInternal(bool f);

#define BUILD_DASSERTS
#ifdef BUILD_DASSERTS
#define DASSERT(x) DASSERTInternal((x))
#else
#define DASSERT(x)
#endif

void EnableAsserts(bool f);

#define VERIFY(x) (  (x) ? Noop() : Break())
#define CASSERT(x) {int rg_noname[!!(x) ? 1 : -1]; rg_noname[0];}
void FormatTimeMinutesSecondsMs(float flTimeInSeconds, char* lpszBuffer, int cchBuffer);

bool IsEqualGUID(const boost::uuids::uuid& id1, const boost::uuids::uuid& id2);

HRESULT DownloadFileTo(std::wstring strURL, std::wstring strTargetFile);
HRESULT HitURL(std::wstring strURL);

bool SaveBufferToFile(LPCTSTR lpszPath, void* pvData, int cbData);

// useful to set up member variables like m_fWorking inside a thdproc
class AutoBool
{
public:
  AutoBool(bool& f) : m_f(f)
  {
    m_f = true;
  }
  ~AutoBool()
  {
    m_f = false;
  }
private:
  bool& m_f;
};

HRESULT DownloadFileTo(std::wstring strURL, std::wstring strTargetFile);
bool MultiPartPost(std::string strTarget, std::map<std::string,std::string> mapPostParams, std::string strFileToUpload);
HRESULT DownloadFileToMemory(std::wstring strURL, std::vector<char>& outBuf);
HRESULT HitURL(std::wstring strURL);

SYSTEMTIME SecondsSince1970ToSYSTEMTIME(int cSeconds, bool fInLocalTime);
FILETIME SecondsSince1970ToFILETIME(int cSeconds);
bool IsFileTimeEqual(const FILETIME& ft1, const FILETIME& ft2);

DWORD GetSecondsSince1970();
DWORD GetSecondsSince1970GMT();

bool ArtAtoi(LPCSTR lpsz, int cch, int* pOut);

LONGLONG GetFileSize(const std::wstring& path);
bool DoesFileExist(const std::wstring& path);

std::string FormatDecimals(float fl, int decimals);

// compares two strings, ignoring spaces
int nospacecompare(LPCTSTR lpsz1, LPCTSTR lpsz2);

struct RECTF
{
  RECTF()
  {
    left = top = right = bottom = 0;
  }
  RECTF(float left,float top,float right, float bottom) : left(left),top(top),right(right),bottom(bottom) {}

  RECTF Intersect(const RECTF& rcOther) const
  {
    RECTF rc;
    rc.left = ArtMax(rcOther.left,left);
    rc.right = ArtMin(rcOther.right,right);
    rc.top = ArtMax(rcOther.top,top);
    rc.bottom = ArtMin(rcOther.bottom,bottom);

    // bounds checking (we don't want an inverted rectangle)
    rc.right = ArtMax(rc.right,rc.left);
    rc.bottom = ArtMax(rc.bottom,rc.top);
    return rc;
  }
  float left,top,right,bottom;
  float Width() const {return right-left;}
  float Height() const {return bottom-top;}
  float CenterX() const {return (left + right)/2;}
  float CenterY() const {return (top + bottom)/2;}
  float Area() const {return (right-left)*(bottom-top);}
  bool Contains(float flX, float flY) const {return flX >= left && flX <= right && flY >= top && flY <= bottom;}
};

bool IsNaN(float f);
bool GetLastModifiedTime(const std::wstring& strFile, time_t* pTime);

std::wstring GetFilePath(LPCTSTR lpszFullPath);


// input: "tourdegiro/selectprefs.cfg"
// output: "c:\users\art\appdata\roaming\tourdegiro\selectprefs.cfg"
std::wstring GetRoamingPath(std::wstring strPath);

bool Regression(const std::vector<double>& rgX, const std::vector<double>& rgY, std::vector<double>& coefsOut);
//bool ArtGetOpenFileName(HWND hWndOwner, LPCTSTR lpszTitle, LPTSTR lpszPath, int cchPath, LPCTSTR lpszFilter);
//bool ArtGetSaveFileName(HWND hWndOwner, LPCTSTR lpszTitle, LPTSTR lpszPath, int cchPath, LPCTSTR lpszFilter);

// flMult: bigger numbers -> greater lag in response.  Smaller numbers: quicker response, but also choppiness
template<class TClass>
TClass ExpBlend(TClass flOld, TClass flNew, float flMult, float flDT)
{
  float flR = pow(flMult,flDT);
  return flR * flOld + (1-flR)*flNew;
}

unsigned int ArtGetTime();
template<class T>
class AnimHelper
{
public:
  AnimHelper(const T& goalState) : m_goalState(goalState),m_startState(goalState)
  {
    m_tmEnd = ArtGetTime(); // assume we're already there
  };

  void Set(float flTimeToTakeSeconds, const T& startState, const T& goalState)
  {
    DASSERT(flTimeToTakeSeconds > 0);
    m_goalState = goalState;
    m_startState = startState;
    m_tmStart = ArtGetTime();
    m_tmEnd = ArtGetTime() + (int)(1000*flTimeToTakeSeconds);
  }

  const T Get() const 
  {
    float flPct = GetFraction();
    return flPct*m_goalState + (1-flPct)*m_startState;
  }
  // returns 1.0 when we're at or past the target time, 0.0 when we're at or before our start time
  float GetFraction() const
  {
    const DWORD tmNow = ArtGetTime();
    if(tmNow < m_tmStart) return 0.0f;
    if(tmNow > m_tmEnd) return 1.0f;

    const int msElapsed = tmNow - m_tmStart;
    const int msSpan = m_tmEnd - m_tmStart;
    return (float)msElapsed / (float)msSpan;
  }

private:
private:
  unsigned int m_tmStart;
  unsigned int m_tmEnd;
  T m_goalState;
  T m_startState;
};

bool ArtGetOpenFileName(HWND hWndOwner, LPCTSTR lpszTitle, LPTSTR lpszPath, int cchPath, LPCTSTR lpszFilter);
bool ArtGetFolderSave(HWND hWndOwner, LPCTSTR lpszTitle, LPTSTR lpszPath, int cchPath);

// creates paths until the final one
bool ArtCreateDirectories(LPCTSTR lpszPath);
unsigned int ArtGetTime();
BOOL ArtFileExists(LPCTSTR szPath);
BOOL ArtDirectoryExists(LPCTSTR szPath);
LONG ArtInterlockedExchangeAdd(LONG* plTarget, LONG lAddThis);
LONG ArtInterlockedExchange(LONG* pl,LONG lNewValue);
LONG ArtInterlockedIncrement(LONG* pl);
LONG ArtInterlockedDecrement(LONG* pl);
void ArtGetLocalTime(SYSTEMTIME* pst);
void ArtFileTimeToSystemTime(const FILETIME* pft,SYSTEMTIME* pst);
void OpenWofStream(std::ofstream& o, LPCTSTR lpszPath);
void OpenWifStream(std::ifstream& i, LPCTSTR lpszPath);
bool IsVLCInstalled(std::wstring* pstrOut);
DWORD ArtGetMemoryUsage(DWORD* pcHandles);
void ArtGetTempPath(LPTSTR lpszPath, int cchPath);


void EnableAsserts(bool f);

void SetThreadName( DWORD dwThreadID, const char* threadName);

std::string RandString();

#endif // ARTTOOLS_H_