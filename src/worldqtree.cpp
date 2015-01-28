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


#include <float.h>
#include <GL/glew.h>

#include "worldqtree.h"
#include "worker.h"
#include "eihortshader.h"

extern bool g_needRefresh;
extern unsigned g_nWorkers;
extern Worker *g_workers[];
extern EihortShader *g_shader;
extern jMatrix g_eyeMat;
extern jPlane g_viewFrustum[];

inline void keepMinLevel( unsigned &minLevel, int target, unsigned leafSize ) {
	unsigned tgt = (unsigned)abs( target );
	while( (leafSize<<minLevel) < tgt )
		minLevel++;
}

WorldQTree::WorldQTree( MCRegionMap *regions, MCBlockDesc *blocks, unsigned leafShift )
: newMeshAllowance(0)
, gpuAllowanceLeft(512*1024*1024)
, minGPUAllowanceToLoad(1u<<(leafShift+leafShift+7))
, holdLoading(false)
, unseenLeafHead(NULL), unseenLeafTail(NULL)
, curRenderHead(NULL), curRenderTail(NULL)
, regions(regions)
, blockDesc(blocks)
, leafShift(leafShift)
, leafSize((1u<<leafShift)-2)
, limitLoadDistance(FLT_MAX)
, trisILD(0)
, vtxSpaceILD(0)
, idxSpaceILD(0)
, texSpaceILD(0)
, nMeshesLoading(0)
, lastRender(0)
, fogStart(1.0f), fogEnd(1000.0f)
, viewDistance(1000.0f)
, yfov_2(jPI/4), screenAspect(1.0f)
, nearPlane(0.1f), farPlane(viewDistance)
, frustumIsDirty(true)
{
	Extents ext;
	regions->getWorldBlockExtents( ext.minx, ext.maxx, ext.miny, ext.maxy );

	unsigned minLevel = 0;
	keepMinLevel( minLevel, ext.minx, leafSize );
	keepMinLevel( minLevel, ext.maxx, leafSize );
	keepMinLevel( minLevel, ext.miny, leafSize );
	keepMinLevel( minLevel, ext.maxy, leafSize );

	new( &rootNode ) QTreeNode( this, minLevel, 0, regions->isAnvil() ? 255 : 127 );

	float dx = (float)(rootNode.ext.maxx - rootNode.ext.minx) / 2.0f;
	float dy = (float)(rootNode.ext.maxy - rootNode.ext.miny) / 2.0f;
	float dz = (float)(rootNode.ext.maxz - rootNode.ext.minz);
	unsigned l = minLevel;
	while( true ){
		subVisRadii[l] = sqrtf( dx*dx + dy*dy + dz*dz ) / 2.0f;
		if( l == 0 )
			break;
		dx /= 2.0f;
		dy /= 2.0f;
		l--;
	}

	loadingMutex = SDL_CreateMutex();

	for( unsigned i = 0; i < g_nWorkers; i++ ) {
		meshesLoading[i].leaf = NULL;
		meshesLoading[i].loadedMesh = NULL;
		if( regions->isAnvil() ) {
			meshesLoading[i].map = new MCMap_Anvil( regions );
		} else {
			meshesLoading[i].map = new MCMap_MCRegion( regions );
		}
	}

	regions->setListener( this );

	jMatrixSetIdentity( &eyeMat );
	fogColor[0] = 0.0f;
	fogColor[1] = 0.0f;
	fogColor[2] = 0.0f;
	fogColor[3] = 1.0f;
}

WorldQTree::~WorldQTree() {
	while( curRenderHead )
		freeLeafMesh( curRenderHead );
	while( unseenLeafHead )
		freeLeafMesh( unseenLeafHead );

	SDL_DestroyMutex( loadingMutex );
}

void WorldQTree::setPosition( const jMatrix *mat ) {
	jMatrixCopy( &eyeMat, mat );
	frustumIsDirty = true;
}

void WorldQTree::setViewDistance( float dist ) {
	viewDistance = dist;
	frustumIsDirty = true;
}

void WorldQTree::setCameraParams( float yfov, float aspect, float n, float f ) {
	yfov_2 = yfov / 2;
	screenAspect = aspect;
	nearPlane = n;
	farPlane = f;
	frustumIsDirty = true;
}

void WorldQTree::setFog( float start, float end, float *color ) {
	fogStart = start;
	fogEnd = end;
	fogColor[0] = color[0];
	fogColor[1] = color[1];
	fogColor[2] = color[2];
}

const jPlane *WorldQTree::getFrustum() const {
	if( frustumIsDirty )
		const_cast<WorldQTree*>(this)->buildViewFrustum();
	return frustum;
}

void WorldQTree::draw() {
	if( nMeshesLoading || !meshesToKill.empty() ) {
		SDL_mutexP( loadingMutex );

		while( !meshesToKill.empty() ) {
			QTreeLeaf *leaf = meshesToKill.back();
			if( leaf->mesh )
				freeLeafMesh( leaf );
			leaf->load = true;
			meshesToKill.pop_back();
		}

		if( nMeshesLoading )
			completeLoading();

		SDL_mutexV( loadingMutex );
	}

	lastRender++;
	newMeshAllowance = 80;

	{
		// Move the last frame's render list into the unseen render list
		if( curRenderHead ) {
			if( unseenLeafHead ) {
				unseenLeafHead->prev = curRenderTail;
			} else {
				unseenLeafTail = curRenderTail;
			}
			curRenderTail->next = unseenLeafHead;
			unseenLeafHead = curRenderHead;
		}

		// Regenerate the render lists
		QTreeLeaf *lists[32];
		for( unsigned i = 0; i < sizeof(lists)/sizeof(QTreeLeaf*); i++ )
			lists[i] = NULL; 
		unsigned maxn = 0;
		newLoadDistanceLimit = FLT_MAX;
		generateRenderList( &rootNode, &lists[0], maxn );
		if( maxn || lists[0] ) {
			mergeRenderListsFinal( &lists[0], maxn, curRenderHead, curRenderTail );
		} else {
			curRenderHead = NULL;
			curRenderTail = NULL;
		}
		limitLoadDistance = newLoadDistanceLimit;
		if( limitLoadDistance < FLT_MAX && unseenLeafHead )
			limitLoadDistance += 1.0f;
	}

	mcgeom::RenderContext rctx;
	jVec3Copy( &rctx.viewPos, &eyeMat.pos );
	rctx.renderedTriCount = 0;
	rctx.vertexSize = 0;
	rctx.indexSize = 0;
	rctx.texSize = 0;
	rctx.shader = g_shader;
	rctx.lightModels = lightModels;
	rctx.enableBlockLighting = blockDesc->enableBlockLighting();

	//lightModel.uploadGL();

	initCamera();

	glEnable( GL_FOG );
	glFogi( GL_FOG_MODE, GL_LINEAR );
	glFogf( GL_FOG_START, fogStart );
	glFogf( GL_FOG_END, fogEnd );
	glFogfv( GL_FOG_COLOR, &fogColor[0] );

	g_shader->bindNormal();

	QTreeLeaf *leaf = curRenderHead;
	while( leaf ) {
		leaf->mesh->renderOpaque( &rctx );
		leaf = leaf->next;
	}
	leaf = curRenderTail;
	while( leaf ) {
		leaf->mesh->renderTransparent( &rctx );
		leaf = leaf->prev;
	}

	LightModel::unloadGL();
	g_shader->unbind();

	glDisable( GL_FOG );

	trisILD = rctx.renderedTriCount;
	vtxSpaceILD = rctx.vertexSize;
	idxSpaceILD = rctx.indexSize;
	texSpaceILD = rctx.texSize;

	if( newMeshAllowance < 0 ) // Some newly drawn meshes didn't fit
		g_needRefresh = true;
}

void WorldQTree::drawLoadingCarat() {
	if( nMeshesLoading ) {
		float white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, &white[0] );
		glColor3f( 0.05f, 0.5f, 1.0f );

		for( unsigned i = 0; i < g_nWorkers; i++ ) {
			if( meshesLoading[i].leaf && !meshesLoading[i].leaf->mesh ) {
				Extents &loadingExt = meshesLoading[i].loadingExt;
				/* Square
				glBegin( GL_LINE_LOOP );
				glVertex3i( loadingExt.minx+1, loadingExt.miny+1, 64 );
				glVertex3i( loadingExt.maxx,   loadingExt.miny+1, 64 );
				glVertex3i( loadingExt.maxx,   loadingExt.maxy,   64 );
				glVertex3i( loadingExt.minx+1, loadingExt.maxy,   64 );
				glEnd();
				//*/
				//* Cube
				glBegin( GL_LINE_LOOP );
				glVertex3i( loadingExt.minx+1, loadingExt.miny+1, loadingExt.minz+1 );
				glVertex3i( loadingExt.maxx,   loadingExt.miny+1, loadingExt.minz+1 );
				glVertex3i( loadingExt.maxx,   loadingExt.miny+1, loadingExt.maxz   );
				glVertex3i( loadingExt.maxx,   loadingExt.maxy,   loadingExt.maxz   );
				glVertex3i( loadingExt.maxx,   loadingExt.maxy,   loadingExt.minz+1 );
				glVertex3i( loadingExt.minx+1, loadingExt.maxy,   loadingExt.minz+1 );
				glVertex3i( loadingExt.minx+1, loadingExt.maxy,   loadingExt.maxz   );
				glVertex3i( loadingExt.minx+1, loadingExt.miny+1, loadingExt.maxz   );
				glEnd();
				glBegin( GL_LINES );
				glVertex3i( loadingExt.minx+1, loadingExt.miny+1, loadingExt.maxz   );
				glVertex3i( loadingExt.maxx,   loadingExt.miny+1, loadingExt.maxz   );
				glVertex3i( loadingExt.maxx,   loadingExt.miny+1, loadingExt.minz+1 );
				glVertex3i( loadingExt.maxx,   loadingExt.maxy,   loadingExt.minz+1 );
				glVertex3i( loadingExt.minx+1, loadingExt.maxy,   loadingExt.maxz   );
				glVertex3i( loadingExt.maxx,   loadingExt.maxy,   loadingExt.maxz   );
				glVertex3i( loadingExt.minx+1, loadingExt.miny+1, loadingExt.minz+1 );
				glVertex3i( loadingExt.minx+1, loadingExt.maxy,   loadingExt.minz+1 );
				glEnd();
				//*/
			}
		}

		glColor3f( 1.0f, 1.0f, 1.0f );
	}
#ifndef NDEBUG
	SDL_mutexP( loadingMutex );
	unsigned j = 0;
	for( unsigned i = 0; i < killedExts.size(); i++ ) {
		glBegin( GL_LINE_LOOP );
		float s = killedExts[i].maxz / 128.0f;
		glColor3f( s*0.9f, s*0.3f, s*0.05f );
		glVertex3i( killedExts[i].minx, killedExts[i].miny, 80 );
		glVertex3i( killedExts[i].maxx-1, killedExts[i].miny, 80 );
		glVertex3i( killedExts[i].maxx-1, killedExts[i].maxy-1,   80 );
		glVertex3i( killedExts[i].minx, killedExts[i].maxy-1,   80 );
		glEnd();
		killedExts[i].maxz--;

		if( killedExts[i].maxz )
			killedExts[j++] = killedExts[i];
	}
	while( killedExts.size() > j )
		killedExts.pop_back();
	SDL_mutexV( loadingMutex );
#endif
}

void WorldQTree::chunkChanged( int x, int y ) {
	SDL_mutexP( loadingMutex );

	Extents ext;
	ext.minz = 0;
	ext.maxz = 127;
	MCRegionMap::chunkCoordsToBlockExtents( x, y, ext.minx, ext.maxx, ext.miny, ext.maxy );
	ext.minx--;
	ext.miny--;
	ext.maxx++;
	ext.maxy++;

#ifndef NDEBUG
	killedExts.push_back( ext );
#endif

	reloadArea( &rootNode, &ext );
	g_needRefresh = true;

	SDL_mutexV( loadingMutex );
}

void WorldQTree::kickOutAllMeshes() {
	SDL_mutexP( loadingMutex );
	reloadArea( &rootNode, &rootNode.ext );
	g_needRefresh = true;
	SDL_mutexV( loadingMutex );
}

void WorldQTree::kickOutTheseMeshes( const Extents *ext ) {
	SDL_mutexP( loadingMutex );
	reloadArea( &rootNode, ext );
	g_needRefresh = true;
	SDL_mutexV( loadingMutex );
}

void WorldQTree::pauseLoading( bool pause ) {
	if( pause != holdLoading ) {
		if( !pause )
			g_needRefresh = true;
		holdLoading = pause;
	}
}

void WorldQTree::initCamera() {
	(void)getFrustum(); // Update windowHt_2
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	float windowWd_2 = windowHt_2 * screenAspect;
	glFrustum( -windowWd_2, windowWd_2, -windowHt_2, windowHt_2, nearPlane, farPlane );

	glMatrixMode( GL_MODELVIEW );
	const float Y_FWD_Z_UP[] = {
		1.f, 0.f, 0.f, 0.f,
		0.f, 0.f,-1.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 1.f
	};
	glLoadMatrixf( &Y_FWD_Z_UP[0] );
	float mat[16];
	jMatrix jmat;
	jMatrixSimpleInvert( &jmat, &eyeMat );
	jMatrixToGL( mat, &jmat );
	glMultMatrixf( &mat[0] );
}

void WorldQTree::initCameraNoTrans() {
	(void)getFrustum(); // Update windowHt_2
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	float windowWd_2 = windowHt_2 * screenAspect;
	glFrustum( -windowWd_2, windowWd_2, -windowHt_2, windowHt_2, nearPlane, farPlane );

	glMatrixMode( GL_MODELVIEW );
	const float Y_FWD_Z_UP[] = {
		1.f, 0.f, 0.f, 0.f,
		0.f, 0.f,-1.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 1.f
	};
	glLoadMatrixf( &Y_FWD_Z_UP[0] );
	float mat[16];
	jMatrix jmat;
	jMatrixSimpleInvert( &jmat, &eyeMat );
	jMatrixZeroTranslate( &jmat );
	jMatrixToGL( mat, &jmat );
	glMultMatrixf( &mat[0] );
}

WorldQTree::QTreeNode::QTreeNode( WorldQTree *qtree, unsigned level, int minz, int maxz ) {
	int mlMin = qtree->leafSize << level;

	this->level = level;
	jVec3Zero( &center );
	center.z = 64.0f;
	ext.minx = -mlMin;
	ext.maxx = mlMin-1;
	ext.miny = -mlMin;
	ext.maxy = mlMin-1;
	ext.minz = minz;
	ext.maxz = maxz;

	if( level ) {
		subNodes[0] = subNodes[1] = subNodes[2] = subNodes[3] = NULL;
	} else {
		for( unsigned j = 0; j < 4; j++ ) {
			QTreeLeaf *leaf = leaves[j] = qtree->leafPool.alloc();
			leaf->lastRender = 0;
			leaf->mesh = NULL;
			leaf->load = true;
			leaf->next = NULL;
			leaf->prev = NULL;
			leaf->lastGPUSize = qtree->minGPUAllowanceToLoad;
		}
	}
}

WorldQTree::QTreeNode::QTreeNode( WorldQTree *qtree, const QTreeNode *parent, unsigned quadrant ) {
	level = parent->level - 1;
	ext = parent->ext;
	splitExtents( &ext, quadrant );
	jVec3Set( &center,
		(ext.maxx + ext.minx) / 2.0f,
		(ext.maxy + ext.miny) / 2.0f,
		(ext.maxz + ext.minz) / 2.0f );

	if( level ) {
		subNodes[0] = subNodes[1] = subNodes[2] = subNodes[3] = NULL;
	} else {
		for( unsigned j = 0; j < 4; j++ ) {
			QTreeLeaf *leaf = leaves[j] = qtree->leafPool.alloc();
			leaf->lastRender = 0;
			leaf->mesh = NULL;
			leaf->load = true;
			leaf->next = NULL;
			leaf->prev = NULL;
			leaf->lastGPUSize = qtree->minGPUAllowanceToLoad;
			leaf->lastExtents = ext;
			splitExtents( &leaf->lastExtents, j );
		}
	}
}

void WorldQTree::reloadArea( QTreeNode *node, const Extents *ext ) {
	for( unsigned i = 0; i < 4; i++ ) {
		Extents ext2 = node->ext;
		splitExtents( &ext2, i );
		if( ext2.intersects( *ext ) ) {
			if( node->level ) {
				if( node->subNodes[i] )
					reloadArea( node->subNodes[i], ext );
			} else {
				QTreeLeaf *leaf = node->leaves[i];
				if( leaf->mesh && lastRender - leaf->lastRender > 3 ) {
					// The mesh is not visible - kick it out silently
					meshesToKill.push_back( leaf );
				}
				leaf->load = true;
			}
		}
	}
}

void WorldQTree::completeLoading() {
	QTreeLeaf *toAppend = NULL, *toAppendTail = NULL;

	for( unsigned i = 0; i < g_nWorkers; i++ ) {
		if( meshesLoading[i].loadedMesh ) {
			QTreeLeaf *leaf = meshesLoading[i].leaf;
			MCWorldMeshGroup *wmesh = meshesLoading[i].loadedMesh;
			wmesh->finalizeLoad();

			if( leaf->mesh )
				freeLeafMesh( leaf );

			if( wmesh->isEmpty() ) {
				delete wmesh;
				leaf->lastGPUSize = 0;
			} else {
				unsigned gpuCost = wmesh->getGpuMemUse();
				leaf->lastGPUSize = gpuCost;
				leaf->lastExtents = meshesLoading[i].loadingExt;
				if( gpuCost > gpuAllowanceLeft ) {
					while( unseenLeafTail && gpuCost > gpuAllowanceLeft ) {
						// Start by eating non-visible leaves
						QTreeLeaf *toRemove = unseenLeafTail;
						freeLeafMesh( toRemove );
						toRemove->load = true;
					}
					if( gpuCost > gpuAllowanceLeft ) {
						// No old meshes to free.. start cannibalizing the distant visible ones
						unsigned nToRemove = 0, freedSpace = 0;
						QTreeLeaf *toRemove = curRenderTail;
						while( gpuCost > gpuAllowanceLeft + freedSpace && toRemove && toRemove->distance > leaf->distance ) {
							nToRemove++;
							freedSpace += toRemove->mesh->getGpuMemUse();
							toRemove = toRemove->prev;
						}
						if( gpuCost <= gpuAllowanceLeft + freedSpace ) {
							// There are enough visible meshes farther than this one to make space for it!
							for( unsigned i = 0; i < nToRemove; i++ ) {
								toRemove = curRenderTail;
								freeLeafMesh( toRemove );
								toRemove->load = true;
							}
						}
					}
					if( gpuCost > gpuAllowanceLeft ) {
						// No memory to free... have to give up on this mesh :(
						delete wmesh;
						wmesh = NULL;
						leaf->load = true;
						gpuCost = 0;
						limitLoadDistance = std::min( limitLoadDistance, leaf->distance );
					}
				}

				if( wmesh ) {
					gpuAllowanceLeft -= gpuCost;
					leaf->mesh = wmesh;

					leaf->next = toAppend;
					if( toAppend ) {
						toAppend->prev = leaf;
					} else {
						toAppendTail = leaf;
					}
					toAppend = leaf;

					limitLoadDistance = FLT_MAX;
				}
			}

			meshesLoading[i].loadedMesh = NULL;
			meshesLoading[i].leaf = NULL;
			blockDesc->unlock();
			nMeshesLoading--;
		}
	}
	if( nMeshesLoading == 0 ) {
		for( unsigned i = 0; i < g_nWorkers; i++ )
			meshesLoading[i].map->clearAllLoadedChunks();
	}

	if( toAppend ) {
		if( curRenderTail ) {
			toAppend->prev = curRenderTail;
			curRenderTail->next = toAppend;
		} else {
			curRenderHead = toAppend;
			toAppend->prev = NULL;
		}
		curRenderTail = toAppendTail;
	}
}

void WorldQTree::freeLeafMesh( QTreeLeaf *leaf ) {
	gpuAllowanceLeft += leaf->mesh->getGpuMemUse();
	delete leaf->mesh;
	leaf->mesh = NULL;
	if( leaf->prev ) {
		leaf->prev->next = leaf->next;
	} else {
		(leaf == curRenderHead ? curRenderHead : unseenLeafHead) = leaf->next;
	}
	if( leaf->next ) {
		leaf->next->prev = leaf->prev;
	} else {
		(leaf == curRenderTail ? curRenderTail : unseenLeafTail) = leaf->prev;
	}
}

void WorldQTree::generateRenderList( QTreeNode *node, QTreeLeaf **lists, unsigned &maxn ) {
	float distances[4];
	jVec3 pos, dv;
	float halfDist = (float)(leafSize<<node->level) / 2.0f;
	float visRadius = subVisRadii[node->level];

	jVec3Set( &dv, -halfDist, -halfDist, 0.0f );
	jVec3Add( &pos, &node->center, &dv );
	distances[0] = getVisibleDistance( &frustum[0], &pos, visRadius );

	jVec3Set( &dv, halfDist, -halfDist, 0.0f );
	jVec3Add( &pos, &node->center, &dv );
	distances[1] = getVisibleDistance( &frustum[0], &pos, visRadius );

	jVec3Set( &dv, -halfDist, halfDist, 0.0f );
	jVec3Add( &pos, &node->center, &dv );
	distances[2] = getVisibleDistance( &frustum[0], &pos, visRadius );

	jVec3Set( &dv, halfDist, halfDist, 0.0f );
	jVec3Add( &pos, &node->center, &dv );
	distances[3] = getVisibleDistance( &frustum[0], &pos, visRadius );

	unsigned minI[4] = { 0u, 1u, 2u, 3u };
	if( distances[0] > distances[1] )
		std::swap( minI[0], minI[1] );
	if( distances[2] > distances[3] )
		std::swap( minI[2], minI[3] );
	if( distances[minI[0]] > distances[minI[2]] )
		std::swap( minI[0], minI[2] );
	if( distances[minI[1]] > distances[minI[3]] )
		std::swap( minI[1], minI[3] );
	if( distances[minI[1]] > distances[minI[2]] )
		std::swap( minI[1], minI[2] );

	if( node->level == 0 ) {
		for( unsigned k = 0; k < 4; k++ ) {
			unsigned i = minI[k];
			QTreeLeaf *leaf = node->leaves[i];
			leaf->distance = distances[i];
			if( leaf->distance != FLT_MAX && frustumIntersectsExtents( &frustum[0], leaf->lastExtents ) ) {
				if( leaf->mesh ) {
					if( leaf->lastRender == lastRender - 1 || (newMeshAllowance -= leaf->mesh->getCost()) >= -leaf->mesh->getCost() ) {
						// Remove from the unseen list and add to the current list
						leaf->lastRender = lastRender;
						if( leaf->prev ) {
							leaf->prev->next = leaf->next;
						} else {
							unseenLeafHead = leaf->next;
						}
						if( leaf->next ) {
							leaf->next->prev = leaf->prev;
						} else {
							unseenLeafTail = leaf->prev;
						}
						mergeLeafIntoRenderLists( lists, maxn, leaf );
					}
				}
				if( leaf->load && nMeshesLoading < g_nWorkers && !holdLoading ) {
					if( leaf->distance >= limitLoadDistance ) {
						newLoadDistanceLimit = std::min( leaf->distance, newLoadDistanceLimit );
						continue;
					}
					for( unsigned j = 0; j < g_nWorkers; j++ ) {
						if( !meshesLoading[j].leaf ) {
							leaf->load = false;
							meshesLoading[j].leaf = leaf;
							meshesLoading[j].loadingExt = node->ext;
							splitExtents( &meshesLoading[j].loadingExt, i );
							meshesLoading[j].blocks = blockDesc;
							g_workers[j]->doTask( loadMesh_worker, &meshesLoading[j] );
							blockDesc->lock();
							nMeshesLoading++;
							break;
						}
					}
				}
			}
		}
	} else {
		for( unsigned k = 0; k < 4; k++ ) {
			unsigned i = minI[k];
			if( distances[i] != FLT_MAX ) {
				if( !node->subNodes[i] )
					node->subNodes[i] = new( nodePool.alloc() ) QTreeNode( this, node, i );
				generateRenderList( node->subNodes[i], lists, maxn );
			}
		}
	}
}

void WorldQTree::mergeLeafIntoRenderLists( QTreeLeaf **lists, unsigned &maxn, QTreeLeaf *leaf ) {
	QTreeLeaf *toMerge = leaf;
	leaf->next = NULL;
	unsigned level = 0;
	while( lists[level] ) {
		toMerge = mergeRenderLists( lists[level], toMerge );
		lists[level] = NULL;
		level++;
	}
	lists[level] = toMerge;
	if( level > maxn )
		maxn = level;
}

WorldQTree::QTreeLeaf *WorldQTree::mergeRenderLists( QTreeLeaf *list1, QTreeLeaf *list2 ) {
	QTreeLeaf *head = NULL;
	QTreeLeaf **prevNext = &head;
	while( true ) {
		if( list1->distance < list2->distance ) {
			*prevNext = list1;
			prevNext = &list1->next;
			list1 = list1->next;
			if( !list1 ) {
				*prevNext = list2;
				return head;
			}
		} else {
			*prevNext = list2;
			prevNext = &list2->next;
			list2 = list2->next;
			if( !list2 ) {
				*prevNext = list1;
				return head;
			}
		}
	}
}

WorldQTree::QTreeLeaf *WorldQTree::mergeRenderLists( WorldQTree::QTreeLeaf *list1, WorldQTree::QTreeLeaf *list2, WorldQTree::QTreeLeaf *&tail ) {
	QTreeLeaf *head = NULL;
	QTreeLeaf **prevNext = &head, *prev = NULL;
	while( true ) {
		if( list1->distance < list2->distance ) {
			*prevNext = list1;
			list1->prev = prev;
			if( list1->next ) {
				prev = list1;
				prevNext = &prev->next;
				list1 = list1->next;
			} else {
				list1->next = list2;
				list2->prev = list1;
				tail = list2;
				break;
			}
		} else {
			*prevNext = list2;
			list2->prev = prev;
			if( list2->next ) {
				prev = list2;
				prevNext = &prev->next;
				list2 = list2->next;
			} else {
				list2->next = list1;
				list1->prev = list2;
				tail = list1;
				break;
			}
		}
	}
	while( tail->next ) {
		tail->next->prev = tail;
		tail = tail->next;
	}
	return head;
}

void WorldQTree::mergeRenderListsFinal( QTreeLeaf **lists, unsigned maxn, QTreeLeaf *&head, QTreeLeaf *&tail ) {
	unsigned level = 0;
	while( !lists[level] )
		level++;

	if( level == maxn ) {
		// Only a single list - add in prev pointers and find the tail
		tail = head = lists[level];
		head->prev = NULL;
		while( tail->next ) {
			tail->next->prev = tail;
			tail = tail->next;
		}
		return;
	}

	// Lists must be merged
	QTreeLeaf *toMerge = lists[level];
	for( unsigned i = level + 1; i < maxn; i++ ) {
		if( lists[i] )
			toMerge = mergeRenderLists( lists[i], toMerge );
	}
	head = mergeRenderLists( lists[maxn], toMerge, tail );
}

bool WorldQTree::frustumIntersectsExtents( const jPlane *frustum, const Extents &ext ) {
	jVec3 extCenter, extCenterToAbsCorner;
	jVec3Set( &extCenter,
		((float)ext.maxx + (float)ext.minx) / 2.0f,
		((float)ext.maxy + (float)ext.miny) / 2.0f,
		((float)ext.maxz + (float)ext.minz) / 2.0f );
	jVec3Set( &extCenterToAbsCorner,
		((float)ext.maxx - (float)ext.minx) / 2.0f,
		((float)ext.maxy - (float)ext.miny) / 2.0f,
		((float)ext.maxz - (float)ext.minz) / 2.0f );

	// Check side planes of the frustum
	for( unsigned i = 0; i < 5; i++ ) {
		jVec3 absn;
		jVec3Abs( &absn, &frustum[i].n );
		if( jPlaneDot3( &frustum[i], &extCenter ) < -jVec3Dot( &absn, &extCenterToAbsCorner ) )
			return false;
	}

	// Check the far sphere
	float closestPointDistSq = 0.0f;
	for( unsigned j = 0; j < 3; j++ ) {
		if( frustum[5].v[j] < ext.minv[j] ) {
			float e = (float)ext.minv[j] - frustum[5].v[j];
			closestPointDistSq += e*e;
		} else if( frustum[5].v[j] > ext.maxv[j] ) {
			float e = frustum[5].v[j] - (float)ext.maxv[j];
			closestPointDistSq += e*e;
		}
	}
	return closestPointDistSq < frustum[5].d * frustum[5].d;
	
}

bool WorldQTree::frustumIntersects( const jPlane *frustum, const jVec3 *center, float rad ) {
	for( unsigned i = 0; i < 6; i++ ) {
		if( jPlaneDot3( frustum + i, center ) < -rad )
			return false;
	}
	return true;
}

float WorldQTree::getVisibleDistance( const jPlane *frustum, const jVec3 *center, float rad ) {
	float bSphereRad = rad + frustum[5].d;
	float distSq = jVec3DistSq( center, &frustum[5].n );
	if( distSq > bSphereRad * bSphereRad )
		return FLT_MAX;
	for( unsigned i = 0; i < 5; i++ ) {
		if( jPlaneDot3( frustum + i, center ) < -rad )
			return FLT_MAX;
	}
	return distSq;
}

void WorldQTree::splitExtents( Extents *ext, unsigned corner ) {
	if( corner & 1 ) {
		// Take the right side
		ext->minx = (ext->minx + ext->maxx + 1) / 2;
	} else {
		// Take the left side
		ext->maxx = (ext->minx + ext->maxx + 1) / 2 - 1;
	}

	if( corner & 2 ) {
		// Take the top side
		ext->miny = (ext->miny + ext->maxy + 1) / 2;
	} else {
		// Take the bottom side
		ext->maxy = (ext->miny + ext->maxy + 1) / 2 - 1;
	}
}

void WorldQTree::loadMesh_worker( void *ldmesh_cookie ) {
	WorldQTree::LoadingMesh *ldmesh = (WorldQTree::LoadingMesh*)ldmesh_cookie;
	ldmesh->loadedMesh = MCWorldMeshGroup::generateFromMCMap( ldmesh->map, ldmesh->blocks, ldmesh->loadingExt );
	g_needRefresh = true;
}

void WorldQTree::buildViewFrustum() {
	jPlane plane;
	
	plane.a = plane.d = 0.0f;
	plane.b = sinf( yfov_2 );
	plane.c = cosf( yfov_2 );
	float tan_yfov_2 = plane.b / plane.c;
	jPlaneTransform( frustum + 0, &eyeMat, &plane); // Lower plane
	plane.c = -plane.c;
	jPlaneTransform( frustum + 1, &eyeMat, &plane); // Upper plane

	float xfov_2 = atanf( screenAspect * tan_yfov_2 );
	plane.c = 0.0f;
	plane.a = cosf( xfov_2 );
	plane.b = sinf( xfov_2 );
	jPlaneTransform( frustum + 2, &eyeMat, &plane); // Left plane
	plane.a = -plane.a;
	jPlaneTransform( frustum + 3, &eyeMat, &plane); // Right plane

	plane.a = 0.0f;
	plane.b = 1.0f;
	jPlaneTransform( frustum + 4, &eyeMat, &plane); // Near plane

	/*
	plane.b = -1.0f;
	plane.d = -g_viewDistance-50.0f;
	jPlaneTransform( frustum + 5, &eyeMat, &plane); // Far plane
	*/
	// Last "plane" is now a bounding sphere
	jVec3Copy( &frustum[5].n, &eyeMat.pos );
	frustum[5].d = viewDistance;

	windowHt_2 = nearPlane * tan_yfov_2;

	frustumIsDirty = false;
}



int WorldQTree::lua_setPosition( lua_State *L ) {
	WorldQTree *qtree = getLuaObjectArg<WorldQTree>( L, 1, WORLDQTREE_META );

	float eyeX = (float)luaL_checknumber( L, 2 );
	float eyeY = (float)luaL_checknumber( L, 3 );
	float eyeZ = (float)luaL_checknumber( L, 4 );
	float fwdX = (float)luaL_checknumber( L, 5 );
	float fwdY = (float)luaL_checknumber( L, 6 );
	float fwdZ = (float)luaL_checknumber( L, 7 );
	float upX = (float)luaL_checknumber( L, 8 );
	float upY = (float)luaL_checknumber( L, 9 );
	float upZ = (float)luaL_checknumber( L, 10 );

	// Eihort <-- Minecraft coordinate swap is here:
	jMatrix mat;
	jVec3 eye, fwd, up, right;
	jVec3Set( &eye, eyeZ, eyeX, eyeY );
	jVec3Set( &fwd, fwdZ, fwdX, fwdY );
	jVec3Normalize( &fwd );
	jVec3Set( &up, upZ, upX, upY );
	jVec3Orthogonalize( &up, &up, &fwd );
	jVec3Normalize( &up );
	jVec3Cross( &right, &fwd, &up );
	jMatrixBasis( &mat, &eye, &right, &fwd, &up );

	qtree->setPosition( &mat );

	return 0;
}

int WorldQTree::lua_setViewDistance( lua_State *L ) {
	WorldQTree *qtree = getLuaObjectArg<WorldQTree>( L, 1, WORLDQTREE_META );
	qtree->setViewDistance( (float)luaL_checknumber( L, 2 ) );
	return 0;
}

int WorldQTree::lua_setCameraParams( lua_State *L ) {
	WorldQTree *qtree = getLuaObjectArg<WorldQTree>( L, 1, WORLDQTREE_META );
	qtree->setCameraParams( (float)luaL_checknumber( L, 2 ), (float)luaL_checknumber( L, 3 ), (float)luaL_checknumber( L, 4 ), (float)luaL_checknumber( L, 5 ) );
	return 0;
}

int WorldQTree::lua_setFog( lua_State *L ) {
	WorldQTree *qtree = getLuaObjectArg<WorldQTree>( L, 1, WORLDQTREE_META );
	float color[3];
	color[0] = (float)luaL_checknumber( L, 4 );
	color[1] = (float)luaL_checknumber( L, 5 );
	color[2] = (float)luaL_checknumber( L, 6 );
	qtree->setFog( (float)luaL_checknumber( L, 2 ), (float)luaL_checknumber( L, 3 ), &color[0] );
	return 0;
}

int WorldQTree::lua_getLightModel( lua_State *L ) {
	WorldQTree *qtree = getLuaObjectArg<WorldQTree>( L, 1, WORLDQTREE_META );
	qtree->lightModels[(int)luaL_checknumber( L, 2 )].lua_push();
	return 1;
}

int WorldQTree::lua_isLoading( lua_State *L ) {
	WorldQTree *qtree = getLuaObjectArg<WorldQTree>( L, 1, WORLDQTREE_META );
	lua_pushboolean( L, qtree->isLoading() );
	return 1;
}

int WorldQTree::lua_pauseLoading( lua_State *L ) {
	WorldQTree *qtree = getLuaObjectArg<WorldQTree>( L, 1, WORLDQTREE_META );
	qtree->pauseLoading( !!lua_toboolean( L, 2 ) );
	return 0;
}

int WorldQTree::lua_reloadAll( lua_State *L ) {
	WorldQTree *qtree = getLuaObjectArg<WorldQTree>( L, 1, WORLDQTREE_META );
	qtree->kickOutAllMeshes();
	qtree->regions->checkForRegionChanges();
	return 0;
}

int WorldQTree::lua_reloadRegion( lua_State *L ) {
	WorldQTree *qtree = getLuaObjectArg<WorldQTree>( L, 1, WORLDQTREE_META );
	Extents ext;
	ext.minx = (int)luaL_checknumber( L, 2 );
	ext.maxx = (int)luaL_checknumber( L, 3 );
	ext.miny = (int)luaL_checknumber( L, 4 );
	ext.maxy = (int)luaL_checknumber( L, 5 );
	ext.minz = 0;
	ext.maxz = 127;
	qtree->kickOutTheseMeshes( &ext );
	qtree->regions->checkForRegionChanges();
	return 0;
}

int WorldQTree::lua_setGpuAllowance( lua_State *L ) {
	WorldQTree *qtree = getLuaObjectArg<WorldQTree>( L, 1, WORLDQTREE_META );
	qtree->gpuAllowanceLeft = (unsigned)luaL_checknumber( L, 2 );
	return 0;
}

int WorldQTree::lua_getGpuAllowance( lua_State *L ) {
	WorldQTree *qtree = getLuaObjectArg<WorldQTree>( L, 1, WORLDQTREE_META );
	lua_pushnumber( L, qtree->gpuAllowanceLeft );
	return 1;
}

int WorldQTree::lua_getLastFrameStats( lua_State *L ) {
	WorldQTree *qtree = getLuaObjectArg<WorldQTree>( L, 1, WORLDQTREE_META );
	lua_pushnumber( L, qtree->trisILD );
	lua_pushnumber( L, qtree->vtxSpaceILD );
	lua_pushnumber( L, qtree->idxSpaceILD );
	lua_pushnumber( L, qtree->texSpaceILD );
	return 4;
}

int WorldQTree::lua_render( lua_State *L ) {
	WorldQTree *qtree = getLuaObjectArg<WorldQTree>( L, 1, WORLDQTREE_META );
	qtree->draw();
	if( lua_toboolean( L, 2 ) )
		qtree->drawLoadingCarat();

	return 0;
}

void WorldQTree::createNew( lua_State *L, MCRegionMap *regions, MCBlockDesc *blocks, unsigned leafShift ) {
	WorldQTree *qtree = new WorldQTree( regions, blocks, leafShift );
	qtree->setupLuaObject( L, WORLDQTREE_META );
	for( unsigned i = 0; i < 6; i++ )
		qtree->lightModels[i].initLua( L );
	qtree->lua_push();
}

int WorldQTree::lua_destroy( lua_State *L ) {
	delete getLuaObjectArg<WorldQTree>( L, 1, WORLDQTREE_META );
	return 0;
}

static const luaL_Reg WorldQTree_functions[] = {
	{ "setPosition", &WorldQTree::lua_setPosition },
	{ "setViewDistance", &WorldQTree::lua_setViewDistance },
	{ "setCameraParams", &WorldQTree::lua_setCameraParams },
	{ "setFog", &WorldQTree::lua_setFog },
	{ "getLightModel", &WorldQTree::lua_getLightModel },

	{ "isLoading", &WorldQTree::lua_isLoading },
	{ "pauseLoading", &WorldQTree::lua_pauseLoading },
	{ "reloadAll", &WorldQTree::lua_reloadAll },
	{ "reloadRegion", &WorldQTree::lua_reloadRegion },
	{ "setGpuAllowance", &WorldQTree::lua_setGpuAllowance },
	{ "getGpuAllowanceLeft", &WorldQTree::lua_getGpuAllowance },
	{ "getLastFrameStats", &WorldQTree::lua_getLastFrameStats },

	{ "render", &WorldQTree::lua_render },
	{ "destroy", &WorldQTree::lua_destroy },
	{ NULL, NULL }
};

void WorldQTree::setupLua( lua_State *L ) {
	luaL_newmetatable( L, WORLDQTREE_META );
	lua_pushvalue( L, -1 );
	lua_setfield( L, -2, "__index" );
	luaL_register( L, NULL, &WorldQTree_functions[0] );
	lua_pop( L, 1 );
}
