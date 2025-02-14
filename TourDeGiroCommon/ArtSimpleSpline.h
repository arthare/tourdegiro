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

#pragma once

#define assert(x)

struct NonOgreVector
{
  NonOgreVector() : x(0),y(0),z(0) {};
  NonOgreVector(float x, float y, float z) : x(x), y(y), z(z) {
  }
  float x;
  float y;
  float z;
};
struct NonOgreVector4
{
  NonOgreVector4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
  float x;
  float y;
  float z;
  float w;
};
struct NonOgreMatrix44
{
  float rows[4][4];

  float* operator[] (int x) {
    return rows[x];
  };
  const float* operator[] (int x) const {
    return rows[x];
  };
};
NonOgreVector4 operator * (const NonOgreVector4& vec, const NonOgreMatrix44& mat);
bool operator == (const NonOgreVector& vec, const NonOgreVector& mat);
NonOgreVector operator - (const NonOgreVector& vec, const NonOgreVector& mat);
NonOgreVector operator * (const float x, const NonOgreVector& y);
NonOgreVector operator + (const NonOgreVector& vec1, const NonOgreVector& vec2);

	/** \addtogroup Core
	*  @{
	*/
	/** \addtogroup Math
	*  @{
	*/
	/** A very simple spline class which implements the Catmull-Rom class of splines.
    @remarks
        Splines are bendy lines. You define a series of points, and the spline forms
        a smoother line between the points to eliminate the sharp angles.
    @par
        Catmull-Rom splines are a specialisation of the general Hermite spline. With
        a Hermite spline, you define the start and end point of the line, and 2 tangents,
        one at the start of the line and one at the end. The Catmull-Rom spline simplifies
        this by just asking you to define a series of points, and the tangents are 
        created for you. 
    */
    struct STOREDDATA
    {
      STOREDDATA(const NonOgreVector& v, const void* pv) : v(v),pv(pv) {};
      NonOgreVector v;
      const void* pv;
    };
	class ArtSimpleSpline
    {
    public:
        ArtSimpleSpline();
        ~ArtSimpleSpline();

        /** Adds a control point to the end of the spline. */
        void addPoint(const STOREDDATA& p);

        /** Gets the detail of one of the control points of the spline. */
        const NonOgreVector& getPoint(unsigned short index) const;

        /** Gets the number of control points in the spline. */
        unsigned short getNumPoints(void) const;

        /** Clears all the points in the spline. */
        void clear(void);

        /** Updates a single point in the spline. 
        @remarks
            This point must already exist in the spline.
        */
        void updatePoint(unsigned short index, const STOREDDATA& value);

        /** Returns an interpolated point based on a parametric value over the whole series.
        @remarks
            Given a t value between 0 and 1 representing the parametric distance along the
            whole length of the spline, this method returns an interpolated point.
        @param t Parametric value.
        */
        NonOgreVector interpolate(float t, float* pflNextness, const void** ppvDataPrev, const void** ppvDataNext) const;

        /** Interpolates a single segment of the spline given a parametric value.
        @param fromIndex The point index to treat as t=0. fromIndex + 1 is deemed to be t=1
        @param t Parametric value
        */
        NonOgreVector interpolate(unsigned int fromIndex, float t) const;


        /** Tells the spline whether it should automatically calculate tangents on demand
            as points are added.
        @remarks
            The spline calculates tangents at each point automatically based on the input points.
            Normally it does this every time a point changes. However, if you have a lot of points
            to add in one go, you probably don't want to incur this overhead and would prefer to 
            defer the calculation until you are finished setting all the points. You can do this
            by calling this method with a parameter of 'false'. Just remember to manually call 
            the recalcTangents method when you are done.
        @param autoCalc If true, tangents are calculated for you whenever a point changes. If false, 
            you must call reclacTangents to recalculate them when it best suits.
        */
        void setAutoCalculate(bool autoCalc);

        /** Recalculates the tangents associated with this spline. 
        @remarks
            If you tell the spline not to update on demand by calling setAutoCalculate(false)
            then you must call this after completing your updates to the spline points.
        */
        void recalcTangents(void);

    protected:
        bool mAutoCalc;

        std::vector<STOREDDATA> mPoints;
        std::vector<NonOgreVector> mTangents;

        /// Matrix of coefficients 
        NonOgreMatrix44 mCoeffs;



    };
