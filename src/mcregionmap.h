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

#ifndef MCREGIONMAP_H
#define MCREGIONMAP_H

#include <string>
#include <SDL.h>

#include "luaobject.h"
#include "nbt.h"

struct Coords2D {
	inline bool operator< ( const Coords2D &rhs ) const {
		return x < rhs.x || (x == rhs.x && y < rhs.y);
	}
	int x;
	int y;
};

class MCBiome;
struct lua_State;

class MCRegionMap : public LuaObject {
public:
	explicit MCRegionMap( const char *rootPath, bool anvil = true );
	~MCRegionMap();

	class ChangeListener {
	public:
		virtual ~ChangeListener();
		//virtual void newRegion( int x, int y ) = 0;
		virtual void chunkChanged( int x, int y ) = 0;
	protected:
		ChangeListener();
	};

	static inline void regionCoordsToChunkExtents( int x, int y, int &minx, int &maxx, int &miny, int &maxy ) {
		const unsigned rgShift = 5; // 32 chunks/region
		minx = x << rgShift;
		maxx = minx + (1 << rgShift) - 1;
		miny = y << rgShift;
		maxy = miny + (1 << rgShift) - 1;
	}

	static inline void chunkCoordsToBlockExtents( int x, int y, int &minx, int &maxx, int &miny, int &maxy ) {
		const unsigned rgShift = 4; // 16 chunks/region
		minx = x << rgShift;
		maxx = minx + (1 << rgShift) - 1;
		miny = y << rgShift;
		maxy = miny + (1 << rgShift) - 1;
	}

	void getWorldChunkExtents( int &minx, int &maxx, int &miny, int &maxy );
	void getWorldBlockExtents( int &minx, int &maxx, int &miny, int &maxy );
	inline unsigned getTotalRegionCount() const { return (unsigned)regions.size(); }

	// x and y are in chunk coords (that is, blockxy/16)
	// The function is reentrant
	nbt::Compound *readChunk( int x, int y );

	void changeRoot( const char *newRoot, bool anvil = true );
	const std::string &getRoot() const { return root; }
	bool isAnvil() const { return anvil; }

	void checkForRegionChanges();
	inline void setListener( ChangeListener *l ) { listener = l; }

	// Lua functions
	static int lua_create( lua_State *L );
	static int lua_getRegionCount( lua_State *L );
	static int lua_getRootPath( lua_State *L );
	static int lua_changeRootPath( lua_State *L );
	static int lua_setMonitorState( lua_State *L );
	static int lua_createView( lua_State *L );
	static int lua_destroy( lua_State *L );
	static void setupLua( lua_State *L );

private:
	struct RegionDesc {
		uint32_t *sectors;
		uint32_t *chunkTimes;
	};

	void exploreDirectories();
	void flushRegionSectors();
	void checkRegionForChanges( int x, int y, RegionDesc *region );
	bool getChunkInfo( int x, int y, unsigned &updTime );

	static int updateScanner( void *rgMapCookie );
	const char *getRegionExt() const { return anvil ? "mca" : "mcr"; }

	std::string root;
	bool anvil;

	int minRgX, maxRgX, minRgY, maxRgY;
	typedef std::map< Coords2D, RegionDesc > RegionMap;
	RegionMap regions;

	SDL_Thread *changeThread;
	SDL_mutex *rgDescMutex;
	ChangeListener *listener;
	bool watchUpdates;
};

#endif // MCREGIONMAP_H
