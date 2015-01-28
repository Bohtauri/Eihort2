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

#ifndef MCBLOCKDESC_H
#define MCBLOCKDESC_H

#include "luaobject.h"
#include "mcbiome.h"

#define MCBLOCKDESC_META "MCBlockDesc"

#define SIGNTEXT_BLOCK_ID (1u<<12)
#define BLOCK_ID_COUNT ((1u<<12)+1)

namespace mcgeom {
	class BlockGeometry;
}

class MCBlockDesc : public LuaObject {
public:
	MCBlockDesc();
	~MCBlockDesc();

	inline unsigned shouldHighlight( unsigned id ) const { return blockFlags[id] & 0x80u; }
	inline unsigned getSolidity( unsigned id, unsigned dir ) const { return blockFlags[id] & (1u<<dir); }
	inline mcgeom::BlockGeometry *getGeometry( unsigned id ) const { return geometry[id]; }
	inline bool enableBlockLighting() const { return blockLighting; }

	inline void setDefAirSkyLight( unsigned light, bool override = false ) { defAirSkyLight = light; overrideAirSkyLight = override; }
	inline unsigned getDefAirSkyLight() const { return defAirSkyLight; }
	inline bool getDefAirSkyLightOverride() const { return overrideAirSkyLight; }
	
	inline void lock() { lockCount++; }
	void unlock();
	inline bool isLocked() { return lockCount > 0; }

	inline const MCBiome *getBiomes() const { return &biomes; }

	// Lua functions
	static int lua_create( lua_State *L );
	static int lua_setGeometry( lua_State *L );
	static int lua_setSolidity( lua_State *L );
	static int lua_setHighlight( lua_State *L );
	static int lua_noBLockLighting( lua_State *L );
	static int lua_setDefAirSkylight( lua_State *L );
	static int lua_isLocked( lua_State *L );
	static int lua_setBiomeRoot( lua_State *L );
	static int lua_setBiomeChannel( lua_State *L );
	static int lua_setBiomeDefaultPos( lua_State *L );
	static int lua_destroy( lua_State *L );
	static void setupLua( lua_State *L );

private:
	unsigned char blockFlags[BLOCK_ID_COUNT];
	mcgeom::BlockGeometry *geometry[BLOCK_ID_COUNT];
	MCBiome biomes;
	bool blockLighting;

	unsigned defAirSkyLight;
	bool overrideAirSkyLight;

	unsigned lockCount;
};

#endif
