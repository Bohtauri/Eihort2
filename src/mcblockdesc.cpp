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


#include "mcblockdesc.h"
#include "blockmaterial.h"
#include "luaimage.h"

MCBlockDesc::MCBlockDesc() {
	lockCount = 0;
	blockLighting = true;
	defAirSkyLight = 0xfu;
	overrideAirSkyLight = false;
	for( unsigned i = 0; i < BLOCK_ID_COUNT; i++ ) {
		blockFlags[i] = 0;
		geometry[i] = NULL;
	}

	geometry[SIGNTEXT_BLOCK_ID] = new mcgeom::SignTextGeometry();
}

MCBlockDesc::~MCBlockDesc() {
	for( unsigned i = 0; i < BLOCK_ID_COUNT; i++ ) {
		delete geometry[i];
		geometry[i] = NULL;
	}
}

void MCBlockDesc::unlock() {
	assert( isLocked() );
	lockCount--;
}

int MCBlockDesc::lua_create( lua_State *L ) {
	MCBlockDesc *blocks = new MCBlockDesc;
	blocks->setupLuaObject( L, MCBLOCKDESC_META );
	blocks->lua_push();
	return 1;
}

int MCBlockDesc::lua_setGeometry( lua_State *L ) {
	MCBlockDesc *blocks = getLuaObjectArg<MCBlockDesc>( L, 1, MCBLOCKDESC_META );
	unsigned id = (unsigned)luaL_checknumber( L, 2 );
	luaL_argcheck( L, id < BLOCK_ID_COUNT, 2, "Block id is too large" );
	blocks->geometry[id] = getLuaObjectArg<mcgeom::BlockGeometry>( L, 3, BLOCKGEOMETRY_META );
	return 0;
}

int MCBlockDesc::lua_setSolidity( lua_State *L ) {
	MCBlockDesc *blocks = getLuaObjectArg<MCBlockDesc>( L, 1, MCBLOCKDESC_META );
	unsigned id = (unsigned)luaL_checknumber( L, 2 );
	luaL_argcheck( L, id < BLOCK_ID_COUNT, 2, "Block id is too large" );
	if( lua_isboolean( L, 3 ) ) {
		blocks->blockFlags[id] |= 0x3fu;
	} else {
		blocks->blockFlags[id] &= ~0x3fu;
		blocks->blockFlags[id] |= (unsigned)luaL_checknumber( L, 3 ) & 0x3fu;
	}
	return 0;
}

int MCBlockDesc::lua_setHighlight( lua_State *L ) {
	MCBlockDesc *blocks = getLuaObjectArg<MCBlockDesc>( L, 1, MCBLOCKDESC_META );
	unsigned id = (unsigned)luaL_checknumber( L, 2 );
	luaL_argcheck( L, id < BLOCK_ID_COUNT, 2, "Block id is too large" );
	if( lua_toboolean( L, 3 ) ) {
		blocks->blockFlags[id] |= 0x80u;
	} else {
		blocks->blockFlags[id] &= ~0x80u;
	}
	return 0;
}

int MCBlockDesc::lua_noBLockLighting( lua_State *L ) {
	MCBlockDesc *blocks = getLuaObjectArg<MCBlockDesc>( L, 1, MCBLOCKDESC_META );
	blocks->blockLighting = !lua_toboolean( L, 2 );
	return 0;
}

int MCBlockDesc::lua_setDefAirSkylight( lua_State *L ) {
	MCBlockDesc *blocks = getLuaObjectArg<MCBlockDesc>( L, 1, MCBLOCKDESC_META );
	blocks->setDefAirSkyLight( (unsigned)luaL_checknumber( L, 2 ), !!lua_toboolean( L, 3 ) );
	return 0;
}

int MCBlockDesc::lua_isLocked( lua_State *L ) {
	lua_pushboolean( L, getLuaObjectArg<MCBlockDesc>( L, 1, MCBLOCKDESC_META )->isLocked() );
	return 1;
}

int MCBlockDesc::lua_setBiomeRoot( lua_State *L ) {
	getLuaObjectArg<MCBlockDesc>( L, 1, MCBLOCKDESC_META )->biomes.setBiomeRootPath( luaL_checkstring( L, 2 ) );
	return 0;
}

int MCBlockDesc::lua_setBiomeChannel( lua_State *L ) {
	MCBlockDesc *blocks = getLuaObjectArg<MCBlockDesc>( L, 1, MCBLOCKDESC_META );
	unsigned channel = (unsigned)luaL_checknumber( L, 2 );
	if( channel >= MCBiome::MAX_BIOME_CHANNELS ) {
		char msg[256];
		sprintf( msg, "The max number of biome channels is %d", MCBiome::MAX_BIOME_CHANNELS );
		lua_pushstring( L, msg );
		lua_error( L );
	}

	if( lua_isboolean( L, 3 ) ) {
		// Enabling the channel
		SDL_Surface *im = *(SDL_Surface**)luaL_checkudata( L, 4, LUAIMAGE_META );
		blocks->biomes.enableBiomeChannel( channel, im, !!lua_toboolean( L, 3 ) );
	} else {
		// Disable the channel
		unsigned r = (unsigned)(255*luaL_optnumber( L, 3, 1.0 ));
		unsigned g = (unsigned)(255*luaL_optnumber( L, 4, 1.0 ));
		unsigned b = (unsigned)(255*luaL_optnumber( L, 5, 1.0 ));
		unsigned color = 0xff000000u | r | (g<<8) | (b<<16);
		blocks->biomes.disableBiomeChannel( channel, color );
	}
	return 0;
}

int MCBlockDesc::lua_setBiomeDefaultPos( lua_State *L ) {
	unsigned posx = (unsigned)(255*luaL_checknumber( L, 2 ));
	unsigned posy = (unsigned)(255*luaL_checknumber( L, 3 ));
	unsigned short pos = (unsigned short)(posx | (posy << 8));
	getLuaObjectArg<MCBlockDesc>( L, 1, MCBLOCKDESC_META )->biomes.setDefaultPos( pos );
	return 0;
}

int MCBlockDesc::lua_destroy( lua_State *L ) {
	delete getLuaObjectArg<MCBlockDesc>( L, 1, MCBLOCKDESC_META );
	return 0;
}

static const luaL_Reg MCBlockDesc_functions[] = {
	{ "setGeometry", &MCBlockDesc::lua_setGeometry },
	{ "setSolidity", &MCBlockDesc::lua_setSolidity },
	{ "setHighlight", &MCBlockDesc::lua_setHighlight },
	{ "noBlockLighting", &MCBlockDesc::lua_noBLockLighting },
	{ "setDefAirSkylight", &MCBlockDesc::lua_setDefAirSkylight },
	{ "isLocked", &MCBlockDesc::lua_isLocked },
	{ "setBiomeRoot", &MCBlockDesc::lua_setBiomeRoot },
	{ "setBiomeChannel", &MCBlockDesc::lua_setBiomeChannel },
	{ "setBiomeDefaultPos", &MCBlockDesc::lua_setBiomeDefaultPos },
	{ "destroy", &MCBlockDesc::lua_destroy },
	{ NULL, NULL }
};

void MCBlockDesc::setupLua( lua_State *L ) {
	luaL_newmetatable( L, MCBLOCKDESC_META );
	lua_pushvalue( L, -1 );
	lua_setfield( L, -2, "__index" );
	luaL_register( L, NULL, &MCBlockDesc_functions[0] );
	lua_pop( L, 1 );

	lua_pushcfunction( L, &MCBlockDesc::lua_create );
	lua_setfield( L, -2, "newBlockDesc" );
}
