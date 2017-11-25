#pragma once

#ifndef TOOLS_H_
#define TOOLS_H_

#pragma warning(disable:4996) // stupid snprintf/_snprintf_s warning
#pragma warning(disable:4244) // double to float warning
#pragma warning(disable:4305) // double to float warning

float rand(float flMin, float flMax);

void NextSpeedPref();
SPEEDPREF GetSpeedPref();
SPEEDPREF GetNextSpeedPref(SPEEDPREF eCurrent);
void FormatDistanceShort(char* psz, int cch, float flMeters, int decimals, SPEEDPREF eUnits); // same as formatdistance, but prefers feet and meters
void FormatSpeed(char* psz, int cch, float flMetersPerSecond, SPEEDPREF eUnits, char* pszUnits = NULL, int cchUnits = 0);
void FormatMass(char* psz, int cch, float flKg, SPEEDPREF eUnits, char* pszUnits, int cchUnits);

void EXPECT_T(bool f);

// implemented in ogrehelp.cpp
struct GRAPHICS_QOS
{
  GRAPHICS_QOS();

  bool Load();
  void Save();

  enum VERSIONS
  {
    FIRST=1, // first version: only stored direct3d and RTT prefs
    AUG27, // increased fidelity
    SEPT23, // added data file save location
    APRIL8, // added "extreme graphics"
    AUGUST21, // added spectators/shadows/water
    OCT4_2014, // added iTrainingPlanId
    OCT4_2015, // added hide/show human players labels
    NOV29_2015, // added suppressing-the-kickr
    COUNT
  };

  bool fDirect3D;
  bool fShow3DScenery; // whether or not we're showing trees/buildings
  bool fShowClouds;
  bool fShowFields;
  bool fShowHUD;
  bool fShowHDTerrain;
  bool fShowCities;
  bool fHideAILabels;
  bool fSuppressKickrReset;
  bool fPowerlessLabels;

  bool fRenderToTarget;
  SPEEDPREF eSpeedPref;
  float flMaxFieldDist;
  std::wstring strSaveDest; // where are we saving local data to?
  bool fExtremeGraphics; // april8
  bool fShowSpectators; // aug21/2014
  bool fShowShadows; // aug21/2014
  bool fShowWater; // aug21/2014

  int iTrainingPlanId; // oct 4/2014

  bool fNoDownhillMode;

  void Default();
  void Save(std::ostream& out) const;
private:
  void LoadFirstVersion(std::ifstream& in, int first);
  void LoadAug27(std::ifstream& in);
  void LoadSept23(std::ifstream& in);
  void LoadApril8(std::ifstream& in);
  void LoadAugust21_2014(std::ifstream& in);
  void LoadOct4_2014(std::ifstream& in);
  void LoadOct4_2015(std::ifstream& in);
  void LoadNov29_2015(std::ifstream& in);
};
extern GRAPHICS_QOS g_qos;
#endif // TOOLS_H_