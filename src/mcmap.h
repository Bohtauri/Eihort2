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

#ifndef MCMAP_H
#define MCMAP_H

#include <map>
#include <string>

#include "nbt.h"
#include "jmath.h"
#include "mcregionmap.h"
#include "platform.h"

class MCMap {
public:
	explicit MCMap( MCRegionMap *regions );
	~MCMap();

	struct Column {
		inline unsigned getId( int z ) const { return z < minZ ? (z < 0 ? 7 : 0) : z > maxZ ? 0 : id[z-minZ]; }
		static inline unsigned get4bitsAt( unsigned char *dat, unsigned zo )
			{ return (unsigned)((dat[zo>>1] >> ((zo&1)<<2)) & 0xf); }
		inline unsigned get4BitsAtSafe( unsigned char *dat, int z, unsigned high ) const
			{ return z < minZ ? 0 : z > maxZ ? high : get4bitsAt(dat, z-minZ); }
		inline unsigned getBlockLight( int z ) const { return get4BitsAtSafe( blockLight, z, 0 ); }
		inline unsigned getSkyLight( int z ) const { return get4BitsAtSafe( skyLight, z, 0xf ); }
		inline unsigned getData( int z ) const { return get4BitsAtSafe( data, z, 0 ); }
		inline unsigned getHeight() const { return maxZ-minZ+1; }

		unsigned short *id;
		unsigned char *blockLight;
		unsigned char *skyLight;
		unsigned char *data;
		int minZ, maxZ;
	};

	bool getBlockID( int x, int y, int z, unsigned short &id );
	bool getBiomeCoords( int x, int y, unsigned short &c );
	//unsigned char *getBlockLight( int x, int y );
	//unsigned char *getSkyLight( int x, int y );
	//unsigned char *getBlockData( int x, int y );
	bool getColumn( int x, int y, Column &col );

	void getXYExtents( int &minx, int &maxx, int &miny, int &maxy );
	MCRegionMap *getRegions() { return regions; }

	virtual void getExtentsWithin( int &minx, int &maxx, int &miny, int &maxy, int &minz, int &maxz ) = 0;
	
	struct SignDesc {
		int x, y, z;
		bool onWall;
		unsigned orientation;
		const wchar_t *text[4];
	};
	typedef std::list<SignDesc> SignList;
	void getSignsInArea( int minx, int maxx, int miny, int maxy, SignList &signs );
	
	void clearAllLoadedChunks();

protected:
	void exploreDirectories();

	struct Chunk {
		nbt::Compound *nbt;
		unsigned short *id;
		unsigned char *blockLight;
		unsigned char *skyLight;
		unsigned char *data;
		unsigned short *biomes;
		Chunk *nextLoadedChunk;
		Chunk **toMeLoaded;
		Coords2D coords;
		int minZ, maxZ;
		unsigned zHtShift;
	};

	static inline unsigned toLinearCoordInChunk( int x, int y ) {
		return (((unsigned)y&15)<<4) | ((unsigned)x&15);
	}

	inline Chunk *getChunk( int x, int y ) {
		Coords2D coords;
		coords.x = shift_right( y, 4 );
		coords.y = shift_right( x, 4 );
		if( lastChunkX==coords.x && lastChunkY==coords.y )
			return lastChunk;
		return getChunk_impl( coords );
	}
	Chunk *getChunk_impl( Coords2D &coords );
	virtual bool loadChunk( Chunk &chunk ) = 0;
	virtual void unloadChunk( Chunk &chunk ) = 0;

	int lastChunkX, lastChunkY;
	Chunk *lastChunk;

	typedef std::map< Coords2D, Chunk > ChunkMap;
	ChunkMap loadedChunks;
	std::string root;

	void unloadOneChunk();
	Chunk *loadedList;
	Chunk *loadedListTail;
	unsigned nLoadedChunks;

	MCRegionMap *regions;
};

class MCMap_MCRegion : public MCMap {
public:
	explicit MCMap_MCRegion( MCRegionMap *regions );
	~MCMap_MCRegion();

	virtual void getExtentsWithin( int &minx, int &maxx, int &miny, int &maxy, int &minz, int &maxz );

protected:
	virtual bool loadChunk( Chunk &chunk );
	virtual void unloadChunk( Chunk &chunk );
};

class MCMap_Anvil : public MCMap {
public:
	explicit MCMap_Anvil( MCRegionMap *regions );
	~MCMap_Anvil();

	virtual void getExtentsWithin( int &minx, int &maxx, int &miny, int &maxy, int &minz, int &maxz );

protected:
	virtual bool loadChunk( Chunk &chunk );
	virtual void unloadChunk( Chunk &chunk );
};

#endif

