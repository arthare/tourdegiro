#include "stdafx.h"
#include "ArtNet.h"
#include "ArtTools.h"

#include <boost/asio.hpp>

long ICommSocket::m_sCount=0;

int TimeoutRead(boost::asio::ip::tcp::socket& s, char* outbuf, int cbBuf, int flags, int timeout, bool* pfConnectionLost)
{
  *pfConnectionLost = false;

  try
  {
  const DWORD tmStart = ArtGetTime();
  while(true)
  {
    unsigned long cbWaiting = 0;
    
    boost::asio::socket_base::bytes_readable cmdReadable;
    s.io_control(cmdReadable);
    cbWaiting = cmdReadable.get();

    if(cbWaiting > 0)
    {
      boost::asio::mutable_buffer buf(outbuf,cbBuf);
      std::vector<boost::asio::mutable_buffer> lstBuf;
      lstBuf.push_back(buf);
      int cbRead = s.receive(lstBuf,flags);
      return cbRead;
    }
    else
    {
      const DWORD tmNow = ArtGetTime();
      if(tmNow - tmStart > timeout)
      {
        // timed out
        *pfConnectionLost = true;
        break;
      }
      else
      {
        // not timed-out yet
        ArtSleep(10);
      }
    }
  }
  }
  catch(const boost::system::system_error& e)
  {
    e;
    *pfConnectionLost = true;
    return 0;
  }
  catch(...)
  {
    std::cout<<"unknown exception "<<__FILE__<<":"<<__LINE__<<std::endl;
    *pfConnectionLost = true;
    return 0;
  }
  return 0;
}
boost::system::error_code WaitingBind(boost::asio::ip::tcp::acceptor& s, boost::asio::ip::tcp::endpoint& endpoint)
{
  while(true)
  {
    boost::system::error_code error;
    try
    {
      s.bind(endpoint,error);
    }
    catch(const boost::system::system_error& e)
    {
      error = e.code();
    }
    catch(...)
    {
      ArtSleep(500);
    }
    if(error)
    {
      std::cout<<"binding to "<<endpoint.port()<<": err: "<<error.value()<<std::endl;
      ArtSleep(500);
    }
    else
    {
      return boost::system::error_code();
    }
  }
}


bool TCPConnect(boost::asio::io_service& service, boost::asio::ip::tcp::socket& socket, const char* pszTarget, int iTargetPort, boost::asio::ip::tcp::endpoint* pAddrServer)
{
  try
  {
    boost::asio::ip::tcp::resolver resolver(service);
  
    boost::system::error_code resolve_err;

    std::string strResolved = pszTarget;

    char szPort[512];
    snprintf(szPort,sizeof(szPort),"%d",iTargetPort);
    boost::asio::ip::tcp::resolver::query query(strResolved,szPort);
    boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);
  
    std::cout<<"resolved: "<<strResolved<<std::endl;

    socket.connect(*iterator);
  
    *pAddrServer = socket.remote_endpoint();
  }
  catch(const boost::system::system_error& e)
  {
    e;
    return false;
  }
  catch(std::exception& e)
  {
    e;
    return false;
  }
  catch(...)
  {
    return false;
  }
  return true;
}

// blocks until ALL that the caller requested has been received
int RecvStruct(boost::asio::ip::tcp::socket& s, char* buf, int len, int flags)
{
  try
  {
    int cbRecved = 0;
    int cycles = 0;
    while(true)
    {
      std::vector<boost::asio::mutable_buffer> lstBufs;
      lstBufs.push_back(boost::asio::mutable_buffer(buf+cbRecved,len-cbRecved));

      boost::system::error_code err;
      const int cbRecv = s.receive(lstBufs,flags,err);
      if(err.value() != 0)
      {
        std::cout<<"failed during recvstruct after "<<cbRecved<<" bytes: "<<err<<std::endl;
        return 0;
      }
      else if(cbRecv > 0)
      {
        cycles++;
        cbRecved += cbRecv;
        if(cbRecved >= len) 
          return cbRecved;
      }
      else
      {
        return 0;
      }
    }
  }
  catch(boost::system::system_error& e)
  {
    e;
    return 0;
  }
  catch(...)
  {
    std::cout<<"unknown exception "<<__FILE__<<":"<<__LINE__<<std::endl;
    return 0;
  }
}

// blocks until ALL that the caller requested has been received
int SendStruct(boost::asio::ip::tcp::socket& s, const char* buf, int len, int flags)
{
  try
  {
    int cbSent = 0;
    int cycles = 0;
    while(true)
    {
      std::vector<boost::asio::const_buffer> lstBufs;
      lstBufs.push_back(boost::asio::const_buffer(buf+cbSent,len-cbSent));

      boost::system::error_code err;
      const int cbSentNow = s.send(lstBufs,flags,err);
      if(cbSentNow > 0)
      {
        cycles++;
        cbSent += cbSentNow;
        if(cbSent >= len) 
          return cbSent;
      }
      else
      {
        return 0;
      }
    }
  }
  catch(boost::system::system_error& e)
  {
    e;
    return 0;
  }
  catch(...)
  {
    return 0;
  }
}
