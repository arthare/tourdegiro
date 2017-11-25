#include "stdafx.h"
#include "SimpleServer.h"

ManagedCS g_csSetActive;
unordered_map<int,int> g_mapActive; // maps from main ID to the thread that currently owns it

void AddActive(const int mainId)
{
#if 0
  DASSERT(g_csSetActive.IsOwned());
  g_mapActive.insert(std::pair<int,int>(mainId,ArtGetCurrentThreadId()));
#endif
}
void KillActive(const int mainId)
{
#if 0
  AutoLeaveCS _cs(g_csSetActive);
  //DASSERT(g_mapActive.find(mainId) != g_mapActive.end());
  g_mapActive.erase(mainId);
#endif
}
bool AcquireActive(const int key, ActiveTracker* pTracker)
{
#if 0
  AutoLeaveCS _cs(g_csSetActive);
  bool fWasActive = g_mapActive.find(key) != g_mapActive.end();
  if(fWasActive)
  {
    return false;
  }
  else
  {
    // acquire this active-ness while we're atomic.
    //DASSERT(g_mapActive.find(key) == g_mapActive.end());
    new (pTracker) ActiveTracker(key);
    //DASSERT(g_mapActive.find(key) != g_mapActive.end());
    return true;
  }
#else
  return true;
#endif
}