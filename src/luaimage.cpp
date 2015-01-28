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


#include <SDL.h>
#include <SDL_image.h>
#include <lua.hpp>
#include <GL/glew.h>
#include <zlib.h>
#include <png.h>
#include <cassert>
#include <fstream>

#include "luaimage.h"
#include "unzip.h"

#ifdef _WINDOWS
// Remove libpng setjmp warning
#pragma warning( disable: 4611 )
#endif

extern unsigned g_width, g_height;

static void dilateNonAlpha( unsigned char *inc, unsigned w, unsigned h ) {
	for( unsigned y = 0; y < h; y++ ) {
		for( unsigned x = 0; x < w; x++ ) {
			if( !inc[3] ) {
				unsigned r = 0, g = 0, b = 0, a = 0;
				if( x > 0 && inc[-1] ) {
					a += inc[-1];
					r += inc[-1] * inc[-4];
					g += inc[-1] * inc[-3];
					b += inc[-1] * inc[-2];
				}
				if( x < w-1 && inc[7] ) {
					a += inc[7];
					r += inc[7] * inc[4];
					g += inc[7] * inc[5];
					b += inc[7] * inc[6];
				}
				int o = -(int)(w<<2);
				if( y > 0 && inc[o+3] ) {
					a += inc[o+3];
					r += inc[o+3] * inc[o];
					g += inc[o+3] * inc[o+1];
					b += inc[o+3] * inc[o+2];
				}
				o = w<<2;
				if( y < h-1 && inc[o+3] ) {
					a += inc[o+3];
					r += inc[o+3] * inc[o];
					g += inc[o+3] * inc[o+1];
					b += inc[o+3] * inc[o+2];
				}
				if( a ) {
					inc[0] = (unsigned char)(r/a);
					inc[1] = (unsigned char)(g/a);
					inc[2] = (unsigned char)(b/a);
				}
			}
			inc += 4;
		}
	}
}

static void uploadTextureWithMipmaps( unsigned char *tex, unsigned w, unsigned h, bool alphaWeight ) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
  // TODO: Swap colors on Big Endian
    // There must be a better way to do this
  for (unsigned i = 0; i < w*h; ++i)
    *(uint32_t *)&tex[4*i] = bswap(*(uint32_t *)&tex[4*i]);
#endif

	if( alphaWeight )
		dilateNonAlpha( tex, w, h );
	glTexImage2D (GL_TEXTURE_2D, 0, 4, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex); 

	unsigned char *tex2 = new unsigned char[w*h*4];
	unsigned mip = 0;
	unsigned char *tx = tex2;

	while( w != 1 || h != 1 ) {
		w >>= 1;
		if( !w ) w = 1;
		h >>= 1;
		if( !h ) h = 1;
		mip++;
		
		unsigned char *inc = tex, *outc = tx;
		unsigned w3 = w<<3;
		if( alphaWeight ) {
			// Box filter with texels weighted by alpha
			bool noalpha = false;
			for( unsigned y = 0; y < h; y++ ) {
				for( unsigned x = 0; x < w; x++ ) {
					unsigned a = inc[3] + inc[7] + inc[w3+3] + inc[w3+7];
					if( a ) {
						if( a == 255*4 ) {
							unsigned r = inc[0] + inc[4] + inc[w3+0] + inc[w3+4];
							unsigned g = inc[1] + inc[5] + inc[w3+1] + inc[w3+5];
							unsigned b = inc[2] + inc[6] + inc[w3+2] + inc[w3+6];
							outc[0] = (unsigned char)(r>>2);
							outc[1] = (unsigned char)(g>>2);
							outc[2] = (unsigned char)(b>>2);
							outc[3] = 255u;
						} else {
							unsigned r = (unsigned)inc[3]*inc[0] + (unsigned)inc[7]*inc[4]
									   + (unsigned)inc[w3+3]*inc[w3+0] + (unsigned)inc[w3+7]*inc[w3+4];
							unsigned g = (unsigned)inc[3]*inc[1] + (unsigned)inc[7]*inc[5]
									   + (unsigned)inc[w3+3]*inc[w3+1] + (unsigned)inc[w3+7]*inc[w3+5];
							unsigned b = (unsigned)inc[3]*inc[2] + (unsigned)inc[7]*inc[6]
									   + (unsigned)inc[w3+3]*inc[w3+2] + (unsigned)inc[w3+7]*inc[w3+6];
							outc[0] = (unsigned char)(r/a);
							outc[1] = (unsigned char)(g/a);
							outc[2] = (unsigned char)(b/a);
							outc[3] = (unsigned char)(a>>2);
						}
					} else {
						outc[0] = 0;
						outc[1] = 0;
						outc[2] = 0;
						outc[3] = 0;
						noalpha = true;
					}
					inc += 8;
					outc += 4;
				}
				inc += w*8;
			}

			// Dilate the alphaed sections
			if( noalpha )
				dilateNonAlpha( tx, w, h );
		} else {
			// Basic box filter on all channels
			for( unsigned y = 0; y < h; y++ ) {
				for( unsigned x = 0; x < w; x++ ) {
					unsigned r = inc[0] + inc[4] + inc[w3+0] + inc[w3+4];
					unsigned g = inc[1] + inc[5] + inc[w3+1] + inc[w3+5];
					unsigned b = inc[2] + inc[6] + inc[w3+2] + inc[w3+6];
					unsigned a = inc[3] + inc[7] + inc[w3+3] + inc[w3+7];
					outc[0] = (unsigned char)(r>>2);
					outc[1] = (unsigned char)(g>>2);
					outc[2] = (unsigned char)(b>>2);
					outc[3] = (unsigned char)(a>>2);
					inc += 8;
					outc += 4;
				}
				inc += w*8;
			}
		}

		glTexImage2D (GL_TEXTURE_2D, mip, 4, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tx); 

		tex = tx;
		tx += w*h*4;
	}

	delete[] tex2;
}

static SDL_Surface *convertSurfaceTo32Bit( SDL_Surface *s ) {
	if( s->format->Rmask != 0xffu || s->format->Gmask != 0xff00u || s->format->Bmask != 0xff0000u || s->format->Amask != 0xff000000u ) {
		SDL_Surface *s2 = SDL_ConvertSurfaceFormat( s, SDL_PIXELFORMAT_ABGR8888, SDL_SWSURFACE );
		SDL_FreeSurface (s);
		s = s2;
	}
	return s;
}

static int luaImageNew( lua_State *L ) {
	int width = (int)luaL_checknumber( L, 1 );
	int height = (int)luaL_checknumber( L, 2 );
	luaL_argcheck( L, width > 0 && height > 0, 1, "Size must be positive" );
	uint32_t color = (uint32_t)(luaL_optnumber( L, 3, 0.0 ) * 255.0)
	              | ((uint32_t)(luaL_optnumber( L, 4, 0.0 ) * 255.0) << 8)
	              | ((uint32_t)(luaL_optnumber( L, 5, 0.0 ) * 255.0) << 16)
	              | ((uint32_t)(luaL_optnumber( L, 6, 1.0 ) * 255.0) << 24);

	SDL_Surface *surf = SDL_CreateRGBSurface( SDL_SWSURFACE, width, height, 32, 0xffu, 0xff00u, 0xff0000u, 0xff000000u );
	if( !surf )
		return 0;

	if( SDL_LockSurface( surf ) ) {
		lua_pushstring( L, "Failed to lock the new surface." );
		lua_error( L );
	}

	unsigned char *pix = (unsigned char*)surf->pixels;
	for( int y = 0; y < height; y++, pix += surf->pitch ) {
		uint32_t *px = (uint32_t*)pix;
		for( int x = 0; x < width; x++ )
			(*px++) = color;
	}

	SDL_UnlockSurface( surf );

	*(SDL_Surface**)lua_newuserdata( L, sizeof( SDL_Surface* ) ) = surf;
	luaL_newmetatable( L, LUAIMAGE_META );
	lua_setmetatable( L, -2 );
	return 1;
}

static int luaImageLoadFromFile( lua_State *L ) {
	const char *path = luaL_checkstring( L, 1 );
	SDL_Surface *s = IMG_Load( path );
	if( s ) {
		SDL_Surface **pSurf = (SDL_Surface**)lua_newuserdata( L, sizeof( SDL_Surface* ) );
		*pSurf = convertSurfaceTo32Bit( s );
		luaL_newmetatable( L, LUAIMAGE_META );
		lua_setmetatable( L, -2 );
		return 1;
	}
	return 0;
}

static int luaImageLoadFromZip( lua_State *L ) {
	const char *zippath = luaL_checkstring( L, 1 );
	const char *subzippath = luaL_checkstring( L, 2 );
	Unzip zipfile( zippath );
	if( zipfile.good() ) {
		Unzip::Iterator it = zipfile.find( subzippath );
		if( it != zipfile.end() ) {
			std::string buf( it->uncompressed_size(), '\0' );
			void *bufptr = reinterpret_cast<void*>(&*buf.begin());
			if( 0 != it->uncompress( bufptr ) ) {
				SDL_Surface *s = IMG_Load_RW( SDL_RWFromMem( bufptr, int(it->uncompressed_size())), 1 );
				if( s ) {
					SDL_Surface **pSurf = (SDL_Surface**)lua_newuserdata( L, sizeof( SDL_Surface* ) );
					*pSurf = convertSurfaceTo32Bit( s );
					luaL_newmetatable( L, LUAIMAGE_META );
					lua_setmetatable( L, -2 );
					return 1;
				}
			}
		}
	}
	return 0;
}

static int luaImageLoadFromZipDirect( lua_State *L ) {
	const char *zippath = luaL_checkstring( L, 1 );
	std::size_t start = (std::size_t)luaL_checkinteger( L, 2 );
	std::size_t comp_size = (std::size_t)luaL_checkinteger( L, 3 );

	std::ifstream f( zippath, std::ios::in | std::ios::binary );
	if( !f.good() ) {
		lua_pushstring( L, "Failed to open zip file" );
		lua_error( L );
	}
	if( !f.seekg( start, std::ios_base::beg ) ) {
		lua_pushstring( L, "Failed to seek" );
		lua_error( L );
	}

	void *data = NULL;
	std::size_t datasize;
	if( lua_isnumber( L, 4 ) ) {
		std::size_t uncomp_size = (std::size_t)luaL_checkinteger( L, 4 );
		void *inbuf = malloc( comp_size );
		if( !f.read((char*)inbuf, comp_size) ) {
			lua_pushstring( L, "Failed to read from file" );
			lua_error( L );
		}
		z_stream z;
		z.avail_in = (uInt)comp_size;
		z.next_in = reinterpret_cast<Bytef *>(inbuf);
		z.zalloc = NULL;
		z.zfree = NULL;
		if( inflateInit2(&z, -MAX_WBITS) != Z_OK ) {
			lua_pushstring( L, "Failed to initialize inflate stream" );
			lua_error( L );
		}
		data = malloc( uncomp_size );
		datasize = uncomp_size;
		z.avail_out = (uInt)uncomp_size;
		z.next_out = reinterpret_cast<Bytef *>(data);
		int err = inflate(&z, Z_FINISH);
		if( inflateEnd(&z) != Z_OK || err != Z_STREAM_END ) {
			free( data );
			free( inbuf );
			lua_pushstring( L, "Failed to inflate file" );
			lua_error( L );
		}
		free( inbuf );
	} else {
		data = malloc( comp_size );
		datasize = comp_size;
		if( !f.read((char*)data, comp_size) ) {
			lua_pushstring( L, "Failed to read from file" );
			lua_error( L );
		}
	}
	
	SDL_RWops *mem = SDL_RWFromMem( data, int(datasize) );
	SDL_Surface *s = IMG_LoadPNG_RW( mem );
	SDL_FreeRW( mem );
	free( data );
	if( s ) {
		SDL_Surface **pSurf = (SDL_Surface**)lua_newuserdata( L, sizeof( SDL_Surface* ) );
		*pSurf = convertSurfaceTo32Bit( s );
		luaL_newmetatable( L, LUAIMAGE_META );
		lua_setmetatable( L, -2 );
		return 1;
	}
	return 0;
}

static int luaImageScreengrab( lua_State *L ) {
	extern SDL_Window *g_window;
	SDL_Surface *vidSurf = SDL_GetWindowSurface( g_window );
	SDL_Surface *grab = SDL_CreateRGBSurface( SDL_SWSURFACE, g_width, g_height, vidSurf->format->BytesPerPixel*8, vidSurf->format->Rmask, vidSurf->format->Gmask, vidSurf->format->Bmask, vidSurf->format->Amask );
	glReadPixels( 0, 0, g_width, g_height, GL_RGBA, GL_UNSIGNED_BYTE, grab->pixels );
	*(SDL_Surface**)lua_newuserdata( L, sizeof( SDL_Surface* ) ) = grab;
	luaL_newmetatable( L, LUAIMAGE_META );
	lua_setmetatable( L, -2 );
	return 1;
}

static int luaImageUnloadGLTexture( lua_State *L ) {
	GLuint tex = (GLint)luaL_checknumber( L, 1 );
	glDeleteTextures( 1, &tex );
	return 0;
}

static int luaImageWriteToPNG( lua_State *L ) {
	SDL_Surface *im = *(SDL_Surface**)luaL_checkudata( L, 1, LUAIMAGE_META );
	const char *path = luaL_checkstring( L, 2 );

	FILE *f = NULL;
	if( SDL_LockSurface( im ) ) {
		lua_pushboolean( L, false );
		lua_pushstring( L, "Failed to lock the surface" );
		return 2;
	}

#define ABORT_SAVE(reason) do { if(f) fclose(f); delete rows; SDL_UnlockSurface( im ); lua_pushboolean( L, false ); lua_pushstring( L, reason ); return 2; } while(false)
	void **rows = new void*[im->h];
	if( !rows )
		ABORT_SAVE( "Out of memory" );

	f = fopen( path, "wb" );
	if( !f )
		ABORT_SAVE( "Could not open output file" );

	for( int i = 0; i < im->h; i++ )
		rows[im->h - i - 1] = ((char*)im->pixels) + im->pitch * i;

	png_structp png_ptr;
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!png_ptr)
		ABORT_SAVE( "Could not initialize libpng" );

    png_init_io(png_ptr, f);
	png_set_filter(png_ptr, 0, PNG_FILTER_PAETH);

	png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
		ABORT_SAVE( "Could not initialize libpng" );

    if (setjmp(png_jmpbuf(png_ptr)))
		ABORT_SAVE( "Failed to write the png" );

    png_set_IHDR(png_ptr, info_ptr, im->w, im->h,
		8, im->format->BitsPerPixel == 32 ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);
    png_write_image(png_ptr, (png_bytepp)rows);
    png_write_end(png_ptr, NULL);

	png_destroy_write_struct(&png_ptr, &info_ptr);

    fclose(f);
	delete[] rows;
	SDL_UnlockSurface( im );

#undef ABORT_SAVE
	lua_pushboolean( L, true );
	return 1;
}

static int luaImageSub( lua_State *L ) {
	SDL_Surface *im = *(SDL_Surface**)luaL_checkudata( L, 1, LUAIMAGE_META );
	int x = (int)luaL_checknumber( L, 2 );
	int y = (int)luaL_checknumber( L, 3 );
	int w = (int)luaL_checknumber( L, 4 );
	int h = (int)luaL_checknumber( L, 5 );

	if( x < 0 || y < 0 || x + w > im->w || y + h > im->h ) {
		lua_pushstring( L, "The subimage must be inside the image" );
		lua_error( L );
	}

	SDL_Surface *im2 = SDL_CreateRGBSurfaceFrom( (unsigned char*)im->pixels + im->pitch * y + x * 4, w, h,
		im->format->BitsPerPixel, im->pitch, im->format->Rmask, im->format->Gmask, im->format->Bmask, im->format->Amask );
	SDL_Surface *imcopy = SDL_ConvertSurface( im2, im2->format, SDL_SWSURFACE );
	SDL_FreeSurface( im2 );

	*(SDL_Surface**)lua_newuserdata( L, sizeof( SDL_Surface* ) ) = imcopy;
	luaL_newmetatable( L, LUAIMAGE_META );
	lua_setmetatable( L, -2 );
	return 1;
}

static int luaImageCopy( lua_State *L ) {
	SDL_Surface *im = *(SDL_Surface**)luaL_checkudata( L, 1, LUAIMAGE_META );
	SDL_Surface *im2 = SDL_ConvertSurface( im, im->format, SDL_SWSURFACE );
	*(SDL_Surface**)lua_newuserdata( L, sizeof( SDL_Surface* ) ) = im2;
	luaL_newmetatable( L, LUAIMAGE_META );
	lua_setmetatable( L, -2 );
	return 1;
}

static int luaImageUploadToGL( lua_State *L ) {
	SDL_Surface *im = *(SDL_Surface**)luaL_checkudata( L, 1, LUAIMAGE_META );
	GLenum clamp = GL_REPEAT;
	GLenum magFilter = GL_LINEAR;
	GLenum minFilter = GL_LINEAR;
	GLenum mipFilter = GL_NONE;
	GLfloat aniso = 0;
	bool alphaWeightMipmaps = false;

	int i = 2;
	GLuint tex = 0;
	if( lua_isnumber( L, i ) ) {
		lua_Number n = lua_tonumber( L, i );
		if( n < 1.0 ) {
			lua_pushstring( L, "Texture handle must be positive" );
			lua_error( L );
		}
		tex = (GLuint)n;
		i++;
	}

	for( ; i <= lua_gettop( L ); i++ ) {
		if( lua_isstring( L, i ) ) {
			const char *s = lua_tostring( L, i );
			switch( *s ) {
			case 'a':
				if( 0 == strncmp( "aniso_", s, 6 ) )
					aniso = (GLfloat)std::max( 0, atoi( s + 6 ) );
				else goto unknown_flag;
				break;
			case 'c':
				if( 0 == strcmp( "clamp", s ) )
					clamp = GL_CLAMP_TO_EDGE;
				else goto unknown_flag;
				break;
			case 'm':
				if( 0 == strcmp( "mag_nearest", s ) )
					magFilter = GL_NEAREST;
				else if( 0 == strcmp( "mag_linear", s ) )
					magFilter = GL_LINEAR;
				else if( 0 == strcmp( "min_nearest", s ) )
					minFilter = GL_NEAREST;
				else if( 0 == strcmp( "min_linear", s ) )
					minFilter = GL_LINEAR;
				else if( 0 == strcmp( "mip_nearest", s ) )
					mipFilter = GL_NEAREST;
				else if( 0 == strcmp( "mip_linear", s ) )
					mipFilter = GL_LINEAR;
				else if( 0 == strcmp( "mip_none", s ) )
					mipFilter = GL_NONE;
				else if( 0 == strcmp( "mipgen_box", s ) )
					alphaWeightMipmaps = false;
				else if( 0 == strcmp( "mipgen_alphawt_box", s ) )
					alphaWeightMipmaps = true;
				else goto unknown_flag;
				break;
			case 'r':
				if( 0 == strcmp( "repeat", s ) )
					clamp = GL_REPEAT;
				else goto unknown_flag;
				break;
			default:
				goto unknown_flag;
			}
		} else {
unknown_flag: ;
			lua_pushstring( L, "Invalid uploadToGL flag" );
			lua_error( L );
		}
	}

	if( SDL_LockSurface( im ) ) {
		lua_pushstring( L, "Failed to lock the surface" );
		lua_error( L );
	}

	glEnable( GL_TEXTURE_2D );
	if( !tex )
		glGenTextures( 1, &tex );
	glBindTexture( GL_TEXTURE_2D, tex );

    //glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp );

	GLubyte *pixels = (GLubyte*)im->pixels;
	GLubyte *tempPixels = NULL;
	if( im->pitch != im->w*4 ) {
		tempPixels = (GLubyte*)malloc( im->w*im->h*4 );
		GLubyte *pxDest = tempPixels;
		for( int y = 0; y < im->h; y++ ) {
			memcpy( pxDest, pixels, im->w*4 );
			pxDest += im->w*4;
			pixels += im->pitch;
		}
		pixels = tempPixels;
	}

	if( mipFilter != GL_NONE ) {
		if( mipFilter == GL_LINEAR ) {
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter == GL_LINEAR ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR );
			if( GLEW_EXT_texture_filter_anisotropic && aniso > 0 )
				glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso ); 
		} else {
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter == GL_LINEAR ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST );
		}
	    uploadTextureWithMipmaps( pixels, im->w, im->h, alphaWeightMipmaps );
		//glTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE );
	} else {
	    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter );
		glTexImage2D( GL_TEXTURE_2D, 0, 4, im->w, im->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels ); 
	}

	free( tempPixels );

	glBindTexture( GL_TEXTURE_2D, 0 );
	SDL_UnlockSurface( im );

	lua_pushnumber( L, tex );
	return 1;
}

static int luaImageGrayToAlpha( lua_State *L ) {
	SDL_Surface *im = *(SDL_Surface**)luaL_checkudata( L, 1, LUAIMAGE_META );
	SDL_Surface *gray = *(SDL_Surface**)luaL_checkudata( L, 2, LUAIMAGE_META );

	if( im->w != gray->w || im->h != gray->h ) {
		lua_pushstring( L, "Images must be the same size" );
		lua_error( L );
	}

	if( SDL_LockSurface( im ) || SDL_LockSurface( gray ) ) {
		lua_pushstring( L, "Failed to lock the surface" );
		lua_error( L );
	}

	unsigned char *destPix = (unsigned char*)im->pixels;
	unsigned char *srcPix = (unsigned char*)gray->pixels;
	for( int y = 0; y < im->h; y++ ) {
		unsigned char *dest = destPix;
		unsigned char *src = srcPix;

		for( int x = 0; x < im->w; x++ ) {
			unsigned mix = src[3];
			dest[3] = (unsigned char)(mix * (((unsigned)src[0] + src[1] + src[2]) / 3) >> 8);
			
			mix = 255 - mix;
			dest[0] = (unsigned char)(((unsigned)dest[0] * mix) >> 8);
			dest[1] = (unsigned char)(((unsigned)dest[1] * mix) >> 8);
			dest[2] = (unsigned char)(((unsigned)dest[2] * mix) >> 8);

			dest += 4;
			src += 4;
		}

		destPix += im->pitch;
		srcPix += gray->pitch;
	}

	SDL_UnlockSurface( im );
	SDL_UnlockSurface( gray );

	return 0;
}

static int luaImagePut( lua_State *L ) {
	SDL_Surface *im = *(SDL_Surface**)luaL_checkudata( L, 1, LUAIMAGE_META );
	SDL_Surface *isrc = *(SDL_Surface**)luaL_checkudata( L, 2, LUAIMAGE_META );
	int xo = (int)luaL_checknumber( L, 3 );
	int yo = (int)luaL_checknumber( L, 4 );
	int xsh = 0, ysh = 0;

	int w = isrc->w, h = isrc->h;
	if( xo < 0 ) {
		xsh = -xo;
		w += xo;
		xo = 0;
	}
	if( yo < 0 ) {
		ysh = -yo;
		h += yo;
		yo = 0;
	}
	if( xo + w > im->w )
		w = im->w - xo;
	if( yo + h > im->h )
		h = im->h - yo;
	if( w <= 0 || h <= 0 )
		return 0;

	if( SDL_LockSurface( im ) || SDL_LockSurface( isrc ) ) {
		lua_pushstring( L, "Failed to lock the surface" );
		lua_error( L );
	}

	unsigned char *destPix = (unsigned char*)im->pixels + 4*xo + im->pitch*yo;
	unsigned char *srcPix = (unsigned char*)isrc->pixels + 4*xsh + isrc->pitch*ysh;
	for( int y = 0; y < h; y++ ) {
		unsigned char *dest = destPix;
		unsigned char *src = srcPix;

		for( int x = 0; x < w; x++ ) {
			dest[0] = src[0];
			dest[1] = src[1];
			dest[2] = src[2];
			dest[3] = src[3];
			dest += 4;
			src += 4;
		}

		destPix += im->pitch;
		srcPix += isrc->pitch;
	}

	SDL_UnlockSurface( im );
	SDL_UnlockSurface( isrc );

	return 0;
}

static int luaImageDestroy( lua_State *L ) {
	SDL_Surface **pim = (SDL_Surface**)luaL_checkudata( L, 1, LUAIMAGE_META );
	SDL_FreeSurface( *pim );
	*pim = NULL;
	return 0;
}

static int luaImageRelease( lua_State *L ) {
	SDL_Surface **pim = (SDL_Surface**)luaL_checkudata( L, 1, LUAIMAGE_META );
	*pim = NULL;
	return 0;
}

static int luaImageGet( lua_State *L ) {
	SDL_Surface *im = *(SDL_Surface**)luaL_checkudata( L, 1, LUAIMAGE_META );
	if( !lua_isstring( L, 2 ) )
		return 0;
	const char *what = lua_tostring( L, 2 );

	switch( *what ) {
	case 'c':
		if( 0 == strcmp( what, "copy" ) ) {
			lua_pushcfunction( L, luaImageCopy );
			return 1;
		}
		break;
	case 'd':
		if( 0 == strcmp( what, "destroy" ) ) {
			lua_pushcfunction( L, luaImageDestroy );
			return 1;
		}
		break;
	case 'g':
		if( 0 == strcmp( what, "grayToAlpha" ) ) {
			lua_pushcfunction( L, luaImageGrayToAlpha );
			return 1;
		}
		break;
	case 'h':
		if( 0 == strcmp( what, "height" ) ) {
			lua_pushnumber( L, im->h );
			return 1;
		}
		break;
	case 'p':
		if( 0 == strcmp( what, "put" ) ) {
			lua_pushcfunction( L, luaImagePut );
			return 1;
		}
		break;
	case 'r':
		if( 0 == strcmp( what, "release" ) ) {
			lua_pushcfunction( L, luaImageRelease );
			return 1;
		}
		break;
	case 's':
		if( 0 == strcmp( what, "sub" ) ) {
			lua_pushcfunction( L, luaImageSub );
			return 1;
		}
		break;
	case 'u':
		if( 0 == strcmp( what, "uploadToGL" ) ) {
			lua_pushcfunction( L, luaImageUploadToGL );
			return 1;
		}
		break;
	case 'w':
		if( 0 == strcmp( what, "width" ) ) {
			lua_pushnumber( L, im->w );
			return 1;
		} else if( 0 == strcmp( what, "writePNG" ) ) {
			lua_pushcfunction( L, luaImageWriteToPNG );
			return 1;
		}
		break;
	}

	return 0;
}

void LuaImage_setupLua( lua_State *L ) {
	luaL_newmetatable( L, LUAIMAGE_META );
	lua_pushcfunction( L, &luaImageGet );
	lua_setfield( L, -2, "__index" );
	lua_pushcfunction( L, &luaImageDestroy );
	lua_setfield( L, -2, "__gc" );
	lua_pop( L, 1 );

	lua_pushcfunction( L, &luaImageNew );
	lua_setfield( L, -2, "newImage" );
	lua_pushcfunction( L, &luaImageLoadFromFile );
	lua_setfield( L, -2, "loadImage" );
	lua_pushcfunction( L, &luaImageLoadFromZip );
	lua_setfield( L, -2, "loadImageZip" );
	lua_pushcfunction( L, &luaImageLoadFromZipDirect );
	lua_setfield( L, -2, "loadImageZipDirect" );
	lua_pushcfunction( L, &luaImageScreengrab );
	lua_setfield( L, -2, "screengrab" );
	lua_pushcfunction( L, &luaImageUnloadGLTexture );
	lua_setfield( L, -2, "unloadTexture" );
}

