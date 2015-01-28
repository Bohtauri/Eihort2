/* Copyright (c) 2012, Jason Lloyd-Price and Antti Hakkinen
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
#include <GL/glew.h>
#include <cassert>
#include <cmath>
#include <cstdio>

#include "uidrawcontext.h"
#include "platform.h"

static bool font_initialized = false;
static unsigned gl_font_tex;
extern unsigned g_width, g_height;
//extern float g_aspect;

struct Glyph {
	unsigned x, y, w, h;
};

Glyph glyphs[] = {
	{ 8, 8, 16, 64 },
	{ 32, 8, 8, 64 },
	{ 48, 8, 13, 64 },
	{ 69, 8, 26, 64 },
	{ 103, 8, 19, 64 },
	{ 130, 8, 46, 64 },
	{ 184, 8, 34, 64 },
	{ 226, 8, 6, 64 },
	{ 240, 8, 15, 64 },
	{ 263, 8, 14, 64 },
	{ 285, 8, 20, 64 },
	{ 313, 8, 31, 64 },
	{ 352, 8, 9, 64 },
	{ 369, 8, 13, 64 },
	{ 390, 8, 8, 64 },
	{ 406, 8, 21, 64 },
	{ 8, 80, 27, 64 },
	{ 43, 80, 13, 64 },
	{ 64, 80, 23, 64 },
	{ 95, 80, 21, 64 },
	{ 124, 80, 28, 64 },
	{ 160, 80, 23, 64 },
	{ 191, 80, 25, 64 },
	{ 224, 80, 24, 64 },
	{ 256, 80, 24, 64 },
	{ 288, 80, 25, 64 },
	{ 321, 80, 8, 64 },
	{ 337, 80, 9, 64 },
	{ 354, 80, 31, 64 },
	{ 393, 80, 30, 64 },
	{ 431, 80, 31, 64 },
	{ 470, 80, 15, 64 },
	{ 8, 152, 40, 64 },
	{ 56, 152, 32, 64 },
	{ 96, 152, 22, 64 },
	{ 126, 152, 28, 64 },
	{ 162, 152, 32, 64 },
	{ 202, 152, 21, 64 },
	{ 231, 152, 21, 64 },
	{ 260, 152, 33, 64 },
	{ 301, 152, 33, 64 },
	{ 342, 152, 8, 64 },
	{ 358, 152, 16, 64 },
	{ 382, 152, 29, 64 },
	{ 419, 152, 20, 64 },
	{ 447, 152, 45, 64 },
	{ 500, 152, 33, 64 },
	{ 541, 152, 38, 64 },
	{ 8, 224, 23, 64 },
	{ 39, 224, 38, 64 },
	{ 85, 224, 29, 64 },
	{ 122, 224, 19, 64 },
	{ 149, 224, 30, 64 },
	{ 187, 224, 31, 64 },
	{ 226, 224, 32, 64 },
	{ 266, 224, 51, 64 },
	{ 325, 224, 29, 64 },
	{ 362, 224, 29, 64 },
	{ 399, 224, 25, 64 },
	{ 432, 224, 13, 64 },
	{ 453, 224, 21, 64 },
	{ 482, 224, 13, 64 },
	{ 503, 224, 30, 64 },
	{ 541, 224, 26, 64 },
	{ 8, 296, 12, 64 },
	{ 28, 296, 22, 64 },
	{ 58, 296, 24, 64 },
	{ 90, 296, 20, 64 },
	{ 118, 296, 25, 64 },
	{ 151, 296, 22, 64 },
	{ 181, 296, 17, 64 },
	{ 206, 296, 24, 64 },
	{ 238, 296, 23, 64 },
	{ 269, 296, 8, 64 },
	{ 285, 296, 13, 64 },
	{ 306, 296, 23, 64 },
	{ 337, 296, 8, 64 },
	{ 353, 296, 39, 64 },
	{ 400, 296, 24, 64 },
	{ 432, 296, 27, 64 },
	{ 8, 368, 25, 64 },
	{ 41, 368, 25, 64 },
	{ 74, 368, 17, 64 },
	{ 99, 368, 13, 64 },
	{ 120, 368, 16, 64 },
	{ 144, 368, 23, 64 },
	{ 175, 368, 26, 64 },
	{ 209, 368, 40, 64 },
	{ 257, 368, 23, 64 },
	{ 288, 368, 26, 64 },
	{ 322, 368, 21, 64 },
	{ 351, 368, 18, 64 },
	{ 377, 368, 5, 64 },
	{ 390, 368, 18, 64 },
	{ 416, 368, 35, 64 },
	{ 0, 0, 0, 0 }
};

const unsigned glyph_count = sizeof(glyphs)/sizeof(Glyph) - 1;
const unsigned font_bmp_width = 587;
const unsigned font_bmp_height = 440;
const unsigned font_char_height = glyphs[0].h;
const float SPACE_SIZE_RATIO = glyphs[0].w / (float)glyphs[0].h;
const float TAB_SIZE_RATIO = 4*SPACE_SIZE_RATIO;

void errorOut( const char *msg );

UIDrawContext::UIDrawContext( bool initGL )
: x(0.0f)
, y(0.0f)
{
	white();

	if( !font_initialized ) {
		// Load the font
		extern char g_programRoot[MAX_PATH];
		char screen_fontBIN[MAX_PATH];
		snprintf( screen_fontBIN, MAX_PATH, RESOURCE_FOLDER_FORMAT "screen_font.bin", g_programRoot );
		FILE *f = fopen( screen_fontBIN, "rb" );
		if( !f ) {
			f = fopen( "screen_font.bin", "rb" );
			if( !f )
				errorOut( "screen_font.bin is missing." );
		}

		fseek( f, 0, SEEK_END );
		unsigned sz = (unsigned)ftell( f );
		fseek( f, 0, SEEK_SET );
		unsigned char *t = new unsigned char[2*sz];
		fread( t, sz, 1, f );
		fclose( f );

		// Transform into luminance/alpha
		for( unsigned n = 2*sz; n; ) {
			n -= 2;
			t[n] = 0xff;
			t[n+1] = t[n>>1];
		}


		glEnable( GL_TEXTURE_2D );
		glGenTextures( 1, &gl_font_tex );
		glBindTexture( GL_TEXTURE_2D, gl_font_tex );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR ); 
		glTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE ); 
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, font_bmp_height, font_bmp_width, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, t );
		glDisable( GL_TEXTURE_2D );

		delete[] t;
		font_initialized = true;
	}

	if( initGL ) {
		glDisable( GL_DEPTH_TEST );
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable( GL_ALPHA_TEST );
		glAlphaFunc( GL_GREATER, 0.01f );
		glDisable(GL_CULL_FACE);
		float white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, &white[0] );
		glMatrixMode( GL_PROJECTION );
		glLoadIdentity();
		glMatrixMode( GL_MODELVIEW );
		glLoadIdentity();
		glScalef( 2.0f, -2.0f, 1.0f );
		glTranslatef( -0.5f, -0.5f, 0.0f );
	}
}

UIDrawContext::~UIDrawContext() {
}

void UIDrawContext::pushTransform() {
	glPushMatrix();
}

void UIDrawContext::transformToRect( UIRect *r ) {
	glTranslatef( r->x, r->y, 0.0f );
	glScalef( r->w, r->h, 1.0f );
}

void UIDrawContext::rotate( float ) {
	assert( false );
}

void UIDrawContext::popTransform() {
	glPopMatrix();
}

// Clip rectangle management
//void UIDrawContext::pushClipRect( UIRect *r );
//void UIDrawContext::popClipRect();

// Colors
void UIDrawContext::color( unsigned color ) {
	colors[0] = color;
	colors[1] = color;
	colors[2] = color;
	colors[3] = color;
}

void UIDrawContext::color2( unsigned color1, unsigned color2 ) {
	colors[0] = color1;
	colors[1] = color2;
	colors[2] = color1;
	colors[3] = color2;
}

void UIDrawContext::color4( const unsigned *colors ) {
	this->colors[0] = colors[0];
	this->colors[1] = colors[1];
	this->colors[2] = colors[2];
	this->colors[3] = colors[3];
}

inline void setGLColor( unsigned c ) {
	glColor4ub( (c>>16)&0xff, (c>>8)&0xff, c&0xff, (c>>24)&0xff );
}

inline void makeGLVertex( float x, float y, unsigned c ) {
	setGLColor( c );
	glVertex3f( x, y, 0.0f );
}

inline void makeGLVertex( float x, float y, float s, float t ) {
	glTexCoord2f( s, t );
	glVertex3f( x, y, 0.0f );
}

// Draw stuff
void UIDrawContext::lineTo( float dx, float dy ) {
	glBegin( GL_LINES );
	makeGLVertex( x, y, colors[0] );
	makeGLVertex( dx, dy, colors[1] );
	glEnd();
	x = dx; y = dy;
}

void UIDrawContext::lineRectTo( float dx, float dy ) {
	glBegin( GL_LINE_LOOP );
	makeGLVertex( x, y, colors[0] );
	makeGLVertex( dx, y, colors[1] );
	makeGLVertex( dx, dy, colors[2] );
	makeGLVertex( x, dy, colors[3] );
	glEnd();
	x = dx; y = dy;
}

void UIDrawContext::filledRectTo( float dx, float dy ) {
	glBegin( GL_QUADS );
	makeGLVertex( x, y, colors[0] );
	makeGLVertex( dx, y, colors[1] );
	makeGLVertex( dx, dy, colors[2] );
	makeGLVertex( x, dy, colors[3] );
	glEnd();
	x = dx; y = dy;
}

void UIDrawContext::lineStrip( unsigned n, const float *xy ) {
	setGLColor( colors[0] );
	glEnableClientState( GL_VERTEX_ARRAY );
	glVertexPointer( 2, GL_FLOAT, 0, xy );
	glDrawArrays( GL_LINE_STRIP, 0, (int)n );
	glDisableClientState( GL_VERTEX_ARRAY );
}

// Text
void UIDrawContext::setFontV( float dx, float dy ) {
	textDownV[0] = dx;
	textDownV[1] = dy;
	float dxSq = dx*dx, dySq = dy*dy;
	textVLen = sqrtf( dxSq + dySq );
	float aspSq = 1;//g_aspect*g_aspect;
	textHLen = sqrtf( dxSq*aspSq + dySq/aspSq );
}

void UIDrawContext::setFontVectorsAA( float dx, float dy ) {
	textRightV[0] = dx;
	textRightV[1] = 0.0f;
	textHLen = dx;
	textDownV[0] = 0.0f;
	textDownV[1] = dy;
	textVLen = dy;
}

void UIDrawContext::setFontVectors( float *X, float *Y ) {
	textRightV[0] = X[0];
	textRightV[1] = X[1];
	textHLen = sqrtf( X[0]*X[0] + X[1]*X[1] );
	textDownV[0] = Y[0];
	textDownV[1] = Y[1];
	textVLen = sqrtf( Y[0]*Y[0] + Y[1]*Y[1] );
}

void UIDrawContext::transformPtToV( float &dx, float &dy ) {
	dx /= g_width;
	dy /= g_height;
}

void UIDrawContext::transformVToPt( float &dx, float &dy ) {
	dx *= g_width;
	dy *= g_height;
}

void UIDrawContext::layoutText( const char *text, UIDrawContext::TextLayout *layout ) {
	textify( text );
	if( layout ) {
		layout->lines = nTextLines;
		layout->totalHeight = textVLen * nTextLines;
		layout->minWidth = maxTextLineW;
	}
}

void UIDrawContext::drawText( UIDrawContext::TextAlignment align ) {
	glEnable( GL_TEXTURE_2D );
	glEnable (GL_BLEND);
	glBindTexture( GL_TEXTURE_2D, gl_font_tex );

	glBegin( GL_QUADS );
	setGLColor( colors[0] );

	//float textRightV[2];
	float temp;
	//textRightV[0] = textDownV[1];// / g_aspect;
	//textRightV[1] = -textDownV[0];// * g_aspect;

	switch( align & 0xf0 ) {
		case TOP: break;
		case V_CENTER:
			temp = (textH / textVLen - nTextLines) / 2.0f;
			x += textDownV[0] * temp;
			y += textDownV[1] * temp;
			break;
		case BOTTOM:
			temp = textH / textVLen - nTextLines;
			x += textDownV[0] * temp;
			y += textDownV[1] * temp;
			break;
	}

	for( unsigned i = 0; i < nTextLines; i++ ) {
		float xc = x, yc = y;

		// Align the line
		switch( align & 0xf ) {
			case LEFT: break;
			case CENTER:
				temp = (textW - textLineWidths[i]) / (2.0f * textHLen);
				xc += textRightV[0] * temp;
				yc += textRightV[1] * temp;
				break;
			case RIGHT:
				temp = (textW - textLineWidths[i]) / textHLen;
				xc += textRightV[0] * temp;
				yc += textRightV[1] * temp;
				break;
		}


		const char *t = textLines[i], *endT = textLines[i+1];
		while( t != endT ) {
			char ch = *t;
			if( ch <= ' ' ) {
				// Control character
				switch( ch ) {
					case '\0': assert(false); break;
					case '\n': break;
					case ' ':
					case '\t':
						// Tab or space
						temp = ch==' ' ? SPACE_SIZE_RATIO : TAB_SIZE_RATIO;
						xc += textRightV[0] * temp;
						yc += textRightV[1] * temp;
						break;
					case '\x10':
						if( t[1] != '\0' && t[2] != '\0' && t[3] != '\0' ) {
							// Change the color
							glColor3f( (t[1] - '0') / 9.0f, (t[2] - '0') / 9.0f, (t[3] - '0') / 9.0f );
							t += 3;
							break;
						}
						// Fall-through if one of the next chars is \0 to prevent
						// buffer overrun
					default:
						// Unknown control code - replace with #
						ch = '#';
						goto draw_solid_char;
						break;
				}
			} else {
draw_solid_char:
				// Solid character
				Glyph *g = glyphs + (ch-' ');
				makeGLVertex( xc+textDownV[0], yc+textDownV[1], (g->y + g->h) / (float)font_bmp_height, (g->x)      / (float)font_bmp_width );
				makeGLVertex( xc             , yc             , (g->y)        / (float)font_bmp_height, (g->x)      / (float)font_bmp_width );
				temp = g->w / (float)font_char_height;
				xc += textRightV[0] * temp;
				yc += textRightV[1] * temp;
				makeGLVertex( xc             , yc             , (g->y)        / (float)font_bmp_height, (g->x+g->w) / (float)font_bmp_width );
				makeGLVertex( xc+textDownV[0], yc+textDownV[1], (g->y + g->h) / (float)font_bmp_height, (g->x+g->w) / (float)font_bmp_width );
				
			}
			t++;
		}

		x += textDownV[0];
		y += textDownV[1];
	}

	glEnd();
	glDisable( GL_TEXTURE_2D );
	glDisable (GL_BLEND);
}

// Cursor text
void UIDrawContext::setAltText( const char *text ) {
	alt = text;
}

void UIDrawContext::textify( const char *text ) {
	nTextLines = 0;
	textLines[0] = text;
	maxTextLineW = 0.0f;
	const char *t = text;

	float w = 0.0f;
	float wordWidth = 0.0f;
	float spaceWidth = 0.0f;
	const char *wordStart = t;

	while( true ) {
		char ch = *t;
		if( ch <= ' ' ) {
			// Control character
			if( !(ch & 0x10) && wordWidth > 0.0f ) {
				// End the last word
				if( w > 0.0f && w + spaceWidth + wordWidth > textW ) {
					// Need to insert a linebreak
					newLine( wordStart, w );
					w = wordWidth;
				} else {
					w += wordWidth + spaceWidth;
				}
				wordWidth = 0.0f;
				spaceWidth = 0.0f;
			}
			switch( ch ) {
				case '\0': // End of string
					newLine( t, w );
					return;
				case '\n': // Newline
					spaceWidth = 0.0f;
					newLine( ++t, w );
					w = 0.0f;
					break;
				case ' ':
				case '\t':
					// Tab or space
					spaceWidth += textHLen * (*t==' ' ? SPACE_SIZE_RATIO : TAB_SIZE_RATIO);
					wordStart = ++t;
					break;
				case '\x10':
					// Color - ignore the next 3 chars
					if( t[1] != '\0' && t[2] != '\0' && t[3] != '\0' ) {
						t += 4;
						break;
					}
					// Fall-through if one of the next chars is \0 to prevent
					// buffer overrun
				default:
					// Unknown control code - replace with #
					ch = '#';
					goto solid_char;
					break;
			}
		} else {
solid_char:
			// Solid character
			wordWidth += glyphs[ch-' '].w * textHLen / font_char_height;
			++t;
		}
	}
}

void UIDrawContext::newLine( const char *txtStart, float lastW ) {
	if( lastW > maxTextLineW )
		maxTextLineW = lastW;
	textLineWidths[nTextLines] = lastW;
	nTextLines++;
	textLines[nTextLines] = txtStart;
}
