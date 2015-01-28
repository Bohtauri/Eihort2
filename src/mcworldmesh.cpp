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


#include <cassert>
#include <GL/glew.h>
#include "mcworldmesh.h"
#include "mcmap.h"
#include "blockmaterial.h"
#include "mcbiome.h"
#include "mcblockdesc.h"


MCWorldMesh::MCWorldMesh()
: lightTex(0)
, meta(NULL)
, bld(NULL)
, opaqueEnd(0)
, transpEnd(0)
, vtx_vbo(0)
, idx_vbo(0)
, hasOpaque(false)
, hasTransparent(false)
, nextMesh(NULL)
{
}

MCWorldMesh::~MCWorldMesh() {
	if( lightTex )
		glDeleteTextures( 1, &lightTex );

	if( biomeSrc )
		biomeSrc->freeBiomeTextures( &biomeTex[0] );

	if( meta ) {
		glDeleteBuffers( 2, &vtx_vbo );
		free( meta );
	}
}

class IslandHole {
public:
	IslandHole( bool firstPointVisible )
		: firstBlockIsNonVisible(!firstPointVisible)
	{ }
	~IslandHole()
	{ }

	const mcgeom::Point &insidePoint() const { return inside; }
	mcgeom::Point &insidePoint() { return inside; }
	std::vector< mcgeom::Point > &blocks() { return contourBlocks; }
	std::vector< mcgeom::Point > &points() { return contourPoints; }
	const std::vector< mcgeom::Point > &blocks() const { return contourBlocks; }
	const std::vector< mcgeom::Point > &points() const { return contourPoints; }

	bool isRemovable() const {
		if( !(firstBlockIsNonVisible && contourPoints.size() == 4) )
			return false;

		int dist1 = abs( contourPoints[0].v[0] - contourPoints[1].v[0] ) +
			        abs( contourPoints[0].v[1] - contourPoints[1].v[1] ) +
					abs( contourPoints[0].v[2] - contourPoints[1].v[2] );
		int dist2 = abs( contourPoints[1].v[0] - contourPoints[2].v[0] ) +
			        abs( contourPoints[1].v[1] - contourPoints[2].v[1] ) +
					abs( contourPoints[1].v[2] - contourPoints[2].v[2] );
		return dist1 == 1 && dist2 == 1;
	}

private:
	bool firstBlockIsNonVisible;
	mcgeom::Point inside;
	std::vector< mcgeom::Point > contourBlocks;
	std::vector< mcgeom::Point > contourPoints;
};

class MeshBuilder {
public:
	MeshBuilder( const Extents &pow2Ext, const Extents &hullExt, const MCBlockDesc *blockDesc )
		: biomeCoords(NULL), pow2Ext(pow2Ext), hullExt(hullExt), blockDesc(blockDesc)
	{
		sizex = (unsigned)(pow2Ext.maxx-pow2Ext.minx+1);
		sizey = (unsigned)(pow2Ext.maxy-pow2Ext.miny+1);
		sizez = (unsigned)(pow2Ext.maxz-pow2Ext.minz+1);
		shiftx = getPow2( sizex );
		shifty = getPow2( sizey );
		shiftz = getPow2( sizez );
		totalSize = 1u << (shiftx + shifty + shiftz);
		blockInfo = new unsigned short[totalSize];
		memset( blockInfo, 0, totalSize<<1 );

		lightingTex = new unsigned char[totalSize<<1];
		memset( lightingTex, 0, totalSize<<1 );

		for( unsigned i = 0; i < BLOCK_ID_COUNT; i++ )
			geomStreams[i] = NULL;

		origin[0] = pow2Ext.minx + (sizex>>1);
		origin[1] = pow2Ext.miny + (sizey>>1);
		origin[2] = pow2Ext.minz + (sizez>>1);
	}

	~MeshBuilder() {
		delete[] blockInfo;
		delete[] lightingTex;
	}

	const Extents *getExtents() { return &hullExt; }
	const Extents *getLightExtents() { return &pow2Ext; }
	const MCBlockDesc *getBlockDesc() { return blockDesc; }

	void markDone( const mcgeom::Point &pt, unsigned dir ) { markDone(pt.x,pt.y,pt.z,dir); }
	void markDone( int x, int y, int z, unsigned dir ) {
		blockInfo[toLinCoord(x,y,z)] |= (unsigned char)(1<<dir);
	}
	bool isDone( const mcgeom::Point &pt, unsigned dir ) { return isDone(pt.x,pt.y,pt.z,dir); }
	bool isDone( int x, int y, int z, unsigned dir ) {
		return 0 != (blockInfo[toLinCoord(x,y,z)] & (1<<dir));
	}
	void flagEdge( const mcgeom::Point &pt, unsigned dir ) { flagEdge(pt.x,pt.y,pt.z,dir); }
	void flagEdge( int x, int y, int z, unsigned dir ) {
		blockInfo[toLinCoord(x,y,z)] |= 1u << (dir+8);
	}
	unsigned isEdgeFlagged( const mcgeom::Point &pt, unsigned dir ) { return isEdgeFlagged(pt.x,pt.y,pt.z,dir); }
	unsigned isEdgeFlagged( int x, int y, int z, unsigned dir ) {
		return blockInfo[toLinCoord(x,y,z)] & (1u << (dir+8));
	}
	void unflag( const mcgeom::Point &pt ) { unflag(pt.x,pt.y,pt.z); }
	void unflag( int x, int y, int z ) {
		blockInfo[toLinCoord(x,y,z)] &= 0xffu;
	}
	bool isFlaggedOrDone( const mcgeom::Point &pt, unsigned islDir, unsigned mvDir ) { return isFlaggedOrDone(pt.x,pt.y,pt.z,islDir,mvDir); }
	bool isFlaggedOrDone( int x, int y, int z, unsigned islDir, unsigned mvDir ) {
		return 0 != (blockInfo[toLinCoord(x,y,z)] & ((1u<<(mvDir+8)) | (1u<<islDir)));
	}

	mcgeom::GeometryCluster *getGeometryCluster( unsigned id ) {
		mcgeom::GeometryCluster *str = geomStreams[id];
		if( !str )
			str = geomStreams[id] = blockDesc->getGeometry(id)->newCluster();
		return str;
	}

	MCWorldMesh *buildFinalWorldMesh() {
		// Actually create the mesh object
		MCWorldMesh *wmesh = new MCWorldMesh;
		wmesh->bld = this;

		return wmesh;
	}

	struct GeomAndCluster {
		mcgeom::BlockGeometry *geom;
		mcgeom::GeometryCluster *cluster;

		inline bool operator<( const GeomAndCluster &other ) const {
			return *geom < *other.geom;
		}
	};

	void finalizeMesh( MCWorldMesh *wmesh ) {
		// Finalize the geometry
		std::vector< GeomAndCluster > renderOrder;

		for( unsigned i = 0; i < BLOCK_ID_COUNT; i++ ) {
			if( geomStreams[i] ) {
				if( !geomStreams[i]->destroyIfEmpty() ) {
					GeomAndCluster gc;
					gc.geom = blockDesc->getGeometry( i );
					gc.cluster = geomStreams[i];
					renderOrder.push_back( gc );
				}
				geomStreams[i] = NULL;
			}
		}

		wmesh->opaqueEnd = 0;
		wmesh->transpEnd = 0;
		wmesh->hasTransparent = false;
		wmesh->hasOpaque = false;
		wmesh->vtxMem = 0;
		wmesh->idxMem = 0;
		wmesh->texMem = 0;

		if( !renderOrder.empty() ) {
			std::sort( renderOrder.begin(), renderOrder.end() );
			
			mcgeom::GeometryStream metaStream, vtxStream, idxStream;
			for( std::vector< GeomAndCluster >::const_iterator it = renderOrder.begin(); it != renderOrder.end(); ++it ) {
				if( it->geom->getRenderGroup() >= mcgeom::RenderGroup::TRANSPARENT ) {
					if( !wmesh->hasTransparent ) {
						wmesh->opaqueEnd = metaStream.getVertSize();
						wmesh->hasTransparent = true;
					}
				} else {
					wmesh->hasOpaque = true;
				}

				it->cluster->finalize( &metaStream, &vtxStream, &idxStream );
			}

			wmesh->transpEnd = metaStream.getVertSize();
			if( !wmesh->hasTransparent )
				wmesh->opaqueEnd = wmesh->transpEnd;
			wmesh->cost = std::min( 20u, 1u + (metaStream.getVertSize() >> 9) );
			wmesh->meta = malloc( metaStream.getVertSize() );
			memcpy( wmesh->meta, metaStream.getVertices(), metaStream.getVertSize() );


			glGenBuffers( 2, &wmesh->vtx_vbo );
			glBindBuffer( GL_ARRAY_BUFFER, wmesh->vtx_vbo );
			glBufferData( GL_ARRAY_BUFFER, vtxStream.getVertSize(), vtxStream.getVertices(), GL_STATIC_DRAW );
			wmesh->vtxMem += vtxStream.getVertSize();

			glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, wmesh->idx_vbo );
			glBufferData( GL_ELEMENT_ARRAY_BUFFER, idxStream.getVertSize(), idxStream.getVertices(), GL_STATIC_DRAW );
			wmesh->idxMem += idxStream.getVertSize();

			glBindBuffer( GL_ARRAY_BUFFER, 0 );
			glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );

			glGenTextures( 1, &wmesh->lightTex );
			glEnable( GL_TEXTURE_3D );
			glBindTexture( GL_TEXTURE_3D, wmesh->lightTex );

			glTexParameterf (GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameterf (GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glTexParameterf (GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameterf (GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameterf (GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

			glTexParameteri( GL_TEXTURE_3D, GL_GENERATE_MIPMAP, GL_FALSE ); 

			glTexImage3D( GL_TEXTURE_3D, 0, GL_LUMINANCE4_ALPHA4, (int)sizex, (int)sizey, (int)sizez, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, lightingTex );
			wmesh->texMem += sizex * sizey * sizez;

			glDisable( GL_TEXTURE_3D );
			glBindTexture( GL_TEXTURE_3D, 0 );

			wmesh->biomeSrc = blockDesc->getBiomes();
			wmesh->texMem += wmesh->biomeSrc->finalizeBiomeTextures( biomeCoords, pow2Ext.minx, pow2Ext.maxx, pow2Ext.miny, pow2Ext.maxy, &wmesh->biomeTex[0] );
		} else {
			wmesh->meta = NULL;
			wmesh->cost = 0;

			wmesh->biomeSrc = NULL;
			wmesh->biomeTex[0] = 0;
			wmesh->biomeTex[1] = 0;
			wmesh->biomeTex[2] = 0;
		}

		wmesh->origin[0] = origin[0];
		wmesh->origin[1] = origin[1];
		wmesh->origin[2] = origin[2];
		wmesh->lightTexScale[0] = (1.0/16.0) / sizex;
		wmesh->lightTexScale[1] = (1.0/16.0) / sizey;
		wmesh->lightTexScale[2] = (1.0/16.0) / sizez;
		wmesh->bld = NULL;

		delete this;
	}

	IslandHole *newHole( bool visible ) {
		holes.push_back( IslandHole(visible) );
		return &holes.back();
	}
	void clearHoles() {
		holes.clear();
	}
	std::list< IslandHole > &getHoles() {
		return holes;
	}

	inline void toLocalSpace( mcgeom::Point &pt ) {
		pt.x -= origin[0];
		pt.y -= origin[1];
		pt.z -= origin[2];
	}
	void toLocalSpace( std::vector< mcgeom::Point > &points ) {
		for( std::vector< mcgeom::Point >::iterator it = points.begin(); it != points.end(); ++it )
			toLocalSpace( *it );
	}

	void transformHolesToLocalSpace() {
		for( std::list< IslandHole >::iterator it = holes.begin(); it != holes.end(); ++it ) {
			toLocalSpace( it->blocks() );
			toLocalSpace( it->points() );
			toLocalSpace( it->insidePoint() );
		}
	}

	unsigned gatherHoleContourPoints( unsigned *&ends, mcgeom::Point *&points, mcgeom::Point *&inside ) {
		ends = new unsigned[holes.size()];
		inside = new mcgeom::Point[holes.size()];
		unsigned pointCount = 0, n = 0;
		for( std::list< IslandHole >::const_iterator it = holes.begin(); it != holes.end(); ++it ) {
			if( !it->isRemovable() ) {
				ends[n] = (pointCount += (unsigned)it->points().size());
				inside[n] = it->insidePoint();
				n++;
			}
		}
		
		if( n ) {
			mcgeom::Point *pt = points = new mcgeom::Point[ends[n-1]];
			for( std::list< IslandHole >::const_iterator it = holes.begin(); it != holes.end(); ++it ) {
				if( !it->isRemovable() ) {
					memcpy( pt, &it->points()[0], sizeof(mcgeom::Point) * it->points().size() );
					pt += it->points().size();
				}
			}
		} else {
			points = NULL;
			delete[] ends;
			ends = NULL;
			delete[] inside;
			inside = NULL;
		}

		return n;
	}

	inline void setLightingAt( int x, int y, int z, unsigned block, unsigned sky ) {
		unsigned i = toLLinCoord(x,y,z) << 1;
		lightingTex[i] = std::max( lightingTex[i], (unsigned char)(block<<4) );
		lightingTex[i+1] = std::max( lightingTex[i+1], (unsigned char)(sky<<4) );
	}

	inline void setBiomeCoords( unsigned short *coords ) {
		biomeCoords = coords;
	}

private:
	inline unsigned getPow2( unsigned i ) {
		if( i ) {
			i >>= 1;
			unsigned shift = 0;
			while( i ) {
				shift++;
				i >>= 1;
			}
			return shift;
		}
		return 0;
	}

	inline unsigned toLinCoord( int x, int y, int z ) {
		return (unsigned)(z-pow2Ext.minz) + ((unsigned)(x-pow2Ext.minx)<<shiftz) + ((unsigned)(y-pow2Ext.miny)<<(shiftz+shiftx));
	}
	inline unsigned toLLinCoord( int x, int y, int z ) {
		// Ordered as we want GL to order the texture
		return (unsigned)(x-pow2Ext.minx) + ((unsigned)(y-pow2Ext.miny)<<shiftx) + ((unsigned)(z-pow2Ext.minz)<<(shifty+shiftx));
	}

	mcgeom::GeometryCluster *geomStreams[BLOCK_ID_COUNT];
	// blockInfo:
	// Bits 0-5: Done flags for each block in each direction
	// Bits 8-11: Edge flags for use during island construction
	unsigned sizex, sizey, sizez, totalSize;
	unsigned shiftx, shifty, shiftz;
	unsigned short *blockInfo;
	int origin[3];
	unsigned short *biomeCoords;

	unsigned char *lightingTex;
	Extents pow2Ext, hullExt;

	std::list< IslandHole > holes;

	const MCBlockDesc *blockDesc;
};

inline void moveCoordsInDir( mcgeom::IslandDesc *island, int *coords, unsigned dir ) {
	switch( dir ) {
	case 0: coords[island->xax] -= island->xd;
		    coords[island->zax] -= island->xslope; break;
	case 1: coords[island->yax] -= island->yd;
		    coords[island->zax] -= island->yslope; break;
	case 2: coords[island->xax] += island->xd;
		    coords[island->zax] += island->xslope; break;
	case 3: coords[island->yax] += island->yd;
		    coords[island->zax] += island->yslope; break;
	}
}

inline void moveCoordsInDir1( mcgeom::IslandDesc *island, int *coords, unsigned dir ) {
	switch( dir ) {
	case 0: coords[island->xax] -= island->xd1;
		    coords[island->zax] -= island->xslope; break;
	case 1: coords[island->yax] -= island->yd1;
		    coords[island->zax] -= island->yslope; break;
	case 2: coords[island->xax] += island->xd1;
		    coords[island->zax] += island->xslope; break;
	case 3: coords[island->yax] += island->yd1;
		    coords[island->zax] += island->yslope; break;
	}
}

/*
static void adjustStartingPointAlongAxis( mcgeom::Point &pos, bool adjust, unsigned ax, unsigned zax, bool alongPosD, int slope ) {
	if( adjust ) {
		pos[ax]++;
		if( withD )
	}
}
*/

static bool scanContourAndFlag( MeshBuilder &bld, mcgeom::IslandDesc *island, MCMap *map, const mcgeom::Point &start, unsigned startDir, std::vector< mcgeom::Point > &contourBlocks, std::vector< mcgeom::Point > &contourPoints ) {

	mcgeom::Point pos = start;
	mcgeom::Point contourPt = pos;
	unsigned islDir = startDir, lastDir = startDir;
	bool exploratory = false;
	
	// Adjust the starting point for the contour based on the starting direction
	if( island->islandAxis&1 ) contourPt.v[island->zax]++;
	switch( startDir ) {
	case 0:
		contourPt.v[island->xax] += island->xd<0 ? 1 : 0;
		contourPt.v[island->yax] += island->yd>0 ? 1 : 0;
		break;
	case 1:
		contourPt.v[island->xax] += island->xd<0 ? 1 : 0;
		contourPt.v[island->yax] += island->yd<0 ? 1 : 0;
		break;
	case 2:
		contourPt.v[island->xax] += island->xd>0 ? 1 : 0;
		contourPt.v[island->yax] += island->yd<0 ? 1 : 0;
		break;
	case 3:
		contourPt.v[island->xax] += island->xd>0 ? 1 : 0;
		contourPt.v[island->yax] += island->yd>0 ? 1 : 0;
		break;
	}


	do {
		if( contourBlocks.size() > 9999 ) // Failsafe
			return false;

		// Try to move in islDir
		mcgeom::Point nextPos = pos;
		moveCoordsInDir( island, nextPos.v, islDir );

		// Is it out of the bounds of the area we are building a mesh for?
		if( !bld.getExtents()->contains( nextPos.x, nextPos.y, nextPos.z ) )
			goto dont_continue_island;

		// Does the map exist here? Does the block have the same id?
		MCMap::Column nextCol;
		if( !map->getColumn( nextPos.x, nextPos.y, nextCol ) ||
			nextCol.getId( nextPos.z ) != island->origin.block.id )
			goto dont_continue_island;

		// Complex island check
		if( island->continueIsland ) {
			mcgeom::InstanceContext nextBlock;
			nextBlock.block.pos = nextPos;
			nextBlock.block.id = (unsigned short)nextCol.getId( nextPos.z );
			nextBlock.block.data = (unsigned short)nextCol.getData( nextPos.z );
			nextBlock.sides[4].id = (unsigned short)(nextPos.z > bld.getExtents()->minz ? nextCol.getId( nextPos.z - 1 ) : 1u);
			nextBlock.sides[5].id = (unsigned short)(nextPos.z < bld.getExtents()->maxz ? nextCol.getId( nextPos.z + 1 ) : 1u);
			if( !island->continueIsland( island, &nextBlock ) )
				goto dont_continue_island;
		}

		// Is the block visible?
		if( island->checkVisibility | island->checkFacingSameId ) {
			unsigned short facingId;
			if( island->zax == 2 ) {
				facingId = (unsigned short)nextCol.getId( nextPos.z + island->zd );
			} else {
				nextPos.v[island->zax] += island->zd;
				if( !map->getBlockID( nextPos.x, nextPos.y, nextPos.z, facingId ) )
					// Invisible - skip the block
					goto dont_continue_island;
				nextPos.v[island->zax] -= island->zd;
			}
			if( (island->checkFacingSameId && facingId == island->origin.block.id) ||
				(island->checkVisibility && bld.getBlockDesc()->getSolidity( facingId, island->islandAxis )) )
				// Invisible - skip the block
				goto dont_continue_island;
		}

		// Continue the island
		lastDir = islDir;
		if( exploratory ) {
			// Output corner point
			contourPoints.push_back( contourPt );

			// Output corner block
			contourBlocks.push_back( pos );

			// There is no sense in exploring twice in a row
			moveCoordsInDir1( island, contourPt.v, islDir );
			bld.flagEdge( nextPos, (islDir - 1) & 3 );
			exploratory = false;
		} else {
			// Explore clockwise
			exploratory = true;
			islDir = (islDir - 1) & 3;
		}

		pos = nextPos;

		continue;

dont_continue_island:
		if( exploratory ) {
			exploratory = false;
		} else {
			// Output contour point
			contourPoints.push_back( contourPt );
			if( lastDir == islDir ) {
				// Output corner block
				contourBlocks.push_back( pos );
			}
		}

		// Mark the edge
		bld.flagEdge( pos, islDir );

		// Rotate counterclockwise
		islDir = (islDir + 1) & 3;
		moveCoordsInDir1( island, contourPt.v, islDir );

	} while( islDir != startDir || pos != start );

	if( contourBlocks.size() > 1 && contourBlocks.front() == contourBlocks.back() )
		contourBlocks.pop_back();

	return true;
}

static void holeScan( MeshBuilder &bld, mcgeom::IslandDesc *island, MCMap *map, const mcgeom::Point &start, unsigned dir ) {
	// First check if the starting position has overlapping flags
	// This takes care of the case where the island has 1-square wide sections
	if( bld.isEdgeFlagged( start, dir ) )
		return;

	mcgeom::Point pos = start;
	bld.markDone( pos, island->islandAxis );
	mcgeom::Point nextPos;
	bool holeIsVisible = true;
	while( true ) {
		// Try to move one square
		nextPos = pos;
		moveCoordsInDir( island, nextPos.v, dir );

		if( bld.isEdgeFlagged( nextPos, dir ) )
			break; // Hit the other side!

		// Does the map exist here?
		MCMap::Column nextCol;
		if( !map->getColumn( nextPos.x, nextPos.y, nextCol ) ) {
			holeIsVisible = false;
			goto its_a_hole;
		}

		// Is the block visible?
		if( island->checkVisibility | island->checkFacingSameId ) {
			unsigned short facingId;
			if( island->zax == 2 ) {
				facingId = (unsigned short)nextCol.getId( nextPos.z + island->zd );
			} else {
				nextPos.v[island->zax] += island->zd;
				if( !map->getBlockID( nextPos.x, nextPos.y, nextPos.z, facingId ) ) {
					// Hit the outside - should never happen in current maps
					holeIsVisible = false;
					goto its_a_hole;
				}
				nextPos.v[island->zax] -= island->zd;
			}
			holeIsVisible = !island->checkVisibility || !bld.getBlockDesc()->getSolidity( facingId, island->islandAxis );
			if( (island->checkFacingSameId && facingId == island->origin.block.id) ||
				!holeIsVisible )
				// Invisible - hole!
				goto its_a_hole;
		}

		// Does the next block have the same id?
		if( nextCol.getId( nextPos.z ) != island->origin.block.id )
			goto its_a_hole;

		// Complex island check
		if( island->continueIsland ) {
			mcgeom::InstanceContext nextBlock;
			nextBlock.block.pos = nextPos;
			nextBlock.block.id = (unsigned short)nextCol.getId( nextPos.z );
			nextBlock.block.data = (unsigned short)nextCol.getData( nextPos.z );
			nextBlock.sides[4].id = (unsigned short)(nextPos.z > bld.getExtents()->minz ? nextCol.getId( nextPos.z - 1 ) : 1u);
			nextBlock.sides[5].id = (unsigned short)(nextPos.z < bld.getExtents()->maxz ? nextCol.getId( nextPos.z + 1 ) : 1u);
			if( !island->continueIsland( island, &nextBlock ) )
				goto its_a_hole;
		}

		// Not a hole
		pos = nextPos;
		bld.markDone( pos, island->islandAxis );
	}

	//bld.markDone( pos, island->islandAxis );
	return;

its_a_hole:
	// It's a hole!!
	IslandHole *hole = bld.newHole( holeIsVisible );
	hole->insidePoint() = nextPos;
	scanContourAndFlag( bld, island, map, pos, (dir+1)&3, hole->blocks(), hole->points() );
}

static void searchForHoles( MeshBuilder &bld, mcgeom::IslandDesc *island, MCMap *map, const std::vector< mcgeom::Point > &contourBlocks ) {
	assert( contourBlocks.size() > 1 );

	mcgeom::Point pos = contourBlocks.back();

	for( unsigned i = 0; i < contourBlocks.size(); i++ ) {
		const mcgeom::Point& dest = contourBlocks[i];

		unsigned dir;
		if( pos.v[island->yax] == dest.v[island->yax] ) {
			dir = (pos.v[island->xax] > dest.v[island->xax]) == (island->xd > 0) ? 0 : 2;
		} else {
			// To anyone debugging, if you hit this assert, PREPARE FOR MISERY!
			// This means that something has gone wrong with the island generation
			// such that an island contour has been created which does not end where
			// it began. The causes for this are innumerate and all hard to trace.
			// For example, if an island is initiated not on the periphery of the
			// island (e.g. due to incorrect face flags in MeshBuilder::blockInfo),
			// this will happen. Happy bug hunting!
			assert( pos.v[island->xax] == dest.v[island->xax] );
			dir = (pos.v[island->yax] > dest.v[island->yax]) == (island->yd > 0) ? 1 : 3;
		}
		
		holeScan( bld, island, map, pos, (dir + 1) & 3 );
		do {
			moveCoordsInDir( island, pos.v, dir );
			holeScan( bld, island, map, pos, (dir + 1) & 3 );
		} while( pos != dest );
	}
}

static void unflagContour( MeshBuilder &bld, mcgeom::IslandDesc *island, const std::vector< mcgeom::Point > &contourBlocks, bool mark = true ) {
	assert( contourBlocks.size() > 1 );

	for( unsigned i = 0; i < contourBlocks.size(); i++ ) {
		mcgeom::Point pos = i ? contourBlocks[i-1] : contourBlocks.back();
		const mcgeom::Point& dest = contourBlocks[i];

		unsigned dir;
		if( pos.v[island->yax] == dest.v[island->yax] ) {
			dir = (pos.v[island->xax] > dest.v[island->xax]) == (island->xd > 0) ? 0 : 2;
		} else {
			dir = (pos.v[island->yax] > dest.v[island->yax]) == (island->yd > 0) ? 1 : 3;
		}
		
		bld.unflag( pos );
		if( mark )
			bld.markDone( pos, island->islandAxis );
		do {
			moveCoordsInDir( island, pos.v, dir );
			bld.unflag( pos );
			if( mark )
				bld.markDone( pos, island->islandAxis );
		} while( pos != dest );
	}
}

static void generateMCMapIslands( MeshBuilder &bld, mcgeom::BlockGeometry *geom, mcgeom::IslandDesc *island, MCMap *map ) {
	std::vector< mcgeom::Point > contourBlocks;
	std::vector< mcgeom::Point > contourPoints;
	std::vector< std::vector< mcgeom::BlockData > > holes;

	mcgeom::GeometryCluster *cluster = bld.getGeometryCluster( island->origin.block.id );
	mcgeom::BlockData oriBlock = island->origin.block;
	unsigned nextIslandIndex = 0;
	for( unsigned dir = 0; dir < 6; dir++ ) {
		if( bld.isDone( island->origin.block.pos, dir ) )
			continue;
		
		island->islandAxis = dir;
		island->zax = dir >> 1;
		island->zd = dir&1 ? 1 : -1;
		switch( dir ) {
		case 0:
			island->xax = 2;
			island->xd = -1;
			island->yax = 1;
			island->yd = -1;
			break;
		case 1:
			island->xax = 1;
			island->xd = -1;
			island->yax = 2;
			island->yd = -1;
			break;
		case 2:
			island->xax = 0;
			island->xd = -1;
			island->yax = 2;
			island->yd = -1;
			break;
		case 3:
			island->xax = 2;
			island->xd = -1;
			island->yax = 0;
			island->yd = -1;
			break;
		case 4:
			island->xax = 0;
			island->xd = 1;
			island->yax = 1;
			island->yd = -1;
			break;
		case 5:
			island->xax = 0;
			island->xd = -1;
			island->yax = 1;
			island->yd = -1;
			break;
		}

		island->xd1 = island->xd;
		island->yd1 = island->yd;
		island->xslope = 0;
		island->yslope = 0;
		island->origin.block = oriBlock;
		island->continueIsland = NULL;
		island->checkVisibility = true;
		island->checkFacingSameId = true;
		island->curCluster = cluster;
		island->islandIndex = nextIslandIndex;

		unsigned mode = geom->beginIsland( island );
		if( island->checkVisibility && island->origin.sides[dir].solid )
			continue;
		if( island->checkFacingSameId && island->origin.sides[dir].id == island->origin.block.id )
			continue;
		bool markDone = true;
		nextIslandIndex = 0;
		if( mode ) {
			if( mode & mcgeom::BlockGeometry::ISLAND_CANCEL )
				continue;
			if( mode & mcgeom::BlockGeometry::ISLAND_LOCK_X )
				island->xd *= 99999;
			if( mode & mcgeom::BlockGeometry::ISLAND_LOCK_Y )
				island->yd *= 99999;
			if( mode & mcgeom::BlockGeometry::ISLAND_REPEAT ) {
				assert( mode & mcgeom::BlockGeometry::ISLAND_SINGLE );
				dir--;
				markDone = false;
				nextIslandIndex = island->islandIndex + 1;
			}
		}

		island->checkVisibility &= !bld.getBlockDesc()->shouldHighlight( island->origin.block.id );

		if( scanContourAndFlag( bld, island, map, island->origin.block.pos, 0, contourBlocks, contourPoints ) ) {
			if( contourBlocks.size() > 1 ) {
				if( contourBlocks.size() > 2 ) {
					// The island has an interior - search for holes
					searchForHoles( bld, island, map, contourBlocks );
					for( std::list< IslandHole >::const_iterator it = bld.getHoles().begin(); it != bld.getHoles().end(); ++it )
						searchForHoles( bld, island, map, it->blocks() );
					// Clean up
					unflagContour( bld, island, contourBlocks );
					for( std::list< IslandHole >::const_iterator it = bld.getHoles().begin(); it != bld.getHoles().end(); ++it )
						unflagContour( bld, island, it->blocks() );

					bld.transformHolesToLocalSpace();

					island->nHoles = (unsigned)bld.getHoles().size();
					if( island->nHoles ) {
						island->nHoles = bld.gatherHoleContourPoints( island->holeContourEnd, island->holeContourPoints, island->holeInsidePoint );
						bld.clearHoles();
					}
				} else {
					island->nHoles = 0;
					unflagContour( bld, island, contourBlocks, markDone );
				}

				bld.toLocalSpace( contourBlocks );
				bld.toLocalSpace( contourPoints );
				bld.toLocalSpace( island->origin.block.pos );

				island->nContourBlocks = (unsigned)contourBlocks.size();
				island->contourBlocks = &contourBlocks[0];
				island->nContourPoints = (unsigned)contourPoints.size();
				island->contourPoints = &contourPoints[0];
				geom->emitIsland( cluster, island );

				if( island->nHoles ) {
					delete[] island->holeContourEnd;
					delete[] island->holeContourPoints;
					delete[] island->holeInsidePoint;
				}
			} else {
				// Single square - mark as done and clear flags
				if( markDone )
					bld.markDone( island->origin.block.pos, dir );
				bld.unflag( island->origin.block.pos );

				bld.toLocalSpace( contourBlocks );
				bld.toLocalSpace( contourPoints );
				bld.toLocalSpace( island->origin.block.pos );

				island->nHoles = 0;
				island->nContourBlocks = (unsigned)contourBlocks.size();
				island->contourBlocks = &contourBlocks[0];
				island->nContourPoints = (unsigned)contourPoints.size();
				island->contourPoints = &contourPoints[0];

				geom->emitIsland( cluster, island );
			}
		}

		contourBlocks.clear();
		contourPoints.clear();
		island->origin.block.pos = oriBlock.pos;
	}
}

static void glowAreaAround( MeshBuilder &bld, int x, int y, int z ) {
	const unsigned LIGHT_LEVEL[4] = { 15, 14, 12, 10 };
	for( int xp = x - 1; xp <= x + 1; xp++ ) {
		for( int yp = y - 1; yp <= y + 1; yp++ ) {
			for( int zp = z - 1; zp <= z + 1; zp++ ) {
				if( bld.getLightExtents()->contains( xp, yp, zp ) ) {
					unsigned lv = (xp==x ? 0u : 1u) + (yp==y ? 0u : 1u) + (zp==z ? 0u : 1u);
					bld.setLightingAt( xp, yp, zp, LIGHT_LEVEL[lv], 0 );
				}
			}
		}
	}
}

static void lightMapColumn( MeshBuilder &bld, int x, int y, const MCMap::Column &col, const MCMap::Column *sides ) {
	const unsigned AO_HARSHNESS = 4;

	for( int z = bld.getExtents()->minz; z <= bld.getExtents()->maxz; z++ ) {
		unsigned id = col.getId( z );
		unsigned blockLight = bld.getBlockDesc()->enableBlockLighting() ? col.getBlockLight( z ) : 0u;
		unsigned skyLight = col.getSkyLight( z );
		bld.setLightingAt( x, y, z, blockLight, skyLight );

		if( id > 0 ) {
			if( bld.getBlockDesc()->shouldHighlight( id ) ) {
				glowAreaAround( bld, x, y, z );
			} else {
				// Un-harshen the lighting by letting the light 'seep' into blocks where
				// it doesn't matter
				if( blockLight == 0 && skyLight == 0 ) {
					unsigned maxBlockLight = 0, maxSkyLight = 0;
					for( unsigned i = 0; i < 4; i++ ) {
						if( !bld.getBlockDesc()->getSolidity( sides[i].getId( z ), i ) ) {
							unsigned light = sides[i].getBlockLight( z );
							if( light > maxBlockLight )
								maxBlockLight = light;
							light = sides[i].getSkyLight( z );
							if( light > maxSkyLight )
								maxSkyLight = light;
						}
					}
					if( /*z > col.minZ &&*/ !bld.getBlockDesc()->getSolidity( col.getId( z-1 ), 4 ) ) {
						unsigned light = col.getBlockLight( z-1 );
						if( light > maxBlockLight )
							maxBlockLight = light;
						light = col.getSkyLight( z-1 );
						if( light > maxSkyLight )
							maxSkyLight = light;
					}
					if( /*z < col.maxZ &&*/ !bld.getBlockDesc()->getSolidity( col.getId( z+1 ), 5 ) ) {
						unsigned light = col.getBlockLight( z+1 );
						if( light > maxBlockLight )
							maxBlockLight = light;
						light = col.getSkyLight( z+1 );
						if( light > maxSkyLight )
							maxSkyLight = light;
					}

					if( !bld.getBlockDesc()->enableBlockLighting() )
						maxBlockLight = 0;
					if( maxBlockLight <= AO_HARSHNESS ) {
						maxBlockLight = 0;
					} else {
						maxBlockLight -= AO_HARSHNESS;
					}
					if( maxSkyLight <= AO_HARSHNESS ) {
						maxSkyLight = 0;
					} else {
						maxSkyLight -= AO_HARSHNESS;
					}

					bld.setLightingAt( x, y, z, maxBlockLight, maxSkyLight );
				}
			
				// HACK - Always let light seep into non-solid blocks (especially slabs) from above
				if( bld.getBlockDesc()->enableBlockLighting() && bld.getBlockDesc()->getSolidity( id, 4 ) && z < col.maxZ ) {
					bld.setLightingAt( x, y, z,
						std::max( AO_HARSHNESS, col.getBlockLight( z+1 ) ) - AO_HARSHNESS,
						std::max( AO_HARSHNESS, col.getSkyLight( z+1 ) ) - AO_HARSHNESS );
				}
			}
		} else {
			// This is air
			if( bld.getBlockDesc()->getDefAirSkyLightOverride() )
				// Override the skylight (for the End)
				bld.setLightingAt( x, y, z, blockLight, bld.getBlockDesc()->getDefAirSkyLight() );
		}
	}
}

static void lightMapColumn( MeshBuilder &bld, MCMap *map, int x, int y, unsigned short *allOne ) {
	MCMap::Column col;
	if( map->getColumn( x, y, col ) ) {
		MCMap::Column sides[4];
		bool sideExists[4];
		sideExists[0] = map->getColumn( x-1, y, sides[0] );
		sideExists[1] = map->getColumn( x+1, y, sides[1] );
		sideExists[2] = map->getColumn( x, y-1, sides[2] );
		sideExists[3] = map->getColumn( x, y+1, sides[3] );
		for( unsigned i = 0; i < 4; i++ ) {
			if( !sideExists[i] ) {
				sides[i].id = allOne;
				sides[i].minZ = col.minZ;
			}
		}

		lightMapColumn( bld, x, y, col, &sides[0] );
	}
}

static void outputSignsFromMap( MeshBuilder &bld, MCMap *map, int minx, int maxx, int miny, int maxy ) {
	mcgeom::SignTextGeometry *signGeom = (mcgeom::SignTextGeometry*)bld.getBlockDesc()->getGeometry( SIGNTEXT_BLOCK_ID );
	mcgeom::GeometryCluster *cluster = bld.getGeometryCluster( SIGNTEXT_BLOCK_ID );
	MCMap::SignList signs;
	map->getSignsInArea( minx, maxx, miny, maxy, signs );
	char text[256];
	for( MCMap::SignList::const_iterator it = signs.begin(); it != signs.end(); ++it ) {
		char *t = &text[0];
		unsigned n = sizeof(text);
		for( unsigned i = 0; i < 4; i++ ) {
			const wchar_t *s = it->text[i];
			while( n && *s ) {
				*t++ = (char)*s++;
				n--;
			}
			if( n ) {
				*t++ = '\n';
				n--;
			}
		}
		t[-1] = '\0';

		if( n == sizeof(text) - 4 )
			continue; // Skip empty signs
	
		mcgeom::Point p = {{{ it->x, it->y, it->z }}};
		bld.toLocalSpace( p );

		jMatrix mat;
		//jMatrixSetIdentity( &mat );
		jVec3Zero( &mat.up );
		jVec3Zero( &mat.right );
		//jVec3Set( &mat.right, 0.0f, 0.0f, 0.0f );
		jVec3Set( &mat.fwd, 0.0f, 0.0f, -8.0f );
		jVec3Set( &mat.pos, (float)(p.x * 16), (float)(p.y * 16), (float)(p.z * 16) );
		
		if( it->onWall ) {
			//assert( it->orientation >= 2 );
			
			unsigned ori = (it->orientation - 2) & 3;
			unsigned axis = ori >> 1;
			mat.right.v[axis ^ 1] = (ori & 1) ^ axis ? 16.0f : -16.0f;
			mat.pos.z += 12.0f;
			mat.pos.v[axis] += ori & 1 ? 2.1f : 13.9f;
			mat.pos.v[axis^1] += 8.0f;

			signGeom->emitSign( cluster, &mat, text );
		} else {
			// TODO: Floor signs
			//mat.pos.z += 16.0f;
		}
		
		//signGeom->emitSign( cluster, &mat, text );
	}
}

MCWorldMesh *MCWorldMesh::generateFromMCMap( MCMap *map, const MCBlockDesc *blocks, Extents &hull, Extents &ltext ) {
	unsigned short *allOne = new unsigned short[hull.maxz - hull.minz + 1];
	for( int z = hull.minz; z <= hull.maxz; z++ )
		allOne[z-hull.minz] = 1;

	MeshBuilder *pbld = new MeshBuilder( ltext, hull, blocks );
	MeshBuilder &bld = *pbld;

	for( int x = ltext.minx; x <= ltext.maxx; x++ ) {
		lightMapColumn( bld, map, x, ltext.miny, allOne );
		lightMapColumn( bld, map, x, ltext.maxy, allOne );
	}

	for( int y = ltext.miny; y <= ltext.maxy; y++ ) {
		lightMapColumn( bld, map, ltext.minx, y, allOne );
		lightMapColumn( bld, map, ltext.maxx, y, allOne );
	}

	for( int x = hull.minx; x <= hull.maxx; x++ ) {
		for( int y = hull.miny; y <= hull.maxy; y++ ) {
			MCMap::Column col;
			if( map->getColumn( x, y, col ) ) {
				MCMap::Column sides[4];
				bool sideExists[4];
				sideExists[0] = map->getColumn( x-1, y, sides[0] );
				sideExists[1] = map->getColumn( x+1, y, sides[1] );
				sideExists[2] = map->getColumn( x, y-1, sides[2] );
				sideExists[3] = map->getColumn( x, y+1, sides[3] );
				for( unsigned i = 0; i < 4; i++ ) {
					if( !sideExists[i] ) {
						sides[i].id = allOne;
						sides[i].minZ = col.minZ;
						sides[i].maxZ = col.maxZ;
					}
				}

				lightMapColumn( bld, x, y, col, &sides[0] );

				int stopatz = std::min( hull.maxz, col.maxZ );
				for( int z = std::max( hull.minz, col.minZ ); z <= stopatz; z++ ) {
					unsigned id = col.getId( z );
					mcgeom::BlockGeometry *geom = blocks->getGeometry( id );
					if( geom ) {
						// First check the solidity of all adjacent blocks
						mcgeom::IslandDesc ctx;
						memset(&ctx, 0, sizeof(ctx));
						for( unsigned i = 0; i < 4; i++ )
							ctx.origin.sides[i].solid = !!blocks->getSolidity( ctx.origin.sides[i].id = (unsigned short)sides[i].getId(z), i );
						ctx.origin.sides[4].solid = !!blocks->getSolidity( ctx.origin.sides[4].id = (unsigned short)col.getId(z-1), 4 );
						ctx.origin.sides[5].solid = !!blocks->getSolidity( ctx.origin.sides[5].id = (unsigned short)col.getId(z+1), 5 );

						if( blocks->shouldHighlight( id ) ) {
							for( unsigned i = 0; i < 4; i++ )
								ctx.origin.sides[i].solid = !sideExists[i];
							ctx.origin.sides[4].solid = z <= col.minZ;
							ctx.origin.sides[5].solid = 0;
						} else {
							if( ctx.origin.sides[0].solid && ctx.origin.sides[1].solid && ctx.origin.sides[2].solid
							 && ctx.origin.sides[3].solid && ctx.origin.sides[4].solid && ctx.origin.sides[5].solid ) {
								// All adjacent blocks are solid
								// There is no way to see this block
								continue;
							}
						}

						// The block is potentially visible
						ctx.origin.sides[0].outside = x == hull.minx;
						ctx.origin.sides[1].outside = x == hull.maxx;
						ctx.origin.sides[2].outside = y == hull.miny;
						ctx.origin.sides[3].outside = y == hull.maxy;
						ctx.origin.sides[4].outside = z == hull.minz;
						ctx.origin.sides[5].outside = z == hull.maxz;
						ctx.origin.block.id = (unsigned short)id;
						ctx.origin.block.data = (unsigned short)col.getData( z );
						ctx.origin.block.pos.x = x; ctx.origin.block.pos.y = y; ctx.origin.block.pos.z = z;
						mcgeom::Point worldSpacePos = ctx.origin.block.pos;
						bld.toLocalSpace( ctx.origin.block.pos );
						if( geom->beginEmit( bld.getGeometryCluster( id ), &ctx.origin ) ) {
							ctx.origin.block.pos = worldSpacePos;
							generateMCMapIslands( bld, geom, &ctx, map );
						}
					}
				}
			}
		}
	}
	
	delete[] allOne;

	// Output sign text
	outputSignsFromMap( bld, map, hull.minx, hull.maxx, hull.miny, hull.maxy );

	// Get the biome coordinates
	bld.setBiomeCoords( blocks->getBiomes()->readBiomeCoords( map, bld.getLightExtents()->minx, bld.getLightExtents()->maxx, bld.getLightExtents()->miny, bld.getLightExtents()->maxy ) );

	return bld.buildFinalWorldMesh();
}



void MCWorldMesh::finalizeLoad() {
	assert( bld );
	bld->finalizeMesh( this );
}

void MCWorldMesh::renderOpaque( mcgeom::RenderContext *ctx ) {
	assert( !bld );

	if( hasOpaque ) {
		beginRender( ctx );
		jVec3 oldViewPos;
		jVec3Copy( &oldViewPos, &ctx->viewPos );
		ctx->viewPos.x -= (float)origin[0];
		ctx->viewPos.y -= (float)origin[1];
		ctx->viewPos.z -= (float)origin[2];
		ctx->vertexSize += vtxMem;
		ctx->indexSize += idxMem;
		ctx->texSize += texMem;
		
		unsigned char *cursor = (unsigned char*)meta;
		unsigned char *end = cursor + opaqueEnd;
		do {
			MeshMeta *mesh = (MeshMeta*)cursor;

			cursor += sizeof( MeshMeta );
			mesh->geom->render( (void*&)cursor, ctx );
		} while( cursor < end );

		jVec3Copy( &ctx->viewPos, &oldViewPos );
		endRender();
	}
}

void MCWorldMesh::renderTransparent( mcgeom::RenderContext *ctx ) {
	assert( !bld );

	if( hasTransparent ) {
		beginRender( ctx );
		jVec3 oldViewPos;
		jVec3Copy( &oldViewPos, &ctx->viewPos );
		ctx->viewPos.x -= (float)origin[0];
		ctx->viewPos.y -= (float)origin[1];
		ctx->viewPos.z -= (float)origin[2];
		
		unsigned char *cursor = (unsigned char*)meta;
		unsigned char *end = cursor + transpEnd;
		cursor += opaqueEnd;
		do {

			MeshMeta *mesh = (MeshMeta*)cursor;

			cursor += sizeof( MeshMeta );
			mesh->geom->render( (void*&)cursor, ctx );
		} while( cursor < end );

		jVec3Copy( &ctx->viewPos, &oldViewPos );
		endRender();
	}
}

void MCWorldMesh::beginRender( mcgeom::RenderContext *ctx ) {
	glBindBuffer( GL_ARRAY_BUFFER, vtx_vbo );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, idx_vbo );

	glActiveTexture( GL_TEXTURE1 );
	glEnable( GL_TEXTURE_3D );
	glBindTexture( GL_TEXTURE_3D, lightTex );
	

	glMatrixMode( GL_TEXTURE );
	glLoadIdentity();
	glTranslated( 0.5, 0.5, 0.5 );
	glScaled( lightTexScale[0], lightTexScale[1], lightTexScale[2] );
	
	glActiveTexture( GL_TEXTURE0 );

	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	glTranslated( origin[0], origin[1], origin[2] );
	glScaled( 1.0/16.0, 1.0/16.0, 1.0/16.0 );

	ctx->biomeTextures = biomeTex;
}

void MCWorldMesh::endRender() {
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );

	glActiveTexture( GL_TEXTURE1 );
	glDisable( GL_TEXTURE_3D );
	glBindTexture( GL_TEXTURE_3D, 0 );

	glMatrixMode( GL_MODELVIEW );
	glPopMatrix();

	glMatrixMode( GL_TEXTURE );
	glLoadIdentity();
	glActiveTexture( GL_TEXTURE0 );
	glLoadIdentity();
}


MCWorldMeshGroup::MCWorldMeshGroup()
: firstMesh(NULL)
, vtxMem(0)
, idxMem(0)
, texMem(0)
, cost(0)
{
}

MCWorldMeshGroup::~MCWorldMeshGroup() {
	MCWorldMesh *mesh = firstMesh;
	while( mesh ) {
		MCWorldMesh *nextMesh = mesh->nextMesh;
		delete mesh;
		mesh = nextMesh;
	}
}

MCWorldMeshGroup *MCWorldMeshGroup::generateFromMCMap( MCMap *map, const MCBlockDesc *blocks, Extents &ext ) {
	MCWorldMeshGroup *wmeshg = new MCWorldMeshGroup;
	if( !map->getRegions()->isAnvil() ) {
		Extents ltext( ext.minx - 1, ext.maxx + 1, ext.miny - 1, ext.maxy + 1, ext.minz, ext.maxz );
		wmeshg->firstMesh = MCWorldMesh::generateFromMCMap( map, blocks, ext, ltext );
	} else {
		Extents hull = ext;
		map->getExtentsWithin( ext.minx, ext.maxx, ext.miny, ext.maxy, ext.minz, ext.maxz );
		
		// TODO: More intelligent shrinking and subdivision of the volume
		if( ext.maxz <= 127 )
			hull.maxz = 127;

		Extents ltext( hull.minx - 1, hull.maxx + 1, hull.miny - 1, hull.maxy + 1, hull.minz, hull.maxz );
		wmeshg->firstMesh = MCWorldMesh::generateFromMCMap( map, blocks, hull, ltext );
	}
	return wmeshg;
}
	
void MCWorldMeshGroup::finalizeLoad() {
	for( MCWorldMesh *mesh = firstMesh; mesh; mesh = mesh->nextMesh ) {
		mesh->finalizeLoad();
		vtxMem += mesh->vtxMem;
		idxMem += mesh->idxMem;
		texMem += mesh->texMem;
		cost += mesh->cost;
	}
}

bool MCWorldMeshGroup::isEmpty() const {
	for( MCWorldMesh *mesh = firstMesh; mesh; mesh = mesh->nextMesh )
		if( !mesh->isEmpty() )
			return false;
	return true;
}

void MCWorldMeshGroup::renderOpaque( mcgeom::RenderContext *ctx ) {
	for( MCWorldMesh *mesh = firstMesh; mesh; mesh = mesh->nextMesh )
		mesh->renderOpaque( ctx );
}

void MCWorldMeshGroup::renderTransparent( mcgeom::RenderContext *ctx ) {
	for( MCWorldMesh *mesh = firstMesh; mesh; mesh = mesh->nextMesh )
		mesh->renderTransparent( ctx );
}
