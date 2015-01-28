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


#ifndef MCWORLDMESH_H
#define MCWORLDMESH_H

#include <vector>
#include "blockmaterial.h"
#include "mcbiome.h"

class MCMap;
class MCBlockDesc;

struct Extents {
	Extents() { }
	Extents( int minx, int maxx, int miny, int maxy, int minz, int maxz )
		: minx(minx), miny(miny), minz(minz), maxx(maxx), maxy(maxy), maxz(maxz) { }

	union {
#ifdef __GNUC__
    __extension__ // FIXME: Anonymous structs are not supported by C++.
#endif
		struct {
			int minv[3], maxv[3];
		};
#ifdef __GNUC__
    __extension__ // FIXME: Anonymous structs are not supported by C++.
#endif
		struct {
			int minx, miny, minz;
			int maxx, maxy, maxz;
		};
	};

	inline bool intersects( const Extents &o ) const {
		return minx <= o.maxx && maxx >= o.minx
			&& miny <= o.maxy && maxy >= o.miny
			&& minz <= o.maxz && maxz >= o.minz;
	}

	inline bool contains( int x, int y, int z ) const {
		return x >= minx && x <= maxx && y >= miny && y <= maxy && z >= minz && z <= maxz;
	}
};

class MeshBuilder;

class MCWorldMesh {
	friend class MeshBuilder;
	friend class MCWorldMeshGroup;

public:
	~MCWorldMesh();

	static MCWorldMesh *generateFromMCMap( MCMap *map, const MCBlockDesc *blocks, Extents &hull, Extents &ltext );

	void finalizeLoad();
	inline bool isEmpty() const { return meta == NULL; }
	inline int getCost() { return cost; }
	inline int getGpuMemUse() { return vtxMem+idxMem+texMem; }

	void renderOpaque( mcgeom::RenderContext *ctx );
	void renderTransparent( mcgeom::RenderContext *ctx );

private:
	MCWorldMesh();

	void beginRender( mcgeom::RenderContext *ctx );
	void endRender();

	struct MeshMeta {
		mcgeom::BlockGeometry *geom;
	};

	const MCBiome *biomeSrc;
	unsigned biomeTex[MCBiome::MAX_BIOME_CHANNELS];
	unsigned lightTex;
	double lightTexScale[3];

	void *meta;
	MeshBuilder *bld;

	unsigned opaqueEnd, transpEnd;
	unsigned vtx_vbo, idx_vbo; // Do not reorder
	bool hasOpaque;
	bool hasTransparent;
	double origin[3];

	unsigned vtxMem, idxMem, texMem;
	int cost;

	MCWorldMesh *nextMesh;
};

class MCWorldMeshGroup {
public:
	~MCWorldMeshGroup();

	static MCWorldMeshGroup *generateFromMCMap( MCMap *map, const MCBlockDesc *blocks, Extents &ext );
	
	void finalizeLoad();
	bool isEmpty() const;
	inline int getCost() const { return cost; }
	inline int getGpuMemUse() const { return vtxMem+idxMem+texMem; }

	void renderOpaque( mcgeom::RenderContext *ctx );
	void renderTransparent( mcgeom::RenderContext *ctx );

private:
	MCWorldMeshGroup();

	MCWorldMesh *firstMesh;

	unsigned vtxMem, idxMem, texMem;
	int cost;
};

#endif // MCWORLDMESH_H
