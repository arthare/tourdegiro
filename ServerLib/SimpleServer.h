#pragma once


using namespace std;
#include "ArtTools.h" // for ArtInterlockedExchange...
#include <vector>

struct TDGConnectionResult
{
  TDGConnectionResult()
  {
    memset(this,0,sizeof(*this));
    eResult = REASONUNKNOWN;
  }
  LOGINRESULT eResult;
  char szReserved[20]; // lets put a bit more space here for futureproofing
};

template<class ClientState, class ClientDesc>
struct CLIENTDATA
{
public:
  CLIENTDATA() : fKillRequest(false) // if this gets used, it generally means something has been (incorrectly) automatically made in a map by accident
  {
    fValid = false;
    cTGSBuilds = 0;
  }
  CLIENTDATA(bool fValid) : fKillRequest(false)
  {
    DASSERT(!fValid); // if you need to declare an on-stack clientdata, use this constructor.
    fValid = false;
    cTGSBuilds = 0;
  }
  CLIENTDATA(const ClientDesc& cd, const ClientState& cs, ICommSocketAddressPtr addr, const vector<int>& lstIds, ICommSocketPtr sData) : sData(sData),fKillRequest(false)
  {
    this->lastState = cs;
    this->clientDesc = cd;
    this->clientAddr = addr;
    this->lstIds = lstIds;

    fValid = true;
    cTGSBuilds = 0;
  }
  bool IsValid() const {return fValid;}
  ClientState lastState;
  ClientDesc clientDesc;
  ICommSocketPtr sData;
  ICommSocketAddressPtr clientAddr; // network address of client
  bool fValid;
  bool fInited;
  vector<int> lstIds;
  int cTGSBuilds; // how many times have we build a TGS to send to this guy?
  boost::shared_ptr<boost::thread> hThread;
  bool fKillRequest;
};

typedef boost::shared_ptr<boost::thread> BoostThreadPtr;

template<class ClientStartupInfo>
struct STARTUPINFOZ
{
public:
  STARTUPINFOZ()
  {
    memset(rgIds,0,sizeof(rgIds));
  }
  int rgIds[8]; // MAX_LOCAL_PLAYERS
  boost::uuids::uuid guid; // our connection GUID, so we can reconnect to the server.
  ClientStartupInfo sfInfo;
};

struct THREADSTART
{
  THREADSTART(ICommSocketPtr sData, ICommSocketAddressPtr pRemoteAddr, void* pvServer, const SERVERKEY& key) : sData(sData),pRemoteAddr(pRemoteAddr),pvServer(pvServer),ulAddr(key) {};
  ICommSocketPtr sData;
  ICommSocketAddressPtr pRemoteAddr;
  void* pvServer;
  SERVERKEY ulAddr;
};

class ActiveTracker;
void AddActive(const int key);
void KillActive(const int key);
bool AcquireActive(const int key, ActiveTracker* pTracker); // returns true if we successfully acquired
class ActiveTracker
{
public:
  ActiveTracker() : mainId(-1) {};
  ActiveTracker(const int mainId) : mainId(mainId)
  {
    AddActive(mainId);
  }
  ~ActiveTracker()
  {
    if(mainId >= 0)
    {
      KillActive(mainId);
    }
  }
private:
  const int mainId;
};

template<class TotalGameState,class ClientState,class ClientDesc, class ClientStartupInfo, class ClientC2S, class ServerStatus, class SendHandle> DWORD WINAPI UDPRecvThdProc(LPVOID pv);
template<class TotalGameState,class ClientState,class ClientDesc, class ClientStartupInfo, class ClientC2S, class ServerStatus, class SendHandle> DWORD WINAPI TCPConnectThdProc(LPVOID pv);
template<class TotalGameState,class ClientState,class ClientDesc, class ClientStartupInfo, class ClientC2S, class ServerStatus, class SendHandle> DWORD WINAPI TCPSendThdProc(LPVOID pv);


namespace std
{
    
    template<>
    struct hash<boost::uuids::uuid>
    {
        size_t operator () (const boost::uuids::uuid& uid) const
        {
            return boost::uuids::hash_value(uid);
        }
    };
    
}

template<class TotalGameState,class ClientState,class ClientDesc, class ClientStartupInfo, class ClientC2S, class ServerStatus, class SendHandle>
class SimpleServer
{

  const static int TIMEOUT_TIME = 10000;
public:
  typedef CLIENTDATA<ClientState,ClientDesc> ClientData;
  typedef boost::shared_ptr<ClientData> ClientDataPtr;

  SimpleServer(ICommSocketFactoryPtr pSocketFactory, int iVersion, int iUDPInPort, int iTCPConnectPort) 
    : m_cActiveConnecting(0),
    m_pSocketFactory(pSocketFactory),
    m_iUDPRecvPort(iUDPInPort),
    m_iTCPConnectPort(iTCPConnectPort),
    m_fShutdown(false),
    m_sUDPSocket(pSocketFactory->NewDGramSocket())
  {
  }
  virtual ~SimpleServer()
  {
    m_fShutdown=true;
    
    cout<<"killing udp"<<endl;
    if(m_sUDPSocket)
    {
      m_sUDPSocket->close();
      m_sUDPSocket.reset();
    }
    if(m_hUDPRecvThread.joinable())
    {
      m_hUDPRecvThread.join();
    }
    cout<<"killing tcp listen"<<endl;
    if(m_pSocketFactory)
    {
      m_pSocketFactory->Shutdown();
    }
    if(m_hTCPConnectThread.joinable())
    {
      m_hTCPConnectThread.join();
    }
    
    vector<ICommSocketPtr> lstSockets;
    vector<BoostThreadPtr> lstThreads;
    {
      AutoLeaveCS _cs(m_cs);
      DASSERT(m_cs.IsOwned());
      for(ARTTYPE map<SERVERKEY,ClientDataPtr>::const_iterator i = m_mapClientData.begin(); i != m_mapClientData.end(); i++)
      {
        lstSockets.push_back(i->second->sData);
        lstThreads.push_back(i->second->hThread);
      }
    }

    for(unsigned int x = 0;x < lstSockets.size(); x++)
    {
      cout<<"killing socket and thread "<<x<<endl;
      if(lstSockets[x]) lstSockets[x]->close();
      if(lstThreads[x]) lstThreads[x]->join();
    }
  }
  void Init()
  {
    m_hUDPRecvThread = boost::thread(UDPRecvThdProc<TotalGameState,ClientState,ClientDesc,ClientStartupInfo,ClientC2S,ServerStatus,SendHandle>,this);
    m_hTCPConnectThread = boost::thread(TCPConnectThdProc<TotalGameState,ClientState,ClientDesc,ClientStartupInfo,ClientC2S,ServerStatus,SendHandle>,this);

  }
  void SetShutdown() 
  {
    {
      AutoLeaveCS _cs(m_cs);
      m_pSocketFactory->Shutdown();
    }

    m_fShutdown=true;
  }
  void DisconnectAll()
  {
    AutoLeaveCS _cs(m_cs);
    m_mapGuidIds.clear();
    m_mapClientData.clear();
    cout<<"disconnected for map change"<<endl; 
  }
protected:
  // if we should wait before sending data to this target, returns how many milliseconds we should wait
  virtual bool SimpleServer_StartSendHandle(SendHandle* pHandle, const ClientData& cdTarget) = 0; // set up some data that will be used for calls to SimpleServer_BuildGameState until SimpleServer_IsHandleCurrent returns false
  virtual bool SimpleServer_IsHandleCurrent(const SendHandle& handle) const = 0; // checks to see if the SendHandle is still up to date.  If your game state has moved on, then return false and you'll be able to generate a new one
  virtual int SimpleServer_BuildGameState(SendHandle& sendHandle, const ClientData& cdTarget, TotalGameState* pTGS, bool* pfPlayerDead) const = 0; // builds a TotalGameState object for transmission over the network.  Set pfPlayerDead to true

  virtual void SimpleServer_NotifyNewPlayer(const ClientData& sfData) = 0;
  virtual void SimpleServer_NotifyReconnectedPlayer(const ClientData& sfData) = 0; // says that player with id (blah) has reconnected
  virtual void SimpleServer_NotifyDeadPlayer(const ClientData& sfData) = 0;
  virtual void SimpleServer_NotifyClientState(const ClientData& sfClient, const ClientState& sfNewData) = 0;
  virtual void SimpleServer_BuildStartupInfo(STARTUPINFOZ<ClientStartupInfo>& sfStartup) = 0;
  virtual void SimpleServer_NotifyC2SData(const ClientData& src, const ClientC2S& c2s) = 0; // tells you when a client has sent us special C2S data
  virtual void SimpleServer_AssignIds(const ClientDesc& cd, int* prgIds) = 0;
  virtual void SimpleServer_BuildServerStatus(ServerStatus* pSS) const = 0;
  virtual bool SimpleServer_ValidateLogin(ClientDesc& cd, TDGConnectionResult* pResult) const = 0;
private:
  class ConnectionDecrementer
  {
  public:
    ConnectionDecrementer(LONG* pc) : pcConnections(pc) {};
    ~ConnectionDecrementer()
    {
      if(pcConnections)
      {
        ArtInterlockedDecrement(pcConnections);
      }
    }
    void Done()
    {
      if(pcConnections)
      {
        ArtInterlockedDecrement(pcConnections);
        pcConnections = 0;
      }
    }
  private:
    LONG* pcConnections;
  };
  DWORD TCPSendProc(ICommSocketPtr sData, ICommSocketAddressPtr pRemoteAddr, const SERVERKEY& key)
  {
    tmTotalSleep = 0;
    cTotalLoops = 0;

    {
      ConnectionDecrementer decrementer(&m_cActiveConnecting); // make sure that successful or not, we decrement the number of people actively connecting

      bool fConnectSuccess = false;
      // so after all that, we should have a data socket opened
      {
        if(!sData)
        {
          fConnectSuccess = false;
        }
        else
        {
          unordered_set<SERVERKEY> setWaitFor; // the threads that we should wait for before continuing.  These will be threads that we've set fKillRequest on.  We can't wait right when we set fKillRequest, because we have the critical section at that point
        
          ClientDataPtr cdTarget;
          if(ReceiveNewPlayerInfo(sData,pRemoteAddr))
          {
            AutoLeaveCS _cs(m_cs);

            {
              ARTTYPE std::map<SERVERKEY,ClientDataPtr>::const_iterator iFind = m_mapClientData.find(key);
              if(iFind != m_mapClientData.end())
              {
                cdTarget = iFind->second;

                // we're in.  that means we're the king for cdTarget->lstIds[0].  Therefore, we should go through all the other guys that might be working on cdTarget->lstIds[0] and kill them
                for(ARTTYPE map<SERVERKEY,ClientDataPtr>::const_iterator i = m_mapClientData.begin(); i != m_mapClientData.end(); i++)
                {
                  if(i->second->lstIds.size() > 0 &&
                     cdTarget->lstIds.size() > 0 &&
                     i->second->lstIds[0] == cdTarget->lstIds[0] && 
                     (i->first < key || key < i->first))
                  {
                    // this guy isn't ourselves, and IS talking to the guy we want to talk to.  Let's kill it
                    i->second->fKillRequest = true;
                    setWaitFor.insert(i->first);
                  }
                }


                stringstream ssThreadName;
                ssThreadName<<"Send thd for "<<cdTarget->lstIds[0];
                SetThreadName(ArtGetCurrentThreadId(),ssThreadName.str().c_str());
                fConnectSuccess = true;
              }
            }
          }

          for(unordered_set<SERVERKEY>::const_iterator i = setWaitFor.begin(); i != setWaitFor.end(); i++)
          {
            while(true) // let's wait until this thread is gone, or we've been told to exit
            {
              ClientDataPtr cdWaitFor;
              {
                AutoLeaveCS _cs(m_cs);
                ARTTYPE map<SERVERKEY,ClientDataPtr>::const_iterator iFind = m_mapClientData.find(*i);
                if(iFind != m_mapClientData.end())
                {
                  cdWaitFor = iFind->second;
                }
                else
                {
                  // this guy is dead!  we're done the loop for this SERVERKEY
                  break;
                }
              } // exit critical section

              if(!cdTarget || cdTarget->fKillRequest)
              {
                fConnectSuccess = false;
                break;
              }
              else if(!cdWaitFor->hThread || 
                      !cdWaitFor->hThread->joinable() || 
                      cdWaitFor.use_count() <= 1)
              {
                // this thread has been shut down!
                break; // onto the next thing we have to wait for
              }
              else
              {
                cdWaitFor->fKillRequest = true; // let's keep reminding them in case we had a weird race condition
                ArtSleep(100); // don't destroy our CPU while we wait!
              }
            }
          }
        }
        if(!fConnectSuccess) // or we've been told to die
        {
          if(sData)
          {
            sData->close();
          }
          sData.reset();
          AutoLeaveCS _cs(m_cs);
          if(m_mapClientData.find(key) != m_mapClientData.end())
          {
            m_mapClientData.erase(key);
          }
          cout<<"Did not successfully connect"<<endl;
          return 0;
        }
      }
    }

    cout<<ArtGetCurrentThreadId()<<": tcp send proc"<<endl;
    DWORD tmLastBandwidthUpdate = ArtGetTime();
    DWORD cbSentSinceBandwidthUpdate = 0;


    // thread that queries the subclass for game state, then sends it to all the clients we have connected via their ClientData::sData sockets
    while(!GetShutdown())
    {
      ClientDataPtr cdTarget;
      {
        AutoLeaveCS _cs(m_cs);

        ARTTYPE map<SERVERKEY,ClientDataPtr>::const_iterator iFind = m_mapClientData.find(key);
        if(iFind != m_mapClientData.end())
        {
          cdTarget = iFind->second;
          DASSERT(cdTarget->hThread && cdTarget->hThread->get_id() == boost::this_thread::get_id());
          if(cdTarget->fKillRequest)
          {
            cout<<"Exiting because it was requested we do so"<<endl;
            break; // it has been requested that we exit.  So let's exit
          }
        }
        else
        {
          // this guy must have been erased, so this thread is done
          break;
        }
      }
      
      ActiveTracker tracker;
      if(!cdTarget || !cdTarget->hThread || cdTarget->lstIds.size() <= 0 || !AcquireActive(cdTarget->lstIds[0],&tracker))
      {
        break;
      }

      DASSERT(cdTarget->hThread->get_id() == boost::this_thread::get_id());


      int msLoopWait = 100;
      
      bool fPlayerDead = false;
      { // grab the send handle, and send as many game states as can be squeezed from it before it becomes out-of-date
        SendHandle sendHandle;
        const DWORD tmStart = ArtGetTime();

        if(SimpleServer_StartSendHandle(&sendHandle, *cdTarget))
        {
          int cSends = 0;
          int rgSendTypes[7];
          memset(rgSendTypes,0,sizeof(rgSendTypes));
          TotalGameState tgsLast;
          do
          {
            cdTarget->cTGSBuilds++;
            TotalGameState tgs;
            int msWait = SimpleServer_BuildGameState(sendHandle, *cdTarget, &tgs,&fPlayerDead);
            msLoopWait = min(msWait,msLoopWait);
            //rgSendTypes[tgs.eType]++;
            if(msWait == 0)
            {
              // send now!
              DASSERT(cSends == 0 || memcmp(&tgsLast,&tgs,sizeof(tgsLast)) != 0 || tgs.eType == CHAT_UPDATE);
              tgsLast = tgs;

              cSends++;

              DASSERT(TDGGameState::GetChecksum(&tgs) == tgs.checksum);
              const int cbSent = cdTarget->sData->send(&tgs,sizeof(tgs),0);
              if(cbSent == sizeof(tgs))
              {
                cbSentSinceBandwidthUpdate += cbSent;
                // sent the data.  Let's see if there's anything important waiting for us from the client.    

                int cbWaiting = cdTarget->sData->peekReadable();

                while(cbWaiting >= sizeof(ClientC2S) && !fPlayerDead)
                {
                  // I guess they want to talk to us.
                  //cout<<"We've got "<<cbWaiting<<" bytes waiting on the TCP channel"<<endl;

                  ClientC2S c2s;
                  const int cbReceived = cdTarget->sData->recv(&c2s,sizeof(c2s),0);
                  if(cbReceived >= sizeof(c2s))
                  {
                    SimpleServer_NotifyC2SData(*cdTarget,c2s);
                  }
                  else
                  {
                    // we were told there was enough to fill in a ClientC2S struct, but we didn't get any?  this guy is dead fo sho
                    fPlayerDead = true;
                    break;
                  }
                  cbWaiting = cdTarget->sData->peekReadable();
                }

              }
              else
              {
                fPlayerDead = true;
              }
            }
            else if(msWait < 0)
            {
              fPlayerDead = true;
            }
            else
            {
              break; // we've extracted all the info out of this guy...
            }
          } while(SimpleServer_IsHandleCurrent(sendHandle) && !fPlayerDead);
          //cout<<"Did "<<cSends<<" sends with one handle"<<endl;
        }
        const DWORD tmDone = ArtGetTime();
        const DWORD tmTaken = tmDone-tmStart;
        cTotalLoops++;
        if(tmTaken < 17 && !fPlayerDead)
        {
          DASSERT(!m_cs.IsOwned());
          ArtSleep(17-tmTaken);

          tmTotalSleep += (17-tmTaken);
        }
      }

      if(fPlayerDead)
      {
        cout<<ArtGetCurrentThreadId()<<": Been "<<TIMEOUT_TIME<<"ms since last successful communication with player[s] at "<<key<<".  Cutting."<<endl;
        break;
      }

      DWORD tmNow = ArtGetTime();
      const int cSecondsBetweenOutput = 60;
      if(tmNow - tmLastBandwidthUpdate > cSecondsBetweenOutput*1000)
      {
        cout<<ArtGetCurrentThreadId()<<": Sent "<<cbSentSinceBandwidthUpdate/cSecondsBetweenOutput<<"b/s (avg sleep per iteration: "<<(tmTotalSleep/(cTotalLoops+1))<<")"<<endl;
        tmLastBandwidthUpdate = tmNow;
        cbSentSinceBandwidthUpdate = 0;
      }
    }

    { // since this thread should be the only guy working with a given key at a given time, it should be safe to remove this clientdata
      AutoLeaveCS _cs(m_cs);
      ARTTYPE map<SERVERKEY,ClientDataPtr>::const_iterator iFind = m_mapClientData.find(key);
      if(iFind != m_mapClientData.end())
      {
        ClientDataPtr cd = iFind->second;
        this->m_mapClientData.erase(key);
        _cs.Leave();

        this->SimpleServer_NotifyDeadPlayer(*cd);
      }
    }
    return 0;
  };

  LONGLONG tmTotalSleep;
  int cTotalLoops;

  DWORD UDPRecvProc()
  {
    cout<<"udp thread is "<<ArtGetCurrentThreadId()<<endl;
    if(!m_sUDPSocket) 
    {
      cout<<"no UDP sockets available"<<endl;
      return 0;
    }

    while(!GetShutdown())
    {
      cout<<"trying to bind udp"<<endl;
      if(0 == m_sUDPSocket->open())
      {
        if(0 == m_sUDPSocket->bind(m_iUDPRecvPort))
        {
          break;
        }
      }
      cout<<"failed to bind udp"<<endl;
      ArtSleep(500);
    }
    int cbInput = 0;
    int tmLastReport = 0;
    cout<<ArtGetCurrentThreadId()<<": "<<"Server bound to recv on port "<<m_iUDPRecvPort<<endl;
    while(!GetShutdown())
    {
      DWORD tmNow = ArtGetTime();
      ClientState cs = {0};

      ICommSocketAddressPtr pSenderAddr;
      int cbRecved = 0;
      cbRecved = m_sUDPSocket->recvfrom(&cs,sizeof(cs),&pSenderAddr);
      if(cbRecved <= 0)
      {
        ArtSleep(10);
        continue;
      }
      if(cbRecved == sizeof(cs))
      {
        cbInput += cbRecved;
        bool fFound = false;
        ClientDataPtr cd;
        {
          AutoLeaveCS _cs(m_cs);
          const SERVERKEY addrKey(pSenderAddr);
          DASSERT(m_cs.IsOwned());
          for(ARTTYPE map<SERVERKEY,ClientDataPtr>::iterator i = m_mapClientData.begin() ; i != m_mapClientData.end() && !fFound; i++)
          {
            if(i->first.dwAddr == addrKey.dwAddr)
            {
              // we have a client at that IP.  Let's see if that client is the one that sent this UDP packet by looking for the IDs we know are at that IP

              for(unsigned int x = 0; x < i->second->lstIds.size() && !fFound; x++)
              {
                if(i->second->lstIds[x] == cs.rgPlayerIds[0])
                {
                  fFound = true;
                  cd = i->second;
                }
              }
            }
          }
        }
        if(fFound)
        {
          DASSERT(cd->IsValid());
          SimpleServer_NotifyClientState(*cd,cs);
        }
      }
      else
      {

      }

      if(tmNow - tmLastReport > 10000)
      {
        cout<<"Recved "<<cbInput/10<<"b/sec"<<endl;
        cbInput = 0;
        tmLastReport = tmNow;
      }
    }
    return 0;
  };
  DWORD TCPConnectProc()
  {
    cout<<ArtGetCurrentThreadId()<<": tcp connect proc"<<endl;
    // wait for clients to request a connection.  once connected, send them an initial game-state packet
    
    while(!GetShutdown())
    {
      cout<<"listening for incoming connections to port "<<m_iTCPConnectPort<<endl;
      // socket is ready to listen
        
      bool fSuccess = false;
      SERVERKEY addrKey;
      ICommSocketAddressPtr pRemoteAddr;
      ICommSocketPtr aDataSocket = ICommSocketPtr(m_pSocketFactory->WaitForSocket(m_iTCPConnectPort,&pRemoteAddr));
      if(aDataSocket && pRemoteAddr)
      {
        aDataSocket->set_timeouts(5000,5000);

        addrKey = SERVERKEY(pRemoteAddr);
        fSuccess = true;
        
        bool fSpawnedThread = false;

        {
          AutoLeaveCS _cs(m_cs);
          DASSERT(m_cs.IsOwned());
          if(m_mapClientData.find(addrKey) == m_mapClientData.end())
          {
            // this guy isn't known to us.  Set up a TCP send/recv thread
            ClientDataPtr cd = ClientDataPtr(new ClientData(false));
            m_mapClientData.insert(std::pair<SERVERKEY,ClientDataPtr>(addrKey,cd));
            
            THREADSTART* pThd = new THREADSTART(aDataSocket,pRemoteAddr, this,addrKey);
            DASSERT(!cd->hThread);
            ArtInterlockedIncrement(&m_cActiveConnecting);
            cd->hThread = boost::shared_ptr<boost::thread>(new boost::thread(TCPSendThdProc<TotalGameState,ClientState,ClientDesc,ClientStartupInfo,ClientC2S,ServerStatus,SendHandle>,pThd));
            fSpawnedThread = true;
          }
          else
          {
            // we already know about this guy
          }
        }

        LONG lLoad = ArtInterlockedExchangeAdd(&m_cActiveConnecting,0);
        if(fSpawnedThread && lLoad >= 5)
        {
          ArtSleep(500);
        }
        
        cout<<"accepted connection to port "<<m_iTCPConnectPort<< " from "<<addrKey<<endl;
      }
      else
      {
        cout<<"Failure while setting up wait-for-socket"<<endl;
        ArtSleep(500);
      }

      if(!fSuccess)
      {
        AutoLeaveCS _cs(m_cs);
        aDataSocket.reset();
        cout<<"connectThd: Failed to connect to "<<addrKey<<". erasing"<<endl;
        m_mapClientData.erase(addrKey);
      }
    }
    cout<<"Exited connecting loop"<<endl;
    return 0;
  };
  
  friend DWORD WINAPI UDPRecvThdProc<TotalGameState,ClientState,ClientDesc,ClientStartupInfo, ClientC2S,ServerStatus,SendHandle>(LPVOID pv);
  friend DWORD WINAPI TCPConnectThdProc<TotalGameState,ClientState,ClientDesc,ClientStartupInfo, ClientC2S,ServerStatus,SendHandle>(LPVOID pv);
  friend DWORD WINAPI TCPSendThdProc<TotalGameState,ClientState,ClientDesc,ClientStartupInfo, ClientC2S,ServerStatus,SendHandle>(LPVOID pv);
protected:
  bool GetShutdown() const {return m_fShutdown;}
private:
  bool m_fShutdown;
  // receives the player info via the given socket
  bool ReceiveNewPlayerInfo(ICommSocketPtr& s, ICommSocketAddressPtr pRemoteAddr)
  {
    const LONG lConnectCount = ArtInterlockedExchangeAdd(&m_cActiveConnecting,0);
    bool fRet = false;
    vector<int> lstAssignedIds;
    ClientDesc cd;
    ClientState cs;
    
    bool fIsReconnect = false;
    STARTUPINFOZ<ClientStartupInfo> sfClientInfo;
    int err = 0;
    
    if(lConnectCount > 5)
    {
      // we have more than 5 people trying to actively connect
      TDGConnectionResult connResult;
      connResult.eResult = SERVERTOOBUSY;
      s->send(&connResult,sizeof(connResult),0);
      fRet = false;
      goto exit;
    }

    err = s->recv(&cd,sizeof(cd),0);
    if(err != sizeof(cd))
    {
      fRet = false;
      goto exit;
    }
    if(cd.eConnType == CONNTYPE_QUERYSTATUS)
    {
      // they just want to query our status.  Send them a server-status block
      ServerStatus ss;
      SimpleServer_BuildServerStatus(&ss);
      s->send(&ss,sizeof(ss),0);
      fRet = false;
      goto exit;
    }
    else if(cd.eConnType == ::CONNTYPE_PLAYGAME)
    {
      bool fValidated = false;
      // let's validate the guy's md5 and email
      TDGConnectionResult connResult;
      if(SimpleServer_ValidateLogin(cd,&connResult))
      {
        // validated!
        fValidated = true;
      }
      else
      {
        cout<<"Rejected login from "<<cd.szUsername<<endl;
        fValidated = false;
      }
      s->send(&connResult,sizeof(connResult),0);
      if(!fValidated)
      {
        fRet = false;
        goto exit;
      }
    }
    cout<<ArtGetCurrentThreadId()<<": Received "<<err<<" bytes of clientdesc - "<<cd.cLocalPlayers<<" players at that location"<<endl;
    if(cd.cLocalPlayers <= 0)
    {
      fRet = false;
      goto exit;
    }
    // we should have received sizeof(ClientDesc) bytes.  Now let's get the initial client state...
    err = s->recv(&cs,sizeof(cs),0);
    if(err != sizeof(cs))
    {
      fRet = false;
      goto exit;
    }

    // if we've got a non-null GUID, we should make sure that this guy doesn't already have a thread attempting to connect him
    if(!cd.guid.is_nil())
    {
      AutoLeaveCS _cs(m_cs);
      if(m_setConnecting.find(cd.guid) != m_setConnecting.end())
      {
        fRet = false; // you're already connecting!  go away!
        goto exit;
      }
      m_setConnecting.insert(cd.guid);
    }

    if(!cd.guid.is_nil() && m_mapGuidIds.find(cd.guid) != m_mapGuidIds.end())
    {
      // this must be a reconnection.  if we can find this guy's GUID, we should revive him.
      sfClientInfo.guid = cd.guid;
      const vector<int>& lstGuidIds = m_mapGuidIds[cd.guid];

      for(unsigned int x = 0;x < lstGuidIds.size(); x++) sfClientInfo.rgIds[x] = lstGuidIds[x];
      fIsReconnect = true;
      cout<<"Reconnection from player "<<sfClientInfo.rgIds[0]<<endl;
    }
    else
    {
      SimpleServer_AssignIds(cd, sfClientInfo.rgIds);

      boost::uuids::random_generator gen;
      sfClientInfo.guid = gen();
      fIsReconnect = false;
      cout<<"Assigned player id "<<sfClientInfo.rgIds[0]<<endl;
    }

    
    // so now we have a client state and client description, plus we've sent him the map data for our new client.  Let's chuck that crap into the list...
    for(int x = 0;x < cd.cLocalPlayers; x++) lstAssignedIds.push_back(sfClientInfo.rgIds[x]);
    {
      AutoLeaveCS _cs(m_cs);
      m_mapGuidIds[sfClientInfo.guid] = lstAssignedIds;
    }
    if(lstAssignedIds.size() > 0)
    {
      AddClient(pRemoteAddr, cd, cs, s, lstAssignedIds,fIsReconnect);
      fRet = true;
    }
    else
    {
      fRet = false;
    }

    // as soon as we send the startup info, the client will go into "game" mode, which may include connection reattempts.  So we can't send the StartupInfo
    // until we're past all the points where we might stall.  AddClient may take a LONG time since it needs to do a game-state transaction AND a DB write.
    SimpleServer_BuildStartupInfo(sfClientInfo);
    err = s->send(&sfClientInfo,sizeof(sfClientInfo),0); 
    if(err != sizeof(sfClientInfo)) 
    {
      fRet = false;
      goto exit;
    }
    

exit:
    if(!cd.guid.is_nil())
    {
      AutoLeaveCS _cs(m_cs);
      m_setConnecting.erase(cd.guid); // all done connecting
    }
    return fRet;
  }
  void AddClient(ICommSocketAddressPtr endpoint, const ClientDesc& cd, const ClientState& cs, ICommSocketPtr& sData, const vector<int>& lstIds, bool fIsReconnect)
  {
    ClientDataPtr clientMeta;
    {
      AutoLeaveCS _cs(m_cs);
      clientMeta = ClientDataPtr(new ClientData(cd,cs,endpoint, lstIds, sData));
    
      const SERVERKEY addrKey(endpoint);
      ARTTYPE map<SERVERKEY,ClientDataPtr>::const_iterator iFind = m_mapClientData.find(addrKey);
      if(iFind != m_mapClientData.end())
      {
        // if we've already got a thread working on this address, then just use its handle
        clientMeta->hThread = iFind->second->hThread;
      }
      m_mapClientData[addrKey] = clientMeta;
      if(!clientMeta->hThread) return; // we're boned.  Let's try not to crash here, anyway...

      DASSERT(clientMeta->hThread->get_id() == boost::this_thread::get_id());

      _cs.Leave();
      if(!fIsReconnect)
      {
        SimpleServer_NotifyNewPlayer(*clientMeta);
      }
      else
      {
        // if the underlying implementation wants to permit reconnection, then tell them that this guy is back
        SimpleServer_NotifyReconnectedPlayer(*clientMeta);
      }
    }
  }
private:

protected:
  mutable ManagedCS m_cs;
private:
  boost::thread m_hTCPConnectThread; // thread for receiving incoming connection requests
  boost::thread m_hUDPRecvThread; // thread for receiving UDP state packets
  boost::thread m_hTCPSendThread;
  const int m_iUDPRecvPort;
  const int m_iTCPConnectPort;
  ICommSocketFactoryPtr m_pSocketFactory;
  
  // our UDP socket.  We make this a member so that during shutdown, we can force-kill it
  IDGramCommSocketPtr m_sUDPSocket;

  // mapping from addresses to client data.  must only be accessed within m_cs
  map<SERVERKEY,ClientDataPtr> m_mapClientData;
  map<boost::uuids::uuid,vector<int> > m_mapGuidIds;
  
  unordered_set<boost::uuids::uuid> m_setConnecting; // who is currently connecting?  This lets us bounce fresh connections if they try to connect with the same GUID as one we're already handling

  LONG m_cActiveConnecting; // how many people are connecting to this server right now?
};

template<class TotalGameState,class ClientState,class ClientDesc,class ClientStartupInfo, class ClientC2S, class ServerStatus,class SendHandle>
DWORD WINAPI TCPSendThdProc(LPVOID pv)
{
  cout<<"TCPSendThdProc: "<<ArtGetCurrentThreadId()<<endl;
  THREADSTART* pThd = (THREADSTART*)pv;
  SimpleServer<TotalGameState,ClientState,ClientDesc,ClientStartupInfo,ClientC2S,ServerStatus,SendHandle>* pServer = (SimpleServer<TotalGameState,ClientState,ClientDesc,ClientStartupInfo,ClientC2S,ServerStatus,SendHandle>*)pThd->pvServer;
  DWORD ret = pServer->TCPSendProc(pThd->sData, pThd->pRemoteAddr, pThd->ulAddr);
  delete pThd;
  return ret;
}

template<class TotalGameState,class ClientState,class ClientDesc,class ClientStartupInfo, class ClientC2S, class ServerStatus,class SendHandle>
DWORD WINAPI TCPConnectThdProc(LPVOID pv)
{
  cout<<"TCPConnectThdProc: "<<ArtGetCurrentThreadId()<<endl;
  SimpleServer<TotalGameState,ClientState,ClientDesc,ClientStartupInfo,ClientC2S,ServerStatus,SendHandle>* pServer = (SimpleServer<TotalGameState,ClientState,ClientDesc,ClientStartupInfo,ClientC2S,ServerStatus,SendHandle>*)pv;
  DWORD dw = pServer->TCPConnectProc();
  return dw;
}
template<class TotalGameState,class ClientState,class ClientDesc,class ClientStartupInfo, class ClientC2S, class ServerStatus,class SendHandle>
DWORD WINAPI UDPRecvThdProc(LPVOID pv)
{
  cout<<"UDBRecvThdProc: "<<ArtGetCurrentThreadId()<<endl;

  SimpleServer<TotalGameState,ClientState,ClientDesc,ClientStartupInfo,ClientC2S,ServerStatus,SendHandle>* pServer = (SimpleServer<TotalGameState,ClientState,ClientDesc,ClientStartupInfo,ClientC2S,ServerStatus,SendHandle>*)pv;
  DWORD dw = pServer->UDPRecvProc();
  return dw;
}