/* Copyright (c) 2012, Jason Lloyd-Price
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met: 

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer. 
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

// Math library for JGE
// Jason Lloyd-Price 2006
// Co-opted for use in Eihort

// NOT ALL OF THESE FUNCTIONS HAVE BEEN PROPERLY TESTED
// I DO NOT GUARANTEE THAT THEY ALL WORK!

#ifndef _JMATH_H
#define _JMATH_H

#ifdef NO_ODE
#define dReal float
#define _r(a) (a##f)
#else
#include <ode/odemath.h>
#include <ode/rotation.h>
#define _r(a) REAL(a)
#endif

#include <algorithm>
#include <cmath>

// Randomization functions
#define randr() (rand()/((dReal)RAND_MAX + 1))
#define crandr() (((dReal)(randr()-_r(0.5)))*_r(2.0))

// pi
#define jPI _r(3.1415926535897932384626433832)
// epsilon
#define jEPSILON (_r(2.0)*_r(5.96e-08))

/*
// A very fast but moderately inaccurate inverse square root
// From http://www.math.purdue.edu/~clomont/Math/Papers/2003/InvSqrt.pdf
float jFastInvSqrtf (float x) {
	float xhalf = 0.5f*x;
	int i = *(int*)&x; // get bits for floating value
	i = 0x5f375a86- (i>>1); // gives initial guess y0
	x = *(float*)&i; // convert bits back to float
	x = x*(1.5f-xhalf*x*x); // Newton step, repeating increases accuracy
	return x;
}
//*/

struct jVec3 { // no constructors so it can fit in unions
	union {
#ifdef __GNUC__
    __extension__ // FIXME: Anonymous structs are not supported by C++.
#endif
		struct {
			dReal x, y, z;
			//dReal w;
		};
		dReal v[3];
		//dReal v[4];
	};
};

struct jPlane {
	// A plane in Hessian normal form
	union {
		jVec3 n;
#ifdef __GNUC__
    __extension__
#endif
		struct {
			dReal a, b, c, d;
		};
		dReal v[4];
	};
};

struct jQuaternion {
	union {
#ifdef __GNUC__
    __extension__
#endif
		struct {
			dReal w, x, y, z;
		};
#ifdef __GNUC__
    __extension__
#endif
		struct {
			dReal r;
			jVec3 n;
		};
		dReal q[4];
	};
};

struct jMatrix {
	// 3 col matrix
	union {
#ifdef __GNUC__
    __extension__
#endif
		struct {
			dReal m11, m21, m31;
			dReal m12, m22, m32;
			dReal m13, m23, m33;
			dReal m14, m24, m34;
		};
		dReal cr[4][3];
#ifdef __GNUC__
    __extension__
#endif
		struct {
			jVec3 c1;
			jVec3 c2;
			jVec3 c3;
			jVec3 c4;
		};
#ifdef __GNUC__
    __extension__
#endif
		struct {
			jVec3 right;
			jVec3 fwd;
			jVec3 up;
			jVec3 pos;
		};
		dReal m[12];
	};
};

// ----------------------------------------------------------------------------
//  Vector operations

inline void jVec3Set (jVec3 *out, dReal x, dReal y, dReal z) {
	out->x = x;
	out->y = y;
	out->z = z;
}

template< typename T >
inline void jVec3Make (jVec3 *out, const T *values) {
	out->x = dReal(values[0]);
	out->y = dReal(values[1]);
	out->z = dReal(values[2]);
}

inline void jVec3Copy (jVec3 *out, const jVec3 *in) {
	out->x = in->x;
	out->y = in->y;
	out->z = in->z;
}

// Convert to an array of 3 floats - usefull for sending to glLight* functions
inline void jVec3ToFloat (float *out, const jVec3 *in) {
	out[0] = (float)in->x;
	out[1] = (float)in->y;
	out[2] = (float)in->z;
}

inline void jVec3Min (jVec3 *out, const jVec3 *in1, const jVec3 *in2) {
	out->x = std::min (in1->x, in2->x);
	out->y = std::min (in1->y, in2->y);
	out->z = std::min (in1->z, in2->z);
}

inline void jVec3Max (jVec3 *out, const jVec3 *in1, const jVec3 *in2) {
	out->x = std::max (in1->x, in2->x);
	out->y = std::max (in1->y, in2->y);
	out->z = std::max (in1->z, in2->z);
}

inline dReal jVec3Max( const jVec3 *in ) {
	dReal imax = std::max( in->x, in->y );
	return std::max( imax, in->z );
}

inline unsigned int jVec3MaxDim( const jVec3 *in ) {
	if( in->x > in->y ) {
		return in->x > in->z ? 0u : 2u;
	} else {
		return in->y > in->z ? 1u : 2u;
	}
}

inline void jVec3Abs( jVec3 *out, const jVec3 *in ) {
	out->x = fabs( in->x );
	out->y = fabs( in->y );
	out->z = fabs( in->z );
}

inline dReal jVec3MaxAbs( const jVec3 *in ) {
	jVec3 absVec;
	jVec3Abs( &absVec, in );
	return jVec3Max( &absVec );
}

inline unsigned int jVec3MaxAbsDim( const jVec3 *in ) {
	jVec3 absVec;
	jVec3Abs( &absVec, in );
	return jVec3MaxDim( &absVec );
}

inline void jVec3Zero (jVec3 *out) {
	jVec3Set (out, _r(0.0), _r(0.0), _r(0.0));
}

inline void jVec3One (jVec3 *out) {
	jVec3Set (out, _r(1.0), _r(1.0), _r(1.0));
}

inline dReal jVec3Dot (const jVec3 *in1, const jVec3 *in2) {
	return in1->x*in2->x + in1->y*in2->y + in1->z*in2->z;
}

inline dReal jVec3LengthSq (const jVec3 *v) {
	return jVec3Dot (v, v);
}

inline dReal jVec3Length (const jVec3 *v) {
	return (dReal)sqrt (jVec3LengthSq (v));
}

inline void jVec3Scale (jVec3 *out, const jVec3 *in, dReal s) {
	out->x = in->x * s;
	out->y = in->y * s;
	out->z = in->z * s;
}

inline void jVec3Multiply (jVec3 *out, const jVec3 *in1, const jVec3 *in2) {
	out->x = in1->x * in2->x;
	out->y = in1->y * in2->y;
	out->z = in1->z * in2->z;
}

inline void jVec3Divide (jVec3 *out, const jVec3 *in1, const jVec3 *in2) {
	out->x = in1->x / in2->x;
	out->y = in1->y / in2->y;
	out->z = in1->z / in2->z;
}

inline void jVec3Normalize (jVec3 *out) {
#ifdef NO_ODE
	// Lazy way
	jVec3Scale (out, out, _r(1.0) / jVec3Length (out));
#else
	dNormalize3 (out->v);
#endif
}

inline void jVec3Normalize (jVec3 *out, const jVec3 *in) {
	jVec3Copy (out, in);
	jVec3Normalize (out);
}

inline void jVec3Add (jVec3 *out, const jVec3 *in1, const jVec3 *in2) {
	out->x = in1->x + in2->x;
	out->y = in1->y + in2->y;
	out->z = in1->z + in2->z;
}

inline void jVec3Subtract (jVec3 *out, const jVec3 *in1, const jVec3 *in2) {
	out->x = in1->x - in2->x;
	out->y = in1->y - in2->y;
	out->z = in1->z - in2->z;
}

inline void jVec3MA (jVec3 *out, const jVec3 *in1, dReal s, const jVec3 *in2) {
	out->x = in1->x * s + in2->x;
	out->y = in1->y * s + in2->y;
	out->z = in1->z * s + in2->z;
}

inline void jVec3Lerp (jVec3 *out, const jVec3 *in1, const jVec3 *in2, dReal t) {
	jVec3Scale( out, in2, t );
	jVec3MA( out, in1, _r(1.0) - t, out );
}

inline void jVec3Cross (jVec3 *out, const jVec3 *in1, const jVec3 *in2) {
#ifdef NO_ODE
	out->x = in1->y * in2->z - in1->z * in2->y;
	out->y = in1->z * in2->x - in1->x * in2->z;
	out->z = in1->x * in2->y - in1->y * in2->x;
#else
	dCROSS (out->v, =, in1->v, in2->v);
#endif
}

inline void jVec3Jiggle (jVec3 *out, const jVec3 *in, dReal mag) {
	out->x = in->x + crandr() * mag;
	out->y = in->y + crandr() * mag;
	out->z = in->z + crandr() * mag;
}

inline void jVec3Orthogonalize( jVec3 *out, const jVec3 *in, const jVec3 *n ) {
	jVec3MA( out, n, -jVec3Dot( in, n ), in );
}

inline dReal jVec3DistSq( const jVec3 *in1, const jVec3 *in2 ) {
	jVec3 diff;
	jVec3Subtract( &diff, in1, in2 );
	return jVec3LengthSq( &diff );
}

inline dReal jVec3Dist( const jVec3 *in1, const jVec3 *in2 ) {
	jVec3 diff;
	jVec3Subtract( &diff, in1, in2 );
	return jVec3Length( &diff );
}

// ----------------------------------------------------------------------------
// Matrix operations

inline void jMatrixMultiply (jMatrix *out, const jMatrix *m1, const jMatrix *m2) {
	for (int row = 0; row < 3; row++) {
		// Do the 3x3 rotation matrix
		for (int col = 0; col < 3; col++) {
			out->cr[col][row] = m1->cr[0][row] * m2->cr[col][0] + m1->cr[1][row] * m2->cr[col][1] + m1->cr[2][row] * m2->cr[col][2];
		}
		// Do the translate
		out->cr[3][row] = m1->cr[0][row] * m2->cr[3][0] + m1->cr[1][row] * m2->cr[3][1] + m1->cr[2][row] * m2->cr[3][2] + m1->cr[3][row];
	}
}

inline void jMatrixSetIdentity (jMatrix *out) {
	out->m11 = out->m22 = out->m33 = 1.0f;

	out->m12 = out->m13 = out->m14 = 0.0f;
	out->m21 = out->m23 = out->m24 = 0.0f;
	out->m31 = out->m32 = out->m34 = 0.0f;
}

// Convert a jMatrix to an array of dReals ready for opengl
inline void jMatrixToGL (dReal *glMat, const jMatrix *mat) {
	glMat[0] = mat->m11; glMat[4] = mat->m12; glMat[8] = mat->m13; glMat[12] = mat->m14;
	glMat[1] = mat->m21; glMat[5] = mat->m22; glMat[9] = mat->m23; glMat[13] = mat->m24;
	glMat[2] = mat->m31; glMat[6] = mat->m32; glMat[10] = mat->m33; glMat[14] = mat->m34;
	glMat[3] = _r(0.0); glMat[7] = _r(0.0); glMat[11] = _r(0.0); glMat[15] = _r(1.0);
}

inline void jMatrixToTransposeGL (dReal *glMat, const jMatrix *mat) {
	glMat[0] = mat->m11; glMat[1] = mat->m12; glMat[2] = mat->m13; glMat[3] = mat->m14;
	glMat[4] = mat->m21; glMat[5] = mat->m22; glMat[6] = mat->m23; glMat[7] = mat->m24;
	glMat[8] = mat->m31; glMat[9] = mat->m32; glMat[10] = mat->m33; glMat[11] = mat->m34;
	glMat[12] = _r(0.0); glMat[13] = _r(0.0); glMat[14] = _r(0.0); glMat[15] = _r(1.0);
}

inline void jMatrixFromGL (jMatrix *mat, const dReal *glMat) {
	mat->m11 = glMat[0]; mat->m12 = glMat[4]; mat->m13 = glMat[8]; mat->m14 = glMat[12];
	mat->m21 = glMat[1]; mat->m22 = glMat[5]; mat->m23 = glMat[9]; mat->m24 = glMat[13];
	mat->m31 = glMat[2]; mat->m32 = glMat[6]; mat->m33 = glMat[10]; mat->m34 = glMat[14];
}

inline void jMatrixSetCol (jMatrix *out, const jVec3 *in, int col) {
	out->cr[col][0] = in->x;
	out->cr[col][1] = in->y;
	out->cr[col][2] = in->z;
}

inline void jMatrixGetCol (jVec3 *out, const jMatrix *in, int col) {
	out->x = in->cr[col][0];
	out->y = in->cr[col][1];
	out->z = in->cr[col][2];
}

inline void jMatrixSetRow (jMatrix *out, const jVec3 *in, int row) {
	out->cr[0][row] = in->x;
	out->cr[1][row] = in->y;
	out->cr[2][row] = in->z;
}

inline void jMatrixGetRow (jVec3 *out, const jMatrix *in, int row) {
	out->x = in->cr[0][row];
	out->y = in->cr[1][row];
	out->z = in->cr[2][row];
}

inline void jMatrixSwapCols (jMatrix *io, int r1, int r2) {
	std::swap (io->cr[0][r1], io->cr[0][r2]);
	std::swap (io->cr[1][r1], io->cr[1][r2]);
	std::swap (io->cr[2][r1], io->cr[2][r2]);
}

inline void jMatrixSwapRows (jMatrix *io, int r1, int r2) {
	std::swap (io->cr[r1][0], io->cr[r2][0]);
	std::swap (io->cr[r1][1], io->cr[r2][1]);
	std::swap (io->cr[r1][2], io->cr[r2][2]);
}

inline void jMatrixCopy (jMatrix *out, const jMatrix *in) {
	*out = *in;
}

// ----------------------------------------------
//  Translation

inline void jMatrixZeroTranslate (jMatrix *out) {
	out->m14 = out->m24 = out->m34 = 0.0f;
}

inline void jMatrixFillTranslate (jMatrix *out, const jVec3 *in) {
	jMatrixSetCol (out, in, 3);
}

inline void jMatrixFillTranslate (jMatrix *io, dReal x, dReal y, dReal z) {
	io->m14 = x;
	io->m24 = y;
	io->m34 = z;
}

inline void jMatrixInPlaceTranslate (jMatrix *io, const jVec3 *in) {
	io->m14 += in->x;
	io->m24 += in->y;
	io->m34 += in->z;
}

inline void jMatrixInPlaceTranslate (jMatrix *io, dReal x, dReal y, dReal z) {
	io->m14 += x;
	io->m24 += y;
	io->m34 += z;
}

inline void jMatrixInPlaceTranslateM (jMatrix *io, const jVec3 *in) {
	io->m14 += in->x * io->m11 + in->y * io->m12 + in->z * io->m13;
	io->m24 += in->x * io->m21 + in->y * io->m22 + in->z * io->m23;
	io->m34 += in->x * io->m31 + in->y * io->m32 + in->z * io->m33;
}

inline void jMatrixInPlaceTranslateM (jMatrix *io, dReal x, dReal y, dReal z) {
	io->m14 += x * io->m11 + y * io->m12 + z * io->m13;
	io->m24 += x * io->m21 + y * io->m22 + z * io->m23;
	io->m34 += x * io->m31 + y * io->m32 + z * io->m33;
}

inline void jMatrixExtractTranslate (jVec3 *out, const jMatrix *in) {
	out->x = in->m14;
	out->y = in->m24;
	out->z = in->m34;
}

// ----------------------------------------------
//  Scaling

inline void jMatrixSetScale (jMatrix *out, dReal x, dReal y, dReal z) {
	out->m11 = x;
	out->m22 = y;
	out->m33 = z;

	out->m12 = out->m13 = out->m14 = 0.0f;
	out->m21 = out->m23 = out->m24 = 0.0f;
	out->m31 = out->m32 = out->m34 = 0.0f;
}

inline void jMatrixSetScale (jMatrix *out, const jVec3 *scale) {
	jMatrixSetScale (out, scale->x, scale->y, scale->z);
}

// In place scaling - will scale the model in the model's coordinate frame
inline void jMatrixInPlaceScaleM (jMatrix *out, dReal x, dReal y, dReal z) {
	out->m11 *= x; out->m12 *= y; out->m13 *= z;
	out->m21 *= x; out->m22 *= y; out->m23 *= z;
	out->m31 *= x; out->m32 *= y; out->m33 *= z;
}

// In place scaling - will scale the model in the world's coordinate frame
inline void jMatrixInPlaceScale (jMatrix *out, dReal x, dReal y, dReal z) {
	out->m11 *= x; out->m12 *= x; out->m13 *= x; out->m14 *= x;
	out->m21 *= y; out->m22 *= y; out->m23 *= y; out->m24 *= y;
	out->m31 *= z; out->m32 *= z; out->m33 *= z; out->m34 *= z;
}

// Scales the ENTIRE matrix by s
inline void jMatrixScale( jMatrix *out, const jMatrix *in, dReal s ) {
	jVec3Scale( &out->c1, &in->c1, s );
	jVec3Scale( &out->c2, &in->c2, s );
	jVec3Scale( &out->c3, &in->c3, s );
	jVec3Scale( &out->c4, &in->c4, s );
}

// Multiply and add an entire matrix
inline void jMatrixMA( jMatrix *out, const jMatrix *in1, dReal s, const jMatrix *in2 ) {
	jVec3MA( &out->c1, &in1->c1, s, &in2->c1 );
	jVec3MA( &out->c2, &in1->c2, s, &in2->c2 );
	jVec3MA( &out->c3, &in1->c3, s, &in2->c3 );
	jVec3MA( &out->c4, &in1->c4, s, &in2->c4 );
}

inline dReal jMatrixScaleFactor( const jMatrix *in ) {
	jVec3 scales;
	scales.x = jVec3LengthSq( &in->c1 );
	scales.y = jVec3LengthSq( &in->c2 );
	scales.z = jVec3LengthSq( &in->c3 );
	dReal scaleSq = jVec3Max( &scales );
	if( fabs( scaleSq - _r(1.0) ) < 10*jEPSILON )
		return _r(1.0); // 90% of the time, it will be 1
	return sqrtf( scaleSq );
}

// ----------------------------------------------
//  Rotation

#ifndef NO_ODE

inline void jMatrixFromODE( jMatrix *out, const dMatrix3 in ) {
	jMatrixSetRow( out, (jVec3*)&in[0], 0 );
	jMatrixSetRow( out, (jVec3*)&in[4], 1 );
	jMatrixSetRow( out, (jVec3*)&in[8], 2 );
	jMatrixZeroTranslate (out);
}

inline void jMatrixToODERotation( dMatrix3 out, const jMatrix *in ) {
	jMatrixGetRow( (jVec3*)&out[0], in, 0 );
	jMatrixGetRow( (jVec3*)&out[4], in, 1 );
	jMatrixGetRow( (jVec3*)&out[8], in, 2 );
}

inline void jMatrixSetRotationAxis (jMatrix *out, dReal ax, dReal ay, dReal az, dReal angle) {
	dMatrix3 tmat;
	dRFromAxisAndAngle (tmat, ax, ay, az, angle);
	jMatrixFromODE( out, tmat );
}

inline void jMatrixSetRotationAxis (jMatrix *out, const jVec3 *axis, dReal angle) {
	jMatrixSetRotationAxis (out, axis->x, axis->y, axis->z, angle);
}

inline void jMatrixSetRotationEuler (jMatrix *out, const jVec3 *angles) {
	dMatrix3 tmat;
	dRFromEulerAngles( tmat, angles->x, angles->y, angles->z );
	jMatrixFromODE( out, tmat );
}

inline void jMatrixInPlaceRotateAxis (jMatrix *io, dReal ax, dReal ay, dReal az, dReal angle) {
	jMatrix temp, rot;
	jMatrixSetRotationAxis (&rot, ax, ay, az, angle);
	temp = *io;
	jMatrixMultiply (io, &temp, &rot);
}

inline void jMatrixInPlaceRotateAxis (jMatrix *io, const jVec3 *axis, dReal angle) {
	jMatrix temp, rot;
	jMatrixSetRotationAxis (&rot, axis, angle);
	temp = *io;
	jMatrixMultiply (io, &temp, &rot);
}

#else

inline void jMatrixSetRotationAxis (jMatrix *out, dReal ax, dReal ay, dReal az, dReal theta) {
	float c = cosf( theta ), s = sinf( theta );
	float c1 = 1.0f - c;
	float xyc1 = ax*ay*c1, xzc1 = ax*az*c1, yzc1 = ay*az*c1;
	float xs = ax*s, ys = ay*s, zs = az*s;

	out->right.x = c + ax*ax*c1;
	out->right.y = xyc1 + zs;
	out->right.z = xzc1 - ys;
	out->fwd.x   = xyc1 - zs;
	out->fwd.y   = c + ay*ay*c1;
	out->fwd.z   = yzc1 + xs;
	out->up.x    = xzc1 + ys;
	out->up.y    = yzc1 - xs;
	out->up.z    = c + az*az*c1;
	out->pos.x   = 0.0f;
	out->pos.y   = 0.0f;
	out->pos.z   = 0.0f;
}

#endif

// ----------------------------------------------
//  Change of basis

inline void jMatrixBasis (jMatrix *out, const jVec3 *origin, const jVec3 *x, const jVec3 *y, const jVec3 *z) {
	// Rotate then translate
	jMatrixSetCol (out, x, 0);
	jMatrixSetCol (out, y, 1);
	jMatrixSetCol (out, z, 2);
	jMatrixSetCol (out, origin, 3);
}

inline void jMatrixBasis (jMatrix *out, const jVec3 *x, const jVec3 *y, const jVec3 *z) {
	// Rotate
	jMatrixSetCol (out, x, 0);
	jMatrixSetCol (out, y, 1);
	jMatrixSetCol (out, z, 2);
	jMatrixZeroTranslate (out);
}

inline void jMatrixInvBasis (jMatrix *out, const jVec3 *origin, const jVec3 *x, const jVec3 *y, const jVec3 *z) {
	// Transpose of the rotation
	jMatrixSetRow (out, x, 0);
	jMatrixSetRow (out, y, 1);
	jMatrixSetRow (out, z, 2);
	// Negative translation by the rotated origin
	out->m14 = -jVec3Dot (origin, x);
	out->m24 = -jVec3Dot (origin, y);
	out->m34 = -jVec3Dot (origin, z);
}

inline void jMatrixInvBasis (jMatrix *out, const jVec3 *x, const jVec3 *y, const jVec3 *z) {
	// Transpose of the rotation
	jMatrixSetRow (out, x, 0);
	jMatrixSetRow (out, y, 1);
	jMatrixSetRow (out, z, 2);
	jMatrixZeroTranslate (out);
}

// ----------------------------------------------
//  Transpose

inline void jMatrixTranspose (jMatrix *out, const jMatrix *in) {
	for (int row = 0; row < 3; row++) {
		for (int col = 0; col < 3; col++) {
			out->cr[row][col] = in->cr[col][row];
		}
	}
	jMatrixZeroTranslate (out);
}

inline void jMatrixInPlaceTranspose (jMatrix *io) {
	for (int row = 0; row < 3; row++) {
		for (int col = 0; col < 3; col++) {
			std::swap (io->cr[row][col], io->cr[col][row]);
		}
	}
	jMatrixZeroTranslate (io);
}

// ----------------------------------------------
//  Inversion

// Quick inversion of a matrix with an orthonormal rotation matrix
inline void jMatrixSimpleInvert (jMatrix *out, const jMatrix *in) {
	jVec3 trans, trans2;
	jMatrixExtractTranslate( &trans, in );
	jVec3Set( &trans2, -jVec3Dot( &trans, &in->c1 ), -jVec3Dot( &trans, &in->c2 ), -jVec3Dot( &trans, &in->c3 ) );
	jMatrixTranspose( out, in );
	jMatrixFillTranslate( out, &trans2 );
}

inline dReal jMatrixDeterminant( const jMatrix *in ) {
	return (in->m11 * in->m22 - in->m21 * in->m12) * in->m33
		 - (in->m11 * in->m32 - in->m31 * in->m12) * in->m23
		 + (in->m21 * in->m32 - in->m31 * in->m22) * in->m13;
}

// Complete inversion of a jMatrix, returns the determinant
inline static dReal jMatrixInvert( jMatrix *out, const jMatrix *in ) {
	// Get the cofactors required to calculate the determinant
    dReal cfA0 = in->m11 * in->m22 - in->m21 * in->m12;
    dReal cfA1 = in->m11 * in->m32 - in->m31 * in->m12;
    dReal cfA3 = in->m21 * in->m32 - in->m31 * in->m22;

	// Get the determinant
    dReal det = cfA0 * in->m33  - cfA1 * in->m23  + cfA3 * in->m13;
    if( fabs( det ) <= jEPSILON )
        return _r(0.0); // Singular matrix

	// Get the other cofactors
    dReal cfB0 = in->m13 * in->m24 - in->m23 * in->m14;
    dReal cfB1 = in->m13 * in->m34 - in->m33 * in->m14;
    dReal cfB3 = in->m23 * in->m34 - in->m33 * in->m24;

	// Create the ajugate matrix
    out->m11 =  in->m22 * in->m33 - in->m32 * in->m23;
    out->m21 = -in->m21 * in->m33 + in->m31 * in->m23;
    out->m31 =  cfA3;
    out->m12 = -in->m12 * in->m33 + in->m32 * in->m13;
    out->m22 =  in->m11 * in->m33 - in->m31 * in->m13;
    out->m32 = -cfA1;
    out->m13 =  in->m12 * in->m23 - in->m22 * in->m13;
    out->m23 = -in->m11 * in->m23 + in->m21 * in->m13;
    out->m33 =  cfA0;
    out->m14 = -in->m12 * cfB3 + in->m22 * cfB1 - in->m32 * cfB0;
    out->m24 =  in->m11 * cfB3 - in->m21 * cfB1 + in->m31 * cfB0;
    out->m34 = -in->m14 * cfA3 + in->m24 * cfA1 - in->m34 * cfA0;

	// Scale by 1/det(in)
	jMatrixScale( out, out, _r(1.0)/det );

	return det;
}

// ----------------------------------------------
//  Transformation

// Transforms a vector by a matrix
inline void jMatrixTransform (jVec3 *out, const jMatrix *m, const jVec3 *in) {
	out->x = m->m11*in->x + m->m12*in->y + m->m13*in->z + m->m14;
	out->y = m->m21*in->x + m->m22*in->y + m->m23*in->z + m->m24;
	out->z = m->m31*in->x + m->m32*in->y + m->m33*in->z + m->m34;
}

// Transform a vector, ignoring translation
inline void jMatrixTransformNormal (jVec3 *out, const jMatrix *m, const jVec3 *in) {
	out->x = m->m11*in->x + m->m12*in->y + m->m13*in->z;
	out->y = m->m21*in->x + m->m22*in->y + m->m23*in->z;
	out->z = m->m31*in->x + m->m32*in->y + m->m33*in->z;
}

// ----------------------------------------------------------------------------
// Quaternion operations

inline void jQuaternionCopy( jQuaternion *out, const jQuaternion *in ) {
	*out = *in;
}

inline void jQuaternionIdentity (jQuaternion *out) {
	out->r = 1.0f;
	out->x = out->y = out->z = 0.0f;
}

inline void jQuaternionMultiply (jQuaternion *out, const jQuaternion *l, const jQuaternion *r) {
	out->w = l->w*r->w - l->x*r->x - l->y*r->y - l->z*l->z;
	out->x = l->w*r->x - l->x*r->w - l->y*r->z - l->z*r->y;
	out->y = l->w*r->y - l->x*r->z - l->y*r->w - l->z*l->x;
	out->z = l->w*r->z - l->x*r->y - l->y*r->x - l->z*l->w;
}

inline dReal jQuaternionDot (const jQuaternion *q1, const jQuaternion *q2) {
	return q1->w*q2->w + q1->x*q2->x + q1->y*q2->y + q1->z*q2->z;
}

inline dReal jQuaternionMagnitudeSq (const jQuaternion *q) {
	return jQuaternionDot( q, q );
}

inline dReal jQuaternionMagnitude (const jQuaternion *q) {
	return sqrt (jQuaternionMagnitudeSq (q));
}

inline dReal jQuaternionNormalize (jQuaternion *q) {
	dReal l = jQuaternionMagnitude (q);
	q->w /= l; q->x /= l; q->y /= l; q->z /= l;
	return l;
}

inline void jQuaternionAdd( jQuaternion *out, const jQuaternion *in1, const jQuaternion *in2 ) {
	out->w = in1->w + in2->w;
	out->x = in1->x + in2->x;
	out->y = in1->y + in2->y;
	out->z = in1->z + in2->z;
}

inline void jQuaternionScale( jQuaternion *out, const jQuaternion *in, dReal s ) {
	out->w = in->w * s;
	out->x = in->x * s;
	out->y = in->y * s;
	out->z = in->z * s;
}

inline void jQuaternionMA( jQuaternion *out, const jQuaternion *in1, dReal s, const jQuaternion *in2 ) {
	out->w = in1->w * s + in2->w;
	out->x = in1->x * s + in2->x;
	out->y = in1->y * s + in2->y;
	out->z = in1->z * s + in2->z;
}

inline void jQuaternionNLerp( jQuaternion *out, const jQuaternion *in1, const jQuaternion *in2, dReal t ) {
	jQuaternionScale( out, in2, t );
	jQuaternionMA( out, in1, (_r(1.0) - t), out );
	jQuaternionNormalize( out );
}

#ifndef NO_ODE

inline void jQuaternionToMatrix (jMatrix *out, const jQuaternion *in) {
	dMatrix3 tmat;
	dRfromQ( tmat, in->q );
	jMatrixFromODE( out, tmat );
}

inline void jMatrixToQuaternion (jQuaternion *out, const jMatrix *in) {
	dMatrix3 tmat;
	jMatrixToODERotation( tmat, in );
	dQfromR (out->q, tmat);
}

inline void jQuaternionRotate (jVec3 *out, const jQuaternion *q, const jVec3 *in) {
	jMatrix R;
 	jQuaternionToMatrix (&R, q);
	jMatrixTransform (out, &R, in);
}

#endif // NO_ODE

// Align a quaternion in the same hemihypersphere as another quaternion
inline void jQuaternionAlign( jQuaternion *io, const jQuaternion *with ) {
	if( jQuaternionDot( io, with ) < _r(0.0) ) {
		jQuaternionScale( io, io, _r(-1.0) );
	}
}

// ----------------------------------------------------------------------------
// Plane operations

inline dReal jPlaneDot3 (const jPlane *p, const jVec3 *v) {
	return p->a * v->x + p->b * v->y + p->c * v->z + p->d;
}

inline void jPlaneToGL (double *glPlane, const jPlane *p) {
	glPlane[0] = (double)p->a;
	glPlane[1] = (double)p->b;
	glPlane[2] = (double)p->c;
	glPlane[3] = (double)p->d;
}

inline void jPlaneToFloats (float *glPlane, const jPlane *p) {
	glPlane[0] = p->a;
	glPlane[1] = p->b;
	glPlane[2] = p->c;
	glPlane[3] = p->d;
}

// Plane-plane intersection
// Assumes the two plane's normals are normalized
inline bool jPlaneIntersect (jVec3 *onLine, jVec3 *dir, const jPlane *p1, const jPlane *p2) {
	// Find the direction of the line
	jVec3Cross (dir, &p1->n, &p2->n);

	if (jVec3LengthSq (dir) < _r(0.00001)) {
		// parallel
		return false;
	}

	// Find a point on the line
	dReal n12 = jVec3Dot (&p1->n, &p2->n);
	dReal det = _r(1.0) - n12*n12;
	dReal c1 = (-p1->d + p2->d * n12) / det;
	dReal c2 = (-p2->d + p1->d * n12) / det;
	jVec3Scale (onLine, &p1->n, c1);
	jVec3MA (onLine, &p2->n, c2, onLine);

	return true;
}

// Ray-plane intersection (technically Line-Plane)
inline bool jPlaneIntersectRay (dReal &t, const jPlane *plane, const jVec3 *pt, const jVec3 *dir) {
	dReal d = jVec3Dot (&plane->n, dir);
	if (fabs(d) < _r(0.00001)) {
		// Ray runs parallel with the plane
		return false;
	}

	t = -jPlaneDot3 (plane, pt) / d;
	return true;
}

// Build a plane from 3 points
inline void jPlaneFrom3Pts (jPlane *plane, const jVec3 *v1, const jVec3 *v2, const jVec3 *v3) {
	jVec3 v12, v13;
	jVec3Subtract (&v12, v2, v1);
	jVec3Subtract (&v13, v3, v1);
	jVec3Cross (&plane->n, &v12, &v13);
	jVec3Normalize (&plane->n);
	plane->d = jVec3Dot (&plane->n, v1);
}

// Build a plane from a normal and point
inline void jPlaneFromPtN (jPlane *plane, const jVec3 *pt, const jVec3 *n) {
	plane->n = *n;
	plane->d = -jVec3Dot (n, pt);
}

// Build a plane from a matrix, facing in the direction of the given axis)
inline void jPlaneFromMatrix (jPlane *plane, const jMatrix *in, int axis) {
	jVec3 pt, n;
	jMatrixExtractTranslate( &pt, in );
	jMatrixGetCol( &n, in, axis );
	jPlaneFromPtN( plane, &pt, &n );
}

// Transform a plane by a matrix
inline void jPlaneTransform (jPlane *out, const jMatrix *mat, const jPlane *plane) {
	jVec3 pt, pt2;
	jVec3Scale (&pt, &plane->n, plane->d);
	jMatrixTransformNormal (&out->n, mat, &plane->n);
	jMatrixTransform (&pt2, mat, &pt);
	out->d = -jVec3Dot (&pt2, &out->n);
}

// Simple copy
inline void jPlaneCopy( jPlane *out, const jPlane *in ) {
	*out = *in;
}

// Flip the plane to face the other direction
inline void jPlaneFlip( jPlane *out, const jPlane *in ) {
	out->a = -in->a;
	out->b = -in->b;
	out->c = -in->c;
	out->d = -in->d;
}

#endif
