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


#include <string>
#include <cassert>

#include "mcmap.h"

MCMap::MCMap( MCRegionMap *regions )
: lastChunkX(0x7fffffff), lastChunkY(0x7fffffff), lastChunk(NULL)
, loadedList(NULL)
, loadedListTail(NULL)
, nLoadedChunks(0)
, regions(regions)
{
}

MCMap::~MCMap() {
}

bool MCMap::getBlockID( int x, int y, int z, unsigned short &id ) {
	if( z < 0 )
		return false;

	Chunk *chunk = getChunk( x, y );
	if( !chunk )
		return false;

	if( z >= chunk->minZ && z <= chunk->maxZ ) {
		id = chunk->id[(toLinearCoordInChunk(x,y) << chunk->zHtShift) + z - chunk->minZ];
	} else {
		id = 0;
	}
	return true;
}

bool MCMap::getBiomeCoords( int x, int y, unsigned short &c ) {
	Chunk *chunk = getChunk( x, y );
	if( !chunk || !chunk->biomes )
		return false;

	c = chunk->biomes[toLinearCoordInChunk(x,y)];
	return true;
}

/*
unsigned short *MCMap::getBlockIDs( int x, int y ) {
	Chunk *chunk = getChunk( x, y );
	if( chunk )
		return chunk->id + (toLinearCoordInChunk(x,y) * chunk->zHeight);
	return NULL;
}

unsigned char *MCMap::getBlockLight( int x, int y ) {
	Chunk *chunk = getChunk( x, y );
	if( chunk )
		return chunk->blockLight + (toLinearCoordInChunk(x,y) * chunk->zHeight / 2);
	return NULL;
}

unsigned char *MCMap::getSkyLight( int x, int y ) {
	Chunk *chunk = getChunk( x, y );
	if( chunk )
		return chunk->skyLight + (toLinearCoordInChunk(x,y) * chunk->zHeight / 2);
	return NULL;
}

unsigned char *MCMap::getBlockData( int x, int y ) {
	Chunk *chunk = getChunk( x, y );
	if( chunk )
		return chunk->data + (toLinearCoordInChunk(x,y) * chunk->zHeight / 2);
	return NULL;
}
*/

bool MCMap::getColumn( int x, int y, MCMap::Column &col ) {
	Chunk *chunk = getChunk( x, y );
	if( chunk ) {
		int pos = toLinearCoordInChunk(x,y) << chunk->zHtShift;
		col.id = chunk->id + pos;
		pos >>= 1;
		col.blockLight = chunk->blockLight + pos;
		col.skyLight = chunk->skyLight + pos;
		col.data = chunk->data + pos;
		col.minZ = chunk->minZ;
		col.maxZ = chunk->maxZ;
		return true;
	}
	return false;
}

void MCMap::getSignsInArea( int minx, int maxx, int miny, int maxy, MCMap::SignList &signs ) {
	int maxChunkX = shift_right( maxx, 4 );
	int maxChunkY = shift_right( maxy, 4 );

	for( int chX = shift_right( minx, 4 ); chX <= maxChunkX; chX++ ) {
		for( int chY = shift_right( miny, 4 ); chY <= maxChunkY; chY++ ) {
			Chunk *chunk = getChunk( chX<<4, chY<<4 );
			if( !chunk )
				continue;

			nbt::Compound *level = (*chunk->nbt)[L"Level"].data.comp;
			nbt::List *entList = (*level)[L"TileEntities"].data.list;
			for( nbt::List::const_iterator it = entList->begin(); it != entList->end(); ++it ) {
				nbt::Compound *te = it->comp;
				if( *(*te)[L"id"].data.str == L"Sign" ) {
					SignDesc sign;
					sign.x = (*te)[L"z"].data.i;
					sign.y = (*te)[L"x"].data.i;
					if( sign.x >= minx && sign.x <= maxx && sign.y >= miny && sign.y <= maxy ) {
						sign.z = (*te)[L"y"].data.i;
						Column col;
						getColumn( sign.x, sign.y, col );
						unsigned id = col.getId( sign.z );
						unsigned data = col.getData( sign.z );
						sign.onWall = id == 68;
						sign.orientation = data;
						sign.text[0] = (*te)[L"Text1"].data.str->c_str();
						sign.text[1] = (*te)[L"Text2"].data.str->c_str();
						sign.text[2] = (*te)[L"Text3"].data.str->c_str();
						sign.text[3] = (*te)[L"Text4"].data.str->c_str();
						signs.push_back( sign );
					}
				}
			}
		}
	}
}

void MCMap::clearAllLoadedChunks() {
	while( loadedList )
		unloadOneChunk();
}

MCMap::Chunk *MCMap::getChunk_impl( Coords2D &coords ) {
	ChunkMap::iterator it = loadedChunks.find( coords );
	lastChunkX = coords.x;
	lastChunkY = coords.y;

	if( it != loadedChunks.end() ) {
		// Move the chunk to the front of the loaded list
		if( it->second.nextLoadedChunk ) {
			it->second.nextLoadedChunk->toMeLoaded = it->second.toMeLoaded;
			*it->second.toMeLoaded = it->second.nextLoadedChunk;

			loadedListTail->nextLoadedChunk = &it->second;
			it->second.toMeLoaded = &loadedListTail->nextLoadedChunk;
		}
	} else {
		// Try to load the chunk
		Chunk chunk;
		chunk.coords = coords;
		chunk.nbt = regions->readChunk( coords.x, coords.y );
		if( !chunk.nbt )
			return lastChunk = NULL;
		if( nLoadedChunks >= 200 )
			unloadOneChunk();
		if( !loadChunk( chunk ) )
			return lastChunk = NULL;

		nLoadedChunks++;
		loadedChunks[coords] = chunk;

		it = loadedChunks.find( coords );
		if( loadedListTail ) {
			loadedListTail->nextLoadedChunk = &it->second;
			it->second.toMeLoaded = &loadedListTail->nextLoadedChunk;
		} else {
			loadedList = &it->second;
			it->second.toMeLoaded = &loadedList;
		}
	}

	lastChunk = loadedListTail = &it->second;
	loadedListTail->nextLoadedChunk = NULL;

	return loadedListTail;
}

void MCMap::unloadOneChunk() {
	assert( loadedList );
	
	Coords2D coords = loadedList->coords;

	delete loadedList->nbt;
	loadedList->nbt = NULL;

	unloadChunk( *loadedList );

	if( loadedList->nextLoadedChunk ) {
		loadedList->nextLoadedChunk->toMeLoaded = loadedList->toMeLoaded;
	} else {
		lastChunkX = 0x7fffffff;
		lastChunkY = 0x7fffffff;
		lastChunk = loadedListTail = NULL;
	}
	*loadedList->toMeLoaded = loadedList->nextLoadedChunk;

	nLoadedChunks--;

	loadedChunks.erase( loadedChunks.find( coords ) );
}

MCMap_MCRegion::MCMap_MCRegion( MCRegionMap *regions )
: MCMap( regions )
{
}

MCMap_MCRegion::~MCMap_MCRegion() {
}

void MCMap_MCRegion::getExtentsWithin( int&, int&, int&, int&, int &minz, int &maxz ) {
	minz = 0;
	maxz = 127;
}

bool MCMap_MCRegion::loadChunk( MCMap::Chunk &chunk ) {
	nbt::Compound *level = (*chunk.nbt)[L"Level"].data.comp;
	chunk.blockLight = (unsigned char*)(*level)[L"BlockLight"].data.bytes;
	chunk.skyLight = (unsigned char*)(*level)[L"SkyLight"].data.bytes;
	chunk.data = (unsigned char*)(*level)[L"Data"].data.bytes;

	chunk.minZ = 0;
	chunk.maxZ = 127;
	chunk.zHtShift = 7;
	chunk.biomes = NULL;

	const unsigned char *idsrc = (unsigned char*)(*level)[L"Blocks"].data.bytes;
	chunk.id = new unsigned short[16*16*128];
	for( unsigned i = 0; i < 16*16*128; i++ )
		chunk.id[i] = idsrc[i];

	return true;
}

void MCMap_MCRegion::unloadChunk( Chunk &chunk ) {
	delete[] chunk.id;
	chunk.id = NULL;
	chunk.blockLight = NULL;
	chunk.skyLight = NULL;
	chunk.data = NULL;
}

MCMap_Anvil::MCMap_Anvil( MCRegionMap *regions )
: MCMap( regions )
{
}

MCMap_Anvil::~MCMap_Anvil() {
}

void MCMap_Anvil::getExtentsWithin( int &minx, int &maxx, int &miny, int &maxy, int &minz, int &maxz ) {
	int minChunkX = shift_right(minx,4), maxChunkX = shift_right(maxx+15,4);
	int minChunkY = shift_right(miny,4), maxChunkY = shift_right(maxy+15,4);
	
	int cminx = INT_MAX, cmaxx = INT_MIN;
	int cminy = INT_MAX, cmaxy = INT_MIN;
	int cminz = INT_MAX, cmaxz = INT_MIN;
	for( int cx = minChunkX; cx <= maxChunkX; cx++ ) {
		for( int cy = minChunkY; cy <= maxChunkY; cy++ ) {
			Coords2D coords = { cy, cx };
			Chunk *chunk = getChunk_impl( coords );
			if( chunk ) {
				cminx = std::min( cminx, cx << 4 );
				cmaxx = std::max( cmaxx, (cx << 4) + 15 );
				cminy = std::min( cminy, cy << 4 );
				cmaxy = std::max( cmaxx, (cy << 4) + 15 );
				cminz = std::min( cminz, chunk->minZ );
				cmaxz = std::max( cmaxz, chunk->maxZ );
			}
		}
	}
	
	minx = std::max( minx, cminx );
	maxx = std::min( maxx, cmaxx );
	miny = std::max( miny, cminy );
	maxy = std::min( maxy, cmaxy );
	minz = std::max( minz, cminz );
	maxz = std::min( maxz, cmaxz );
}

bool MCMap_Anvil::loadChunk( MCMap::Chunk &chunk ) {
	nbt::Compound *level = (*chunk.nbt)[L"Level"].data.comp;
	nbt::List *sections = (*level)[L"Sections"].data.list;

	chunk.minZ = INT_MAX;
	chunk.maxZ = INT_MIN;
	for( nbt::List::const_iterator it = sections->begin(); it != sections->end(); ++it ) {
		int zbase = (*it->comp)[L"Y"].data.b << 4;
		if( zbase < chunk.minZ )
			chunk.minZ = zbase;
		if( zbase + 15 > chunk.maxZ )
			chunk.maxZ = zbase + 15;
	}

	if( chunk.minZ == INT_MAX )
		return false;

	unsigned zHeight = chunk.maxZ - chunk.minZ + 1;
	unsigned zSections = zHeight>>4;
	chunk.zHtShift = 4;
	while( zHeight > (1u << chunk.zHtShift) )
		chunk.zHtShift++;

	unsigned blocks = (16*16) << chunk.zHtShift;
	chunk.id = new unsigned short[blocks];
	chunk.blockLight = new unsigned char[blocks>>1];
	chunk.skyLight = new unsigned char[blocks>>1];
	chunk.data = new unsigned char[blocks>>1];
	memset( chunk.id, 0, blocks<<1 );
	memset( chunk.blockLight, 0, blocks>>1 );
	memset( chunk.skyLight, 0, blocks>>1 );
	memset( chunk.data, 0, blocks>>1 );
	bool *zSectionFilled = new bool[zSections];
	for( unsigned i = 0; i < zSections; i++ )
		zSectionFilled[i] = false;

	for( nbt::List::const_iterator it = sections->begin(); it != sections->end(); ++it ) {
		nbt::Compound *section = it->comp;
		int zbase = (*section)[L"Y"].data.b;
		zbase = (zbase << 4) - chunk.minZ;
		zSectionFilled[zbase>>4] = true;

		unsigned char *idSrc = (unsigned char*)(*section)[L"Blocks"].data.bytes;
		unsigned char *blockLightSrc = (unsigned char*)(*section)[L"BlockLight"].data.bytes;
		unsigned char *skyLightSrc = (unsigned char*)(*section)[L"SkyLight"].data.bytes;
		unsigned char *dataSrc = (unsigned char*)(*section)[L"Data"].data.bytes;
		unsigned char *addSrc = NULL;
		if( section->find( L"Add" ) != section->end() )
			addSrc = (unsigned char*)(*section)[L"Add"].data.bytes;

		for( int zo = 0; zo < 16; zo++ ) {
			for( int yo = 0; yo < 16; yo++ ) {
				for( int xo = 0; xo < 16; xo++ ) {
					unsigned destIdx = (((xo<<4)+yo) << chunk.zHtShift) + zbase + zo;
					unsigned srcIdx = (zo<<8)+(yo<<4)+xo;
					unsigned src4Shift = ((srcIdx&1u)<<2);
					unsigned dest4Shift = ((destIdx&1u)<<2);

					chunk.id[destIdx] = idSrc[srcIdx];
					if( addSrc )
						chunk.id[destIdx] += (unsigned short)((addSrc[srcIdx>>1] >> src4Shift) & 0xfu) << 8;

					chunk.blockLight[destIdx>>1] |= ((blockLightSrc[srcIdx>>1] >> src4Shift) & 0xfu) << dest4Shift;
					chunk.skyLight[destIdx>>1] |= ((skyLightSrc[srcIdx>>1] >> src4Shift) & 0xfu) << dest4Shift;
					chunk.data[destIdx>>1] |= ((dataSrc[srcIdx>>1] >> src4Shift) & 0xfu) << dest4Shift;
				}
			}
		}
	}

	if( level->find( L"Biomes" ) != level->end() ) {
		unsigned char *biomeIds = (unsigned char*)(*level)[L"Biomes"].data.bytes;
		chunk.biomes = new unsigned short[16*16];
		const static unsigned short biomeIdToCoords[] = {
			0xBF7Fu, // Ocean
			0xAD32u, // Plains
			0xff00u, // Desert
			0xEFCBu, // Extreme Hills
			0x704Cu, // Forest
			0xF4F2u, // Taiga
			0x4732u, // Swampland
			0xBF7Fu, // River
			0xff00u, // Hell
			0x7F7Fu, // Sky
			0xBF7Fu, // Frozen Ocean
			0xBF7Fu, // Frozen River
			0xFFFFu, // Ice Plains
			0xFFFFu, // Ice Mountains
			0x1919u, // Mushroom Island
			0x1919u, // Mushroom Island Shore
			0xBF7Fu, // Beach
			0xff00u, // Desert Hills
			0x704Cu, // Forest Hills
			0xF4F2u, // Taiga Hills
			0xEFCBu, // Extreme Hills Edge
			0x1900u, // Jungle
			0x1900u  // Jungle Hills
		};
		for( unsigned i = 0; i < 16*16; i++ ) {
			if( biomeIds[i] < sizeof(biomeIdToCoords)/sizeof(unsigned short) ) {
				chunk.biomes[(i>>4)+((i&0xf)<<4)] = biomeIdToCoords[biomeIds[i]];
			} else {
				chunk.biomes[i] = 0x7F7Fu;
			}
		}
	} else {
		chunk.biomes = NULL;
	}

	for( unsigned zs = 0; zs < zSections; zs++ ) {
		if( !zSectionFilled[zs] ) {
			unsigned zbase = zs << 4;
			for( unsigned x = 0; x < 16; x++ ) {
				for( unsigned y = 0; y < 16; y++ ) {
					unsigned xyc = toLinearCoordInChunk(x,y) << chunk.zHtShift;
					for( unsigned zo = 0; zo < 16; zo+=2 ) {
						// TODO: Copy the top block's luminance
						// for now, full sun in skipped blocks
						chunk.skyLight[(xyc+zbase+zo)>>1] = 0xffu;
					}
				}
			}
		}
	}

	delete[] zSectionFilled;

	return true;
}

void MCMap_Anvil::unloadChunk( Chunk &chunk ) {
	delete[] chunk.id;
	chunk.id = NULL;
	delete[] chunk.blockLight;
	chunk.blockLight = NULL;
	delete[] chunk.skyLight;
	chunk.skyLight = NULL;
	delete[] chunk.data;
	chunk.data = NULL;
	delete[] chunk.biomes;
	chunk.biomes = NULL;
}
