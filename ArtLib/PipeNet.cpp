#include "stdafx.h"
#include "arttools.h"
#include "ArtNet.h"

using namespace std;

const LPCTSTR g_lpszPipeName = L"\\\\.\\pipe\\tdg_pipe";

// connection process goes like this:
// open pipe named g_lpszPipeName
// receive 4-byte LONG indicating the pipe number you should connect to
// connect to g_lpszPipeName<stringified long>.  Ex: L"\\\\.\\pipe\\tdg_pipe5"
// now you're connected for reals!

class ScopedHandle
{
public:
  ScopedHandle(bool fManual)
  {
    m_h = CreateEvent(NULL,fManual,FALSE,NULL);
  }
  ~ScopedHandle()
  {
    CloseHandle(m_h);
  }
  HANDLE GetHandle()
  {
    return m_h;
  }
private:
  HANDLE m_h;
};

class PipeCommSocketAddress : public ICommSocketAddress
{
public:
  virtual unsigned int ToDWORD()
  {
    return 69;
  }
  virtual int get_local_port()
  {
    return 69;
  }
};


bool OverlappedWait(HANDLE hOverlapped, HANDLE hClose, DWORD timeout)
{
  DASSERT(hOverlapped && hClose);
  HANDLE rgWait[2];
  rgWait[0] = hOverlapped;
  rgWait[1] = hClose;
  DWORD dwRet = WaitForMultipleObjects(2,rgWait,FALSE,timeout);
  switch(dwRet)
  {
  case WAIT_OBJECT_0:
    // read/write is done!
    return true;
  default:
  case WAIT_OBJECT_0+1:
    // they want to close the pipe, or it is already closed!
    return false;
  }
}

class PipeCommSocket : public ICommSocket
{
public:
  PipeCommSocket() : m_hPipe(0),m_hClose(false) {};
  PipeCommSocket(HANDLE hPipe) : m_hPipe(hPipe),m_hClose(false) {};
  virtual ~PipeCommSocket() 
  {
    cout<<"destructing "<<this<<endl;
    close();
  };

  virtual int send(const void* buf, int len, int flags)
  {
    DWORD cbSent = 0;
    OVERLAPPED overlapped;
    ScopedHandle overlapHandle(true);
    memset(&overlapped,0,sizeof(overlapped));
    overlapped.hEvent = overlapHandle.GetHandle();
    BOOL fSuccess = WriteFile(m_hPipe,buf,len,&cbSent,&overlapped);
    if(!fSuccess)
    {
      return 0;
    }

    if(!OverlappedWait(overlapped.hEvent, m_hClose.GetHandle(),INFINITE)) return 0;

    return cbSent;
  }

  // returns how many bytes are available for a recv or timeoutRecv call.  Does not block.
  virtual int peekReadable()
  {
    if(!m_hPipe || m_hPipe == INVALID_HANDLE_VALUE) return 0;

    DWORD cbSize=0;
    PeekNamedPipe(m_hPipe,NULL,0,NULL,&cbSize,NULL);
    return cbSize;
  }
  virtual int recv(void* buf, int len, int flags)
  {
    return recvInternal(buf,len,flags,INFINITE);
  }
  virtual int timeoutRecv(void* buf, int len, int flags, int timeout, bool* pfConnectionLost)
  {
    return recvInternal(buf,len,flags,timeout);
  }

  virtual bool connect(const char* pszTarget,int port, ICommSocketAddressPtr* pspResolved);

  // sends the send and receive timeouts, in milliseconds
  virtual void set_timeouts(int recv, int send)
  {
  }

  virtual void close()
  {
    if(m_hPipe)
    {
      cout<<"closing handle "<<m_hPipe<<endl;
      SetEvent(m_hClose.GetHandle());
      CloseHandle(m_hPipe);
      m_hPipe = 0;
    }
  }
  int recvInternal(void* buf, int len, int flags, int timeout)
  {
    if(!m_hPipe || m_hPipe == INVALID_HANDLE_VALUE) return 0;

    DWORD cbRead = 0;
    while(true)
    {
      OVERLAPPED overlapped;
      ScopedHandle overlapHandle(true);
      memset(&overlapped,0,sizeof(overlapped));
      overlapped.hEvent = overlapHandle.GetHandle();
      BOOL fSuccess = ReadFile(m_hPipe,buf,len,&cbRead,&overlapped);
      if(!fSuccess)
      {
        const DWORD err = GetLastError();
        if(err == 0x218) // "waiting for a process to open the other end of the pipe"
        {
          return 0;
        }
        else if(err == ERROR_IO_PENDING)
        {
          // this is fine, the operation is happening asynchronously
          if(OverlappedWait(overlapped.hEvent, m_hClose.GetHandle(),timeout))
          {
            return overlapped.InternalHigh;
          }
          else
          {
            return 0;
          }
        }
        else
        {
          DASSERT(FALSE);
          return 0;
        }
      }
      else
      {
        fSuccess = OverlappedWait(overlapped.hEvent, m_hClose.GetHandle(),timeout);
        if(fSuccess)
        {
          // yay!
          return cbRead;
        }
        else
        {
          return 0;
        }
      }
    }
  }
private:
private:
  HANDLE m_hPipe;
  ScopedHandle m_hClose; // handle that gets signalled when we close this "socket"
};
typedef boost::shared_ptr<ICommSocket> ICommSocketPtr;

class PipeDGramCommSocket : public IDGramCommSocket
{
public:
  virtual int send_to(ICommSocketAddress* target, const void* pvData, int len, int flags, int iTargetPort) = 0;
  virtual int open() = 0;
  virtual int bind(int iLocalPort) = 0;
  virtual int recvfrom(void* pvData,int len,ICommSocketAddressPtr* pspAddress) = 0;
};
typedef boost::shared_ptr<IDGramCommSocket> IDGramCommSocketPtr;

class PipeCommSocketFactory : public ICommSocketFactory
{
public:
  PipeCommSocketFactory() : m_lSocketNumber(0),m_hAcceptor(0),m_fShutdown(false),m_hClose(0)
  {
    m_hClose = CreateEvent(NULL,FALSE,FALSE,NULL);
  }
  virtual ~PipeCommSocketFactory()
  {
    CloseHandle(m_hClose);
  }
  virtual ICommSocket* NewSocket()
  {
    return new PipeCommSocket();
  }
  virtual IDGramCommSocket* NewDGramSocket()
  {
    return 0;
  }

  virtual ICommSocketAddress* NewAddress(const char* pszAddress, int port)
  {
    return new PipeCommSocketAddress();
  }

  // waits for an incoming connection, then returns a representation of the socket received and the address it connected from
  virtual ICommSocketPtr WaitForSocket(int listenPort, ICommSocketAddressPtr* pspRemoteAddress);

  virtual void Shutdown()
  {
    m_fShutdown = true;
    SetEvent(m_hClose);

    if(m_hAcceptor && m_hAcceptor != INVALID_HANDLE_VALUE)
    {
      DisconnectNamedPipe(m_hAcceptor);
      CloseHandle(m_hAcceptor);
      m_hAcceptor = 0;
    }
  }
private:
  void NewAcceptor()
  {
    if(m_fShutdown) return;

    while(true)
    {
      if(m_hAcceptor != INVALID_HANDLE_VALUE && m_hAcceptor)
      {
        DisconnectNamedPipe(m_hAcceptor);
        CloseHandle(m_hAcceptor);
        m_hAcceptor = 0;
      }
      m_hAcceptor = CreateNamedPipe(g_lpszPipeName,FILE_FLAG_FIRST_PIPE_INSTANCE | PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,PIPE_TYPE_BYTE|PIPE_WAIT,1,1024*1024,1024*1024,0,NULL);
      if(!m_hAcceptor || m_hAcceptor == INVALID_HANDLE_VALUE)
      {
        Sleep(10);
      }
      else
      {
        return;
      }
    }
  }
private:
  HANDLE m_hAcceptor;
  HANDLE m_hClose;
  LONG m_lSocketNumber;
  bool m_fShutdown;
};
typedef boost::shared_ptr<ICommSocketFactory> ICommSocketFactoryPtr;




bool PipeCommSocket::connect(const char* pszTarget,int port, ICommSocketAddressPtr* pspResolved)
{
  DASSERT(!m_hPipe); // we're a client!
  HANDLE hPipeConnect = CreateFile(g_lpszPipeName,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
  if(hPipeConnect != INVALID_HANDLE_VALUE)
  {
    // they should respond with a 4-byte LONG indicating the pipe number we should connect to
    LONG lResponse = 0;
    DWORD dwRead = 0;
    if(ReadFile(hPipeConnect,&lResponse,sizeof(lResponse),&dwRead,NULL))
    {
      // ok, now we know which pipe we should talk with.  Let's close this one down
      cout<<"client closing "<<hPipeConnect<<endl;
      CloseHandle(hPipeConnect);
      hPipeConnect = 0;

      TCHAR szNewPipeName[MAX_PATH];
      _snwprintf(szNewPipeName,NUMCHARS(szNewPipeName),L"%s%d",g_lpszPipeName,lResponse);
      HANDLE hPipeFinal = CreateFile(szNewPipeName,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
      if(hPipeFinal && hPipeFinal != INVALID_HANDLE_VALUE)
      {
        // connected!
        *pspResolved = ICommSocketAddressPtr(new PipeCommSocketAddress());
        m_hPipe = hPipeFinal;
        return true;
      }
    }
  }
  if(hPipeConnect != INVALID_HANDLE_VALUE && hPipeConnect)
  {
    cout<<"closing "<<hPipeConnect<<endl;
    CloseHandle(hPipeConnect);
  }
  return false;
}

ICommSocketPtr PipeCommSocketFactory::WaitForSocket(int listenPort, ICommSocketAddressPtr* pspRemoteAddress)
{
  if(m_fShutdown) return ICommSocketPtr();
  NewAcceptor();

  ScopedHandle hOverlapped(true);
  OVERLAPPED overlapped;
  overlapped.hEvent = hOverlapped.GetHandle();
  if(ConnectNamedPipe(m_hAcceptor,&overlapped) || GetLastError() == ERROR_IO_PENDING)
  {
    if(OverlappedWait(overlapped.hEvent,this->m_hClose,INFINITE))
    {
      // someone wants to talk to us!
      // let's figure out their pipe number, and send it to them once we have that new pipe set up
      const LONG lSocketNumber = InterlockedIncrement(&m_lSocketNumber);

      TCHAR szPipeName[MAX_PATH];
      _snwprintf(szPipeName,NUMCHARS(szPipeName),L"%s%d",g_lpszPipeName,lSocketNumber);
      HANDLE hNewPipe = CreateNamedPipe(szPipeName,PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,PIPE_TYPE_BYTE|PIPE_WAIT,1,1024*1024,1024*1024,0,NULL);
      if(hNewPipe != INVALID_HANDLE_VALUE && hNewPipe)
      {
        // successfully created the new pipe.  Let's tell the other end all about it by sending the pipe number back to them
        DWORD dwWritten = 0;
        OVERLAPPED overlapped;
        ScopedHandle overlapHandle(true);
        memset(&overlapped,0,sizeof(overlapped));
        overlapped.hEvent = overlapHandle.GetHandle();
        if(WriteFile(m_hAcceptor,&lSocketNumber,sizeof(lSocketNumber),&dwWritten,&overlapped))
        {
          ICommSocketPtr pRet(new PipeCommSocket(hNewPipe));
          
          bool fSuccess=false;
          { // let's wait for someone to connect to the other side of hNewPipe
            OVERLAPPED overlapped;
            ScopedHandle connectHandle(true);
            overlapped.hEvent = connectHandle.GetHandle();
            if(ConnectNamedPipe(hNewPipe,&overlapped))
            {
              // someone connected already!
              cout<<"Someone has already connected to our pipe!"<<endl;
              fSuccess = true;
            }
            else
            {
              cout<<"We gotta wait for someone to connect to our pipe!"<<endl;
              DWORD dwRet = WaitForSingleObject(overlapped.hEvent,5000);
              if(dwRet == WAIT_OBJECT_0)
              {
                cout<<"someone has connected to our glorious pipe!"<<endl;
                fSuccess = true;
              }
              else
              {
                // no dice
                DASSERT(!fSuccess);
              }
            }
          }

          if(fSuccess)
          {
            *pspRemoteAddress = ICommSocketAddressPtr(new PipeCommSocketAddress());
            return pRet;
          }
        }

        CloseHandle(hNewPipe); // something fucked up.  clean up the pipe...
      }
    }
  }
  return ICommSocketPtr();
}

ICommSocketFactory* CreatePipeSockets()
{
  return new PipeCommSocketFactory();
}