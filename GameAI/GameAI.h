#pragma once
#define DllExport

class IAI;

namespace GameAI
{
  DllExport int GetAICount(void);
  DllExport IAI* GetAI(const int ix, int iTargetWatts);
  DllExport void FreeAI(IAI* pAI);
}