#include "stdafx.h"
#include "ArtVector.h"


Vector2D V2D(double x, double y)
{
  Vector2D v;
  v.m_v[0] = x;
  v.m_v[1] = y;
  return v;
}
Vector2D V2D(const Vector2D& pos, double dAngRadians, double dLen)
{
  Vector2D v;
  v.m_v[0] = pos.m_v[0] + cos(dAngRadians)*dLen;
  v.m_v[1] = pos.m_v[1] + sin(dAngRadians)*dLen;
  return v;
}
Vector3D V3D(double x, double y,double z)
{
  Vector3D v;
  v.m_v[0] = x;
  v.m_v[1] = y;
  v.m_v[2] = z;
  return v;
}