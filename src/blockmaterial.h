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

#ifndef BLOCKMATERIAL_H
#define BLOCKMATERIAL_H

#include <cstdlib>
#include <vector>
#include "jmath.h"
#include "luaobject.h"

struct triangulateio;
class EihortShader;
class LightModel;

#define BLOCKGEOMETRY_META "BlockGeometry"

namespace mcgeom {

class BlockGeometry;

struct Point {
	union {
#ifdef __GNUC__
    __extension__
#endif
		struct {
			int x, y, z;
		};
		int v[3];
	};

	inline bool operator==( const Point& other ) const {
		return x==other.x && y==other.y && z==other.z;
	}
	inline bool operator!=( const Point& other ) const {
		return !(*this == other);
	}
};

namespace RenderGroup {
	const unsigned FIRST = 500;
	const unsigned OPAQUE = 1000;
	const unsigned TRANSPARENT = 2000;
	const unsigned LAST = 3000;
}

struct SideContext {
	unsigned short id;
	bool solid;
	bool outside; // true iff the side is outside the island-starting area
};

struct BlockData {
	Point pos;
	unsigned short id;
	unsigned short data;
};

struct InstanceContext {
	BlockData block;
	SideContext sides[6];
	unsigned cookie;
};

struct RenderContext {
	jVec3 viewPos;
	unsigned renderedTriCount;
	unsigned vertexSize, indexSize, texSize;
	EihortShader *shader;
	unsigned *biomeTextures;
	LightModel *lightModels;
	bool enableBlockLighting;
};

class GeometryCluster;
struct IslandDesc {
	InstanceContext origin;
	unsigned nContourBlocks;
	Point *contourBlocks;
	unsigned nContourPoints;
	Point *contourPoints;
	unsigned nHoles;
	unsigned *holeContourEnd;
	Point *holeInsidePoint;
	Point *holeContourPoints;

	unsigned islandAxis;
	unsigned islandIndex; // for ISLAND_REPEATed islands
	unsigned xax, yax, zax; // Island axes. zax is perpendicular to the island plane
	int xd, yd, zd; // Axis direction: deltas when searching for islands
	int xslope, yslope; // The z delta when moving in the x or y directions
	int xd1, yd1; // Axis direction: 1 or -1

	// To be filled in by beginIsland
	bool checkVisibility, checkFacingSameId;
	typedef bool (*ComplexContinueIsland)( IslandDesc *island, const InstanceContext *nextBlock );
	ComplexContinueIsland continueIsland;
	unsigned cookie;
	void *pcookie;
	GeometryCluster *curCluster;
};

class GeometryStream {
public:
	// NOTE: This class originally was designed to store vertex and index info for
	// a mesh, but has since been hijacked as a generic data buffer for metadata
	// as well as mesh info.

	GeometryStream()
		: verts(NULL), vertSize(0), vertCapacity(0), vertCount(0)
	{ }
	~GeometryStream() {
		free( verts );
	}

	const void *getVertices() const { return verts; }
	void *getVertices() { return verts; }
	unsigned getVertSize() const { return vertSize; }
	unsigned getVertCount() const { return vertCount; }

	const unsigned *getIndices() const { return &indices[0]; }
	unsigned *getIndices() { return &indices[0]; }
	unsigned getIndexBase() const { return getVertCount(); }
	unsigned getTriCount() const { return (unsigned)indices.size()/3; }
	inline void emitTriangle( unsigned i, unsigned j, unsigned k ) {
		indices.push_back( (unsigned)i );
		indices.push_back( (unsigned)j );
		indices.push_back( (unsigned)k );
	}
	inline void emitQuad( unsigned i, unsigned j, unsigned k, unsigned l ) {
		indices.push_back( (unsigned)i );
		indices.push_back( (unsigned)j );
		indices.push_back( (unsigned)k );
		indices.push_back( (unsigned)i );
		indices.push_back( (unsigned)k );
		indices.push_back( (unsigned)l );
	}

	template< typename T >
	inline void emitVertex( const T &src ) { emitVertex( &src, sizeof(T) ); }
	void emitVertex( const void *src, unsigned size );

	inline void alignVertices( unsigned alignment = 4 ) {
		const unsigned char PADDING[] = { 0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad };
		unsigned remainder = vertSize & (alignment-1);
		if( remainder )
			emitVertex( &PADDING[0], alignment - remainder );
	}

private:
	void ensureVertCap( unsigned cap );

	void *verts;
	unsigned vertSize;
	unsigned vertCapacity;
	unsigned vertCount;
	std::vector<unsigned> indices;
};

class GeometryCluster {
public:
	virtual bool destroyIfEmpty() = 0;
	virtual void finalize( GeometryStream *meta, GeometryStream *vtx, GeometryStream *idx ) = 0;

protected:
	GeometryCluster();
	virtual ~GeometryCluster();

	static unsigned getSmallestIndexSize( unsigned nVerts );
	// Returns the smallest index size
	static unsigned compressIndexBuffer( unsigned *indices, unsigned nVerts, unsigned nIndices );
	static unsigned indexSizeToGLType( unsigned size );
};

class MetaGeometryCluster : public GeometryCluster {
public:
	explicit MetaGeometryCluster( BlockGeometry *geom );

	virtual bool destroyIfEmpty();
	virtual void finalize( GeometryStream *meta, GeometryStream *vtx, GeometryStream *idx  );

	inline GeometryStream *getStream() { return &str; }

	inline void incEntry() { n++; }

protected:
	~MetaGeometryCluster();

	GeometryStream str;
	BlockGeometry *geom;
	unsigned n;
};

template< typename Extra >
class SingleStreamGeometryClusterEx : public GeometryCluster {
public:
	explicit SingleStreamGeometryClusterEx( BlockGeometry *geom );
	SingleStreamGeometryClusterEx( BlockGeometry *geom, Extra ex );

	virtual bool destroyIfEmpty();
	virtual void finalize( GeometryStream *meta, GeometryStream *vtx, GeometryStream *idx  );

	inline GeometryStream *getStream() { return &str; }

	struct Meta {
		unsigned vtx_offset, idx_offset;
		unsigned nTris;
		unsigned nVerts;
		unsigned idxType;
	};

protected:
	~SingleStreamGeometryClusterEx();

	void emitExtra( GeometryStream *meta );

	Extra extra;
	GeometryStream str;
	BlockGeometry *geom;
};

struct EmptyStruct { };
typedef SingleStreamGeometryClusterEx<EmptyStruct> SingleStreamGeometryCluster;

template< unsigned N >
class MultiStreamGeometryCluster : public GeometryCluster {
public:
	explicit MultiStreamGeometryCluster( BlockGeometry *geom );

	virtual bool destroyIfEmpty();
	virtual void finalize( GeometryStream *meta, GeometryStream *vtx, GeometryStream *idx  );

	inline GeometryStream *getStream( unsigned i ) { return &str[i]; }
	void setCutoutVector( unsigned i, const jVec3 *cutout ) {
		jVec3Copy( &cutoutVectors[i], cutout );
	}

	struct Meta1 {
		unsigned n;
	};

	struct Meta2 {
		unsigned vtx_offset, idx_offset;
		unsigned idxType;
		unsigned nTris;
		unsigned nVerts;
		unsigned dir;
		jPlane cutoutPlane;
	};

protected:
	~MultiStreamGeometryCluster();

	GeometryStream str[N];
	jVec3 cutoutVectors[N];
	BlockGeometry *geom;
};

typedef MultiStreamGeometryCluster<6> SixSidedGeometryCluster;

class MultiGeometryCluster : public GeometryCluster {
public:
	MultiGeometryCluster();

	virtual bool destroyIfEmpty();
	virtual void finalize( GeometryStream *meta, GeometryStream *vtx, GeometryStream *idx  );

	GeometryCluster *getCluster( unsigned i );
	void newCluster( unsigned i, GeometryCluster *cluster );

	void storeContinueIsland( const IslandDesc *island );
	bool callContinueIsland( IslandDesc *island, const InstanceContext *nextBlock );

	inline void setCookie( unsigned c ) { cookie = c; }
	inline unsigned getCookie() const { return cookie; }

private:
	~MultiGeometryCluster();

	std::vector< GeometryCluster* > clusters;

	unsigned cookie;
	IslandDesc::ComplexContinueIsland ciFn;
	unsigned ciCookie;
	void *cipCookie;
	GeometryCluster *subCluster;
};

// ---------------------------------------------------------------------------
class BlockGeometry : public LuaObject {
public:
	virtual ~BlockGeometry();

	enum IslandMode {
		ISLAND_NORMAL = 0,
		ISLAND_LOCK_X = 0x1,
		ISLAND_LOCK_Y = 0x2,
		ISLAND_SINGLE = 0x3,

		// Bypasses the "done" flagging and repeats islands until ISLAND_CANCEL is sent
		// Cannot be used with un-ISLAND_LOCK_*'d islands
		// Designed primarily to allow rectangular islands in different dimensions (fences)
		ISLAND_REPEAT = 0x4000,
		ISLAND_CANCEL = 0x8000
	};

	inline unsigned getRenderGroup() const { return rg; }
	inline void setRenderGroup( unsigned group ) { rg = group; }

	virtual void render( void *&meta, RenderContext *ctx );
	virtual GeometryCluster *newCluster() = 0;

	virtual bool beginEmit( GeometryCluster *out, InstanceContext *ctx ) = 0;
	virtual IslandMode beginIsland( IslandDesc *ctx );
	virtual void emitIsland( GeometryCluster *out, const IslandDesc *ctx );
	//virtual void exportOBJ( GeometryCluster *cluster );

	inline bool operator< ( const BlockGeometry& other ) {
		return rg < other.rg;
	}

	// Lua functions
	static int lua_setTexScale( lua_State *L );
	static int lua_renderGroupAdd( lua_State *L );
	static int createDataAdapter( lua_State *L );
	static int createRotatingAdapter( lua_State *L );
	static int createFaceBitAdapter( lua_State *L );
	static int createFacingAdapter( lua_State *L );
	static int createTopDifferentAdapter( lua_State *L );
	static int createOpaqueBlockGeometry( lua_State *L );
	static int createBrightOpaqueBlockGeometry( lua_State *L );
	static int createTransparentBlockGeometry( lua_State *L );
	static int createSquashedBlockGeometry( lua_State *L );
	static int createCompactedGeometry( lua_State *L );
	static int createMultiBlockInBlock( lua_State *L );
	static int createMultiCompactedGeometry( lua_State *L );
	static int createBiomeOpaqueGeometry( lua_State *L );
	static int createBiomeAlphaOpaqueGeometry( lua_State *L );
	static int createPortalGeometry( lua_State *L );
	static int createCactusGeometry( lua_State *L );
	static int createBiomeCactusGeometry( lua_State *L );
	static int createRailGeometry( lua_State *L );
	static int createDoorGeometry( lua_State *L );
	static int createStairsGeometry( lua_State *L );
	static int createRedstoneWireGeometry( lua_State *L );
	static int createTorchGeometry( lua_State *L );
	static int createFlowerGeometry( lua_State *L );
	static int createFlowerBiomeGeometry( lua_State *L );
	static int lua_destroy( lua_State *L );
	static void setupLua( lua_State *L );

protected:
	explicit BlockGeometry();

	unsigned rg;
};

class ForwardingMultiGeometryAdapter : public BlockGeometry {
public:
	ForwardingMultiGeometryAdapter();
	virtual ~ForwardingMultiGeometryAdapter();

	virtual GeometryCluster *newCluster();

	virtual bool beginEmit( GeometryCluster *out, InstanceContext *ctx );
	virtual void emitIsland( GeometryCluster *out, const IslandDesc *ctx );
	virtual IslandMode beginIsland( IslandDesc *ctx );

protected:
	virtual unsigned selectGeometry( const InstanceContext *ctx ) = 0;
	virtual unsigned selectGeometry( const IslandDesc *ctx );
	static bool continueIsland( IslandDesc *island, const InstanceContext *nextBlock );

	BlockGeometry *geoms[16];
};

class DataBasedMultiGeometryAdapter : public ForwardingMultiGeometryAdapter {
	// For wool, slabs, beds
public:
	DataBasedMultiGeometryAdapter( unsigned mask, BlockGeometry **geoms );
	virtual ~DataBasedMultiGeometryAdapter();

	virtual bool beginEmit( GeometryCluster *out, InstanceContext *ctx );
	virtual IslandMode beginIsland( IslandDesc *ctx );

protected:
	virtual unsigned selectGeometry( const InstanceContext *ctx );
	static bool continueIsland( IslandDesc *island, const InstanceContext *nextBlock );

	unsigned mask;
};

class RotatingMultiGeometryAdapter : public ForwardingMultiGeometryAdapter {
	// For pumpkins, jack-o-lanterns and beds
public:
	RotatingMultiGeometryAdapter( BlockGeometry **geoms, unsigned *sideToGeoms = NULL );
	virtual ~RotatingMultiGeometryAdapter();

private:
	virtual unsigned selectGeometry( const InstanceContext *ctx );
	virtual unsigned selectGeometry( const IslandDesc *ctx );

	unsigned sideToGeom[6];
};

class FaceBitGeometryAdapter : public ForwardingMultiGeometryAdapter {
	// For vines
public:
	FaceBitGeometryAdapter( BlockGeometry *geom );
	virtual ~FaceBitGeometryAdapter();

private:
	virtual unsigned selectGeometry( const InstanceContext *ctx );
	virtual unsigned selectGeometry( const IslandDesc *ctx );
};

class FacingMultiGeometryAdapter : public ForwardingMultiGeometryAdapter {
	// For furnaces, dispensers and ladders
public:
	FacingMultiGeometryAdapter( BlockGeometry *geom1, BlockGeometry *geom2 );
	virtual ~FacingMultiGeometryAdapter();

private:
	virtual unsigned selectGeometry( const InstanceContext *ctx );
	virtual unsigned selectGeometry( const IslandDesc *ctx );
};

class TopModifiedMultiGeometryAdapter : public ForwardingMultiGeometryAdapter {
	// For snowy grass
public:
	TopModifiedMultiGeometryAdapter( BlockGeometry *normal, BlockGeometry *modified, unsigned modifier );
	virtual ~TopModifiedMultiGeometryAdapter();

private:
	virtual unsigned selectGeometry( const InstanceContext *ctx );
	virtual unsigned selectGeometry( const IslandDesc *ctx );

	unsigned modifier;
};


class SolidBlockGeometry : public BlockGeometry {
public:
	explicit SolidBlockGeometry( unsigned tx, unsigned color = 0xffffffu );
	explicit SolidBlockGeometry( unsigned *tx, unsigned color = 0xffffffu );
	virtual ~SolidBlockGeometry();

	virtual void render( void *&meta, RenderContext *ctx );
	virtual GeometryCluster *newCluster();

	virtual bool beginEmit( GeometryCluster *out, InstanceContext *ctx );
	virtual IslandMode beginIsland( IslandDesc *ctx );
	virtual void emitIsland( GeometryCluster *out, const IslandDesc *ctx );

	inline void setColor( unsigned col ) { color = col; }
	inline void setTexScale( float x, float y ) { xTexScale = x; yTexScale = y; }

protected:
	struct REALVec {
		double v[2];
		void scaleToLenAA( double len ) {
			if( abs(v[0]) < 0.01 ) {
				v[1] = v[1] > 0.0 ? len : -len;
			} else {
				v[0] = v[0] > 0.0 ? len : -len;
			}
		}

		void add( const REALVec &other ) {
			v[0] += other.v[0];
			v[1] += other.v[1];
		}

		void sub( const REALVec &other ) {
			v[0] -= other.v[0];
			v[1] -= other.v[1];
		}
	};

	static void emitContour( GeometryStream *out, const IslandDesc *ctx, int offsetPx );
	static unsigned serializeContourPoints( const IslandDesc *ctx, unsigned nContourPoints, const Point *points, REALVec *outVec, int *segments, int baseSegIdx, std::vector<REALVec> &extraHoles );
	static void islandToTriangleIO( const IslandDesc *ctx, triangulateio *out );
	static void freeIslandTriangleIO( triangulateio *out );
	static void emitTriangulated( GeometryStream *out, const IslandDesc *ctx, triangulateio *tri, int offsetPx );
	static void emitQuad( GeometryStream *out, const IslandDesc *ctx, int offsetPx );
	static void emitQuad( GeometryStream *out, const IslandDesc *ctx, int *offsets );

	void solidBlockRender( void *&meta, RenderContext *ctx );
	void applyColor();

	struct Vertex {
		short pos[3];
	};

	unsigned tex[6];
	unsigned color;
	float xTexScale;
	float yTexScale;
};

typedef SolidBlockGeometry BasicSolidBlockGeometry;

class FoliageBlockGeometry : public SolidBlockGeometry {
public:
	FoliageBlockGeometry( unsigned tx, unsigned foliageTex );
	FoliageBlockGeometry( unsigned *tx, unsigned foliageTex );
	~FoliageBlockGeometry();

	virtual IslandMode beginIsland( IslandDesc *ctx );
	virtual void render( void *&meta, RenderContext *ctx );

protected:
	unsigned foliageTex;
};

class FoliageAlphaBlockGeometry : public FoliageBlockGeometry {
public:
	FoliageAlphaBlockGeometry( unsigned tx, unsigned foliageTex );
	FoliageAlphaBlockGeometry( unsigned *tx, unsigned foliageTex );
	~FoliageAlphaBlockGeometry();

	virtual void render( void *&meta, RenderContext *ctx );
};

class TransparentSolidBlockGeometry : public SolidBlockGeometry {
public:
	explicit TransparentSolidBlockGeometry( unsigned tx, unsigned color = 0xffffffu );
	explicit TransparentSolidBlockGeometry( unsigned *tx, unsigned color = 0xffffffu );
	virtual ~TransparentSolidBlockGeometry();

	virtual void render( void *&meta, RenderContext *ctx );
};

class PortalBlockGeometry : public TransparentSolidBlockGeometry {
public:
	PortalBlockGeometry( unsigned tx, unsigned color = 0xffffffu );
	~PortalBlockGeometry();

	virtual void emitIsland( GeometryCluster *out, const IslandDesc *ctx );
};

class CactusBlockGeometry : public SolidBlockGeometry {
public:
	CactusBlockGeometry( unsigned tx, int *offsets = NULL );
	CactusBlockGeometry( unsigned *tx, int *offsets = NULL );
	~CactusBlockGeometry();

	virtual GeometryCluster *newCluster();

	virtual void render( void *&meta, RenderContext *ctx );
	virtual IslandMode beginIsland( IslandDesc *ctx );
	virtual void emitIsland( GeometryCluster *out, const IslandDesc *ctx );

protected:
	int offsets[6];
};

class BiomeCactusBlockGeometry : public CactusBlockGeometry {
public:
	BiomeCactusBlockGeometry( unsigned tx, int channel, int *offsets = NULL );
	BiomeCactusBlockGeometry( unsigned *tx, int channel, int *offsets = NULL );
	~BiomeCactusBlockGeometry();

	virtual void render( void *&meta, RenderContext *ctx );

protected:
	int biomeChannel;
};

class FullBrightBlockGeometry : public SolidBlockGeometry {
	// Mostly useful for Lava
public:
	FullBrightBlockGeometry( unsigned tx, unsigned color = 0xffffffu );
	FullBrightBlockGeometry( unsigned *tx, unsigned color = 0xffffffu );
	~FullBrightBlockGeometry();

	virtual void render( void *&meta, RenderContext *ctx );
};

class SquashedSolidBlockGeometry : public SolidBlockGeometry {
	// Blocks that span the complete x/y direction, but only part of z
	// The texture is shifted vertically by half a block
public:
	SquashedSolidBlockGeometry( int top, int bottom, unsigned tx, unsigned color = 0xffffffu );
	SquashedSolidBlockGeometry( int top, int bottom, unsigned *tx, unsigned color = 0xffffffu );
	~SquashedSolidBlockGeometry();

	virtual GeometryCluster *newCluster();
	virtual bool beginEmit( GeometryCluster *out, InstanceContext *ctx );
	virtual IslandMode beginIsland( IslandDesc *ctx );
	virtual void emitIsland( GeometryCluster *out, const IslandDesc *ctx );

protected:
	int top, bottom;
};

class CompactedSolidBlockGeometry : public SolidBlockGeometry {
	// Blocks that are 'squashed' in at least 2 dimensions
public:
	CompactedSolidBlockGeometry( int *offsets, unsigned tx, unsigned color = 0xffffffu );
	CompactedSolidBlockGeometry( int *offsets, unsigned *tx, unsigned color = 0xffffffu );
	~CompactedSolidBlockGeometry();

	virtual GeometryCluster *newCluster();
	virtual bool beginEmit( GeometryCluster *out, InstanceContext *ctx );
	virtual IslandMode beginIsland( IslandDesc *ctx );
	virtual void emitIsland( GeometryCluster *out, const IslandDesc *ctx );

protected:
	int offsets[6];
	bool islandViable[3];
	bool fullIslands[3];
};

class MultiBlockInBlockGeometry : public SolidBlockGeometry {
	// Multiple blocks make up this block
public:
	MultiBlockInBlockGeometry( int **offsets, unsigned nEmits, unsigned tx, unsigned color = 0xffffffu );
	MultiBlockInBlockGeometry( int **offsets, unsigned nEmits, unsigned *tx, unsigned color = 0xffffffu );
	~MultiBlockInBlockGeometry();

	virtual bool beginEmit( GeometryCluster *out, InstanceContext *ctx );
	virtual IslandMode beginIsland( IslandDesc *ctx );
	virtual void emitIsland( GeometryCluster *out, const IslandDesc *ctx );
	
	inline void showFace( unsigned emit, unsigned face, bool show ) const { offsets[emit].hide[face] = !show; }
	
protected:
	void setupOffsets( int **offsets, unsigned nEmits );
	
	// Starting offsets
	// Offsets come in sets of 6, and there might be more than one set per dimension
	unsigned offsetCount;
	struct OffsetSet {
		bool hide[6];
		bool collapses;
		int offsets[6];
	};
	OffsetSet *offsets;
};

class CompactedIslandBlockGeometry : public SolidBlockGeometry {
	// Compacted islands in multiple dimensions
	// Designed primarily with fences in mind
public:
	CompactedIslandBlockGeometry( int **offsets, unsigned *nEmits, unsigned tx, unsigned color = 0xffffffu );
	CompactedIslandBlockGeometry( int **offsets, unsigned *nEmits, unsigned *tx, unsigned color = 0xffffffu );
	~CompactedIslandBlockGeometry();

	virtual bool beginEmit( GeometryCluster *out, InstanceContext *ctx );
	virtual IslandMode beginIsland( IslandDesc *ctx );
	virtual void emitIsland( GeometryCluster *out, const IslandDesc *ctx );
	
	inline void showFace( unsigned emit, unsigned face, bool show ) const { offsets[emit].hide[face] = !show; }
	
protected:
	void setupOffsets( int **offsets, unsigned *nEmits );
	
	// Starting offsets for each dimension
	// Offsets come in sets of 6, and there might be more than one set per dimension
	unsigned offsetStart[4];
	struct OffsetSet {
		bool hide[6];
		bool collapses;
		int offsets[6];
	};
	OffsetSet *offsets;
};

class DoorBlockGeometry : public SolidBlockGeometry {
public:
	explicit DoorBlockGeometry( unsigned tx, unsigned color = 0xffffffu );
	virtual ~DoorBlockGeometry();
	
	virtual GeometryCluster *newCluster();
	virtual IslandMode beginIsland( IslandDesc *ctx );
	virtual void emitIsland( GeometryCluster *out, const IslandDesc *ctx );
	
protected:
	// To access protected members of texFlipped
	inline DoorBlockGeometry *texFlippedGeom() { return static_cast<DoorBlockGeometry*>(&texFlipped); }

	SolidBlockGeometry texFlipped;
};

class StairsBlockGeometry : public SolidBlockGeometry {
public:
	explicit StairsBlockGeometry( unsigned tx, unsigned color = 0xffffffu );
	explicit StairsBlockGeometry( unsigned *tx, unsigned color = 0xffffffu );
	virtual ~StairsBlockGeometry();
	
	virtual IslandMode beginIsland( IslandDesc *ctx );
	virtual void emitIsland( GeometryCluster *out, const IslandDesc *ctx );

protected:
	static bool continueIsland( IslandDesc *island, const InstanceContext *nextBlock );
};

class RedstoneWireBlockGeometry : public BlockGeometry {
public:
	RedstoneWireBlockGeometry( unsigned txWire, unsigned txIntersect, unsigned color15 = 0xffff0000u, unsigned color0 = 0xff100000u );
	~RedstoneWireBlockGeometry();

protected:
	struct Vertex {
		short pos[3];
		unsigned short color;
	};
};

class SimpleGeometry : public BlockGeometry {
public:
	virtual ~SimpleGeometry();

	virtual void render( void *&meta, RenderContext *ctx );
	virtual GeometryCluster *newCluster();

protected:
	explicit SimpleGeometry();

	static void emitSimpleQuad( GeometryStream *target, const jMatrix *loc, const jMatrix *tex = NULL );
	void rawRender( void *&meta, RenderContext *ctx );

	struct Vertex {
		jVec3 pos;
		float u, v;
	};
};

class RailGeometry : public SimpleGeometry {
public:
	explicit RailGeometry( unsigned txStraight, unsigned txTurn );
	virtual ~RailGeometry();

	virtual GeometryCluster *newCluster();

	virtual bool beginEmit( GeometryCluster *out, InstanceContext *ctx );
	virtual void render( void *&meta, RenderContext *ctx );

private:
	unsigned texStraight;
	unsigned texTurn;
};

class TorchGeometry : public SimpleGeometry {
public:
	explicit TorchGeometry( unsigned tx );
	virtual ~TorchGeometry();

	virtual bool beginEmit( GeometryCluster *out, InstanceContext *ctx );
	virtual void render( void *&meta, RenderContext *ctx );

private:
	unsigned tex;
};

class FlowerGeometry : public SimpleGeometry {
public:
	explicit FlowerGeometry( unsigned tx );
	virtual ~FlowerGeometry();

	virtual void render( void *&meta, RenderContext *ctx );

	virtual bool beginEmit( GeometryCluster *out, InstanceContext *ctx );
	virtual IslandMode beginIsland( IslandDesc *ctx );
	virtual void emitIsland( GeometryCluster *out, const IslandDesc *ctx );

protected:
	unsigned tex;
};

class BiomeFlowerGeometry : public FlowerGeometry {
public:
	BiomeFlowerGeometry( unsigned tx, unsigned biomeTex );
	virtual ~BiomeFlowerGeometry();

	virtual void render( void *&meta, RenderContext *ctx );

protected:
	unsigned biomeTex;
};

class SignTextGeometry : public BlockGeometry {
public:
	SignTextGeometry();
	virtual ~SignTextGeometry();

	virtual GeometryCluster *newCluster();
	virtual void render( void *&meta, RenderContext *ctx );

	virtual bool beginEmit( GeometryCluster *out, InstanceContext *ctx );
	void emitSign( GeometryCluster *out, const jMatrix *pos, const char *text );
};

} // namespace mcgeom

#endif
