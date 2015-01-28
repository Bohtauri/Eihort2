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


#include <GL/glew.h>
#include <algorithm>
#include "lightmodel.h"

LightModel::LightModel()
: skyPower(1.0f)
, tex(0)
, texDirty(true)
{
	for( unsigned i = 0; i < 16; i++ ) {
		float f = i / 15.0f;
		interp[i] = f*f;
	}

	float initDark[] = { 7/255.f, 7/255.f, 7/255.f, 1.0f };
	float initSky[] = { 252.f/255.f, 252.f/255.f, 252.f/255.f, 1.0f };
	float initBlock[] = { 1.5f, 1.2f, 1.f, 1.0f };
	setDarkColor( initDark );
	setSkyBrightColor( initSky, skyPower );
	setBlockBrightColor( initBlock );

	glGenTextures( 1, &tex );
}

LightModel::~LightModel() {
	glDeleteTextures( 1, &tex );
}

void LightModel::setDarkColor( float *col ) {
	for( unsigned i = 0; i < 4; i++ ) {
		dark[i] = col[i];
		skyDelta[i] = skyBright[i] - dark[i];
		blockDelta[i] = blockBright[i] - dark[i];
	}
	texDirty = true;
}

void LightModel::setSkyBrightColor( float *col, float power ) {
	for( unsigned i = 0; i < 4; i++ ) {
		skyBright[i] = col[i];
		skyDelta[i] = skyBright[i] - dark[i];
	}
	skyPower = power;
	texDirty = true;
}

void LightModel::setBlockBrightColor( float *col ) {
	for( unsigned i = 0; i < 4; i++ ) {
		blockBright[i] = col[i];
		blockDelta[i] = blockBright[i] - dark[i];
	}
	texDirty = true;
}

void LightModel::uploadGL() {
	glActiveTexture( GL_TEXTURE3 );
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, tex );

	if( texDirty )
		regenTex();

	glActiveTexture( GL_TEXTURE0 );
}

void LightModel::unloadGL() {
	glActiveTexture( GL_TEXTURE3 );
	glBindTexture( GL_TEXTURE_2D, 0 );
	glDisable( GL_TEXTURE_2D );
	glActiveTexture( GL_TEXTURE0 );
}

void LightModel::initLua( lua_State *L ) {
	setupLuaObject( L, LIGHTMODEL_META );
}

void LightModel::regenTex() {
	unsigned char pix[4*16*16], *px = pix;
	for( unsigned y = 0; y < 16; y++ ) {
		for( unsigned x = 0; x < 16; x++, px += 4 ) {
			for( unsigned i = 0; i < 4; i++ ) {
				float v = dark[i] + skyDelta[i] * interp[y] + blockDelta[i] * interp[x] * std::max( 0.0f, 1.0f - skyPower * interp[y] );
				px[i] = (unsigned char)(255.0f * std::min( 1.0f, std::max( 0.0f, v ) ));
			}
		}
	}

	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix );

	texDirty = false;
}


int LightModel::lua_setDark( lua_State *L ) {
	LightModel *lm = getLuaObjectArg<LightModel>( L, 1, LIGHTMODEL_META );

	float col[4];
	for( unsigned i = 0; i < 3; i++ )
		col[i] = (float)luaL_checknumber( L, i+2 );
	col[3] = (float)luaL_optnumber( L, 5, 1.0 );

	lm->setDarkColor( col );
	return 0;
}

int LightModel::lua_setSky( lua_State *L ) {
	LightModel *lm = getLuaObjectArg<LightModel>( L, 1, LIGHTMODEL_META );

	float col[4];
	for( unsigned i = 0; i < 3; i++ )
		col[i] = (float)luaL_checknumber( L, i+2 );
	col[3] = (float)luaL_optnumber( L, 5, 1.0 );

	lm->setSkyBrightColor( col, (float)luaL_optnumber( L, 5, 1.0 ) );
	return 0;
}

int LightModel::lua_setBlock( lua_State *L ) {
	LightModel *lm = getLuaObjectArg<LightModel>( L, 1, LIGHTMODEL_META );

	float col[4];
	for( unsigned i = 0; i < 3; i++ )
		col[i] = (float)luaL_checknumber( L, i+2 );
	col[3] = (float)luaL_optnumber( L, 5, 1.0 );

	lm->setBlockBrightColor( col );
	return 0;
}

static const luaL_Reg LightModel_functions[] = {
	{ "setDark", &LightModel::lua_setDark },
	{ "setSky", &LightModel::lua_setSky },
	{ "setBlock", &LightModel::lua_setBlock },
	{ NULL, NULL }
};

void LightModel::setupLua( lua_State *L ) {
	luaL_newmetatable( L, LIGHTMODEL_META );
	lua_pushvalue( L, -1 );
	lua_setfield( L, -2, "__index" );
	luaL_register( L, NULL, &LightModel_functions[0] );
	lua_pop( L, 1 );
}
