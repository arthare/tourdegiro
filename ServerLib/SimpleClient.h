#pragma once

#include "SimpleServer.h"

#ifndef _DEBUG
#define BOOST_CB_DISABLE_DEBUG
#endif


template<class T>
DWORD WINAPI _RecvThreadProc(LPVOID pv);
template<class T>
DWORD WINAPI _SendThreadProc(LPVOID pv);

template<class TotalGameState,class ClientState,class ClientDesc, class ClientStartupInfo, class ClientC2SData,class ServerStatus>
class SimpleClient
{
public:
  SimpleClient(ICommSocketFactoryPtr pSocketFactory, int iVersion, int iUDPOutPort, int iTCPConnectPort) 
    : m_pSocketFactory(pSocketFactory),
      m_iUDPSendPort(iUDPOutPort),
      m_iTCPConnectPort(iTCPConnectPort),
      m_fQuit(false),
      m_guid(),
      m_iPortOffset(0),
      m_tmLastConnectAttempt(0)
  {
  }
  virtual ~SimpleClient()
  {
    Disconnect();
  }
  
  void Disconnect()
  {
    m_fQuit = true;
    
    if(m_sSend)
    {
      m_sSend->close();
      m_sSend.reset();
    }
    if(m_sRecv)
    {
      m_sRecv->close();
      m_sRecv.reset();
    }
    try
    {
    m_hSendThd.join();
    }
    catch(...)
    {
      // whatevs...
    }
    try
    {
    m_hRecvThd.join();
    }
    catch(...)
    {
      // whatevs
    }
    m_fQuit = false;
    m_guid = boost::uuids::uuid();
  }
  bool Connect(const char* pszTarget, TDGConnectionResult* pConnResult)
  {
    m_strTarget = pszTarget;

    if(m_sRecv)
    {
      m_sRecv->close();
      m_sRecv.reset();
    }

    m_iPortOffset = 0;

    bool fSuccess = false;
    cout<<"Connecting to TCP port "<<m_iTCPConnectPort + m_iPortOffset<<" (offset "<<m_iPortOffset<<")"<<endl;
    // we want to establish a TCP connection to the server at pszTarget.  We will send our clientdesc and client state, then it will send us the initial game state.  Then we will start listening for TCP packets

    int err = 0;
    ICommSocketPtr s(m_pSocketFactory->NewSocket());
    if(s->connect(pszTarget,m_iTCPConnectPort + m_iPortOffset, &m_addrServer))
    {
      s->set_timeouts(5000,5000);

      ClientDesc cd;
      SimpleClient_BuildDesc(&cd);
      cd.guid = m_guid;
      ClientState ct;
      SimpleClient_BuildState(&ct);

      
      //cout<<"Connecting with guid "<<boost::lexical_cast<std::string>(cd.guid)<<endl;
      err = s->send(&cd,sizeof(cd),0);
      if(err == sizeof(cd))
      {
        err = s->recv(pConnResult,sizeof(*pConnResult),0);
        if(err == sizeof(*pConnResult))
        {
          if(pConnResult->eResult == LOGINSUCCESS)
          {
            m_iUDPLocalPort = 63939;
            err = s->send(&ct,sizeof(ct),0);
            if(err == sizeof(ct))
            {
              STARTUPINFOZ<ClientStartupInfo> startupInfo;
              err = s->recv(&startupInfo,sizeof(startupInfo),0);
              if(err == sizeof(startupInfo))
              {
                //cout<<"Server assigned us "<<startupInfo.guid<<" (took "<<(tmAfter-tmBefore)<<"ms)"<<endl;

                SimpleClient_SetStartupInfo(startupInfo);

                m_guid = startupInfo.guid;
                m_sRecv = s; // store the socket for the other thread to use
                fSuccess = true;
              }
              else
              {
                pConnResult->eResult = CONNECTIONFAILURE;
              }
            }
            else
            {
              pConnResult->eResult = CONNECTIONFAILURE;
            }
          }
          else
          {
            cout<<"Wasn't a login success"<<endl;
          }
        }
        else
        {
          cout<<"Failed to recv connResult"<<endl;
          pConnResult->eResult = CONNECTIONFAILURE;
        }
      }
      else
      {
        pConnResult->eResult = CONNECTIONFAILURE;
      }
    }

    if(fSuccess && !m_hSendThd.joinable())
    {
      // set up sending thread, that'll hammer the server with UDP packets indicating our current state
      m_hSendThd = boost::thread(_SendThreadProc<SimpleClient<TotalGameState,ClientState,ClientDesc,ClientStartupInfo,ClientC2SData,ServerStatus> >,this);
    }

    if(fSuccess && !m_hRecvThd.joinable())
    {
      // connect the data-receive thread...
      
      // if we were successful, we should start our client-update thread which will receive TotalGameStates in UDP packets, and tell the inheriting class about them via SimpleClient_NotifyGameState
      m_hRecvThd = boost::thread(_RecvThreadProc<SimpleClient<TotalGameState,ClientState,ClientDesc,ClientStartupInfo,ClientC2SData,ServerStatus> >,this);
    }

    if(!fSuccess)
    {
      pConnResult->eResult = pConnResult->eResult == REASONUNKNOWN ? CONNECTIONFAILURE : pConnResult->eResult;
      if(s)
      {
        s->close();
      }
      s.reset();
      m_sRecv.reset();
    }
    return fSuccess;
  }

  virtual void SimpleClient_BuildDesc(ClientDesc* pDesc) const = 0;
  virtual void SimpleClient_BuildState(ClientState* pState) const = 0;
  virtual void SimpleClient_SetStartupInfo(const STARTUPINFOZ<ClientStartupInfo>& info) = 0;
  virtual bool SimpleClient_NotifyGameState(const TotalGameState& tgs) = 0; // return false if the data failed integrity testing.  The server will then give you a chance to re-sync
  virtual bool SimpleClient_GetSpecialData(ClientC2SData* pData) = 0; // fill up pData and return true if you have any special data to send to the server
  virtual bool SimpleClient_ShouldReconnect() = 0; // returns whether the client needs a reconnect
protected:
  boost::uuids::uuid GetConnectionGuid() const {return m_guid;}
private:
  DWORD RecvThreadProc()
  {
    //FILE* pFile = fopen("c:\\temp\\recved.bin","wb");

    boost::circular_buffer<unsigned char> circbuf(10000);
    while(!m_fQuit)
    {
      
      char szBuf[sizeof(TotalGameState)*2];
      bool fConnectionLost = false;
      int err = m_sRecv->timeoutRecv(szBuf,sizeof(szBuf),0,10000,&fConnectionLost);
      if(err > 0)
      {
        // stick it in the circular buffer
        for(int x = 0;x < err; x++)
        {
          circbuf.push_back(szBuf[x]);
        }
      }
      DASSERT(circbuf.size() < 5000); // we should never get this far behind
      TotalGameState tgs;
      while(!fConnectionLost && circbuf.size() > sizeof(tgs))
      {
        memcpy(&tgs,&circbuf[0],sizeof(tgs));
        //fwrite(&tgs,sizeof(tgs),1,pFile);
        circbuf.erase(circbuf.begin(),circbuf.begin()+sizeof(tgs));
        SimpleClient_NotifyGameState(tgs);
        
        if(SimpleClient_ShouldReconnect())
        {
          // it's been 5 seconds.  we're boned.
          fConnectionLost = true;
        }
        else
        {
          //DASSERT(tmTaken < 10); // Don't block the recv thread!!!!  if it spends too long not receiving, the server may drop it!

          ClientC2SData c2sData; // we want to check if the client wants to send any special data to the server
          while(SimpleClient_GetSpecialData(&c2sData))
          {
            int cbSent = m_sRecv->send(&c2sData,sizeof(c2sData),0);
            if(cbSent != sizeof(c2sData))
            {
              // we may have an issue...
              fConnectionLost = true;
              break;
            }
          }
        }
      }
      if(fConnectionLost)
      {
        circbuf.clear();
        while(!m_fQuit)
        {
          { // make sure that we don't DDOS the server by limiting reconnect speeds
            const DWORD tmSinceLastConnect = ArtGetTime() - m_tmLastConnectAttempt;
            if(tmSinceLastConnect < 5000)
            {
              ArtSleep(5000 - tmSinceLastConnect);
            }
            m_tmLastConnectAttempt = ArtGetTime();
          }
          cout<<"Lost connection to "<<m_strTarget<<".  Attempting reconnect"<<endl;
          // lost the connection!!
          TDGConnectionResult connResult;
          if(Connect(m_strTarget.c_str(),&connResult) && connResult.eResult == LOGINSUCCESS)
          {
            cout<<"Connection recovered... hopefully."<<endl;
            break;
          }
          else
          {
            cout<<"Failed attempt at reconnection.  Trying again..."<<endl;
            ArtSleep(100);
          }
        }
      }
    }
    m_sRecv.reset();
    return 0;
  }
  DWORD SendThreadProc()
  {
    /*
    ICommSocketAddressPtr addrServer = m_addrServer;
    
    unsigned int iCurrentLocalPort = 0;
    if(iCurrentLocalPort != m_iUDPLocalPort || !m_sSend)
    {
      if(m_sSend)
      {
        m_sSend.reset();
      }
      m_sSend = IDGramCommSocketPtr(this->m_pSocketFactory->NewDGramSocket());
      if(!m_sSend) return 0; // this socket factory doesn't support datagram :(
      ICommSocketAddressPtr localEndpoint(m_pSocketFactory->NewAddress("127.0.0.1",m_iUDPLocalPort));
      if(0 == m_sSend->open())
      {
        if(0 == m_sSend->bind(m_iUDPLocalPort))
        {
          iCurrentLocalPort = m_iUDPLocalPort;
        }
      }
    }
    while(!m_fQuit)
    {
      ClientState cs;
      SimpleClient_BuildState(&cs);

      ICommSocketAddressPtr target(m_addrServer);

      const int cbSent = m_sSend->send_to(target.get(),&cs,sizeof(cs),0,m_iUDPSendPort + m_iPortOffset);
        
      if(cbSent == sizeof(cs))
      {
        // successful send!
      }
      else
      {
        cout<<"Failed to send game state to server"<<endl;
      }
      ArtSleep(100);
    }*/
    return 0;
  }
  friend DWORD WINAPI _RecvThreadProc<SimpleClient<TotalGameState,ClientState,ClientDesc,ClientStartupInfo, ClientC2SData,ServerStatus> >(LPVOID pvParam);
  friend DWORD WINAPI _SendThreadProc<SimpleClient<TotalGameState,ClientState,ClientDesc,ClientStartupInfo, ClientC2SData,ServerStatus> >(LPVOID pvParam);
private:
  bool m_fQuit;

  unsigned short m_iUDPLocalPort;

  const int m_iUDPSendPort; // for client->server data
  const int m_iTCPConnectPort; // for initial connection and server->client data
  int m_iPortOffset; // tells us the port offset for connecting to a server

  boost::thread m_hRecvThd;
  boost::thread m_hSendThd;
  
  IDGramCommSocketPtr m_sSend;
  
  ICommSocketPtr m_sRecv; // TCP socket for receiving data
  ICommSocketAddressPtr m_addrServer;

  boost::uuids::uuid m_guid; // our identifying guid for reconnecting.  On first connection, we want to use GUID_NULL.  The server will tell us a GUID to use in future reconnects.
  string m_strTarget;
  DWORD m_tmLastConnectAttempt; // when did we last try to connect to the server?  Let's limit reconnect attempts to once every 5 seconds
protected:
  ICommSocketFactoryPtr m_pSocketFactory;
};


template<class T>
DWORD WINAPI _RecvThreadProc(LPVOID pv)
{
  cout<<"_RecvThreadProc: "<<ArtGetCurrentThreadId()<<endl;
  T* p = (T*)pv;
  return p->RecvThreadProc();
}
template<class T>
DWORD WINAPI _SendThreadProc(LPVOID pv)
{
  cout<<"SendThdProc: "<<ArtGetCurrentThreadId()<<endl;
  T* p = (T*)pv;
  return p->SendThreadProc();
}