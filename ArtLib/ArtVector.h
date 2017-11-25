
#pragma once

template<int n>
class Vector
{
public:
	Vector()
	{
		for(int x = 0;x < n;x++)
		{
			m_v[x] = 0;
		}
	}
	Vector<n> operator + (const Vector<n>& v) const
	{
		Vector<n> res;
		for(int x = 0; x < n; x++)
		{
			res.m_v[x] = m_v[x] + v.m_v[x];
		}
		return res;
	}
	Vector<n> operator - (const Vector<n>& v) const
	{
		Vector<n> res;
		for(int x = 0; x < n; x++)
		{
			res.m_v[x] = m_v[x] - v.m_v[x];
		}
		return res;
	}
	Vector<n> operator * (const double& d) const
	{
		Vector<n> res;
		for(int x = 0; x < n; x++)
		{
			res.m_v[x] = m_v[x] * d;
		}
		return res;
	}
	Vector<n> operator / (const double& d) const
	{
		Vector<n> res;
		for(int x = 0; x < n; x++)
		{
			res.m_v[x] = m_v[x] / d;
		}
		return res;
	}
	Vector<n> operator *= (const double& d)
	{
		for(int x = 0;x < n; x++)
		{
			m_v[x] *= d;
		}
		return *this;
	}
	Vector<n> operator += (const Vector<n>& v)
	{
		for(int x = 0;x < n; x++)
		{
			m_v[x] += v.m_v[x];
		}
		return *this;
	}
	Vector<n> operator /= (const double d)
	{
		for(int x = 0;x < n; x++)
		{
			m_v[x] /= d;
		}
		return *this;
	}
	double Length() const
	{
		double dSum = 0;
		for(int x = 0;x < n; x++)
		{
			dSum += m_v[x]*m_v[x];
		}
		return sqrt(dSum);
	}
	double LengthSq() const
	{
		double dSum = 0;
		for(int x = 0;x < n; x++)
		{
			dSum += m_v[x]*m_v[x];
		}
    return dSum;
	}
	Vector<n> Unit() const
	{
		return (*this) / Length();
	}
	double DP(const Vector<n>& v) const
	{
		double dSum = 0;
		for(int x = 0;x < n; x++)
		{
			dSum += m_v[x] * v.m_v[x];
		}
		return dSum;
	}
  Vector<3> Cross(const Vector<3>& v) const
  {
    Vector<3> ret;
    ret.m_v[0] = m_v[1]*v.m_v[2] - m_v[2]*v.m_v[1];
    ret.m_v[1] = m_v[2]*v.m_v[0] - m_v[0]*v.m_v[2];
    ret.m_v[2] = m_v[0]*v.m_v[1] - m_v[1]*v.m_v[0];
    return ret;
  }
	double AngleBetween(const Vector<n>& v) const
	{
		// |A||B|cos t = A dot B
		// cos t = A dot B / |A||B|
		// t = acos( A dot B / |A||B| )
		double rs = this->DP(v) / (Length() * v.Length());
		return acos(rs);
	}
	Vector<n> operator -() const
	{
		Vector<n> ret;
		for(int x = 0;x < n; x++)
		{
			ret.m_v[x] = -m_v[x];
		}
		return ret;
	}
	bool operator != (const Vector<n>& pt) const
	{
		for(int x = 0; x < n; x++)
		{
			if(m_v[x] != pt.m_v[x]) return true;
		}
		return false;
	}
  bool operator == (const Vector<n>& pt) const
  {
    for(int x = 0; x < n; x++)
		{
			if(m_v[x] != pt.m_v[x]) return false;
		}
		return true;
  }
	Vector<2> RotateAboutOrigin(const double dAngRadians) const
	{
		Vector<2> vResult;
		vResult.m_v[0] = m_v[0] * cos(dAngRadians) - m_v[1] * sin(dAngRadians);
		vResult.m_v[1] = m_v[1] * cos(dAngRadians) + m_v[0] * sin(dAngRadians);
		return vResult;
	}
	double m_v[n];
};

template<int n>
class Line
{
public:
  Line(const Vector<n>& pt, const Vector<n>& dir)
  {
    m_pt = pt;
    m_direction = dir;
  }

  // returns how far along this line lnOther intersects.
  bool IntersectLine(const Line<2>& lnOther, double* pdOut)
  {
    if(m_direction == lnOther.m_direction)
    {
      return false; // no single solution
    }
    // x1 = px1 + t1 * dx1
    // x2 = px2 + t2 * dx2
    // px1 + t1 * dx1 = px2 + t2 * dx2
    // t1 * dx1 = px2 + t2 * dx2 - px1
    // t1 = (px2 + t2 * dx2 - px1) / dx1 [1]

    // y1 = py1 + t1 * dy1
    // y2 = py2 + t2 * dy2
    // t1 = (py2 + t2 * dy2 - py1) / dy1 [2]
    // [1] = [2]
    // (px2 + t2 * dx2 - px1) / dx1 = (py2 + t2 * dy2 - py1) / dy1
    // dy1(px2 + t2 * dx2 - px1) = dx1(py2 + t2 * dy2 - py1)
    // dy1*px2 + dy1*t2*dx2 - dy1*px1 = dx1*py2 + dx1*t2*dy2 - dx1*py1
    // dy1 * t2 * dx2 - dx1*t2*dy2 = dy1*px1 - dy1*px2 + dx1*py2 - dx1*py1
    // t2(dy1*dx2 - dx1*dy2) = dy1(px1-px2) + dx1(py2 - py1)
    // t2 = [dy1(px1-px2) + dx1(py2 - py1)] / (dy1*dx2 - dx1*dy2)

    const double dx2 = m_direction.m_v[0];
    const double dy2 = m_direction.m_v[1];
    const double dx1 = lnOther.m_direction.m_v[0];
    const double dy1 = lnOther.m_direction.m_v[1];
    const double px2 = m_pt.m_v[0];
    const double py2 = m_pt.m_v[1];
    const double px1 = lnOther.m_pt.m_v[0];
    const double py1 = lnOther.m_pt.m_v[1];
    const double dDenon = (dy1*dx2 - dx1*dy2);
    if(dDenon != 0)
    {
      const double dT2 = (dy1*(px1-px2) + dx1*(py2 - py1)) / dDenon;
      *pdOut = dT2;
      return true;
    }
    return false;
  }
  Vector<n> m_direction; // the direction that this line goes
  Vector<n> m_pt; // the point through which this line goes
};

typedef Vector<2> Vector2D;
typedef Vector<3> Vector3D;

Vector2D V2D(double x, double y);
Vector3D V3D(double x, double y,double z);
Vector2D V2D(const Vector2D& pos, double dAngRadians, double dLen);