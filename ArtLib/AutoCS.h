#pragma once

#ifndef _WIN32

class ManagedCS
{
  ManagedCS(const ManagedCS& other) {};
  const ManagedCS& operator = (const ManagedCS&other);
public:
  ManagedCS();
  ~ManagedCS();
  
  void Enter();
  void Leave();
  bool IsOwned() const;
  void lock() {Enter();}
  void unlock() {Leave();}
  bool Try();
private:
  boost::recursive_mutex m_mutex;
  bool m_fLocked;
};

#else


class ManagedCS
{
  ManagedCS(const ManagedCS& other) {};
  const ManagedCS& operator = (const ManagedCS&other) {};
public:
  ManagedCS();
  ~ManagedCS();

  void Enter();
  void Leave();
  bool IsOwned() const;
  void lock() {Enter();}
  void unlock() {Leave();}
  bool Try();
private:
  void* wincs;
};
#endif
class AutoLeaveCS
{
public:
  AutoLeaveCS(ManagedCS& cs) : m_cs(cs),fLeft(false)
  {
    m_cs.Enter();
  }
  ~AutoLeaveCS()
  {
    if(!fLeft)
    {
      m_cs.Leave();
    }
  }
  void Leave()
  {
    m_cs.Leave();
    fLeft = true;
  }
private:
  ManagedCS& m_cs;
  bool fLeft;
};