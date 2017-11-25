#include "stdafx.h"
#include "ArtNet.h"

typedef boost::shared_ptr<boost::asio::ip::tcp::socket> BoostSocketPtr;
typedef boost::shared_ptr<boost::asio::ip::udp::socket> BoostUDPSocketPtr;


int TimeoutRead(boost::asio::ip::tcp::socket& s, char* buf, int cbBuf, int flags, int timeout, bool* pfConnectionLost);

// linux is a pile of bullshit, and actually keeps ports open AFTER a process has exited.  Therefore, after shutting down
// an app, you've got to wait like 2 minutes.  This function blocks until the port is available.
boost::system::error_code WaitingBind(boost::asio::ip::tcp::acceptor& s, boost::asio::ip::tcp::endpoint& endpoint);

bool TCPConnect(boost::asio::io_service& service, boost::asio::ip::tcp::socket& socket, const char* pszTarget, int iTargetPort, boost::asio::ip::tcp::endpoint* pAddrServer);
int RecvStruct(boost::asio::ip::tcp::socket& s, char* buf, int len, int flags);
int SendStruct(boost::asio::ip::tcp::socket& s, const char* buf, int len, int flags);

class SendTimeoutAdjust
{
public:
  SendTimeoutAdjust(unsigned int dwTimeout) : m_dwTimeout(dwTimeout) {};
  
  template<class Protocol>
  int level(const Protocol& p) const {return SOL_SOCKET;}
  
  template<class Protocol>
  int name(const Protocol& p) const {return SO_SNDTIMEO;}
  
  template<class Protocol>
  const void* data(const Protocol& p) const {return &m_dwTimeout;}
  
  template<class Protocol>
  size_t size(const Protocol& p) const {return sizeof(m_dwTimeout);}
private:
  unsigned int m_dwTimeout;
};
class RecvTimeoutAdjust
{
public:
  RecvTimeoutAdjust(unsigned int dwTimeout) : m_dwTimeout(dwTimeout) {};
  
  template<class Protocol>
  int level(const Protocol& p) const {return SOL_SOCKET;}
  
  template<class Protocol>
  int name(const Protocol& p) const {return SO_SNDTIMEO;}
  
  template<class Protocol>
  const void* data(const Protocol& p) const {return &m_dwTimeout;}
  
  template<class Protocol>
  size_t size(const Protocol& p) const {return sizeof(m_dwTimeout);}
private:
  unsigned int m_dwTimeout;
};

class BoostCommSocket : public ICommSocket
{
public:
  BoostCommSocket(boost::asio::io_service& service) : m_service(service),m_s(new boost::asio::ip::tcp::socket(service)) 
  {
#ifdef _MSC_VER
    const long lNow = InterlockedIncrement(&s_lBoosts);
    std::cout<<"Currently "<<lNow<<" boosts in service"<<std::endl;
#endif
  };
  virtual ~BoostCommSocket() 
  {
#ifdef _MSC_VER
    const long lNow = InterlockedDecrement(&s_lBoosts);
    std::cout<<"Currently "<<lNow<<" boosts in service"<<std::endl;
#endif
    close();
  };
  static long s_lBoosts;

  virtual int send(const void* buf, int len, int flags)
  {
    return SendStruct(*m_s,(const char*)buf,len,flags);
  }
  virtual int peekReadable()
  {
    try
    {
      boost::asio::socket_base::bytes_readable cmdReadable;
      m_s->io_control(cmdReadable);
      return cmdReadable.get();
    }
    catch(...)
    {
      return 0;
    }
  }
  virtual int recv(void* buf, int len, int flags)
  {
    return RecvStruct(*m_s,(char*)buf,len,flags);
  }
  virtual int timeoutRecv(void* buf, int len, int flags, int timeout, bool* pfConnectionLost)
  {
    return TimeoutRead(*m_s,(char*)buf,len,flags,timeout,pfConnectionLost);
  }

  virtual bool connect(const char* pszTarget,int port, ICommSocketAddressPtr* pspResolved);

  // sends the send and receive timeouts, in milliseconds
  virtual void set_timeouts(int recv, int send)
  {
    try
    {
      if(m_s)
      {
        SendTimeoutAdjust adjusts(send);
        m_s->set_option(adjusts);
        RecvTimeoutAdjust adjustr(recv);
        m_s->set_option(adjustr);
      }
    }
    catch(...)
    {
    }
  }

  virtual void close()
  {
    try
    {
      if(m_s && m_s->is_open())
      {
        m_s->close();
      }
    }
    catch(...)
    {
    }
  }

  BoostSocketPtr GetBoostSocket() {return m_s;}
private:
  boost::asio::io_service& m_service;
  BoostSocketPtr m_s;
};
long BoostCommSocket::s_lBoosts = 0;

typedef boost::shared_ptr<ICommSocket> ICommSocketPtr;

class BoostCommSocketAddress : public ICommSocketAddress
{
public:
  BoostCommSocketAddress(const boost::asio::ip::tcp::endpoint& endpoint) : m_endpoint(endpoint) {};
  virtual ~BoostCommSocketAddress() {};

  virtual unsigned int ToDWORD()
  {
    return m_endpoint.address().to_v4().to_ulong();
  }
  
  virtual int get_local_port()
  {
    return m_endpoint.port();
  }

  boost::asio::ip::tcp::endpoint GetEndPoint() const {return m_endpoint;}
private:
  boost::asio::ip::tcp::endpoint m_endpoint;
};

class BoostDGramCommSocket : public IDGramCommSocket
{
public:
  BoostDGramCommSocket(boost::asio::io_service& service) : m_send(new boost::asio::ip::udp::socket(service))
  {

  };
  virtual ~BoostDGramCommSocket() {};

  virtual int send_to(ICommSocketAddress* target, const void* pvData, int len, int flags, int iTargetPort)
  {
    try
    {
      BoostCommSocketAddress* pAddr = (BoostCommSocketAddress*)target;
      boost::asio::ip::tcp::endpoint epTarget = pAddr->GetEndPoint();

      std::vector<boost::asio::const_buffer> lst;
      lst.push_back(boost::asio::const_buffer(&pvData,len));
            
      boost::asio::ip::udp::endpoint udpTarget(epTarget.address().to_v4(),iTargetPort);
    
      boost::system::error_code ec;
      m_send->send_to(lst,udpTarget,0,ec);
      if(!ec)
      {
        return len;
      }
      return 0;
    }
    catch(...)
    {
      return 0;
    }
  }
  virtual int open()
  {
    try
    {
      if(m_send->is_open())
      {
        m_send->close();
      }

      m_send->open(boost::asio::ip::udp::v4());
      return 0;
    }
    catch(...)
    {
      return -1;
    }
  }
  virtual void close()
  {
    try
    {
      if(m_send->is_open())
      {
        m_send->close();
      }
      
    }
    catch(...)
    {
      
    }
    m_send.reset();
  }
  virtual int bind(int iLocalPort)
  {
    try
    {
      boost::asio::ip::udp::endpoint ep(boost::asio::ip::udp::v4(), iLocalPort);
      m_send->bind(ep);
    }
    catch(boost::system::system_error& e)
    {
      return 1;
    }
    return 0;
  }

  virtual int recvfrom(void* pvData,int len,ICommSocketAddressPtr* pspAddress)
  {
    try
    {
      std::vector<boost::asio::mutable_buffer> lstBufs;
      lstBufs.push_back(boost::asio::mutable_buffer(pvData,len));

      boost::asio::ip::udp::endpoint senderAddr;

      const int ret = m_send->receive_from(lstBufs,senderAddr);

      boost::asio::ip::tcp::endpoint tcp(senderAddr.address(),senderAddr.port());
      *pspAddress = ICommSocketAddressPtr(new BoostCommSocketAddress(tcp));
      return ret;
    }
    catch(...)
    {
      return 0;
    }
  }
private:
  BoostUDPSocketPtr m_send;
};
typedef boost::shared_ptr<IDGramCommSocket> IDGramCommSocketPtr;

class BoostCommSocketFactory : public ICommSocketFactory
{
public:
  BoostCommSocketFactory() : m_sAcceptor(m_service) 
  {

  };
  virtual ~BoostCommSocketFactory() {};

  virtual ICommSocket* NewSocket()
  {
    return new BoostCommSocket(m_service);
  }
  virtual IDGramCommSocket* NewDGramSocket()
  {
    return new BoostDGramCommSocket(m_service);
  }

  virtual ICommSocketPtr WaitForSocket(int listenPort, ICommSocketAddressPtr* pspRemoteAddress)
  {
    try
    {
      if(!m_sAcceptor.is_open() || m_sAcceptor.local_endpoint().port() != listenPort)
      {
        if(m_sAcceptor.is_open())
        {
          m_sAcceptor.close();
        }

        boost::system::error_code err;
        boost::asio::ip::address addr = boost::asio::ip::address::from_string("0.0.0.0",err);
        boost::asio::ip::tcp::endpoint localBind(addr, listenPort);
        m_sAcceptor.open(localBind.protocol(),err);
        err = WaitingBind(m_sAcceptor,localBind);
        if(err != 0) return ICommSocketPtr();
      }

      m_sAcceptor.listen();
      ICommSocketPtr pRet(NewSocket());
      BoostCommSocket* pCommSocket = (BoostCommSocket*)pRet.get();
      m_sAcceptor.accept(*pCommSocket->GetBoostSocket());

      *pspRemoteAddress = ICommSocketAddressPtr(new BoostCommSocketAddress(pCommSocket->GetBoostSocket()->remote_endpoint()));
      return pRet;
    }
    catch(...)
    {
      return ICommSocketPtr();
    }
  }
  virtual ICommSocketAddress* NewAddress(const char* pszAddress, int port)
  {
    try
    {
      boost::system::error_code err;
      boost::asio::ip::address addr = boost::asio::ip::address::from_string(pszAddress,err);
      boost::asio::ip::tcp::endpoint end(addr,port);
      return new BoostCommSocketAddress(end);
    }
    catch(...)
    {
      return NULL;
    }
  }
  virtual void Shutdown()
  {
    m_sAcceptor.close();
  }
private:
  boost::asio::io_service m_service;
  boost::asio::ip::tcp::acceptor m_sAcceptor;
};
typedef boost::shared_ptr<ICommSocketFactory> ICommSocketFactoryPtr;


bool BoostCommSocket::connect(const char* pszTarget,int port, ICommSocketAddressPtr* pspResolved)
{
  try
  {
    boost::asio::ip::tcp::endpoint endpoint;
    if(TCPConnect(m_service,*m_s,pszTarget,port,&endpoint))
    {
      *pspResolved = ICommSocketAddressPtr(new BoostCommSocketAddress(endpoint));
      return true;
    }
  }
  catch(...)
  {
  }

  return false;
}


ICommSocketFactory* CreateBoostSockets()
{
  return new BoostCommSocketFactory();
}