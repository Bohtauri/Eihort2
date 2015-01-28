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
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.*/

#include <GL/glew.h>
#include <memory.h>
#include <float.h>
#include <cassert>

#include "blockmaterial.h"
#include "triangle.h"
#include "eihortshader.h"
#include "uidrawcontext.h"
#include "lightmodel.h"

extern unsigned g_width, g_height;

namespace mcgeom {

const unsigned TEX_MATRIX_X_COORD[] = { 4, 4, 0, 0, 4, 4 };
const float TEX_MATRIX_X_SCALE[] = { -1/16.0f, 1/16.0f, 1/16.0f, -1/16.0f, 1/16.0f, 1/16.0f };
const unsigned TEX_MATRIX_Y_COORD[] = { 9, 9, 9, 9, 1, 1 };
const float TEX_MATRIX_Y_SCALE[] = { -1/16.0f, -1/16.0f, -1/16.0f, -1/16.0f, 1/16.0f, 1/16.0f };

void GeometryStream::ensureVertCap( unsigned cap ) {
	if( !vertCapacity )
		vertCapacity = 128;

	while( vertCapacity < cap )
		vertCapacity <<= 1;

	void *newVerts = malloc( vertCapacity );
	memcpy( newVerts, verts, vertSize );
	free( verts );
	verts = newVerts;
}

void GeometryStream::emitVertex( const void *src, unsigned size ) {
	if( vertSize + size > vertCapacity )
		ensureVertCap( vertSize + size );
	memcpy( (unsigned char*)verts + vertSize, src, size );
	vertSize += size;
	vertCount++;
}

GeometryCluster::GeometryCluster()
{
}

GeometryCluster::~GeometryCluster() {
}

unsigned GeometryCluster::getSmallestIndexSize( unsigned nVerts ) {
	if( nVerts < 256 ) {
		return 1;
	} else if( nVerts < 256*256 ) {
		return 2;
	} else {
		return 4;
	}
}

unsigned GeometryCluster::compressIndexBuffer( unsigned *indices, unsigned nVerts, unsigned nIndices ) {
	unsigned indexSize = getSmallestIndexSize( nVerts );

	if( indexSize == 4 )
		return 4;

	if( indexSize == 2 ) {
		unsigned short *dest = (unsigned short*)indices;
		while( nIndices-- )
			*(dest++) = (unsigned short)*(indices++);
	} else { // 1
		unsigned char *dest = (unsigned char*)indices;
		while( nIndices-- )
			*(dest++) = (unsigned char)*(indices++);
	}
	return indexSize;
}

unsigned GeometryCluster::indexSizeToGLType( unsigned size ) {
	switch( size ) {
	case 1: return GL_UNSIGNED_BYTE;
	case 2: return GL_UNSIGNED_SHORT;
	default: return GL_UNSIGNED_INT;
	}
}

MetaGeometryCluster::MetaGeometryCluster( BlockGeometry *geom )
: geom(geom), n(0)
{
}

MetaGeometryCluster::~MetaGeometryCluster() {
}

bool MetaGeometryCluster::destroyIfEmpty() {
	if( n == 0 ) {
		delete this;
		return true;
	}
	return false;
}

void MetaGeometryCluster::finalize( GeometryStream *meta, GeometryStream*, GeometryStream* ) {
	meta->emitVertex( geom );
	meta->emitVertex( n );
	meta->emitVertex( str.getVertices(), str.getVertSize() );
}


template< typename Extra >
SingleStreamGeometryClusterEx<Extra>::SingleStreamGeometryClusterEx( BlockGeometry *geom )
: geom(geom)
{
}

template< typename Extra >
SingleStreamGeometryClusterEx<Extra>::SingleStreamGeometryClusterEx( BlockGeometry *geom, Extra ex )
: extra(ex), geom(geom)
{
}

template< typename Extra >
SingleStreamGeometryClusterEx<Extra>::~SingleStreamGeometryClusterEx() {
}

template< typename Extra >
bool SingleStreamGeometryClusterEx<Extra>::destroyIfEmpty() {
	if( str.getTriCount() == 0 ) {
		delete this;
		return true;
	}
	return false;
}

template< typename Extra >
void SingleStreamGeometryClusterEx<Extra>::finalize( GeometryStream *meta, GeometryStream *vtx, GeometryStream *idx ) {
	Meta mdata;

	vtx->alignVertices();
	mdata.vtx_offset = vtx->getVertSize();
	vtx->emitVertex( str.getVertices(), str.getVertSize() );
	mdata.nVerts = str.getVertCount();

	unsigned idxSize = compressIndexBuffer( str.getIndices(), str.getVertCount(), str.getTriCount()*3 );

	idx->alignVertices( idxSize );
	mdata.idx_offset = idx->getVertSize();
	idx->emitVertex( str.getIndices(), str.getTriCount()*3*idxSize );
	mdata.nTris = str.getTriCount();
	mdata.idxType = indexSizeToGLType( idxSize );

	meta->emitVertex( geom );
	emitExtra( meta );
	meta->emitVertex( mdata );

	delete this;
}

template< typename Extra >
void SingleStreamGeometryClusterEx<Extra>::emitExtra( GeometryStream *meta ) {
	meta->emitVertex( extra );
}

template<>
void SingleStreamGeometryClusterEx<EmptyStruct>::emitExtra( GeometryStream* ) {
}

template< unsigned N >
MultiStreamGeometryCluster<N>::MultiStreamGeometryCluster( BlockGeometry *geom )
: geom(geom)
{
	for( unsigned i = N > 6 ? 6 : N; i--; ) {
		jVec3Zero( &cutoutVectors[i] );
		cutoutVectors[i].v[i>>1] = i&1 ? 1.0f : -1.0f;
	}
}

template< unsigned N >
MultiStreamGeometryCluster<N>::~MultiStreamGeometryCluster() {
}

template< unsigned N >
bool MultiStreamGeometryCluster<N>::destroyIfEmpty() {
	for( unsigned i = 0; i < N; i++ )
		if( str[i].getVertCount() )
			return false;

	delete this;
	return true;
}

template< unsigned N >
void MultiStreamGeometryCluster<N>::finalize( GeometryStream *meta, GeometryStream *vtx, GeometryStream *idx ) {
	Meta1 m1;
	m1.n = 0;
	for( unsigned i = 0; i < N; i++ ) {
		if( str[i].getVertCount() )
			m1.n++;
	}

	meta->emitVertex( geom );
	meta->emitVertex( m1 );

	for( unsigned i = 0; i < N; i++ ) {
		if( str[i].getVertCount() ) {
			Meta2 m2;
			vtx->alignVertices();
			m2.vtx_offset = vtx->getVertSize();
			m2.nVerts = str[i].getVertCount();

			unsigned idxType = compressIndexBuffer( str[i].getIndices(), str[i].getVertCount(), str[i].getTriCount()*3 );
			idx->alignVertices( idxType );
			m2.idx_offset = idx->getVertSize();
			m2.idxType = indexSizeToGLType( idxType );
			m2.nTris = str[i].getTriCount();

			m2.dir = i;

			unsigned vtxSize = str[i].getVertSize();
			vtx->emitVertex( str[i].getVertices(), vtxSize );
			
			unsigned idxSize = m2.nTris*3*idxType;
			idx->emitVertex( str[i].getIndices(), idxSize );

			if( cutoutVectors[i].x == 0.0f && cutoutVectors[i].y == 0.0f && cutoutVectors[i].z == 0.0f ) {
				// Never cut out
				jVec3Zero( &m2.cutoutPlane.n );
				m2.cutoutPlane.d = 1.0f;
			} else {
				// Get a good cutout plane
				jVec3Copy( &m2.cutoutPlane.n, &cutoutVectors[i] );
				m2.cutoutPlane.d = 0.0f;
				unsigned vtxStride = str[i].getVertSize()/m2.nVerts;
				float minD = FLT_MAX;
				for( unsigned v = 0; v < vtxSize; v += vtxStride ) {
					short *pos = (short*)((char*)str[i].getVertices() + v);
					float d = m2.cutoutPlane.n.x * pos[0] + m2.cutoutPlane.n.y * pos[1] + m2.cutoutPlane.n.z * pos[2];
					if( d < minD )
						minD = d;
				}
				m2.cutoutPlane.d = -minD / 16.0f;
			}

			meta->emitVertex( m2 );
		}
	}

	delete this;
}

MultiGeometryCluster::MultiGeometryCluster()
{
}

MultiGeometryCluster::~MultiGeometryCluster() {
}

bool MultiGeometryCluster::destroyIfEmpty() {
	bool nonEmpty = false;
	for( unsigned i = 0; i < clusters.size(); i++ ) {
		if( clusters[i] ) {
			if( clusters[i]->destroyIfEmpty() ) {
				clusters[i] = NULL;
			} else {
				nonEmpty = true;
			}
		}
	}

	if( !nonEmpty ) {
		delete this;
		return true;
	}

	return false;
}

void MultiGeometryCluster::finalize( GeometryStream *meta, GeometryStream *vtx, GeometryStream *idx ) {
	for( unsigned i = 0; i < clusters.size(); i++ ) {
		if( clusters[i] ) {
			clusters[i]->finalize( meta, vtx, idx );
			clusters[i] = NULL;
		}
	}
}

GeometryCluster *MultiGeometryCluster::getCluster( unsigned i ) {
	if( i >= clusters.size() )
		return NULL;
	return clusters[i];
}

void MultiGeometryCluster::newCluster( unsigned i, GeometryCluster *cluster ) {
	if( i >= clusters.size() )
		clusters.resize( i+1, NULL );
	assert( clusters[i] == NULL );
	clusters[i] = cluster;
}

void MultiGeometryCluster::storeContinueIsland( const IslandDesc *island ) {
	ciFn = island->continueIsland;
	ciCookie = island->cookie;
	cipCookie = island->pcookie;
	subCluster = island->curCluster;
}

bool MultiGeometryCluster::callContinueIsland( IslandDesc *island, const InstanceContext *nextBlock ) {
	if( !ciFn )
		return true;

	unsigned cookie = island->cookie;
	void *pcookie = island->pcookie;
	island->cookie = ciCookie;
	island->pcookie = cipCookie;
	island->curCluster = subCluster;
	bool ret = ciFn( island, nextBlock );
	island->cookie = cookie;
	island->pcookie = pcookie;
	island->curCluster = this;

	return ret;
}

BlockGeometry::BlockGeometry()
{
	rg = RenderGroup::LAST;
}

BlockGeometry::~BlockGeometry() {
}

void BlockGeometry::render( void *&meta, RenderContext *ctx ) {
	assert( false );
	(void)meta;
	(void)ctx;
}

void BlockGeometry::emitIsland( GeometryCluster*, const IslandDesc* ) {
}

BlockGeometry::IslandMode BlockGeometry::beginIsland( IslandDesc* ) {
	return ISLAND_NORMAL;
}

ForwardingMultiGeometryAdapter::ForwardingMultiGeometryAdapter()
{
	for( unsigned i = 0; i < 16; i++ )
		geoms[i] = NULL;
}

ForwardingMultiGeometryAdapter::~ForwardingMultiGeometryAdapter() {
}

GeometryCluster *ForwardingMultiGeometryAdapter::newCluster() {
	return new MultiGeometryCluster;
}

bool ForwardingMultiGeometryAdapter::beginEmit( GeometryCluster *outCluster, InstanceContext *ctx ) {
	MultiGeometryCluster *out = static_cast<MultiGeometryCluster*>(outCluster);
	unsigned idx = selectGeometry( ctx );
	assert( geoms[idx] );
	if( !out->getCluster( idx ) )
		out->newCluster( idx, geoms[idx]->newCluster() );
	return geoms[idx]->beginEmit( out->getCluster( idx ), ctx );
}

void ForwardingMultiGeometryAdapter::emitIsland( GeometryCluster *outCluster, const IslandDesc *ctx ) {
	MultiGeometryCluster *out = static_cast<MultiGeometryCluster*>(outCluster);
	unsigned idx = selectGeometry( ctx );
	assert( geoms[idx] );
	if( !out->getCluster( idx ) )
		out->newCluster( idx, geoms[idx]->newCluster() );
	geoms[idx]->emitIsland( out->getCluster( idx ), ctx );
}

BlockGeometry::IslandMode ForwardingMultiGeometryAdapter::beginIsland( IslandDesc *ctx ) {
	unsigned idx = selectGeometry( ctx );
	if( !geoms[idx] )
		return ISLAND_CANCEL;

	MultiGeometryCluster *out = static_cast<MultiGeometryCluster*>(ctx->curCluster);
	if( !out->getCluster( idx ) )
		out->newCluster( idx, geoms[idx]->newCluster() );
	GeometryCluster *subCluster = out->getCluster( idx );
	ctx->curCluster = subCluster;
	ctx->continueIsland = NULL;

	BlockGeometry::IslandMode ret = geoms[idx]->beginIsland( ctx );

	out->storeContinueIsland( ctx );
	ctx->continueIsland = &continueIsland;
	ctx->cookie = idx;
	ctx->pcookie = (void*)this;
	ctx->curCluster = out;
	return ret;
}

unsigned ForwardingMultiGeometryAdapter::selectGeometry( const IslandDesc *ctx ) {
	return selectGeometry( &ctx->origin );
}

bool ForwardingMultiGeometryAdapter::continueIsland( IslandDesc *island, const InstanceContext *nextBlock ) {
	IslandDesc ctx;
  memset(&ctx, 0, sizeof(ctx));
	ctx.origin = *nextBlock;
	ctx.islandAxis = island->islandAxis;
	return island->cookie == ((ForwardingMultiGeometryAdapter*)island->pcookie)->selectGeometry( &ctx )
		&& static_cast<MultiGeometryCluster*>(island->curCluster)->callContinueIsland( island, nextBlock );
}


DataBasedMultiGeometryAdapter::DataBasedMultiGeometryAdapter( unsigned mask, BlockGeometry **geoms )
: mask(mask)
{
	assert( geoms[0] );
	rg = geoms[0]->getRenderGroup();
	for( unsigned i = 0; i < 16; i++ )
		this->geoms[i] = geoms[i];
}

DataBasedMultiGeometryAdapter::~DataBasedMultiGeometryAdapter() {
}

bool DataBasedMultiGeometryAdapter::beginEmit( GeometryCluster *outCluster, InstanceContext *ctx ) {
	MultiGeometryCluster *out = static_cast<MultiGeometryCluster*>(outCluster);
	unsigned idx = selectGeometry( ctx );
	assert( geoms[idx] );
	if( !out->getCluster( idx ) )
		out->newCluster( idx, geoms[idx]->newCluster() );
	unsigned short oldData = ctx->block.data;
	ctx->block.data &= ~mask;
	bool ret = geoms[idx]->beginEmit( out->getCluster( idx ), ctx );
	ctx->block.data = oldData;
	return ret;
}

BlockGeometry::IslandMode DataBasedMultiGeometryAdapter::beginIsland( IslandDesc *ctx ) {
	unsigned idx = DataBasedMultiGeometryAdapter::selectGeometry( &ctx->origin );
	assert( geoms[idx] );
	MultiGeometryCluster *out = static_cast<MultiGeometryCluster*>(ctx->curCluster);
	GeometryCluster *subCluster = out->getCluster( idx );
	assert( subCluster );
	ctx->curCluster = subCluster;
	ctx->continueIsland = NULL;

	BlockGeometry::IslandMode ret = geoms[idx]->beginIsland( ctx );

	out->storeContinueIsland( ctx );
	ctx->continueIsland = &continueIsland;
	ctx->cookie = mask;
	ctx->curCluster = out;
	return ret;
}

unsigned DataBasedMultiGeometryAdapter::selectGeometry( const InstanceContext *ctx ) {
	return ctx->block.data & mask;
}

bool DataBasedMultiGeometryAdapter::continueIsland( IslandDesc *island, const InstanceContext *nextBlock ) {
	return ((nextBlock->block.data ^ island->origin.block.data) & island->cookie) == 0
		&& static_cast<MultiGeometryCluster*>(island->curCluster)->callContinueIsland( island, nextBlock );
}

RotatingMultiGeometryAdapter::RotatingMultiGeometryAdapter( BlockGeometry **geoms, unsigned *geomSides )
{
	unsigned defSides[6];
	if( !geomSides ) {
		geomSides = &defSides[0];
		defSides[0] = 1;
		for( unsigned i = 1; i < 6; i++ )
			defSides[i] = 0;
	}
	for( unsigned i = 0; i < 6; i++ ) {
		sideToGeom[i] = geomSides[i];
		this->geoms[sideToGeom[i]] = geoms[sideToGeom[i]];
	}
	rg = geoms[sideToGeom[5]]->getRenderGroup();
}

RotatingMultiGeometryAdapter::~RotatingMultiGeometryAdapter() {
}

unsigned RotatingMultiGeometryAdapter::selectGeometry( const InstanceContext* ) {
	return sideToGeom[0];
}

unsigned RotatingMultiGeometryAdapter::selectGeometry( const IslandDesc *ctx ) {
	if( ctx->islandAxis < 4 ) {
		unsigned dir = ((((ctx->islandAxis & 1) << 1) | ((ctx->islandAxis & 2) >> 1)) + 2) & 3;
		return sideToGeom[(ctx->origin.block.data + dir) & 3];
	} else {
		return sideToGeom[ctx->islandAxis];
	}
}

FaceBitGeometryAdapter::FaceBitGeometryAdapter( BlockGeometry *geom )
{
	assert( geom );
	rg = geom->getRenderGroup();
	geoms[0] = geom;
	geoms[1] = NULL;
}

FaceBitGeometryAdapter::~FaceBitGeometryAdapter() {
}

unsigned FaceBitGeometryAdapter::selectGeometry( const InstanceContext* ) {
	return 0;
}

unsigned FaceBitGeometryAdapter::selectGeometry( const IslandDesc *ctx ) {
	if( ctx->islandAxis >= 4 ) {
		// TODO: Vines attach to the bottom of solid blocks
		return 1u;
	}

	unsigned ax = ((ctx->islandAxis & 1u) << 1) - (ctx->islandAxis >> 1);
	ax &= 3u;
	return ctx->origin.block.data & (1u << ax) ? 0u : 1u;
}

FacingMultiGeometryAdapter::FacingMultiGeometryAdapter( BlockGeometry *geom1, BlockGeometry *geom2 )
{
	assert( geom1 || geom2 );
	rg = geom1 ? geom1->getRenderGroup() : geom2->getRenderGroup();
	geoms[0] = geom1;
	geoms[1] = geom2;
}

FacingMultiGeometryAdapter::~FacingMultiGeometryAdapter() {
}

unsigned FacingMultiGeometryAdapter::selectGeometry( const InstanceContext* ) {
	return geoms[0] ? 0u : 1u;
}

unsigned FacingMultiGeometryAdapter::selectGeometry( const IslandDesc *ctx ) {
	return ctx->islandAxis == ctx->origin.block.data - 2u ? 1u : 0u;
}

TopModifiedMultiGeometryAdapter::TopModifiedMultiGeometryAdapter( BlockGeometry *normal, BlockGeometry *modified, unsigned modifier )
: modifier(modifier)
{
	rg = normal->getRenderGroup();
	geoms[0] = normal;
	geoms[1] = modified;
}

TopModifiedMultiGeometryAdapter::~TopModifiedMultiGeometryAdapter() {
}

unsigned TopModifiedMultiGeometryAdapter::selectGeometry( const InstanceContext *ctx ) {
	return ctx->sides[5].id == modifier ? 1 : 0;
}

unsigned TopModifiedMultiGeometryAdapter::selectGeometry( const IslandDesc *ctx ) {
	return TopModifiedMultiGeometryAdapter::selectGeometry( &ctx->origin );
}

SolidBlockGeometry::SolidBlockGeometry( unsigned tx, unsigned color )
: color(color)
{
	rg = RenderGroup::OPAQUE;
	for( unsigned i = 0; i < 6; i++ )
		tex[i] = tx;
	xTexScale = 1.0f;
	yTexScale = 1.0f;
}

SolidBlockGeometry::SolidBlockGeometry( unsigned *tx, unsigned color )
: color(color)
{
	rg = RenderGroup::OPAQUE;
	for( unsigned i = 0; i < 6; i++ )
		tex[i] = tx[i];
	xTexScale = 1.0f;
	yTexScale = 1.0f;
}

SolidBlockGeometry::~SolidBlockGeometry() {
}

GeometryCluster *SolidBlockGeometry::newCluster() {
	return new SixSidedGeometryCluster( this );
}

void SolidBlockGeometry::render( void *&metaData, RenderContext *ctx ) {
	ctx->shader->bindTexGen();
	applyColor();
	solidBlockRender( metaData, ctx );
}

void SolidBlockGeometry::solidBlockRender( void *&metaData, RenderContext *ctx ) {
	SixSidedGeometryCluster::Meta1 *m1 = (SixSidedGeometryCluster::Meta1*)metaData;
	SixSidedGeometryCluster::Meta2 *m2 = (SixSidedGeometryCluster::Meta2*)((char*)metaData + sizeof(SixSidedGeometryCluster::Meta1));

	glEnableClientState( GL_VERTEX_ARRAY );
	glEnable( GL_TEXTURE_2D );

	glMatrixMode( GL_TEXTURE );
	float glmat[16];
	for( unsigned i = 0; i < 16; i++ )
		glmat[i] = 0.0f;

	unsigned prevT = 0;

	for( unsigned i = 0; i < m1->n; i++, m2++ ) {
		if( jPlaneDot3( &m2->cutoutPlane, &ctx->viewPos ) >= 0.0f ) {
			if( prevT != tex[m2->dir] )
				glBindTexture( GL_TEXTURE_2D, prevT = tex[m2->dir] );

			glVertexPointer( 3, GL_SHORT, sizeof( Vertex ), (void*)m2->vtx_offset );
			glNormal3fv( m2->cutoutPlane.n.v );
			ctx->lightModels[m2->dir].uploadGL();
			//const float *lo = LIGHT_OFFSETS + (m2->dir << 1);
			//ctx->shader->setLightOffset( lo[0], lo[1] );

			unsigned texCoordX = TEX_MATRIX_X_COORD[m2->dir];
			unsigned texCoordY = TEX_MATRIX_Y_COORD[m2->dir];
			glmat[texCoordX] = TEX_MATRIX_X_SCALE[m2->dir] * xTexScale;
			glmat[texCoordY] = TEX_MATRIX_Y_SCALE[m2->dir] * yTexScale;
			glLoadMatrixf( &glmat[0] );

			ctx->renderedTriCount += m2->nTris;
			glDrawElements( GL_TRIANGLES, m2->nTris*3, m2->idxType, (void*)m2->idx_offset );

			glmat[texCoordX] = 0.0f;
			glmat[texCoordY] = 0.0f;
		}
	}

	glDisableClientState( GL_VERTEX_ARRAY );
	glDisable( GL_TEXTURE_2D );

	metaData = (void*)m2;
}

bool SolidBlockGeometry::beginEmit( GeometryCluster*, InstanceContext* ) {
	return true;
}

BlockGeometry::IslandMode SolidBlockGeometry::beginIsland( IslandDesc *ctx ) {
	if( !tex[ctx->islandAxis] )
		return ISLAND_CANCEL;
	return BlockGeometry::beginIsland( ctx );
}

void SolidBlockGeometry::emitIsland( GeometryCluster *outCluster, const IslandDesc *ctx ) {
	emitContour( ((SixSidedGeometryCluster*)outCluster)->getStream( ctx->islandAxis ), ctx, 0 );
}

void SolidBlockGeometry::emitContour( GeometryStream *out, const IslandDesc *ctx, int offsetPx ) {
	if( ctx->nContourPoints == 4 && ctx->nHoles == 0 ) {
		// Big quad
		emitQuad( out, ctx, offsetPx );
	} else {
		// Strange shape
		triangulateio triIn;
		triangulateio triOut;
		islandToTriangleIO( ctx, &triIn );
		triOut.pointlist = NULL;
		triOut.pointmarkerlist = NULL;
		triOut.trianglelist = NULL;
		triOut.segmentlist = NULL;
		triangulate( const_cast<char*>("QjzpBP"), &triIn, &triOut, NULL );
		freeIslandTriangleIO( &triIn );
		emitTriangulated( out, ctx, &triOut, offsetPx );
	}
}

unsigned SolidBlockGeometry::serializeContourPoints( const IslandDesc *ctx, unsigned nContourPoints, const Point *points, REALVec *outVec, int *segments, int baseSegIdx, std::vector<REALVec> &extraHoles ) {
	unsigned pointCount = 1;
	int prevPoint = 0;

	outVec[0].v[0] = (double)ctx->xd1 * points[0].v[ctx->xax];
	outVec[0].v[1] = (double)ctx->yd1 * points[0].v[ctx->yax];
	segments[1] = baseSegIdx;

	for( unsigned i = 1; i < nContourPoints; i++ ) {
		double x = (double)ctx->xd1 * points[i].v[ctx->xax];
		double y = (double)ctx->yd1 * points[i].v[ctx->yax];
		unsigned pointIndex;

		// TODO: Use a better algorithm for this!
		// Slows down big contour processing to cover a very rare case
		for( pointIndex = 0; pointIndex < pointCount; pointIndex++ ) {
			if( abs( x - outVec[pointIndex].v[0] ) < 0.01 && abs( y - outVec[pointIndex].v[1] ) < 0.01 ) {
				// An extra hole point is needed here
				REALVec d1 = {{ outVec[pointIndex+1].v[0] - x, outVec[pointIndex+1].v[1] - y }};
				REALVec d2 = {{ outVec[prevPoint].v[0] - x, outVec[prevPoint].v[1] - y }};
				d1.scaleToLenAA( 0.5 );
				d2.scaleToLenAA( 0.5 );
				d1.add( d2 );
				REALVec v = outVec[pointIndex];
				v.add( d1 );
				extraHoles.push_back( v );
				/*
				// An extra point for direction independence
				v = outVec[pointIndex];
				v.sub( d1 );
				extraHoles.push_back( v );
				*/
				goto found_identical_point;
			}
		}

		outVec[pointCount].v[0] = x;
		outVec[pointCount].v[1] = y;
		pointCount++;

found_identical_point:
		segments[i<<1] = baseSegIdx + prevPoint;
		segments[(i<<1)+1] = baseSegIdx + pointIndex;
		prevPoint = pointIndex;
	}
	segments[0] = baseSegIdx + prevPoint;

	return pointCount;
}

void SolidBlockGeometry::islandToTriangleIO( const IslandDesc *ctx, struct triangulateio *out ) {
	std::vector< REALVec > extraHolePoints;

	unsigned totalPointCount = ctx->nContourPoints;
	if( ctx->nHoles )
		totalPointCount += ctx->holeContourEnd[ctx->nHoles-1];

	REALVec *contourReals = new REALVec[totalPointCount];
	out->pointlist = &contourReals[0].v[0];
	int *segments = new int[totalPointCount<<1];
	out->segmentlist = &segments[0];

	// Construct the main contour PSLG
	unsigned pointCount = serializeContourPoints( ctx, ctx->nContourPoints, ctx->contourPoints, contourReals, segments, 0, extraHolePoints );
	
	if( ctx->nHoles ) {
		// Create PSLG points and segments for all the holes
		contourReals += pointCount;
		segments += ctx->nContourPoints<<1;
		unsigned contourPointIdx = 0;
		for( unsigned i = 0; i < ctx->nHoles; i++ ) {
			unsigned nPointsIn = ctx->holeContourEnd[i] - contourPointIdx;
			unsigned nPointsOut = serializeContourPoints( ctx, nPointsIn, ctx->holeContourPoints + contourPointIdx, contourReals, segments, pointCount, extraHolePoints );
			contourReals += nPointsOut;
			segments += nPointsIn<<1;
			contourPointIdx += nPointsIn;
			pointCount += nPointsOut;
		}
	}
	
	// Coalesce the hole list
	out->numberofholes = (int)(ctx->nHoles + (unsigned)extraHolePoints.size());
	if( out->numberofholes ) {
		REALVec *holes = new REALVec[out->numberofholes];
		out->holelist = &holes[0].v[0];
		unsigned i = 0;
		for( ; i < ctx->nHoles; i++ ) {
			holes[i].v[0] = (double)ctx->xd1 * ((double)ctx->holeInsidePoint[i].v[ctx->xax] + 0.5);
			holes[i].v[1] = (double)ctx->yd1 * ((double)ctx->holeInsidePoint[i].v[ctx->yax] + 0.5);
		}
		for( unsigned j = 0; j < extraHolePoints.size(); j++, i++ )
			holes[i] = extraHolePoints[j];
	}

	out->numberofpoints = pointCount;
	out->numberofpointattributes = 0;
	out->pointmarkerlist = NULL;
	out->numberofsegments = totalPointCount;
	out->segmentmarkerlist = NULL;
	out->numberofregions = 0;
}

void SolidBlockGeometry::freeIslandTriangleIO( triangulateio *out ) {
	delete[] out->segmentlist;
	delete[] out->pointlist;

	if( out->numberofholes )
		delete[] out->holelist;
}

void SolidBlockGeometry::emitTriangulated( GeometryStream *target, const IslandDesc *ctx, triangulateio *tri, int offsetPx ) {
	unsigned indexBase = target->getIndexBase();

	Vertex vtx;
	vtx.pos[ctx->zax] = (short)(ctx->contourPoints[0].v[ctx->zax] * 16 + ctx->zd * offsetPx );
	for( unsigned i = 0; i < (unsigned)tri->numberofpoints; i++ ) {
		vtx.pos[ctx->xax] = (short)(ctx->xd1 * (int)floor( tri->pointlist[i<<1] ) * 16);
		vtx.pos[ctx->yax] = (short)(ctx->yd1 * (int)floor( tri->pointlist[(i<<1)+1] ) * 16);
		
		target->emitVertex( vtx );
	}

	int *triangle = tri->trianglelist;
	for( unsigned i = 0; i < (unsigned)tri->numberoftriangles; i++ ) {
		target->emitTriangle( (unsigned)triangle[0] + indexBase, (unsigned)triangle[1] + indexBase, (unsigned)triangle[2] + indexBase );
		triangle += 3;
	}

	trifree( tri->pointlist );
	trifree( tri->trianglelist );
	//trifree( tri->segmentlist );
}

void SolidBlockGeometry::emitQuad( GeometryStream *target, const IslandDesc *ctx, int offsetPx ) {
	unsigned indexBase = target->getIndexBase();

	short offset = (short)(ctx->zd * offsetPx);
	Vertex vtx;
	for( unsigned i = 0; i < 4; i++ ) {
		vtx.pos[0] = (short)(ctx->contourPoints[i].x * 16);
		vtx.pos[1] = (short)(ctx->contourPoints[i].y * 16);
		vtx.pos[2] = (short)(ctx->contourPoints[i].z * 16);
		vtx.pos[ctx->zax] += offset;

		target->emitVertex( vtx );
	}

	target->emitQuad( indexBase, indexBase+1, indexBase+2, indexBase+3 );
}

void SolidBlockGeometry::emitQuad( GeometryStream *target, const IslandDesc *ctx, int *offsets ) {
	unsigned indexBase = target->getIndexBase();

	// Find the max extents of the quad
	Point highPos = ctx->contourPoints[0];
	for( unsigned i = 1; i < 4; i++ ) {
		for( unsigned j = 0; j < 3; j++ ) {
			if( ctx->contourPoints[i].v[j] > highPos.v[j] )
				highPos.v[j] = ctx->contourPoints[i].v[j];
		}
	}
	if( (ctx->islandAxis & 1) == 0 )
		highPos.v[ctx->islandAxis >> 1]++;

	// Emit the offset vertices
	Vertex vtx;
	for( unsigned i = 0; i < 4; i++ ) {
		vtx.pos[0] = (short)(ctx->contourPoints[i].x * 16 + (ctx->contourPoints[i].x == highPos.x ? offsets[1] : -offsets[0]));
		vtx.pos[1] = (short)(ctx->contourPoints[i].y * 16 + (ctx->contourPoints[i].y == highPos.y ? offsets[3] : -offsets[2]));
		vtx.pos[2] = (short)(ctx->contourPoints[i].z * 16 + (ctx->contourPoints[i].z == highPos.z ? offsets[5] : -offsets[4]));

		target->emitVertex( vtx );
	}

	target->emitQuad( indexBase, indexBase+1, indexBase+2, indexBase+3 );
}

void SolidBlockGeometry::applyColor() {
	float colors[4];
	colors[0] = ((color>>16)&0xff) / 255.0f;
	colors[1] = ((color>>8)&0xff) / 255.0f;
	colors[2] = (color&0xff) / 255.0f;
	colors[3] = 1.0f;
	glMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, &colors[0] );
}

FoliageBlockGeometry::FoliageBlockGeometry( unsigned tx, unsigned foliageTex )
: SolidBlockGeometry(tx), foliageTex(foliageTex)
{
}

FoliageBlockGeometry::FoliageBlockGeometry( unsigned *tx, unsigned foliageTex )
: SolidBlockGeometry(tx), foliageTex(foliageTex)
{
}

FoliageBlockGeometry::~FoliageBlockGeometry() {
}

BlockGeometry::IslandMode FoliageBlockGeometry::beginIsland( IslandDesc *ctx ) {
	ctx->checkFacingSameId = false;
	return SolidBlockGeometry::beginIsland( ctx );
}

void FoliageBlockGeometry::render( void *&meta, RenderContext *ctx ) {
	ctx->shader->bindFoliage();
	applyColor();
	glActiveTexture( GL_TEXTURE2 );
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, ctx->biomeTextures[foliageTex] );
	glActiveTexture( GL_TEXTURE0 );

	solidBlockRender( meta, ctx );

	glActiveTexture( GL_TEXTURE2 );
	glDisable( GL_TEXTURE_2D );
	glActiveTexture( GL_TEXTURE0 );
}

FoliageAlphaBlockGeometry::FoliageAlphaBlockGeometry( unsigned tx, unsigned foliageTex )
: FoliageBlockGeometry( tx, foliageTex )
{
}

FoliageAlphaBlockGeometry::FoliageAlphaBlockGeometry( unsigned *tx, unsigned foliageTex )
: FoliageBlockGeometry( tx, foliageTex )
{
}

FoliageAlphaBlockGeometry::~FoliageAlphaBlockGeometry() {
}

void FoliageAlphaBlockGeometry::render( void *&meta, RenderContext *ctx ) {
	ctx->shader->bindFoliageAlpha();
	applyColor();
	glActiveTexture( GL_TEXTURE2 );
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, ctx->biomeTextures[foliageTex] );
	glActiveTexture( GL_TEXTURE0 );

	solidBlockRender( meta, ctx );

	glActiveTexture( GL_TEXTURE2 );
	glDisable( GL_TEXTURE_2D );
	glActiveTexture( GL_TEXTURE0 );
}


TransparentSolidBlockGeometry::TransparentSolidBlockGeometry( unsigned tx, unsigned color )
: SolidBlockGeometry( tx, color )
{
	rg = RenderGroup::TRANSPARENT;
}

TransparentSolidBlockGeometry::TransparentSolidBlockGeometry( unsigned *tx, unsigned color )
: SolidBlockGeometry( tx, color )
{
	rg = RenderGroup::TRANSPARENT;
}

TransparentSolidBlockGeometry::~TransparentSolidBlockGeometry() {
}

void TransparentSolidBlockGeometry::render( void *&metaData, RenderContext *ctx ) {
	glEnable( GL_BLEND );
	glAlphaFunc( GL_GREATER, 0.01f );
	SolidBlockGeometry::render( metaData, ctx );
	glAlphaFunc( GL_GREATER, 0.5f );
	glDisable( GL_BLEND );
}

PortalBlockGeometry::PortalBlockGeometry( unsigned tx, unsigned color )
: TransparentSolidBlockGeometry(tx, color)
{
}

PortalBlockGeometry::~PortalBlockGeometry() {
}

void PortalBlockGeometry::emitIsland( GeometryCluster *outCluster, const IslandDesc *island ) {
	unsigned portalAxis = 0;
	if( island->origin.sides[2].id != island->origin.block.id && island->origin.sides[3].id != island->origin.block.id )
		portalAxis = 1;
	
	GeometryStream *out = ((SixSidedGeometryCluster*)outCluster)->getStream( island->islandAxis );
	if( (island->islandAxis >> 1) == portalAxis || island->nContourPoints != 4 ) {
		emitContour( out, island, -6 );
	} else {
		int offsets[6];
		for( unsigned i = 0; i < 6; i++ )
			offsets[i] = 0;
		offsets[portalAxis<<1] = offsets[(portalAxis<<1)+1] = -6;
		emitQuad( out, island, &offsets[0] );
	}
}


CactusBlockGeometry::CactusBlockGeometry( unsigned tx, int *offsets )
: SolidBlockGeometry( tx )
{
	if( offsets ) {
		for( unsigned i = 0; i < 6; i++ )
			this->offsets[i] = offsets[i];
	} else {
		for( unsigned i = 0; i < 4; i++ )
			this->offsets[i] = -1;
		this->offsets[4] = 0;
		this->offsets[5] = 0;
	}
}

CactusBlockGeometry::CactusBlockGeometry( unsigned *tx, int *offsets )
: SolidBlockGeometry( tx )
{
	if( offsets ) {
		for( unsigned i = 0; i < 6; i++ )
			this->offsets[i] = offsets[i];
	} else {
		for( unsigned i = 0; i < 4; i++ )
			this->offsets[i] = -1;
		this->offsets[4] = 0;
		this->offsets[5] = 0;
	}
}

CactusBlockGeometry::~CactusBlockGeometry() {
}

GeometryCluster *CactusBlockGeometry::newCluster() {
	SixSidedGeometryCluster *cluster = new SixSidedGeometryCluster( this );
	jVec3 zero;
	jVec3Zero( &zero );
	for( unsigned i = 0; i < 4; i++ )
		cluster->setCutoutVector( i, &zero );
	return cluster;
}

BlockGeometry::IslandMode CactusBlockGeometry::beginIsland( IslandDesc *ctx ) {
	ctx->checkFacingSameId = false;
	ctx->checkVisibility = false;
	return SolidBlockGeometry::beginIsland( ctx );
}

void CactusBlockGeometry::emitIsland( GeometryCluster *outCluster, const IslandDesc *ctx ) {
	emitContour( ((SixSidedGeometryCluster*)outCluster)->getStream( ctx->islandAxis ), ctx, offsets[ctx->islandAxis] );
}

void CactusBlockGeometry::render( void *&meta, RenderContext *ctx ) {
	glDisable( GL_CULL_FACE );
	SolidBlockGeometry::render( meta, ctx );
	glEnable( GL_CULL_FACE );
}

BiomeCactusBlockGeometry::BiomeCactusBlockGeometry( unsigned tx, int channel, int *offsets )
: CactusBlockGeometry( tx, offsets ), biomeChannel(channel)
{
}

BiomeCactusBlockGeometry::BiomeCactusBlockGeometry( unsigned *tx, int channel, int *offsets )
: CactusBlockGeometry( tx, offsets ), biomeChannel(channel)
{
}

BiomeCactusBlockGeometry::~BiomeCactusBlockGeometry() {
}

void BiomeCactusBlockGeometry::render( void *&meta, RenderContext *ctx ) {
	ctx->shader->bindFoliage();
	applyColor();
	glActiveTexture( GL_TEXTURE2 );
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, ctx->biomeTextures[biomeChannel] );
	glActiveTexture( GL_TEXTURE0 );
	glDisable( GL_CULL_FACE );

	solidBlockRender( meta, ctx );

	glEnable( GL_CULL_FACE );
	glActiveTexture( GL_TEXTURE2 );
	glDisable( GL_TEXTURE_2D );
	glActiveTexture( GL_TEXTURE0 );
}

FullBrightBlockGeometry::FullBrightBlockGeometry( unsigned tx, unsigned color )
: SolidBlockGeometry( tx, color )
{
	rg = RenderGroup::OPAQUE;
}

FullBrightBlockGeometry::FullBrightBlockGeometry( unsigned *tx, unsigned color )
: SolidBlockGeometry( tx, color )
{
	rg = RenderGroup::OPAQUE;
}

FullBrightBlockGeometry::~FullBrightBlockGeometry() {
}

void FullBrightBlockGeometry::render( void *&metaData, RenderContext *ctx ) {
	ctx->shader->bindTexGen();
	if( ctx->enableBlockLighting )
		ctx->shader->setLightOffset( 1.0f, 1.0f );
	solidBlockRender( metaData, ctx );
	ctx->shader->setLightOffset( 0.0f, 0.0f );
}

SquashedSolidBlockGeometry::SquashedSolidBlockGeometry( int top, int bottom, unsigned tx, unsigned color )
: SolidBlockGeometry( tx, color )
, top(top)
, bottom(bottom)
{
}

SquashedSolidBlockGeometry::SquashedSolidBlockGeometry( int top, int bottom, unsigned *tx, unsigned color )
: SolidBlockGeometry( tx, color )
, top(top)
, bottom(bottom)
{
}

SquashedSolidBlockGeometry::~SquashedSolidBlockGeometry() {
}

GeometryCluster *SquashedSolidBlockGeometry::newCluster() {
	SixSidedGeometryCluster *cluster = new SixSidedGeometryCluster( this );
	jVec3 n;
	jVec3Set( &n, 0.0f, 0.0f, -(16-2*bottom) / 16.0f );
	cluster->setCutoutVector( 4, &n );
	jVec3Set( &n, 0.0f, 0.0f, (16-2*top) / 16.0f );
	cluster->setCutoutVector( 5, &n );

	return cluster;
}

bool SquashedSolidBlockGeometry::beginEmit( GeometryCluster*, InstanceContext *ctx ) {
	bool sidesAllSolid = ctx->sides[0].solid & ctx->sides[1].solid & ctx->sides[2].solid & ctx->sides[3].solid;
	if( top )
		ctx->sides[5].solid &= sidesAllSolid;
	if( bottom )
		ctx->sides[4].solid &= sidesAllSolid;
	return true;
}

BlockGeometry::IslandMode SquashedSolidBlockGeometry::beginIsland( IslandDesc *ctx ) {
	IslandMode superMode = SolidBlockGeometry::beginIsland( ctx );
	if( ctx->islandAxis < 4 ) {
		// Lock the world Z axis
		if( ctx->xax == 2 ) {
			return (IslandMode)(superMode | ISLAND_LOCK_X);
		} else {
			return (IslandMode)(superMode | ISLAND_LOCK_Y);
		}
	} else {
		ctx->checkVisibility = !((ctx->islandAxis == 5 && top) || (bottom && ctx->islandAxis == 4));
		ctx->checkFacingSameId = ctx->checkVisibility;
		return superMode;
	}
}

void SquashedSolidBlockGeometry::emitIsland( GeometryCluster *outCluster, const IslandDesc *ctx ) {
	if( ctx->islandAxis >= 4 ) {
		emitContour( ((SixSidedGeometryCluster*)outCluster)->getStream( ctx->islandAxis ), ctx, ctx->islandAxis == 4 ? bottom : top );
	} else {
		// Islands on these axes can only be quads (the Z axis is locked)
		GeometryStream *target = ((SixSidedGeometryCluster*)outCluster)->getStream( ctx->islandAxis );
		unsigned indexBase = target->getIndexBase();

		// Which z level is the top?
		int topz = ctx->contourPoints[0].z;
		for( unsigned i = 1; i < 4; i++ ) {
			if( ctx->contourPoints[i].z > topz )
				topz = ctx->contourPoints[i].z;
		}

		// Emit the vertices
		Vertex vtx;
		for( unsigned i = 0; i < 4; i++ ) {
			vtx.pos[0] = (short)(ctx->contourPoints[i].x * 16);
			vtx.pos[1] = (short)(ctx->contourPoints[i].y * 16);
			// Offset the z axis
			vtx.pos[2] = (short)(ctx->contourPoints[i].z * 16 + (ctx->contourPoints[i].z == topz ? top : -bottom));

			target->emitVertex( vtx );
		}

		target->emitQuad( indexBase, indexBase+1, indexBase+2, indexBase+3 );
	}
}

CompactedSolidBlockGeometry::CompactedSolidBlockGeometry( int *offsets, unsigned tx, unsigned color )
: SolidBlockGeometry( tx, color )
{
	for( unsigned i = 0; i < 6; i++ )
		this->offsets[i] = offsets[i];
	for( unsigned i = 0; i < 6; i += 2 )
		islandViable[i>>1] = offsets[i] == 0 && offsets[i+1] == 0;
	for( unsigned i = 0; i < 3; i++ )
		fullIslands[i] = islandViable[(i+1)%3] && islandViable[(i+2)%3];
}

CompactedSolidBlockGeometry::CompactedSolidBlockGeometry( int *offsets, unsigned *tx, unsigned color )
: SolidBlockGeometry( tx, color )
{
	for( unsigned i = 0; i < 6; i++ )
		this->offsets[i] = offsets[i];
	for( unsigned i = 0; i < 6; i += 2 )
		islandViable[i>>1] = offsets[i] == 0 && offsets[i+1] == 0;
	for( unsigned i = 0; i < 3; i++ )
		fullIslands[i] = islandViable[(i+1)%3] && islandViable[(i+2)%3];
}

CompactedSolidBlockGeometry::~CompactedSolidBlockGeometry() {
}

GeometryCluster *CompactedSolidBlockGeometry::newCluster() {
	SixSidedGeometryCluster *cluster = new SixSidedGeometryCluster( this );
	for( unsigned i = 0; i < 6; i++ ) {
		jVec3 n;
		jVec3Set( &n, 0.0f, 0.0f, (16-2*offsets[i]) / (i&1 ? 16.0f : -16.0f) );
		cluster->setCutoutVector( 4, &n );
	}

	return cluster;
}

bool CompactedSolidBlockGeometry::beginEmit( GeometryCluster*, InstanceContext *ctx ) {
	for( unsigned i = 0; i < 6; i++ ) {
		if( offsets[i] != 0 ) {
			ctx->sides[i].solid = 0u;
		} else if( offsets[i^1] == 0 && ctx->sides[i].id == ctx->block.id ) {
			ctx->sides[i].solid = 1u;
		}
	}
	return true;
}

BlockGeometry::IslandMode CompactedSolidBlockGeometry::beginIsland( IslandDesc *ctx ) {
	unsigned ret = SolidBlockGeometry::beginIsland( ctx );
	if( !islandViable[ctx->xax] )
		ret |= ISLAND_LOCK_X;
	if( !islandViable[ctx->yax] )
		ret |= ISLAND_LOCK_Y;

	ctx->checkVisibility = offsets[ctx->islandAxis] == 0;
	ctx->checkFacingSameId = false;

	return (IslandMode)ret;
}

void CompactedSolidBlockGeometry::emitIsland( GeometryCluster *outCluster, const IslandDesc *ctx ) {
	if( fullIslands[ctx->islandAxis>>1] ) {
		emitContour( ((SixSidedGeometryCluster*)outCluster)->getStream( ctx->islandAxis ), ctx, offsets[ctx->islandAxis] );
	} else {
		// Islands on these axes can only be quads
		GeometryStream *target = ((SixSidedGeometryCluster*)outCluster)->getStream( ctx->islandAxis );
		emitQuad( target, ctx, &offsets[0] );
	}
}

MultiBlockInBlockGeometry::MultiBlockInBlockGeometry( int **offsets, unsigned nEmits, unsigned tx, unsigned color )
: SolidBlockGeometry(tx, color), offsets(NULL)
{
	setupOffsets( offsets, nEmits );
}

MultiBlockInBlockGeometry::MultiBlockInBlockGeometry( int **offsets, unsigned nEmits, unsigned *tx, unsigned color )
: SolidBlockGeometry(tx, color), offsets(NULL)
{
	setupOffsets( offsets, nEmits );
}

MultiBlockInBlockGeometry::~MultiBlockInBlockGeometry() {
	delete[] offsets;
}

bool MultiBlockInBlockGeometry::beginEmit( GeometryCluster*, InstanceContext *ctx ) {
	for( unsigned i = 0; i < 6; i++ )
		ctx->sides[i].solid = 0;
	
	return true;
}

BlockGeometry::IslandMode MultiBlockInBlockGeometry::beginIsland( IslandDesc *island ) {
	island->checkFacingSameId = false;
	island->checkVisibility = false;
		
	return ISLAND_SINGLE;
}

void MultiBlockInBlockGeometry::emitIsland( GeometryCluster *out, const IslandDesc *island ) {
	OffsetSet *end = offsets + offsetCount;
	for( OffsetSet *o = offsets; o != end; o++ ) {
		if( o->hide[island->islandAxis] )
			continue;
			
		emitQuad( static_cast<SixSidedGeometryCluster*>(out)->getStream(island->islandAxis), island, &o->offsets[0] );
	}
}

void MultiBlockInBlockGeometry::setupOffsets( int **offsets, unsigned nEmits ) {
	offsetCount = nEmits;
	this->offsets = new OffsetSet[nEmits];
	
	for( unsigned i = 0; i < nEmits; i++ ) {
		OffsetSet *o = &this->offsets[i];
		for( unsigned j = 0; j < 6; j++ ) {
			o->offsets[j] = offsets[i][j];
			o->hide[j] = false;
		}
	}
}

CompactedIslandBlockGeometry::CompactedIslandBlockGeometry( int **offsets, unsigned *nEmits, unsigned tx, unsigned color )
: SolidBlockGeometry(tx, color), offsets(NULL)
{
	setupOffsets( offsets, nEmits );
}

CompactedIslandBlockGeometry::CompactedIslandBlockGeometry( int **offsets, unsigned *nEmits, unsigned *tx, unsigned color )
: SolidBlockGeometry(tx, color), offsets(NULL)
{
	setupOffsets( offsets, nEmits );
}

CompactedIslandBlockGeometry::~CompactedIslandBlockGeometry() {
	delete[] offsets;
}

bool CompactedIslandBlockGeometry::beginEmit( GeometryCluster*, InstanceContext *ctx ) {
	for( unsigned i = 0; i < 6; i++ )
		ctx->sides[i].solid = 0;
	
	return true;
}

BlockGeometry::IslandMode CompactedIslandBlockGeometry::beginIsland( IslandDesc *island ) {
	IslandMode mode = SolidBlockGeometry::beginIsland( island );
	island->checkFacingSameId = false;
	island->checkVisibility = false;
	for( ; island->islandIndex < 3; island->islandIndex++ ) {
		if( (island->islandAxis >> 1) == island->islandIndex ) {
			if( island->origin.sides[island->islandAxis].id == island->origin.block.id )
				continue;
		} else if( !island->origin.sides[island->islandIndex<<1].outside && island->origin.sides[island->islandIndex<<1].id == island->origin.block.id ) {
			continue;
		}
		
		mode = (IslandMode)(mode | ISLAND_REPEAT);
		if( island->xax != island->islandIndex )
			mode = (IslandMode)(mode | ISLAND_LOCK_X);
		if( island->yax != island->islandIndex )
			mode = (IslandMode)(mode | ISLAND_LOCK_Y);
		return mode;
	}
	
	island->islandIndex = 0;
	return ISLAND_CANCEL;
}

void CompactedIslandBlockGeometry::emitIsland( GeometryCluster *out, const IslandDesc *island ) {
	OffsetSet *end = offsets + offsetStart[island->islandIndex+1];
	bool extendBackwards = island->origin.sides[island->islandIndex<<1].outside && island->origin.sides[island->islandIndex<<1].id == island->origin.block.id;
	for( OffsetSet *o = offsets + offsetStart[island->islandIndex]; o != end; o++ ) {
		if( o->hide[island->islandAxis] )
			continue;

		if( extendBackwards ) {
			int offsets[6];
			memcpy( &offsets[0], &o->offsets[0], sizeof(offsets) );
			unsigned i = island->islandIndex << 1;
			offsets[i] = -offsets[i+1];
			emitQuad( static_cast<SixSidedGeometryCluster*>(out)->getStream(island->islandAxis), island, &offsets[0] );
		} else {
			if( island->nContourBlocks == 1 ) {
				if( o->collapses )
					continue;
				// TODO: Reinstate this if:
				//if( o->offsets[island->islandAxis] == 0 && isSolidBlock( island->origin.sides[island->islandAxis].id, island->islandAxis ^ 1u ) ) // Face is flush?
				//	continue;
			}

			emitQuad( static_cast<SixSidedGeometryCluster*>(out)->getStream(island->islandAxis), island, &o->offsets[0] );
		}
	}
}

void CompactedIslandBlockGeometry::setupOffsets( int **offsets, unsigned *nEmits ) {
	unsigned nOffsetSets = 0;
	for( unsigned i = 0; i < 3; i++ )
		nOffsetSets += nEmits[i];
	this->offsets = new OffsetSet[nOffsetSets];
	
	unsigned inpOffset = 0;
	offsetStart[0] = 0;
	for( unsigned dim = 0; dim < 3; dim++ ) {
		for( unsigned i = 0; i < nEmits[dim]; i++ ) {
			OffsetSet *o = &this->offsets[inpOffset];
			for( unsigned j = 0; j < 6; j++ ) {
				o->offsets[j] = offsets[inpOffset][j];
				o->hide[j] = false;
			}
			o->collapses = false;
			for( unsigned i = 0; i < 3; i++ ) {
				if( o->offsets[i<<1] + o->offsets[(i<<1)+1] < -16 )
					o->collapses = true;
			}
			inpOffset++;
		}
		offsetStart[dim+1] = inpOffset;
	}
}

DoorBlockGeometry::DoorBlockGeometry( unsigned tx, unsigned color )
: SolidBlockGeometry(tx,color), texFlipped(tx,color)
{
	texFlipped.setTexScale( -1.0f, 1.0f );
}

DoorBlockGeometry::~DoorBlockGeometry() {
}
	
GeometryCluster *DoorBlockGeometry::newCluster() {
	MultiGeometryCluster *cluster = new MultiGeometryCluster;
	cluster->newCluster( 0, SolidBlockGeometry::newCluster() );
	cluster->newCluster( 1, texFlipped.newCluster() );
	return cluster;
}

static unsigned dataToIndentedDir( unsigned data ) {
	bool open = (data & 4) != 0;
	data &= 3;
	if( open )
		data++;
	const unsigned DIR[] = { 3, 1, 2, 0, 3 };
	return DIR[data];
}

BlockGeometry::IslandMode DoorBlockGeometry::beginIsland( IslandDesc *island ) {
	IslandMode mode = SolidBlockGeometry::beginIsland( island );
	island->checkFacingSameId = false;

	unsigned data = island->origin.block.data;
	bool top = (data & 8) != 0;
	unsigned otherSide = top ? 4 : 5;
	if( island->origin.sides[otherSide].id == island->origin.block.id )
		island->origin.sides[otherSide].solid = true;

	island->origin.sides[dataToIndentedDir( data )].solid = false;
	
	return (IslandMode)(mode | ISLAND_SINGLE);
}

void DoorBlockGeometry::emitIsland( GeometryCluster *outCluster, const IslandDesc *ctx ) {
	MultiGeometryCluster *out = static_cast<MultiGeometryCluster*>(outCluster);

	const int DOOR_INDENTATION = -13;

	bool open = (ctx->origin.block.data & 4) != 0;
	unsigned indentedDir = dataToIndentedDir( ctx->origin.block.data );

	if( (ctx->islandAxis >> 1) == (indentedDir >> 1) ) {
		// This is a door face
		GeometryStream *tgt = static_cast<SixSidedGeometryCluster*>(out->getCluster(open ^ (ctx->islandAxis == indentedDir) ? 0 : 1))->getStream(ctx->islandAxis);
		emitContour( tgt, ctx, ctx->islandAxis == indentedDir ? DOOR_INDENTATION : 0 );
	} else {
		GeometryStream *tgt = static_cast<SixSidedGeometryCluster*>(out->getCluster(0))->getStream(ctx->islandAxis);
		int offsets[6];
		for( unsigned i = 0; i < 6; i++ )
			offsets[i] = 0;
		offsets[indentedDir] = DOOR_INDENTATION;
		
		emitQuad( tgt, ctx, &offsets[0] );
	}
}

StairsBlockGeometry::StairsBlockGeometry( unsigned tx, unsigned color )
: SolidBlockGeometry( tx, color )
{
}

StairsBlockGeometry::StairsBlockGeometry( unsigned *tx, unsigned color )
: SolidBlockGeometry( tx, color )
{
}

StairsBlockGeometry::~StairsBlockGeometry() {
}
	
BlockGeometry::IslandMode StairsBlockGeometry::beginIsland( IslandDesc *island ) {
	IslandMode mode = SolidBlockGeometry::beginIsland( island );
	unsigned asc = (island->origin.block.data & 3) ^ 3; // Ascending towards asc
	unsigned allowedAxis = (asc >> 1) ^ 1;
	unsigned inverted = island->origin.block.data & 4;

	island->continueIsland = &continueIsland;
	if( island->islandAxis >= 4 ) {
		island->checkFacingSameId = false;
		if( island->islandAxis == (inverted ? 5u : 4u) )
			return ISLAND_NORMAL;

		island->checkVisibility = false;
		return (IslandMode)(mode | (island->xax == allowedAxis ? ISLAND_LOCK_Y : ISLAND_LOCK_X));
	}

	island->checkFacingSameId = false;
	if( island->islandAxis == asc )
		return mode;

	island->checkVisibility = false;
	if( island->xax == allowedAxis )
		return (IslandMode)(mode | ISLAND_LOCK_Y);
	if( island->yax == allowedAxis )
		return (IslandMode)(mode | ISLAND_LOCK_X);
	return (IslandMode)(mode | ISLAND_SINGLE);
}

void StairsBlockGeometry::emitIsland( GeometryCluster *outCluster, const IslandDesc *island ) {
	SixSidedGeometryCluster *out = static_cast<SixSidedGeometryCluster*>(outCluster);

	unsigned asc = (island->origin.block.data & 3) ^ 3; // Ascending towards asc
	unsigned inverted = island->origin.block.data & 4;
	if( island->islandAxis == (inverted ? 5u : 4u) || island->islandAxis == asc ) {
		// Bottom (or top for inverted) or back face
		emitContour( out->getStream( island->islandAxis ), island, 0 );
	} else if( island->islandAxis == (inverted ? 4u : 5u) ) {
		// "Top" faces
		int offsets[6];
		for( unsigned i = 0; i < 6; i++ )
			offsets[i] = 0;
		offsets[asc^1] = -8;
		emitQuad( out->getStream( island->islandAxis ), island, &offsets[0] );
		offsets[asc^1] = 0;
		offsets[asc] = -8;
		offsets[inverted ? 4 : 5] = -8;
		emitQuad( out->getStream( island->islandAxis ), island, &offsets[0] );
	} else {//if( island->islandAxis == asc ^ 1 ) {
		// Front faces
		// TODO: Move side faces into a separate emit
		int offsets[6];
		for( unsigned i = 0; i < 6; i++ )
			offsets[i] = 0;
		offsets[inverted ? 4 : 5] = -8;
		emitQuad( out->getStream( island->islandAxis ), island, &offsets[0] );
		offsets[inverted ? 4 : 5] = 0;
		offsets[inverted ? 5 : 4] = -8;
		offsets[asc^1] = -8;
		emitQuad( out->getStream( island->islandAxis ), island, &offsets[0] );
	//} else {
	//	// Side faces
	//	assert( island->contourBlocks == 1 );
	}
}

bool StairsBlockGeometry::continueIsland( IslandDesc *island, const InstanceContext *nextBlock ) {
	return nextBlock->block.data == island->origin.block.data;
}


SimpleGeometry::SimpleGeometry()
: BlockGeometry()
{
}

SimpleGeometry::~SimpleGeometry() {
}

GeometryCluster *SimpleGeometry::newCluster() {
	return new SingleStreamGeometryCluster( this );
}

void SimpleGeometry::render( void *&metaData, RenderContext *ctx ) {
	ctx->shader->bindNormal();
	ctx->lightModels[5].uploadGL();
	//ctx->shader->setLightOffset( 0.0f, 0.0f );

	rawRender( metaData, ctx );
}

void SimpleGeometry::rawRender( void *&metaData, RenderContext *ctx ) {
	SingleStreamGeometryCluster::Meta *meta = (SingleStreamGeometryCluster::Meta*)metaData;

	glEnableClientState( GL_VERTEX_ARRAY );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );

	glVertexPointer( 3, GL_FLOAT, sizeof( Vertex ), (void*)meta->vtx_offset );
	glTexCoordPointer( 2, GL_FLOAT, sizeof( Vertex ), (void*)(meta->vtx_offset + 12) );

	ctx->renderedTriCount += meta->nTris;
	glDrawElements( GL_TRIANGLES, meta->nTris*3, meta->idxType, (void*)meta->idx_offset );

	glDisableClientState( GL_VERTEX_ARRAY );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );

	metaData = &meta[1];
}

void SimpleGeometry::emitSimpleQuad( GeometryStream *target, const jMatrix *loc, const jMatrix *tex ) {
	jMatrix id;
	if( tex == NULL ) {
		jMatrixSetIdentity( &id );
		tex = &id;
	}
	
	Vertex v;
	jVec3Scale( &v.pos, &loc->pos, 16.0f );
	v.u = tex->pos.x; v.v = tex->pos.z;
	
	unsigned idxBase = target->getIndexBase();

	target->emitVertex( v );
	jVec3MA( &v.pos, &loc->right, 16.0f, &v.pos );
	v.u += tex->right.x;
	v.v += tex->right.z;
	target->emitVertex( v );
	jVec3MA( &v.pos, &loc->up, 16.0f, &v.pos );
	v.u += tex->up.x;
	v.v += tex->up.z;
	target->emitVertex( v );
	jVec3MA( &v.pos, &loc->right, -16.0f, &v.pos );
	v.u -= tex->right.x;
	v.v -= tex->right.z;
	target->emitVertex( v );

	target->emitQuad( idxBase, idxBase+1, idxBase+2, idxBase+3 );
}


RailGeometry::RailGeometry( unsigned txStraight, unsigned txTurn )
: texStraight(txStraight), texTurn(txTurn)
{
	rg = RenderGroup::OPAQUE + 100;
}

RailGeometry::~RailGeometry() {
}

GeometryCluster *RailGeometry::newCluster() {
	MultiGeometryCluster *clust = new MultiGeometryCluster;
	clust->newCluster( 0, new SingleStreamGeometryClusterEx<unsigned>( this, 0 ) );
	clust->newCluster( 1, new SingleStreamGeometryClusterEx<unsigned>( this, 1 ) );
	return clust;
}

void RailGeometry::render( void *&meta, RenderContext *ctx ) {
	glEnable( GL_TEXTURE_2D );
	glNormal3f( 0.0f, 0.0f, 1.0f );
	
	unsigned *mid = (unsigned*)meta;
	meta = &mid[1];
	if( mid[0] == 0 ) {
		glBindTexture( GL_TEXTURE_2D, texStraight );
		SimpleGeometry::render( meta, ctx );
	} else {
		glBindTexture( GL_TEXTURE_2D, texTurn );
		SimpleGeometry::render( meta, ctx );
	}

	glDisable( GL_TEXTURE_2D );
}

bool RailGeometry::beginEmit( GeometryCluster *outCluster, InstanceContext *ctx ) {
	outCluster = ((MultiGeometryCluster*)outCluster)->getCluster( ctx->block.data < 6 ? 0 : 1 );
	GeometryStream *out = ((SingleStreamGeometryClusterEx<unsigned>*)outCluster)->getStream();

	jVec3 v1, v2, v3, v4;
	float z = ctx->block.pos.z * 16.0f + 1.0f;
	float x = (float)ctx->block.pos.x * 16.0f, y = (float)ctx->block.pos.y * 16.0f;
	const float ONE = 16.0f; // One meter is 16 pixels

	switch( ctx->block.data ) {
	case 0: // Flat east/west track
		jVec3Set( &v1, x, y, z );
		jVec3Set( &v2, x+ONE, y, z );
		jVec3Set( &v3, x+ONE, y+ONE, z );
		jVec3Set( &v4, x, y+ONE, z );
		break;
	case 1: // Flat north/south track
		jVec3Set( &v1, x+ONE, y, z );
		jVec3Set( &v2, x+ONE, y+ONE, z );
		jVec3Set( &v3, x, y+ONE, z );
		jVec3Set( &v4, x, y, z );
		break;
	case 3: // Ascending south
		jVec3Set( &v1, x, y+ONE, z );
		jVec3Set( &v2, x, y, z+ONE );
		jVec3Set( &v3, x+ONE, y, z+ONE );
		jVec3Set( &v4, x+ONE, y+ONE, z );
		break;
	case 2: // Ascending north
		jVec3Set( &v1, x+ONE, y, z );
		jVec3Set( &v2, x+ONE, y+ONE, z+ONE );
		jVec3Set( &v3, x, y+ONE, z+ONE );
		jVec3Set( &v4, x, y, z );
		break;
	case 5: // Ascending east
		jVec3Set( &v1, x, y, z );
		jVec3Set( &v2, x+ONE, y, z+ONE );
		jVec3Set( &v3, x+ONE, y+ONE, z+ONE );
		jVec3Set( &v4, x, y+ONE, z );
		break;
	case 4: // Ascending west
		jVec3Set( &v1, x+ONE, y+ONE, z );
		jVec3Set( &v2, x, y+ONE, z+ONE );
		jVec3Set( &v3, x, y, z+ONE );
		jVec3Set( &v4, x+ONE, y, z );
		break;
	case 7: // Northeast corner
		jVec3Set( &v1, x, y+ONE, z );
		jVec3Set( &v2, x, y, z );
		jVec3Set( &v3, x+ONE, y, z );
		jVec3Set( &v4, x+ONE, y+ONE, z );
		break;
	case 8: // Southeast corner
		jVec3Set( &v1, x+ONE, y+ONE, z );
		jVec3Set( &v2, x, y+ONE, z );
		jVec3Set( &v3, x, y, z );
		jVec3Set( &v4, x+ONE, y, z );
		break;
	case 9: // Southwest corner
		jVec3Set( &v1, x+ONE, y, z );
		jVec3Set( &v2, x+ONE, y+ONE, z );
		jVec3Set( &v3, x, y+ONE, z );
		jVec3Set( &v4, x, y, z );
		break;
	case 6: // Northwest corner
		jVec3Set( &v1, x, y, z );
		jVec3Set( &v2, x+ONE, y, z );
		jVec3Set( &v3, x+ONE, y+ONE, z );
		jVec3Set( &v4, x, y+ONE, z );
		break;
	}

	Vertex v;
	jVec3Copy( &v.pos, &v1 );
	v.u = 0.0f; v.v = 0.0f;
	
	unsigned idxBase = out->getIndexBase();

	out->emitVertex( v );
	jVec3Copy( &v.pos, &v2 );
	v.v += 1.0f;
	out->emitVertex( v );
	jVec3Copy( &v.pos, &v3 );
	v.u = 1.0f;
	out->emitVertex( v );
	jVec3Copy( &v.pos, &v4 );
	v.v = 0.0f;
	out->emitVertex( v );

	out->emitQuad( idxBase, idxBase+1, idxBase+2, idxBase+3 );

	return false;
}


TorchGeometry::TorchGeometry( unsigned tx )
: tex(tx)
{
	rg = RenderGroup::OPAQUE + 100;
}

TorchGeometry::~TorchGeometry() {
}

void TorchGeometry::render( void *&meta, RenderContext *ctx ) {
	if( jVec3LengthSq( &ctx->viewPos ) > 200.0f*200.0f ) {
		meta = (char*)meta + sizeof(SingleStreamGeometryCluster::Meta);
		return;
	}

	//glColor3f( 1.0f, 1.0f, 1.0f );
	glEnable( GL_BLEND );
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, tex );
	//glNormal3f( 0.0f, 0.0f, 0.0f );

	ctx->shader->bindNormal();
	if( ctx->enableBlockLighting )
		ctx->shader->setLightOffset( 1.0f, 0.0f );
	SimpleGeometry::render( meta, ctx );
	ctx->shader->setLightOffset( 0.0f, 0.0f );

	glDisable( GL_TEXTURE_2D );
	glDisable( GL_BLEND );
}


bool TorchGeometry::beginEmit( GeometryCluster *outCluster, InstanceContext *ctx ) {
	GeometryStream *out = ((SingleStreamGeometryCluster*)outCluster)->getStream();

	jVec3 base, top;
	jVec3Set( &base, (float)ctx->block.pos.x, (float)ctx->block.pos.y, (float)ctx->block.pos.z );
	jVec3Copy( &top, &base );
	top.z += 1.0f;

	const float BASE_MOVE = 0.5f;
	const float ONWALL_MOVE_Z = 3.0f / 16.0f;
	const float TOP_MOVE = 2.0f/16.0f;
	const float TORCH_WIDTH = 2/16.0f;
	switch( ctx->block.data ) {
	case 1: // Pointing South
		top.y -= TOP_MOVE;
		base.y -= BASE_MOVE;
		top.z += ONWALL_MOVE_Z;
		base.z += ONWALL_MOVE_Z;
		break;
	case 2: // North
		top.y += TOP_MOVE;
		base.y += BASE_MOVE;
		top.z += ONWALL_MOVE_Z;
		base.z += ONWALL_MOVE_Z;
		break;
	case 3: // West
		top.x -= TOP_MOVE;
		base.x -= BASE_MOVE;
		top.z += ONWALL_MOVE_Z;
		base.z += ONWALL_MOVE_Z;
		break;
	case 4: // East
		top.x += TOP_MOVE;
		base.x += BASE_MOVE;
		top.z += ONWALL_MOVE_Z;
		base.z += ONWALL_MOVE_Z;
		break;
	case 5: // On floor
		break;
	}

	jMatrix pos, tx;
	jMatrixSetIdentity( &tx );
	tx.pos.z = 1.0f;
	tx.up.z = -1.0f;
	jVec3Subtract( &pos.up, &top, &base );
	jVec3Copy( &pos.pos, &base );
	jVec3Set( &pos.right, 1.0f, 0.0f, 0.0f );
	pos.pos.y += 0.5f - TORCH_WIDTH/2;
	emitSimpleQuad( out, &pos, &tx );

	pos.right.x = -1.0f;
	pos.pos.y += TORCH_WIDTH;
	pos.pos.x += 1.0f;
	emitSimpleQuad( out, &pos, &tx );

	pos.right.x = 0.0f;
	pos.right.y = 1.0f;
	pos.pos.y = base.y;
	pos.pos.x = base.x + 0.5f + TORCH_WIDTH/2;
	emitSimpleQuad( out, &pos, &tx );

	pos.right.y = -1.0f;
	pos.pos.y += 1.0f;
	pos.pos.x -= TORCH_WIDTH;
	emitSimpleQuad( out, &pos, &tx );

	jVec3Set( &pos.right, TORCH_WIDTH, 0.0f, 0.0f );
	jVec3Set( &pos.up, 0.0f, TORCH_WIDTH, 0.0f );
	jVec3Lerp( &pos.pos, &base, &top, 0.5f + TORCH_WIDTH );
	pos.pos.x += 0.5f - TORCH_WIDTH/2;
	pos.pos.y += 0.5f - TORCH_WIDTH/2;
	tx.pos.x = 0.5f - TORCH_WIDTH/2;
	tx.pos.z = 0.5f;
	tx.right.x = TORCH_WIDTH;
	tx.up.z = -TORCH_WIDTH;
	emitSimpleQuad( out, &pos, &tx );

	return false;
}


FlowerGeometry::FlowerGeometry( unsigned tx )
: tex(tx)
{
	rg = RenderGroup::OPAQUE + 100;
}

FlowerGeometry::~FlowerGeometry() {
}

void FlowerGeometry::render( void *&meta, RenderContext *ctx ) {
	ctx->shader->bindNormal();
	ctx->lightModels[5].uploadGL();
	//ctx->shader->setLightOffset( 0.0f, 0.0f );

	glDisable( GL_CULL_FACE );
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, tex );
	rawRender( meta, ctx );
	glDisable( GL_TEXTURE_2D );
	glEnable( GL_CULL_FACE );
}

bool FlowerGeometry::beginEmit( GeometryCluster*, InstanceContext *ctx ) {
	// Only emit one island - vertically
	for( unsigned i = 0; i < 6; i++ )
		ctx->sides[i].solid = true;
	ctx->sides[2].solid = false;

	return true;
}

BlockGeometry::IslandMode FlowerGeometry::beginIsland( IslandDesc *ctx ) {
	ctx->checkVisibility = false;
	ctx->checkFacingSameId = false;
	//return ctx->xax == 2 ? BlockGeometry::ISLAND_LOCK_Y : BlockGeometry::ISLAND_LOCK_X; // Lock the lateral axis
	return ISLAND_SINGLE;
}

void FlowerGeometry::emitIsland( GeometryCluster *outCluster, const IslandDesc *ctx ) {
	// TODO: Proper emit for tall reeds
	GeometryStream *out = ((SingleStreamGeometryCluster*)outCluster)->getStream();

	jMatrix pos, tx;
	jMatrixSetIdentity( &tx );
	tx.pos.z = 1.0f;
	tx.up.z = -1.0f;

	jVec3Set( &pos.pos, (float)ctx->origin.block.pos.x, (float)ctx->origin.block.pos.y, (float)ctx->origin.block.pos.z );
	jVec3Set( &pos.up, 0.0f, 0.0f, 1.0f );
	jVec3Set( &pos.right, 1.0f, 1.0f, 0.0f );
	emitSimpleQuad( out, &pos, &tx );

	jVec3Add( &pos.pos, &pos.right, &pos.pos );
	jVec3Set( &pos.right, -1.0f, -1.0f, 0.0f );
	tx.pos.x = 1.0f;
	tx.right.x = -1.0f;
	emitSimpleQuad( out, &pos, &tx );

	tx.pos.x = 0.0f;
	tx.right.x = 1.0f;
	jVec3Set( &pos.pos, (float)ctx->origin.block.pos.x, (float)ctx->origin.block.pos.y, (float)ctx->origin.block.pos.z );
	jVec3Set( &pos.up, 0.0f, 0.0f, 1.0f );
	jVec3Set( &pos.right, 1.0f, -1.0f, 0.0f );
	pos.pos.y += 1.0f;
	emitSimpleQuad( out, &pos, &tx );

	jVec3Add( &pos.pos, &pos.right, &pos.pos );
	jVec3Set( &pos.right, -1.0f, 1.0f, 0.0f );
	tx.pos.x = 1.0f;
	tx.right.x = -1.0f;
	emitSimpleQuad( out, &pos, &tx );
}

BiomeFlowerGeometry::BiomeFlowerGeometry( unsigned tx, unsigned biomeTex )
: FlowerGeometry( tx ), biomeTex(biomeTex)
{
}

BiomeFlowerGeometry::~BiomeFlowerGeometry() {
}

void BiomeFlowerGeometry::render( void *&meta, RenderContext *ctx ) {
	ctx->shader->bindFoliage();
	ctx->lightModels[5].uploadGL();
	//ctx->shader->setLightOffset( 0.0f, 0.0f );

	glActiveTexture( GL_TEXTURE2 );
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, ctx->biomeTextures[biomeTex] );
	glActiveTexture( GL_TEXTURE0 );

	glMatrixMode( GL_TEXTURE );
	const float texMat[] = {
		1.f/16, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f,
		0.f, -1.f/16, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f
	};
	glLoadMatrixf( &texMat[0] );

	glDisable( GL_CULL_FACE );
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, tex );
	rawRender( meta, ctx );
	glDisable( GL_TEXTURE_2D );
	glEnable( GL_CULL_FACE );

	glActiveTexture( GL_TEXTURE2 );
	glDisable( GL_TEXTURE_2D );
	glActiveTexture( GL_TEXTURE0 );
}

SignTextGeometry::SignTextGeometry()
{
	rg = RenderGroup::OPAQUE + 100;
}

SignTextGeometry::~SignTextGeometry() {
}

GeometryCluster *SignTextGeometry::newCluster() {
	return new MetaGeometryCluster( this );
}

void SignTextGeometry::render( void *&meta, RenderContext *ctx ) {
	char *cursor = (char*)meta;
	unsigned n = *(unsigned*)cursor;
	cursor += sizeof(unsigned);

	const float SIGN_CUTOFF_DIST = 100.0f;

	if( jVec3LengthSq( &ctx->viewPos ) > SIGN_CUTOFF_DIST*SIGN_CUTOFF_DIST ) {
		for( unsigned i = 0; i < n; i++ ) {
			cursor += 16*sizeof(float);
			unsigned len = *(unsigned*)cursor;
			cursor += sizeof(unsigned);
			cursor += len;
		}
	} else {
		ctx->shader->unbind();
		LightModel::unloadGL();

		glMatrixMode( GL_TEXTURE );
		glPushMatrix();
		glLoadIdentity();
		glMatrixMode( GL_MODELVIEW );
		//glPushMatrix();
		//glLoadIdentity();

		UIDrawContext draw( false );
		draw.setFontVectorsAA( 0.15f, 0.25f );
		draw.setWidth( 2.0f );
		draw.setHeight( 1.0f );
		draw.color( 0xff000000 );

		glActiveTexture( GL_TEXTURE1 );
		glDisable( GL_TEXTURE_3D );
		glActiveTexture( GL_TEXTURE0 );
		glDisable( GL_CULL_FACE );

		for( unsigned i = 0; i < n; i++ ) {
			draw.moveTo( -1.0f, 0.0f );
			glPushMatrix();
			glMultMatrixf( (float*)cursor );
			cursor += 16*sizeof(float);

			unsigned len = *(unsigned*)cursor;
			cursor += sizeof(unsigned);
			draw.layoutText( cursor );
			draw.drawText( UIDrawContext::CENTER );

			cursor += len;
			glPopMatrix();
		}

		glActiveTexture( GL_TEXTURE1 );
		glEnable( GL_TEXTURE_3D );
		glActiveTexture( GL_TEXTURE0 );
		glEnable( GL_CULL_FACE );

		glMatrixMode( GL_TEXTURE );
		glPopMatrix();
		glMatrixMode( GL_MODELVIEW );
		//glPopMatrix();
	}

	meta = cursor;
}

bool SignTextGeometry::beginEmit( GeometryCluster*, InstanceContext* ) {
	return false;
}

void SignTextGeometry::emitSign( GeometryCluster *outCluster, const jMatrix *pos, const char *text ) {
	MetaGeometryCluster *out = static_cast<MetaGeometryCluster*>(outCluster);
	GeometryStream *str = out->getStream();
	float glMat[16];
	jMatrixToGL( &glMat[0], pos );
	str->emitVertex( &glMat[0], sizeof(glMat) );
	unsigned len = (strlen( text ) + 4) & ~3;
	str->emitVertex( len );
	str->emitVertex( text, len );
	out->incEntry();
}



// Lua functions
static unsigned luaL_checktextureid( lua_State *L, int idx ) {
	return (unsigned)luaL_checknumber( L, idx );
}

static unsigned luaL_opttextureid( lua_State *L, int idx, unsigned def ) {
	return (unsigned)luaL_optnumber( L, idx, def );
}

static void luaL_checkfacingtextures( lua_State *L, int idx, unsigned *tx ) {
	tx[0] = luaL_checktextureid( L, idx );
	tx[1] = tx[2] = tx[3] = tx[0];
	tx[4] = luaL_opttextureid( L, idx+1, tx[0] );
	tx[5] = luaL_opttextureid( L, idx+2, tx[4] );
	if( lua_isnumber( L, idx+3 ) ) {
		tx[1] = tx[3] = tx[4];
		tx[4] = tx[5];
		tx[5] = luaL_checktextureid( L, idx+3 );
		if( lua_isnumber( L, idx + 4 ) ) {
			tx[2] = tx[4];
			tx[3] = tx[5];
			tx[4] = luaL_checktextureid( L, idx+4 );
			tx[5] = luaL_checktextureid( L, idx+5 );
			std::swap( tx[0], tx[2] );
			std::swap( tx[1], tx[3] );
		}
	}
}

int BlockGeometry::lua_setTexScale( lua_State *L ) {
	BlockGeometry *geom = getLuaObjectArg<BlockGeometry>( L, 1, BLOCKGEOMETRY_META );
	SolidBlockGeometry *solidGeom = dynamic_cast<SolidBlockGeometry*>( geom );
	luaL_argcheck( L, solidGeom, 1, "Geom must be a variant of a SolidBlockGeometry" );

	solidGeom->setTexScale( (float)luaL_checknumber( L, 2 ), (float)luaL_checknumber( L, 3 ) );
	return 0;
}

int BlockGeometry::lua_renderGroupAdd( lua_State *L ) {
	BlockGeometry *geom = getLuaObjectArg<BlockGeometry>( L, 1, BLOCKGEOMETRY_META );
	geom->rg += (unsigned)luaL_checknumber( L, 2 );
	return 0;
}

int BlockGeometry::createDataAdapter( lua_State *L ) {
	unsigned mask = (unsigned)luaL_checknumber( L, 1 );
	BlockGeometry *geoms[16];
	int n = 2;
	for( unsigned i = 0; i < 16; i++ ) {
		if( (mask & i) == i ) {
			geoms[i] = getLuaObjectArg<BlockGeometry>( L, n, BLOCKGEOMETRY_META );
			n++;
		} else {
			geoms[i] = NULL;
		}
	}

	BlockGeometry *geom = new DataBasedMultiGeometryAdapter( mask, &geoms[0] );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createRotatingAdapter( lua_State *L ) {
	BlockGeometry *geoms[2];
	geoms[0] = getLuaObjectArg<BlockGeometry>( L, 1, BLOCKGEOMETRY_META );
	geoms[1] = getLuaObjectArg<BlockGeometry>( L, 2, BLOCKGEOMETRY_META );

	BlockGeometry *geom = new RotatingMultiGeometryAdapter( &geoms[0] );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createFaceBitAdapter( lua_State *L ) {
	BlockGeometry *subGeom = getLuaObjectArg<BlockGeometry>( L, 1, BLOCKGEOMETRY_META );

	BlockGeometry *geom = new FaceBitGeometryAdapter( subGeom );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createFacingAdapter( lua_State *L ) {
	BlockGeometry *geoms[2];
	geoms[0] = lua_toboolean( L, 1 ) ? getLuaObjectArg<BlockGeometry>( L, 1, BLOCKGEOMETRY_META ) : NULL;
	geoms[1] = getLuaObjectArg<BlockGeometry>( L, 2, BLOCKGEOMETRY_META );

	BlockGeometry *geom = new FacingMultiGeometryAdapter( geoms[0], geoms[1] );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createTopDifferentAdapter( lua_State *L ) {
	BlockGeometry *sideGeom = getLuaObjectArg<BlockGeometry>( L, 1, BLOCKGEOMETRY_META );
	BlockGeometry *diffGeom = getLuaObjectArg<BlockGeometry>( L, 2, BLOCKGEOMETRY_META );
	unsigned diffId = (unsigned)luaL_checknumber( L, 3 );

	BlockGeometry *geom = new TopModifiedMultiGeometryAdapter( sideGeom, diffGeom, diffId );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createOpaqueBlockGeometry( lua_State *L ) {
	unsigned tx[6];
	luaL_checkfacingtextures( L, 1, &tx[0] );

	BlockGeometry *geom = new BasicSolidBlockGeometry( &tx[0] );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createBrightOpaqueBlockGeometry( lua_State *L ) {
	unsigned tx[6];
	luaL_checkfacingtextures( L, 1, &tx[0] );

	BlockGeometry *geom = new FullBrightBlockGeometry( &tx[0] );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createTransparentBlockGeometry( lua_State *L ) {
	unsigned tx[6];
	unsigned rg = (unsigned)luaL_checknumber( L, 1 );
	luaL_checkfacingtextures( L, 2, &tx[0] );

	BlockGeometry *geom = new TransparentSolidBlockGeometry( &tx[0] );
	geom->setRenderGroup( RenderGroup::TRANSPARENT + rg );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createSquashedBlockGeometry( lua_State *L ) {
	unsigned tx[6];
	int top = (int)luaL_checknumber( L, 1 );
	int bottom = (int)luaL_checknumber( L, 2 );
	luaL_checkfacingtextures( L, 3, &tx[0] );

	BlockGeometry *geom = new SquashedSolidBlockGeometry( top, bottom, &tx[0] );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createCompactedGeometry( lua_State *L ) {
	unsigned tx[6];
	int offsets[6];
	offsets[0] = (int)luaL_checknumber( L, 3 );
	offsets[1] = (int)luaL_checknumber( L, 4 );
	offsets[2] = (int)luaL_checknumber( L, 1 );
	offsets[3] = (int)luaL_checknumber( L, 2 );
	offsets[4] = (int)luaL_checknumber( L, 5 );
	offsets[5] = (int)luaL_checknumber( L, 6 );
	luaL_checkfacingtextures( L, 7, &tx[0] );

	BlockGeometry *geom = new CompactedSolidBlockGeometry( &offsets[0], &tx[0] );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createMultiBlockInBlock( lua_State *L ) {
	std::vector<int*> allOffsets;
	std::vector<bool> showFace;

	luaL_argcheck( L, lua_istable(L,1), 1, "Expected table" );

	int n = luaL_getn( L, 1 );
	if( n % 6 != 0 ) {
		lua_pushstring( L, "Invalid face offsets" );
		lua_error( L );
	}

	n /= 6;
	unsigned emits = n;
	for( int i = 0; i < n; i++ ) {
		int *offsets = new int[6];
		allOffsets.push_back( offsets );
		for( int j = 0; j < 6; j++ ) {
			lua_rawgeti( L, 1, i*6+j+1 );
			if( !lua_isnumber( L, -1 ) ) {
				lua_pushstring( L, "Invalid face offsets" );
				lua_error( L );
			}
			lua_Number offNum = lua_tonumber( L, -1 );
			int off = (int)offNum;
			lua_pop( L, 1 );

			showFace.push_back( offNum < 0.0001 );
			offsets[j] = -abs(off);
		}
	}

	unsigned tx[6];
	luaL_checkfacingtextures( L, 2, &tx[0] );
	mcgeom::MultiBlockInBlockGeometry *geom = new mcgeom::MultiBlockInBlockGeometry( &allOffsets[0], emits, &tx[0] );

	unsigned k = 0;
	for( unsigned i = 0; i < allOffsets.size(); i++ ) {
		for( unsigned j = 0; j < 6; j++, k++ ) {
			if( !showFace[k] )
				geom->showFace( i, j, false );
		}
		delete[] allOffsets[i];
	}

	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createMultiCompactedGeometry( lua_State *L ) {
	std::vector<int*> allOffsets;
	std::vector<bool> showFace;
	unsigned emitsPerDir[3] = { 0u, 0u, 0u };

	for( unsigned dir = 0; dir < 3; dir++ ) {
		luaL_argcheck( L, lua_istable(L,dir+1), dir+1, "Expected table" );

		int n = luaL_getn( L, dir+1 );
		if( n % 6 != 0 ) {
			lua_pushstring( L, "Invalid face offsets" );
			lua_error( L );
		}

		n /= 6;
		emitsPerDir[dir] = n;
		for( int i = 0; i < n; i++ ) {
			int *offsets = new int[6];
			allOffsets.push_back( offsets );
			for( int j = 0; j < 6; j++ ) {
				lua_rawgeti( L, dir+1, i*6+j+1 );
				if( !lua_isnumber( L, -1 ) ) {
					lua_pushstring( L, "Invalid face offsets" );
					lua_error( L );
				}
				lua_Number offNum = lua_tonumber( L, -1 );
				int off = (int)offNum;
				lua_pop( L, 1 );

				showFace.push_back( offNum < 0.0001 );
				offsets[j] = -abs(off);
			}
		}
	}

	unsigned tx[6];
	luaL_checkfacingtextures( L, 4, &tx[0] );
	mcgeom::CompactedIslandBlockGeometry *geom = new mcgeom::CompactedIslandBlockGeometry( &allOffsets[0], &emitsPerDir[0], &tx[0] );

	unsigned k = 0;
	for( unsigned i = 0; i < allOffsets.size(); i++ ) {
		for( unsigned j = 0; j < 6; j++, k++ ) {
			if( !showFace[k] )
				geom->showFace( i, j, false );
		}
		delete[] allOffsets[i];
	}

	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createBiomeOpaqueGeometry( lua_State *L ) {
	unsigned tx[6];
	unsigned biomeChannel = (unsigned)luaL_checknumber( L, 1 );
	luaL_checkfacingtextures( L, 2, &tx[0] );

	BlockGeometry *geom = new FoliageBlockGeometry( &tx[0], biomeChannel );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createBiomeAlphaOpaqueGeometry( lua_State *L ) {
	unsigned tx[6];
	unsigned biomeChannel = (unsigned)luaL_checknumber( L, 1 );
	luaL_checkfacingtextures( L, 2, &tx[0] );

	BlockGeometry *geom = new FoliageAlphaBlockGeometry( &tx[0], biomeChannel );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createPortalGeometry( lua_State *L ) {
	unsigned tx = luaL_checktextureid( L, 1 );

	BlockGeometry *geom = new PortalBlockGeometry( tx );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createCactusGeometry( lua_State *L ) {
	unsigned tx[6];
	int offsets[6];
	if( lua_isnumber( L, 1 ) ) {
		int off = (int)luaL_checknumber( L, 1 );
		for( unsigned i = 0; i < 4; i++ )
			offsets[i] = off;
		offsets[4] = offsets[5] = 0;
	} else {
		int n = luaL_getn( L, 1 );
		luaL_argcheck( L, n == 1 || n == 6, 2, "Offset table must be one number or 6." );
		if( n == 1 ) {
			lua_rawgeti( L, 1, 1 );
			int off = (int)luaL_checknumber( L, -1 );
			lua_pop( L, 1 );
			for( unsigned i = 0; i < 6; i++ )
				offsets[i] = off;
		} else {
			for( unsigned i = 0; i < 6; i++ ) {
				lua_rawgeti( L, 1, i + 1 );
				int off = (int)luaL_checknumber( L, -1 );
				lua_pop( L, 1 );
				offsets[i] = off;
			}
		}
	}
	luaL_checkfacingtextures( L, 2, &tx[0] );

	BlockGeometry *geom = new CactusBlockGeometry( &tx[0], offsets );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createBiomeCactusGeometry( lua_State *L ) {
	unsigned tx[6];
	int offsets[6];
	int biomeChannel = (int)luaL_checknumber( L, 1 );
	if( lua_isnumber( L, 2 ) ) {
		int off = (int)luaL_checknumber( L, 2 );
		for( unsigned i = 0; i < 4; i++ )
			offsets[i] = off;
		offsets[4] = offsets[5] = 0;
	} else if( lua_istable( L, 2 ) ) {
		int n = luaL_getn( L, 2 );
		luaL_argcheck( L, n == 1 || n == 6, 2, "Offset table must be one number or 6." );
		if( n == 1 ) {
			lua_rawgeti( L, 2, 1 );
			int off = (int)luaL_checknumber( L, -1 );
			lua_pop( L, 1 );
			for( unsigned i = 0; i < 6; i++ )
				offsets[i] = off;
		} else {
			for( unsigned i = 0; i < 6; i++ ) {
				lua_rawgeti( L, 2, i + 1 );
				int off = (int)luaL_checknumber( L, -1 );
				lua_pop( L, 1 );
				offsets[i] = off;
			}
		}
	}
	luaL_checkfacingtextures( L, 3, &tx[0] );

	BlockGeometry *geom = new BiomeCactusBlockGeometry( &tx[0], biomeChannel, offsets );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createRailGeometry( lua_State *L ) {
	unsigned txStraight = luaL_checktextureid( L, 1 );
	unsigned txTurn = luaL_opttextureid( L, 2, txStraight );

	BlockGeometry *geom = new RailGeometry( txStraight, txTurn );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createDoorGeometry( lua_State *L ) {
	unsigned tx = luaL_checktextureid( L, 1 );

	BlockGeometry *geom = new DoorBlockGeometry( tx );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createStairsGeometry( lua_State *L ) {
	unsigned tx[6];
	luaL_checkfacingtextures( L, 1, &tx[0] );

	BlockGeometry *geom = new StairsBlockGeometry( &tx[0] );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createRedstoneWireGeometry( lua_State *L ) {
	// TODO: Redstone wires
	(void)L;
	return 0;
}

int BlockGeometry::createTorchGeometry( lua_State *L ) {
	unsigned tx = luaL_checktextureid( L, 1 );

	BlockGeometry *geom = new TorchGeometry( tx );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createFlowerGeometry( lua_State *L ) {
	unsigned tx = luaL_checktextureid( L, 1 );

	BlockGeometry *geom = new FlowerGeometry( tx );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::createFlowerBiomeGeometry( lua_State *L ) {
	unsigned biomeChannel = (unsigned)luaL_checknumber( L, 1 );
	unsigned tx = luaL_checktextureid( L, 2 );

	BlockGeometry *geom = new BiomeFlowerGeometry( tx, biomeChannel );
	geom->setupLuaObject( L, BLOCKGEOMETRY_META );
	geom->lua_push();
	return 1;
}

int BlockGeometry::lua_destroy( lua_State *L ) {
	delete getLuaObjectArg<BlockGeometry>( L, 1, BLOCKGEOMETRY_META );
	*(void**)luaL_checkudata( L, 1, BLOCKGEOMETRY_META ) = NULL;
	return 0;
}

static const luaL_Reg BlockGeometry_functions[] = {
	{ "setTexScale", &BlockGeometry::lua_setTexScale },
	{ "renderGroupAdd", &BlockGeometry::lua_renderGroupAdd },
	{ "destroy", &BlockGeometry::lua_destroy },
	{ NULL, NULL }
};

static const luaL_Reg BlockGeometry_geom[] = {
	{ "dataAdapter", &BlockGeometry::createDataAdapter },
	{ "rotatingAdapter", &BlockGeometry::createRotatingAdapter },
	{ "faceBitAdapter", &BlockGeometry::createFaceBitAdapter },
	{ "facingAdapter", &BlockGeometry::createFacingAdapter },
	{ "topDifferentAdapter", &BlockGeometry::createTopDifferentAdapter },

	{ "opaqueBlock", &BlockGeometry::createOpaqueBlockGeometry },
	{ "brightOpaqueBlock", &BlockGeometry::createBrightOpaqueBlockGeometry },
	{ "transparentBlock", &BlockGeometry::createTransparentBlockGeometry },
	{ "squashedBlock", &BlockGeometry::createSquashedBlockGeometry },
	{ "compactedBlock", &BlockGeometry::createCompactedGeometry },
	{ "multiBlockInBlock", &BlockGeometry::createMultiBlockInBlock },
	{ "multiCompactedBlock", &BlockGeometry::createMultiCompactedGeometry },
	{ "biomeOpaqueBlock", &BlockGeometry::createBiomeOpaqueGeometry },
	{ "biomeAlphaOpaqueBlock", &BlockGeometry::createBiomeAlphaOpaqueGeometry },
	{ "portal", &BlockGeometry::createPortalGeometry },
	{ "cactus", &BlockGeometry::createCactusGeometry },
	{ "biomeCactus", &BlockGeometry::createBiomeCactusGeometry },
	{ "rail", &BlockGeometry::createRailGeometry },
	{ "door", &BlockGeometry::createDoorGeometry },
	{ "stairs", &BlockGeometry::createStairsGeometry },
	//{ "redstone", &BlockGeometry::createRedstoneWireGeometry },
	{ "torch", &BlockGeometry::createTorchGeometry },
	{ "flower", &BlockGeometry::createFlowerGeometry },
	{ "biomeFlower", &BlockGeometry::createFlowerBiomeGeometry },

	{ NULL, NULL }
};

void BlockGeometry::setupLua( lua_State *L ) {
	luaL_newmetatable( L, BLOCKGEOMETRY_META );
	lua_pushvalue( L, -1 );
	lua_setfield( L, -2, "__index" );
	luaL_register( L, NULL, &BlockGeometry_functions[0] );
	lua_pop( L, 1 );

	lua_newtable( L );
	luaL_register( L, NULL, &BlockGeometry_geom[0] );
	lua_setfield( L, -2, "geom" );
}



} // namespace mcgeom
