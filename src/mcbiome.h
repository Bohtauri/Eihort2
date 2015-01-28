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

#ifndef MCBIOME_H
#define MCBIOME_H

#include <string>

struct SDL_Surface;

class MCMap;

class MCBiome {
public:
	MCBiome();
	~MCBiome();

	enum{ MAX_BIOME_CHANNELS = 8 };

	inline void setBiomeRootPath( const char *s ) { biomePath = s; }
	inline const char *getBiomeRootPath() const { return biomePath.c_str(); }

	void disableBiomeChannel( unsigned channel, unsigned color );
	void enableBiomeChannel( unsigned channel, SDL_Surface *surf, bool upperTriangle );
	inline void setDefaultPos( unsigned short pos ) { defPos = pos; }

	// Texture management
	unsigned finalizeBiomeTextures( unsigned short *coords, int minx, int maxx, int miny, int maxy, unsigned *textures ) const;
	void freeBiomeTextures( unsigned *textures ) const;

	// Reads all biome channels for a region of the world
	unsigned short *readBiomeCoords( MCMap *map, int minx, int maxx, int miny, int maxy ) const;

private:
	//void loadColours( const char *fnFormat );
	//unsigned *loadColoursFrom( const char *fn );
	void emptyChannel( unsigned channel );
	bool readBiomeCoords_extracted( int minx, int maxx, int miny, int maxy, unsigned short *dest ) const;
	bool readBiomeCoords_anvil( MCMap *map, int minx, int maxx, int miny, int maxy, unsigned short *dest ) const;
	unsigned coordsToTexture( unsigned w, unsigned h, unsigned len, unsigned *colours, unsigned short *coords ) const;

	struct BiomeChannel {
		bool enabled;
		bool upperTriangle;
		unsigned short defPos;
		unsigned defTex;
		SDL_Surface *colours;
	};
	unsigned short defPos;
	unsigned enabled;
	BiomeChannel channels[MAX_BIOME_CHANNELS];
	std::string biomePath;
};

#endif // MCBIOME_H
