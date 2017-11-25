#pragma once
#include <boost/shared_ptr.hpp>

class ICommSocketAddress;
typedef boost::shared_ptr<ICommSocketAddress> ICommSocketAddressPtr;


class ICommSocket
{
public:
  ICommSocket()
  {
    //const LONG lRet = InterlockedIncrement(&m_sCount);
    //std::cout<<"++There are "<<lRet<<" ICommSockets lying around"<<std::endl;
  }
  virtual ~ICommSocket()
  {
    //const LONG lRet = InterlockedDecrement(&m_sCount);
    //std::cout<<"--There are "<<lRet<<" ICommSockets lying around"<<std::endl;
  }
  virtual int send(const void* buf, int len, int flags) = 0;

  // returns how many bytes are available for a recv or timeoutRecv call.  Does not block.
  virtual int peekReadable() = 0;
  virtual int recv(void* buf, int len, int flags) = 0;
  virtual int timeoutRecv(void* buf, int len, int flags, int timeout, bool* pfConnectionLost) = 0;

  virtual bool connect(const char* pszTarget,int port, ICommSocketAddressPtr* pspResolved) = 0;

  // sends the send and receive timeouts, in milliseconds
  virtual void set_timeouts(int recv, int send) = 0;


  virtual void close() = 0;
private:
  static long m_sCount;
};
typedef boost::shared_ptr<ICommSocket> ICommSocketPtr;

class IDGramCommSocket
{
public:
  virtual ~IDGramCommSocket() {};
  
  virtual int send_to(ICommSocketAddress* target, const void* pvData, int len, int flags, int iTargetPort) = 0;
  virtual int open() = 0;
  virtual void close() = 0;
  virtual int bind(int iLocalPort) = 0;
  virtual int recvfrom(void* pvData,int len,ICommSocketAddressPtr* pspAddress) = 0;
};
typedef boost::shared_ptr<IDGramCommSocket> IDGramCommSocketPtr;

class ICommSocketFactory
{
public:
  virtual ~ICommSocketFactory() {};
  
  virtual ICommSocket* NewSocket() = 0;
  virtual IDGramCommSocket* NewDGramSocket() = 0;

  virtual ICommSocketAddress* NewAddress(const char* pszAddress, int port) = 0;

  // waits for an incoming connection, then returns a representation of the socket received and the address it connected from
  virtual ICommSocketPtr WaitForSocket(int listenPort, ICommSocketAddressPtr* pspRemoteAddress) = 0;

  virtual void Shutdown() = 0;
};
typedef boost::shared_ptr<ICommSocketFactory> ICommSocketFactoryPtr;

class ICommSocketAddress
{
public:
  virtual unsigned int ToDWORD() = 0;
  virtual int get_local_port() = 0;
};
typedef boost::shared_ptr<ICommSocketAddress> ICommSocketAddressPtr;

ICommSocketFactory* CreateBoostSockets();
ICommSocketFactory* CreatePipeSockets();
