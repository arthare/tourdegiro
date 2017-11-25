#include "stdafx.h"
#include "LocalStatsStore.h"
using namespace std;

SimpleLocalStatsStore::SimpleLocalStatsStore()
{
  ifstream in;
  wstring strPath = GetRoamingPath(L"TourDeGiro\\weights.txt");
  string strPathA(strPath.begin(),strPath.end());
  in.open(strPathA.c_str(),std::ios_base::in);
  

  string strFirst;
  in>>strFirst;

  if(strFirst.compare("dec14") == 0)
  {
    LoadDec14(in);
  }
  else
  {
    in.seekg(0); // in the original version, we would've just read some guy's name
    LoadOriginal(in);
  }

  bool fFirst = true;
  bool fNewVersion = false;
  
  in.close();
}
void SimpleLocalStatsStore::LoadOriginal(istream& in)
{
  while(!in.eof() && !in.fail())
  {
    string strName;
    float flMass;
    in>>strName;
    in>>flMass;
     
    m_mapMass[strName] = flMass;
    m_lstOrigNames.push_back(strName);
  }
}
void SimpleLocalStatsStore::LoadDec14(istream& in)
{
  while(!in.eof() && !in.fail())
  {
    string strName;
    float flMass;

    char line[500];
    line[0] = 0;
    do
    {
      in.getline(line,sizeof(line));
    } while(!line[0] && !in.eof() && !in.fail()); // keep loading until we fail or we actually get a line
    
    string strLine = line;
    int iMarker = strLine.find("|||");
    if(iMarker >= 0 && iMarker < strLine.length() - 3)
    {
      // we're good...
      const char* pszAfterMarker = &strLine[iMarker+3];
      strLine[iMarker]=0;
      strName = strLine.c_str(); // the .c_str() here is necessary so that strName re-interprets the data.  Otherwise it accepts/copies the entire original string, including the null terminator and stuff AFTER the null terminator
      flMass = atoi(pszAfterMarker);
      if(flMass > 0)
      {
        m_mapMass[strName] = flMass;
        m_lstOrigNames.push_back(strName);
      }
      else
      {
        // this particular line was bad, but we'll continue
      }
    }
    else
    {
      break; // bad format
    }
  }
}

SimpleLocalStatsStore::~SimpleLocalStatsStore()
{
  ofstream out;
  wstring strPath = GetRoamingPath(L"TourDeGiro\\weights.txt");
  string strPathA(strPath.begin(),strPath.end());
  out.open(strPathA.c_str());

  out<<"dec14"<<endl;
  for(map<string,float>::const_iterator i = m_mapMass.begin(); i != m_mapMass.end(); i++)
  {
    DASSERT(i->second >= 0 && i->second <= 200);
    if(i->first.length() > 0)
    {
      out<<i->first;
      out<<"|||";
      out<<i->second;
      out<<endl;
    }
  }
  out.close();
}

float SimpleLocalStatsStore::GetMass(const std::string& strName, float flMassDefault) const
{
  map<string,float>::const_iterator i = m_mapMass.find(strName);
  if(m_mapMass.find(strName) != m_mapMass.end())
  {
    return i->second;
  }
  else
  {
    return flMassDefault;
  }
}
void SimpleLocalStatsStore::SetMass(const std::string& strName, float flMassKg)
{
  m_mapMass[strName] = flMassKg;
}
void SimpleLocalStatsStore::GetNameList(std::vector<std::string>& lstNames) const
{
  lstNames = m_lstOrigNames;
  std::sort(lstNames.begin(),lstNames.end());
}