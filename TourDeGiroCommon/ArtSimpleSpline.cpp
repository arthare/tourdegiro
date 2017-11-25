/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2012 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/

#include "stdafx.h"
#include "ArtSimpleSpline.h"

//---------------------------------------------------------------------
ArtSimpleSpline::ArtSimpleSpline()
{
    // Set up matrix
    // Hermite polynomial
    mCoeffs[0][0] = 2;
    mCoeffs[0][1] = -2;
    mCoeffs[0][2] = 1;
    mCoeffs[0][3] = 1;
    mCoeffs[1][0] = -3;
    mCoeffs[1][1] = 3;
    mCoeffs[1][2] = -2;
    mCoeffs[1][3] = -1;
    mCoeffs[2][0] = 0;
    mCoeffs[2][1] = 0;
    mCoeffs[2][2] = 1;
    mCoeffs[2][3] = 0;
    mCoeffs[3][0] = 1;
    mCoeffs[3][1] = 0;
    mCoeffs[3][2] = 0;
    mCoeffs[3][3] = 0;

    mAutoCalc = true;
}
//---------------------------------------------------------------------
ArtSimpleSpline::~ArtSimpleSpline()
{
}
//---------------------------------------------------------------------
void ArtSimpleSpline::addPoint(const STOREDDATA& p)
{
    mPoints.push_back(p);
    if (mAutoCalc)
    {
        recalcTangents();
    }
}
//---------------------------------------------------------------------
NonOgreVector ArtSimpleSpline::interpolate(float t, float* pflNextness, const void** ppvDataPrev, const void** ppvDataNext) const
{
    // Currently assumes points are evenly spaced, will cause velocity
    // change where this is not the case
    // TODO: base on arclength?

        
    // Work out which segment this is in
    float fSeg = t * (mPoints.size() - 1);
    unsigned int segIdx = (unsigned int)fSeg;
    // Apportion t 
    t = (fSeg - segIdx);
    if(pflNextness) (*pflNextness) = t;

    if(ppvDataPrev && segIdx >= 0) *ppvDataPrev = mPoints[segIdx].pv;
    if(ppvDataNext && segIdx < mPoints.size() - 1) *ppvDataNext = mPoints[segIdx+1].pv;

    return interpolate(segIdx, t);

}
//---------------------------------------------------------------------
NonOgreVector ArtSimpleSpline::interpolate(unsigned int fromIndex, float t) const
{
    // Bounds check
    assert (fromIndex < mPoints.size() &&
        "fromIndex out of bounds");

    if ((fromIndex + 1) == mPoints.size())
    {
        // Duff request, cannot blend to nothing
        // Just return source
        return mPoints[fromIndex].v;

    }

    // Fast special cases
    if (t == 0.0f)
    {
        return mPoints[fromIndex].v;
    }
    else if(t == 1.0f)
    {
        return mPoints[fromIndex + 1].v;
    }

    // Real interpolation
    // Form a vector of powers of t
    float t2, t3;
    t2 = t * t;
    t3 = t2 * t;
    NonOgreVector4 powers(t3, t2, t, 1);


    // Algorithm is ret = powers * mCoeffs * Matrix4(point1, point2, tangent1, tangent2)
    const NonOgreVector& point1 = mPoints[fromIndex].v;
    const NonOgreVector& point2 = mPoints[fromIndex+1].v;
    const NonOgreVector& tan1 = mTangents[fromIndex];
    const NonOgreVector& tan2 = mTangents[fromIndex+1];
    NonOgreMatrix44 pt;

    pt[0][0] = point1.x;
    pt[0][1] = point1.y;
    pt[0][2] = point1.z;
    pt[0][3] = 1.0f;
    pt[1][0] = point2.x;
    pt[1][1] = point2.y;
    pt[1][2] = point2.z;
    pt[1][3] = 1.0f;
    pt[2][0] = tan1.x;
    pt[2][1] = tan1.y;
    pt[2][2] = tan1.z;
    pt[2][3] = 1.0f;
    pt[3][0] = tan2.x;
    pt[3][1] = tan2.y;
    pt[3][2] = tan2.z;
    pt[3][3] = 1.0f;

    //NonOgreVector4 ret = powers * mCoeffs * pt; // nov 11 2017: let's scrap the spline stuff.  I'm sure I screwed something up with my ersatz matrices.
    NonOgreVector ret(t * point2 + (1-t)*point1);


    return NonOgreVector(ret.x, ret.y, ret.z);




}
//---------------------------------------------------------------------
void ArtSimpleSpline::recalcTangents(void)
{
    // Catmull-Rom approach
    // 
    // tangent[i] = 0.5 * (point[i+1] - point[i-1])
    //
    // Assume endpoint tangents are parallel with line with neighbour

    size_t i, numPoints;
    bool isClosed;

    numPoints = mPoints.size();
    if (numPoints < 2)
    {
        // Can't do anything yet
        return;
    }

    // Closed or open?
    if (mPoints[0].v == mPoints[numPoints-1].v)
    {
        isClosed = true;
    }
    else
    {
        isClosed = false;
    }

    mTangents.resize(numPoints);



    for(i = 0; i < numPoints; ++i)
    {
        if (i ==0)
        {
            // Special case start
            if (isClosed)
            {
                // Use numPoints-2 since numPoints-1 is the last point and == [0]
                mTangents[i] = 0.5 * (mPoints[1].v - mPoints[numPoints-2].v);
            }
            else
            {
                mTangents[i] = 0.5 * (mPoints[1].v - mPoints[0].v);
            }
        }
        else if (i == numPoints-1)
        {
            // Special case end
            if (isClosed)
            {
                // Use same tangent as already calculated for [0]
                mTangents[i] = mTangents[0];
            }
            else
            {
                mTangents[i] = 0.5 * (mPoints[i].v - mPoints[i-1].v);
            }
        }
        else
        {
            mTangents[i] = 0.5 * (mPoints[i+1].v - mPoints[i-1].v);
        }
            
    }



}
//---------------------------------------------------------------------
const NonOgreVector& ArtSimpleSpline::getPoint(unsigned short index) const
{
    assert (index < mPoints.size() && "Point index is out of bounds!!");

    return mPoints[index].v;
}
//---------------------------------------------------------------------
unsigned short ArtSimpleSpline::getNumPoints(void) const
{
    return (unsigned short)mPoints.size();
}
//---------------------------------------------------------------------
void ArtSimpleSpline::clear(void)
{
    mPoints.clear();
    mTangents.clear();
}
//---------------------------------------------------------------------
void ArtSimpleSpline::updatePoint(unsigned short index, const STOREDDATA& value)
{
    assert (index < mPoints.size() && "Point index is out of bounds!!");

    mPoints[index] = value;
    if (mAutoCalc)
    {
        recalcTangents();
    }
}
//---------------------------------------------------------------------
void ArtSimpleSpline::setAutoCalculate(bool autoCalc)
{
    mAutoCalc = autoCalc;
}


NonOgreVector4 operator * (const NonOgreVector4& vec, const NonOgreMatrix44& mat)
{
  NonOgreVector4 ret(vec.x * mat[0][0] + vec.y * mat[0][1] + vec.z * mat[0][2] + vec.w * mat[0][3],
                     vec.x * mat[1][0] + vec.y * mat[1][1] + vec.z * mat[1][2] + vec.w * mat[1][3],
                     vec.x * mat[2][0] + vec.y * mat[2][1] + vec.z * mat[2][2] + vec.w * mat[2][3],
                     vec.x * mat[3][0] + vec.y * mat[3][1] + vec.z * mat[3][2] + vec.w * mat[3][3]);
  return ret;
}
bool operator == (const NonOgreVector& vec1, const NonOgreVector& vec2)
{
  return vec1.x == vec2.x &&
         vec1.y == vec2.y &&
         vec1.z == vec2.z;
}
NonOgreVector operator - (const NonOgreVector& vec1, const NonOgreVector& vec2)
{
  NonOgreVector ret;
  ret.x = vec1.x - vec2.x;
  ret.y = vec1.y - vec2.y;
  ret.z = vec1.z - vec2.z;
  return ret;
}
NonOgreVector operator + (const NonOgreVector& vec1, const NonOgreVector& vec2)
{
  NonOgreVector ret;
  ret.x = vec1.x + vec2.x;
  ret.y = vec1.y + vec2.y;
  ret.z = vec1.z + vec2.z;
  return ret;
}
NonOgreVector operator * (const float scalar, const NonOgreVector& vec)
{
  NonOgreVector ret;
  ret.x = scalar * vec.x;
  ret.y = scalar * vec.y;
  ret.z = scalar * vec.z;
  return ret;
}