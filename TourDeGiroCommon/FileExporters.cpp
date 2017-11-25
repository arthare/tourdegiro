#include "stdafx.h"
#include "FileExporters.h"

string MakeUTCTime(int unixTime,bool fIncludeZ)
{

  // change me!
  SYSTEMTIME st = SecondsSince1970ToSYSTEMTIME(unixTime, !fIncludeZ);
  
  string strZ = "";
  if(fIncludeZ)
  {
    strZ = "Z";
  }

  char szBuf[1000];
  snprintf(szBuf,sizeof(szBuf),"%04d-%02d-%02dT%02d:%02d:%02d%s",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,strZ.c_str());
  return szBuf;
}

std::wstring WriteTCXFile(LPCTSTR lpszPath, bool fOverwrite, bool fTrainerMode, IConstPlayerDataPtrConst pPlayer, const std::vector<RECORDEDDATA> lstData)
{
  if(lstData.size() <= 0) return L"";

  TCHAR szAdjusted[MAX_PATH];
  wcscpy(szAdjusted,lpszPath);
  wcscat(szAdjusted,L".tcx");
  int count = 1;
  while(!fOverwrite && DoesFileExist(szAdjusted))
  {
    _snwprintf(szAdjusted,NUMCHARS(szAdjusted),L"%s.%d.tcx",lpszPath,count);
    count++;
  }

  
  ofstream out;
  OpenWofStream(out,szAdjusted);

  unsigned int tmNow = ::GetSecondsSince1970();
  string strTmAndId = MakeUTCTime(tmNow - pPlayer->GetTimeRidden(),true);

  const float flDistance = lstData.back().dist.ToMeters() - lstData.front().dist.ToMeters();

  out<<"<?xml version=\"1.0\"?>"<<endl;
  out<<"<TrainingCenterDatabase xmlns=\"http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"http://www.garmin.com/xmlschemas/ActivityExtension/v2 http://www.garmin.com/xmlschemas/ActivityExtensionv2.xsd http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2 http://www.garmin.com/xmlschemas/TrainingCenterDatabasev2.xsd\">"<<endl;
  out<<"<Activities>"<<endl;
  out<<"<Activity Sport=\"Biking\">"<<endl;
  out<<"<Id>"<<strTmAndId<<"</Id>"<<endl;
  out<<"<Lap StartTime=\""<<strTmAndId<<"\">"<<endl;
  out<<"<TotalTimeSeconds>"<<((int)pPlayer->GetTimeRidden())<<"</TotalTimeSeconds>"<<endl;
  out<<"<DistanceMeters>"<<(int)(flDistance)<<"</DistanceMeters>"<<endl;
  out<<"<Calories>"<<(int)(pPlayer->GetEnergySpent()/1000.0)<<"</Calories>"<<endl;
  out<<"<AverageHeartRateBpm>"<<endl;
  out<<"<Value>0</Value>"<<endl;
  out<<"</AverageHeartRateBpm>"<<endl;
  out<<"<MaximumHeartRateBpm>"<<endl;
  out<<"<Value>0</Value>"<<endl;
  out<<"</MaximumHeartRateBpm>"<<endl;
  out<<"<Intensity>Active</Intensity>"<<endl;
  out<<"<TriggerMethod>Manual</TriggerMethod>"<<endl;
  out<<"<Track>"<<endl;
  
  const unsigned int secondsRidden = pPlayer->GetTimeRidden();
  const unsigned int rideStartTime = tmNow - secondsRidden;
  float tmLast = lstData.front().flTime;
      for(unsigned int x = 0;x < lstData.size(); x++)
      {
        const RECORDEDDATA& rec = lstData[x];
        if(rec.dist.ToMeters() <= 0 && !fTrainerMode)
        {
          // a zero-distance sample, but we want nonwarmup stuff
          continue;
        }
        if(rec.flTime - tmLast < 1.1)
        {
          // too close together
          continue;
        }
        tmLast = rec.flTime;

        const unsigned int tmAtDataPoint = rideStartTime + (unsigned int)rec.flTime;

        string strTm = MakeUTCTime(tmAtDataPoint,true);
        
        out<<"<Trackpoint>"<<endl;
        out<<"<Time>"<<strTm<<"</Time>"<<endl;
        out<<"<DistanceMeters>"<<(int)rec.dist.ToMeters()<<"</DistanceMeters>"<<endl;
        out<<"<HeartRateBpm xsi:type=\"HeartRateInBeatsPerMinute_t\">"<<endl;
        out<<"<Value>"<<(int)rec.hr<<"</Value>"<<endl;
        out<<"</HeartRateBpm>"<<endl;
        out<<"<Cadence>"<<(int)rec.cadence<<"</Cadence>"<<endl;
        out<<"<AltitudeMeters>"<<(int)rec.flElev<<"</AltitudeMeters>"<<endl;
        out<<"<Extensions>"<<endl;
        out<<"<TPX xmlns=\"http://www.garmin.com/xmlschemas/ActivityExtension/v2\">"<<endl;

        char szSpeed[100];
        snprintf(szSpeed,sizeof(szSpeed),"%4.1f",rec.flSpeed);

        out<<"<Speed>"<<szSpeed<<"</Speed>"<<endl;
        out<<"<Watts>"<<(int)rec.power<<"</Watts>"<<endl;
        out<<"</TPX>"<<endl;
        out<<"</Extensions>"<<endl;
        out<<"</Trackpoint>"<<endl;
      }
    out<<"</Track>"<<endl;
    out<<"</Lap>"<<endl;
    out<<"<Creator xsi:type=\"Device_t\">"<<endl;
    out<<"<Name>Tour de Giro</Name>"<<endl;
    out<<"<UnitId>0</UnitId>"<<endl;
    out<<"<ProductID>0</ProductID>"<<endl;
    out<<"<Version>"<<endl;
    out<<"<VersionMajor>1</VersionMajor>"<<endl;
    out<<"<VersionMinor>0</VersionMinor>"<<endl;
    out<<"<BuildMajor>0</BuildMajor>"<<endl;
    out<<"<BuildMinor>0</BuildMinor>"<<endl;
    out<<"</Version>"<<endl;
    out<<"</Creator>"<<endl;

    out<<"</Activity>"<<endl;
    out<<"</Activities>"<<endl;
    out<<"<Author xsi:type=\"Application_t\">"<<endl;
    out<<"<Name>Tour de Giro</Name>"<<endl;
    out<<"<Build>"<<endl;
    out<<"<Version>"<<endl;
    out<<"<VersionMajor>1</VersionMajor>"<<endl;
    out<<"<VersionMinor>0</VersionMinor>"<<endl;
    out<<"<BuildMajor>0</BuildMajor>"<<endl;
    out<<"<BuildMinor>0</BuildMinor>"<<endl;
    out<<"</Version>"<<endl;
    out<<"<Type>Beta</Type>"<<endl;
    out<<"</Build>"<<endl;
    out<<"<LangID>en</LangID>"<<endl;
    out<<"<PartNumber>000-00000-00</PartNumber>"<<endl;
    out<<"</Author>"<<endl;
    out<<"</TrainingCenterDatabase>"<<endl;

    out.close();

    return szAdjusted;
}

void WritePWXFile(LPCTSTR lpszPath, bool fOverwrite, bool fTrainerMode, IConstPlayerDataPtrConst pPlayer, const std::vector<RECORDEDDATA> lstData)
{
  if(lstData.size() <= 0) return;

  TCHAR szAdjusted[MAX_PATH];
  wcscpy(szAdjusted,lpszPath);
  wcscat(szAdjusted,L".pwx");
  int count = 1;
  while(!fOverwrite && DoesFileExist(szAdjusted))
  {
    _snwprintf(szAdjusted,NUMCHARS(szAdjusted),L"%s.%d.pwx",lpszPath,count);
    count++;
  }

  ofstream out;
  OpenWofStream(out,szAdjusted);
  out<<"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>"<<endl;
  out<<"<pwx xmlns=\"http://www.peaksware.com/PWX/1/0\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" creator=\"Tour de Giro\" version=\"1.0\" xsi:schemaLocation=\"http://www.peaksware.com/PWX/1/0 http://www.peaksware.com/PWX/1/0/pwx.xsd\">"<<endl;
    out<<"<workout>";
      out<<"<athlete>";
        out<<"<name />";
        out<<"<weight>"<<(int)pPlayer->GetMassKg()<<"</weight>"<<endl;
      out<<"</athlete>";
      out<<"<sportType>Bike</sportType>";
      out<<"<device id=\"\">";
        out<<"<make>";
        switch(pPlayer->GetPowerType())
        {
          case ANT_POWER: out<<"ANT+ PM"; break;
          case ANT_SPEED: out<<"ANT+ S/C"; break;
          case ANT_SPEEDONLY: out<<"ANT+ Speed-only"; break;
          case COMPUTRAINER: out<<"CompuTrainer"; break;
          case COMPUTRAINER_ERG: out<<"CompuTrainer(Erg)"; break;
          case CHEATING: out<<"Pacer"; break;
          case SPECTATING: out<<"Spectating"; break;
          case AI_POWER: out<<"AI"; break;
          case WAHOO_KICKR: out<<"WK"; break;
          case ANT_FEC: out<<"FE-C"; break;
          case POWERCURVE: out<<"Powercurve"; break;
          case PT_UNKNOWN: out<<"Unknown"; break;
        }
        out<<"</make>";
        out<<"<model>0</model>";
        out<<"<extension />";
      out<<"</device>";
      out<<"<time>"<<MakeUTCTime(::GetSecondsSince1970GMT(),false).c_str()<<"</time>";
      out<<"<summarydata>";
        out<<"<beginning>0</beginning>";
        out<<"<duration>"<<(int)(lstData.back().flTime - lstData.front().flTime)<<"</duration>";
        out<<"<durationstopped>0</durationstopped>";
        out<<"<work>"<<(int)(pPlayer->GetEnergySpent()/1000.0f)<<"</work>";
        // there's more, but I don't feel like putting it in
      out<<"</summarydata>";

      for(unsigned int x = 0;x < lstData.size(); x++)
      {
        const RECORDEDDATA& rec = lstData[x];
        if(rec.dist.ToMeters() <= 0 && !fTrainerMode)
        {
          continue;
        }

        out<<"<sample>";
          out<<"<timeoffset>"<<rec.flTime<<"</timeoffset>";
          out<<"<hr>"<<rec.hr<<"</hr>";
          out<<"<spd>"<<rec.flSpeed<<"</spd>";
          out<<"<pwr>"<<rec.power<<"</pwr>";
          out<<"<torq>0</torq>";
          out<<"<cad>"<<rec.cadence<<"</cad>";
          out<<"<dist>"<<rec.dist.ToMeters()<<"</dist>";
          out<<"<alt>"<<rec.flElev<<"</alt>";
        out<<"</sample>";
      }
    out<<"</workout>";
  out<<"</pwx>";
}

void WriteCSVFile(LPCTSTR lpszPath, bool fOverwrite, bool fTrainerMode, const std::vector<RECORDEDDATA> lstData)
{
  TCHAR szAdjusted[MAX_PATH];
  wcscpy(szAdjusted,lpszPath);
  wcscat(szAdjusted,L".csv");
  int count = 1;
  while(!fOverwrite && DoesFileExist(szAdjusted))
  {
    _snwprintf(szAdjusted,NUMCHARS(szAdjusted),L"%s.%d.csv",lpszPath,count);
    count++;
  }

  ofstream out;
  OpenWofStream(out,szAdjusted);
  out<<"Minutes,Torq (N-m),Km/h,Watts,Km,Cadence,Hrate,ID,Altitude (m)\r\n";
  for(unsigned int x = 0;x < lstData.size(); x++)
  {
    const RECORDEDDATA& rec = lstData[x];

    if(rec.dist.ToMeters() <= 0 && !fTrainerMode)
    {
      continue;
    }
    out<<rec.flTime / 60.0f<<",";
    out<<0<<",";
    out<<rec.flSpeed*3.6<<",";
    out<<rec.power<<",";
    out<<rec.dist.ToMeters()/1000<<",";
    out<<rec.cadence<<",";
    out<<rec.hr<<",";
    out<<0<<",";
    out<<rec.flElev<<",";
    out<<endl;
  }
  out.close();
}
