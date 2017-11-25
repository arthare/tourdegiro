#include "stdafx.h"
#include "AutoCS.h"
#ifdef _WIN32
ManagedCS::ManagedCS()
{
  wincs = new CRITICAL_SECTION;
  InitializeCriticalSection((LPCRITICAL_SECTION)wincs);
}
ManagedCS::~ManagedCS()
{
  DeleteCriticalSection((LPCRITICAL_SECTION)wincs);
  delete wincs;
}

void ManagedCS::Enter()
{
  EnterCriticalSection((LPCRITICAL_SECTION)wincs);
}
void ManagedCS::Leave()
{
  LeaveCriticalSection((LPCRITICAL_SECTION)wincs);
}
bool ManagedCS::IsOwned() const
{
  return (DWORD)((LPCRITICAL_SECTION)wincs)->OwningThread == GetCurrentThreadId();
}
bool ManagedCS::Try()
{
  return !!TryEnterCriticalSection((LPCRITICAL_SECTION)wincs);
}
#else

ManagedCS::ManagedCS() : m_fLocked(false)
{
}
ManagedCS::~ManagedCS()
{
}
  
void ManagedCS::Enter()
{
  m_fLocked = true;
  m_mutex.lock();
}
void ManagedCS::Leave()
{
  m_mutex.unlock();
  m_fLocked = false;
}
bool ManagedCS::IsOwned() const
{
  return m_fLocked;
}
bool ManagedCS::Try()
{
  return m_mutex.try_lock();
}

#endif
