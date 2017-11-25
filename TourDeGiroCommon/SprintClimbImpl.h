#pragma once

class ICrossingTimer
{
public:
  virtual ~ICrossingTimer() {};
  // indicates that a player has been detected crossing the leadin position of a segment
  // the ICrossingTimer should note that.  When the same player is 
  virtual void StartedSegment(float time, int id) = 0;

  // indicates that a player is done a segment
  virtual bool DoneSegment(float time, int id, float* pflElapsedTime) = 0;
};

class SprintClimbImpl : public SprintClimbPoint
{
public:
  SprintClimbImpl(const SPRINTCLIMBDATA_RAW& pt, float flActualDistMeters, float flPositionDistance, float flRise, bool fFinishMode);
  virtual ~SprintClimbImpl() {};

  virtual bool GetPointsEarned(const ORIGDIST& flLastDist, const ORIGDIST& flThisDist, int ixCurLap, bool fLastLap, SPRINTCLIMBDATA* pScoreData, int idPlayer, float flCurRaceTime) ARTOVERRIDE;

  // returns the description of this scoring source on the nth lap
  virtual std::string GetDescription(int ixLap) ARTOVERRIDE;
  
  // tells the scoring source how many people to use for scoring calculations.  This will be called at the start of the race, once the server knows how many starters there are
  virtual void SetPeopleCount(int cPeople) ARTOVERRIDE;
  
  virtual void SetMaxPoints(float flPoints) ARTOVERRIDE;
  
  // gets the key description of this thing (position, points, type, and name) for network transmission
  virtual void GetRaw(SPRINTCLIMBDATA_RAW* pRaw) const ARTOVERRIDE;

  virtual bool IsFinishMode() const ARTOVERRIDE {return m_fFinishMode;}

  virtual float GetNextPoints(int iLap) const ARTOVERRIDE;
private:
  std::vector<int> m_lstFinishersPerLap; // the i'th element indicates how many people have finished the i'th lap
  std::string m_strName; // what's our name?
  ORIGDIST m_flPosition; // where in each lap do we exist as a percentage of the lap distance?
  SCORETYPE m_eType; // what is our type?
  float m_flFirstPoints; // how many points is first worth?
  int m_cTotalPeople; // how many people should we do scoring for?
  ORIGDIST m_flLeadInDist; // what's our lead-in in percentage?

  boost::shared_ptr<ICrossingTimer> m_spTimer;
  int m_meters;
  bool m_fFinishMode;
  const float m_flPositionDistance;
  const float m_flRise;
};