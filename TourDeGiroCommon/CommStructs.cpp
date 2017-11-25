
#include "stdafx.h"
#include "CommStructs.h"
#include "Player.h"
const char* SYNC_STRING = "abcdefghijklmnopqrstuvwxyz";

// validates a given position slot to make sure that we're not getting garbage
bool IsValidPositionSlot(const POSITIONUPDATE& pos,int ixSlot)
{
  if(pos.GetPlayerId(ixSlot) < 0) return false;
  if(pos.GetPlayerId(ixSlot) == INVALID_PLAYER_ID) return false;

  if(pos.GetPlayerId(ixSlot) < 0) return false;
  return true;
}

//float g_rgLastPlayerPos[2000];
//bool g_fLastPlayerPosSet = false;

void POSITIONUPDATE::SetPlayerStats(int ix, int id, const IConstPlayer* pPlayer)
{
  if(!pPlayer)
  {
    rgPlayerPos[ix] = 0;
    rgPlayerLaps[ix] = 0;
    rgPlayerIds[ix] = INVALID_PLAYER_ID;
    rgPlayerWeights[ix] = 0;
    return;
  }
  //if(!g_fLastPlayerPosSet)
  //{
  //  memset(g_rgLastPlayerPos,0,sizeof(g_rgLastPlayerPos));
  //}
  SetLane(ix,pPlayer->GetLane());
  SetSpeed(ix,pPlayer->GetSpeed());
  SetPower(ix,pPlayer->GetPower());
  rgPlayerPos[ix] = pPlayer->GetDistance().flDistance;
  rgPlayerLaps[ix] = pPlayer->GetDistance().iCurrentLap;
  rgPlayerIds[ix] = id;
  rgPlayerWeights[ix] = (unsigned short)(pPlayer->GetMassKg()*10);
  if(!pPlayer->Player_IsAI()) 
    rgPlayerFlags[ix] |= PLAYERFLAG_HUMAN;

  if(pPlayer->Player_IsFrenemy()) 
    rgPlayerFlags[ix] |= PLAYERFLAG_FRENEMY;

  if(pPlayer->GetActionFlags() & ACTION_FLAG_DOOMEDAI) SetDead(ix);
  if(pPlayer->GetActionFlags() & ACTION_FLAG_GHOST) SetGhost(ix);
  if(pPlayer->GetActionFlags() & ACTION_FLAG_SPECTATOR) 
    SetSpectator(ix);
  if(pPlayer->GetId() & GHOST_BIT) SetGhost(ix);
  //DASSERT(rgPlayerPos[ix] >= g_rgLastPlayerPos[id]);
  //g_rgLastPlayerPos[id] = rgPlayerPos[ix];
}
int TDGGameState::GetChecksum(const TDGGameState* in)
{
  int sum = 0;
  const char* pBuffer = (const char*)in;
  for(int x = 0;x < sizeof(*in) - sizeof(int); x++) // the -sizeof(int) makes sure we don't checksum the checksum!
  {
    sum += pBuffer[x];
  }
  return sum;
}


string MakeIPString(unsigned int ip)
{
  unsigned char* pbIP = (unsigned char*)&ip;
  char szRet[100];
  snprintf(szRet,sizeof(szRet),"%d.%d.%d.%d",pbIP[0],pbIP[1],pbIP[2],pbIP[3]);
  return string(szRet);
}

ostream& operator<< (ostream &out, const SERVERKEY &cPoint)
{
  unsigned char* pbIP = (unsigned char*)&cPoint.dwAddr;
  out<<(int)pbIP[0]<<"."<<(int)pbIP[1]<<"."<<(int)pbIP[2]<<"."<<(int)pbIP[3]<<":"<<(int)cPoint.dwPort;
  return out;
}

MD5::MD5(const char* str)
{
  md5((unsigned char*)str,strlen(str),(unsigned int*)szMD5);

}
