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

#ifndef SKY_H
#define SKY_H

#include "jmath.h"
#include "glshader.h"
#include "luaobject.h"

class EihortShader;
class WorldQTree;

class Sky : public LuaObject {
public:
	Sky();
	~Sky();

	// Time goes from -1 to 1, where 0 is noon and -1 and 1 are midnight
	float setTime( float time );
	void setColors( unsigned horiz, unsigned top );
	void setSunMoonTex( unsigned sun, unsigned moon );
	unsigned getOptimalFogColor() { return fogColor; }
	void renderSky( const jPlane *frustum, const jVec3 *nOffset );
	void renderSunMoon( WorldQTree *qtree );

	// Lua functions
	static int lua_create( lua_State *L );
	static int lua_setTime( lua_State *L );
	static int lua_setColors( lua_State *L );
	static int lua_setSunMoon( lua_State *L );
	static int lua_getOptimalFogColor( lua_State *L );
	static int lua_render( lua_State *L );
	static int lua_destroy( lua_State *L );
	static void setupLua( lua_State *L );

private:
	GLShaderObject skyVtxShader;
	GLShaderObject skyFragShader;
	GLShader skyShader;

	float sunAngle;
	float sunSize;
	unsigned topColor;
	unsigned horizColor;
	unsigned fogColor;
	unsigned offsetUniform;
	unsigned skyTexId;
	unsigned sunTex, moonTex;
};

#endif // SKY_H
