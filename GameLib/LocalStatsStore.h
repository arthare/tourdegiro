#pragma once


class LocalStatsStore
{
public:
  virtual float GetMass(const std::string& strName, float flMassDefault) const = 0;
  virtual void SetMass(const std::string& strName, float flMassKg) = 0;

  virtual void GetNameList(std::vector<std::string>& lstNames) const = 0;
};

class SimpleLocalStatsStore : public LocalStatsStore
{
public:
  SimpleLocalStatsStore();
  virtual ~SimpleLocalStatsStore();

  virtual float GetMass(const std::string& strName, float flMassDefault) const ARTOVERRIDE;
  virtual void SetMass(const std::string& strName, float flMassKg) ARTOVERRIDE;
  virtual void GetNameList(std::vector<std::string>& lstNames) const ARTOVERRIDE;

private:
  void LoadDec14(istream& in);
  void LoadOriginal(istream& in);
private:
  std::map<std::string,float> m_mapMass;
  std::vector<std::string> m_lstOrigNames;
};