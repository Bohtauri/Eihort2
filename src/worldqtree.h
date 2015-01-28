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

#ifndef WORLDQTREE_H
#define WORLDQTREE_H

#include "mcmap.h"
#include "mcregionmap.h"
#include "mcworldmesh.h"
#include "jmath.h"
#include "luaobject.h"
#include "mcblockdesc.h"
#include "lightmodel.h"
#include "mempool.h"

#define WORLDQTREE_META "WorldView"

class WorldQTree : public LuaObject, public MCRegionMap::ChangeListener {
public:
	explicit WorldQTree( MCRegionMap *regions, MCBlockDesc *blockDesc, unsigned leafShift = 7 );
	virtual ~WorldQTree();

	void setPosition( const jMatrix *mat );
	void setViewDistance( float dist );
	void setCameraParams( float yfov, float aspect, float near, float far );
	void setFog( float start, float end, float *color );
	const jMatrix *getEyeMat() const { return &eyeMat; }
	const jPlane *getFrustum() const;
	void draw();
	void drawLoadingCarat();

	virtual void chunkChanged( int x, int y );
	void kickOutAllMeshes();
	void kickOutTheseMeshes( const Extents *ext );
	unsigned getLoadingCount() const { return nMeshesLoading; }

	void pauseLoading( bool pause );
	inline bool isLoading() const { return getLoadingCount() > 0; }

	void initCamera();
	void initCameraNoTrans();

	// Lua functions
	static int lua_setPosition( lua_State *L );
	static int lua_setViewDistance( lua_State *L );
	static int lua_setCameraParams( lua_State *L );
	static int lua_setFog( lua_State *L );
	static int lua_getLightModel( lua_State *L );
	static int lua_isLoading( lua_State *L );
	static int lua_pauseLoading( lua_State *L );
	static int lua_reloadAll( lua_State *L );
	static int lua_reloadRegion( lua_State *L );
	static int lua_setGpuAllowance( lua_State *L );
	static int lua_getGpuAllowance( lua_State *L );
	static int lua_getLastFrameStats( lua_State *L );
	static int lua_render( lua_State *L );
	static void createNew( lua_State *L, MCRegionMap *regions, MCBlockDesc *blocks, unsigned leafShift );
	static int lua_destroy( lua_State *L );
	static void setupLua( lua_State *L );

private:
	struct QTreeLeaf {
		QTreeLeaf *prev;
		QTreeLeaf *next;
		float distance;
		MCWorldMeshGroup *mesh;
		unsigned lastRender;
		unsigned lastGPUSize;
		Extents lastExtents;
		bool load;
	};

	struct QTreeNode {
		QTreeNode() { }
		QTreeNode( WorldQTree *qtree, unsigned level, int minz, int maxz );
		QTreeNode( WorldQTree *qtree, const QTreeNode *parent, unsigned quadrant );
		Extents ext;
		jVec3 center;
		unsigned level;
		
		union {
			QTreeNode *subNodes[4];
			QTreeLeaf *leaves[4];
		};
	};

	void reloadArea( QTreeNode *node, const Extents *ext );

	void completeLoading();
	void freeLeafMesh( QTreeLeaf *leaf );
	void generateRenderList( QTreeNode *node, QTreeLeaf **lists, unsigned &maxn );
	static void mergeLeafIntoRenderLists( QTreeLeaf **lists, unsigned &maxn, QTreeLeaf *leaf );
	static QTreeLeaf *mergeRenderLists( QTreeLeaf *list1, QTreeLeaf *list2 );
	static QTreeLeaf *mergeRenderLists( QTreeLeaf *list1, QTreeLeaf *list2, QTreeLeaf *&tail );
	static void mergeRenderListsFinal( QTreeLeaf **lists, unsigned maxn, QTreeLeaf *&head, QTreeLeaf *&tail );

	static bool frustumIntersectsExtents( const jPlane *frustum, const Extents &ext );
	static bool frustumIntersects( const jPlane *frustum, const jVec3 *center, float rad );
	static float getVisibleDistance( const jPlane *frustum, const jVec3 *center, float rad );
	static void splitExtents( Extents *ext, unsigned corner );

	struct LoadingMesh {
		QTreeLeaf *leaf;
		MCWorldMeshGroup *loadedMesh;
		const MCBlockDesc *blocks;
		MCMap *map;
		Extents loadingExt;
	};

	static void loadMesh_worker( void *ldmesh );

	void buildViewFrustum();

	LoadingMesh meshesLoading[MAX_WORKERS];
	int newMeshAllowance;
	unsigned gpuAllowanceLeft;
	unsigned minGPUAllowanceToLoad;
	bool holdLoading;
	SDL_mutex *loadingMutex;

	MemoryPool<QTreeNode> nodePool;
	MemoryPool<QTreeLeaf> leafPool;
	QTreeNode rootNode;
	QTreeLeaf *unseenLeafHead, *unseenLeafTail;
	QTreeLeaf *curRenderHead, *curRenderTail;
	MCRegionMap *regions;
	MCBlockDesc *blockDesc;
	unsigned leafShift;
	unsigned leafSize;
	float subVisRadii[32];
	float newLoadDistanceLimit;
	float limitLoadDistance;
	LightModel lightModels[6];

	unsigned trisILD;
	unsigned vtxSpaceILD;
	unsigned idxSpaceILD;
	unsigned texSpaceILD;
	unsigned nMeshesLoading;
	unsigned lastRender;

	// TODO: Move this to a camera class
	jPlane frustum[6];
	jMatrix eyeMat;
	float fogColor[4];
	float fogStart, fogEnd;
	float viewDistance;
	float yfov_2, windowHt_2, screenAspect;
	float nearPlane, farPlane;
	bool frustumIsDirty;

	std::vector< QTreeLeaf* > meshesToKill;
	std::vector< Extents > killedExts;
};

#endif
