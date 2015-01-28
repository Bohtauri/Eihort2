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


#include <SDL_image.h>
#include <GL/glew.h>

#include "mcregionmap.h"
#include "mcmap.h"
#include "mcbiome.h"
#include "platform.h"
#include "endian.h"

inline unsigned short invertFoliage( unsigned short in ) {
	return 0xffffu - in;
}

MCBiome::MCBiome()
: enabled(0)
, biomePath("")
{
	for( unsigned i = 0; i < MAX_BIOME_CHANNELS; i++ ) {
		channels[i].enabled = false;
		channels[i].upperTriangle = false;
		channels[i].defTex = 0u;
		channels[i].colours = NULL;
	}
}

MCBiome::~MCBiome() {
	for( unsigned i = 0; i < MAX_BIOME_CHANNELS; i++ )
		emptyChannel( i );
}

void MCBiome::disableBiomeChannel( unsigned channel, unsigned color ) {
	emptyChannel( channel );

	channels[channel].enabled = false;
	glGenTextures( 1, &channels[channel].defTex );
	glBindTexture( GL_TEXTURE_2D, channels[channel].defTex );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, &color );
	glBindTexture( GL_TEXTURE_2D, 0 );
}

void MCBiome::enableBiomeChannel( unsigned channel, SDL_Surface *surf, bool upperTriangle ) {
	emptyChannel( channel );

	enabled++;
	channels[channel].enabled = true;
	channels[channel].upperTriangle = upperTriangle;
	glGenTextures( 1, &channels[channel].defTex );
	channels[channel].colours = surf;
	surf->refcount++;

	glBindTexture( GL_TEXTURE_2D, channels[channel].defTex );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, ((unsigned*)surf->pixels + defPos) );
	glBindTexture( GL_TEXTURE_2D, 0 );
}

void MCBiome::emptyChannel( unsigned channel ) {
	if( channels[channel].enabled ) {
		enabled--;
		channels[channel].enabled = false;
	}
	if( channels[channel].defTex )
		glDeleteTextures( 1, &channels[channel].defTex );
	channels[channel].defTex = 0;
	SDL_FreeSurface( channels[channel].colours );
	channels[channel].colours = NULL;
}

unsigned MCBiome::finalizeBiomeTextures( unsigned short *coords, int minx, int maxx, int miny, int maxy, unsigned *textures ) const {
	if( !coords ) {
		// Fill in dummy textures
		for( unsigned i = 0; i < MAX_BIOME_CHANNELS; i++ )
			textures[i] = channels[i].defTex;
		return 0;
	}

	unsigned w = (unsigned)(maxx-minx+1), h = (unsigned)(maxy-miny+1);
	unsigned len = w * h;
	unsigned gpuSize = 0;

	// Fill in disabled and lower-triangular textures
	for( unsigned i = 0; i < MAX_BIOME_CHANNELS; i++ ) {
		if( !channels[i].enabled ) {
			textures[i] = channels[i].defTex;
		} else if( !channels[i].upperTriangle ) {
			textures[i] = coordsToTexture( w, h, len, (unsigned*)channels[i].colours->pixels, coords );
			gpuSize += len * 4;
		}
	}

	// Fill in upper-triangular textures
	for( unsigned i = 0; i < len; i++ )
		coords[i] = invertFoliage( coords[i] );
	for( unsigned i = 0; i < MAX_BIOME_CHANNELS; i++ ) {
		if( channels[i].upperTriangle ) {
			textures[i] = coordsToTexture( w, h, len, (unsigned*)channels[i].colours->pixels, coords );
			gpuSize += len * 4;
		}
	}

	delete[] coords;
	return gpuSize;
}

void MCBiome::freeBiomeTextures( unsigned *textures ) const {
	for( unsigned i = 0; i < MAX_BIOME_CHANNELS; i++ ) {
		if( textures[i] != channels[i].defTex )
			glDeleteTextures( 1, textures + i );
	}
}

unsigned short *MCBiome::readBiomeCoords( MCMap *map, int minx, int maxx, int miny, int maxy ) const {
	if( enabled && ((map && map->getRegions()->isAnvil()) || biomePath.length() > 0) ) {
		unsigned w = (unsigned)(maxx-minx+1), h = (unsigned)(maxy-miny+1);
		unsigned len = w * h;
		unsigned short *coords = new unsigned short[len];

		bool readCoords;
		if( map && map->getRegions()->isAnvil() ) {
			readCoords = readBiomeCoords_anvil( map, minx, maxx, miny, maxy, coords );
		} else {
			readCoords = readBiomeCoords_extracted( miny, maxy, minx, maxx, coords );
		}
		if( readCoords )
			return coords;
		if( !readCoords ) {
			delete[] coords;
			coords = NULL;
		}
		return coords;
	}
	return NULL;
}

template<class, class>
struct assert_types_same;

template<class T>
struct assert_types_same<T, T>
{};

bool MCBiome::readBiomeCoords_extracted( int minx, int maxx, int miny, int maxy, unsigned short *out ) const {
	unsigned w = maxx - minx + 1;
	unsigned h = maxy - miny + 1;
	bool loadedSomething = false;
	int maxRgX = shift_right( maxx, 9 );
	int maxRgY = shift_right( maxy, 9 );
	unsigned destXStride = (maxx - minx + 1);
	unsigned short *dest = new unsigned short[w*h];
	unsigned short *origdest = dest;

	for( int x = minx; x <= maxx; ) {
		int rgX = shift_right( x, 9 );
		int thisMaxX = rgX < maxRgX ? ((rgX + 1) << 9) - 1 : maxx;
		int dx = x - (rgX << 9);
		unsigned readWidth = (unsigned)(thisMaxX - x + 1);

		for( int y = miny; y <= maxy; ) {
			int rgY = shift_right( y, 9 );
			int thisMaxY = rgY < maxRgY ? ((rgY + 1) << 9) - 1 : maxy;
			int dy = y - (rgY << 9);

			assert_types_same<unsigned short, uint16_t>();
			unsigned short *tgt = dest + ((y - miny) * destXStride) + (x - minx);

			char biomefn[MAX_PATH];
			snprintf( biomefn, MAX_PATH, "%sb.%d.%d.biome", biomePath.c_str(), rgX, rgY );
			FILE *f = fopen( biomefn, "rb" );
			if( f ) {
				loadedSomething = true;
				int offset = ((dy << 9) + dx)<<1;
				fseek( f, offset, SEEK_SET );
				for( int yy = y; yy <= thisMaxY; yy++ ) {
					fread( tgt, 2, readWidth, f );
					for( int xx = x; xx <= thisMaxX; xx++, tgt++ ) {
						unsigned short coords = bswap_from_big(*tgt);
						unsigned short c1 = coords & 0xffu;
						unsigned short c2 = 0xffu - (coords >> 8);
						if( c1 + c2 >= 0xff ) {
							do {
								if( c1 ) c1--;
								if( c2 ) c2--;
							} while( c1 + c2 >= 0xff );
							coords = c1 | ((0xff-c2) << 8);
						}
						*tgt = coords;
					}
					tgt += destXStride - readWidth;
					offset += 2 << 9;
					fseek( f, offset, SEEK_SET );
				}
				fclose( f );
			} else {
				for( int yy = y; yy <= thisMaxY; yy++ ) {
					for( int xx = x; xx <= thisMaxX; xx++, tgt++ )
						*tgt = defPos;
					tgt += destXStride - readWidth;
				}
			}

			y = thisMaxY + 1;
		}

		x = thisMaxX + 1;
	}

	// Transpose the image to swap X and Y
	for( unsigned yo = 0; yo < h; yo++ ) {
		for( unsigned xo = 0; xo < w; xo++ ) {
			out[xo*h+yo] = origdest[yo*w+xo];
		}	
	}
	delete[] origdest;

	return loadedSomething;
}

bool MCBiome::readBiomeCoords_anvil( MCMap *map, int minx, int maxx, int miny, int maxy, unsigned short *dest ) const {
	bool loadedSomething = false;
	for( int y = miny; y <= maxy; y++ ) {
		for( int x = minx; x <= maxx; x++ ) {
			bool ld = map->getBiomeCoords( x, y, *dest );
			loadedSomething |= ld;
			if( !ld )
				*dest = defPos;
			dest++;
		}
	}
	return loadedSomething;
}

unsigned MCBiome::coordsToTexture( unsigned w, unsigned h, unsigned len, unsigned *colours, unsigned short *coords ) const {
	unsigned tex;
	glGenTextures( 1, &tex );

	glBindTexture( GL_TEXTURE_2D, tex );

	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	//glTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE );

	unsigned *pixels = new unsigned[len];
	for( unsigned i = 0; i < len; i++ ) {
		unsigned col = colours[coords[i]];
		if( (col & 0xffffffu) == 0xffffffu ) // If the biome texture is white, use the other triangle
			col = colours[invertFoliage( coords[i] )];
		pixels[i] = col;
	}

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB5, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0] );
	delete[] pixels;

	return tex;
}


