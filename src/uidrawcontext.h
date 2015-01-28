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

#ifndef UIDRAWCONTEXT_H
#define UIDRAWCONTEXT_H

#include <string>

struct UIRect {
	float x, y, w, h;

	inline UIRect()
		: x(0.0f), y(0.0f), w(0.0f), h(0.0f) { }
	inline UIRect( float x, float y, float w, float h )
		: x(x), y(y), w(w), h(h) { }
	inline UIRect( const UIRect &other )
		: x(other.x), y(other.y), w(other.w), h(other.h) { }

	inline bool isInside( float px, float py ) const {
		return px >= x && px <= x+w && py >= y && py <= y+h;
	}

	inline void set( float rx, float ry, float rw, float rh ) {
		x = rx; y = ry;
		w = rw; h = rh;
	}
	inline void flipY() {
		y = y + h;
		h = -h;
	}
	inline void flipX() {
		x = x + w;
		w = -w;
	}
};

#define CC "\x10"

class UIDrawContext {
public:
	UIDrawContext( bool initGL = true );
	~UIDrawContext();

	// Transforms
	void pushTransform();
	void transformToRect( UIRect *r );
	void rotate( float rad );
	void popTransform();

	// Clip rectangle management
	void pushClipRect( UIRect *r );
	void popClipRect();

	// Move the draw cursor
	inline void moveTo( float x, float y ) { this->x = x; this->y = y; }
	
	// Colors
	void color( unsigned color );
	void color2( unsigned color1, unsigned color2 );
	void color4( const unsigned *colors );
	inline void white() { color( 0xffffffffu ); }

	// Draw stuff
	void lineTo( float x, float y );
	void lineRectTo( float x, float y );
	void filledRectTo( float x, float y );
	void lineStrip( unsigned n, const float *xy );

	// Text
	enum TextAlignment {
		LEFT = 1,
		RIGHT = 2,
		CENTER = 3,
		TOP = 0,
		BOTTOM = 0x10,
		V_CENTER = 0x20,
		ALL_CENTER = 0x23
	};
	inline void setFontSizePt( float sz ) {
		float x = 0.0f;
		transformPtToV( x, sz );
		setFontV( x, sz );
	}
	inline void setFontSize( float sz ) { setFontV( 0.0f, sz ); }
	inline void setFontVPt( float dx, float dy ) {
		transformPtToV( dx, dy );
		setFontV( dx, dy );
	}
	void setFontV( float dx, float dy );
	void setFontVectorsAA( float dx, float dy );
	void setFontVectors( float *X, float *Y );
	void transformPtToV( float &dx, float &dy );
	void transformVToPt( float &dx, float &dy );
	void transformVToGlobal( float &x, float &y );
	inline void setWidth( float w ) { textW = w; }
	inline void setHeight( float h ) { textH = h; }
	struct TextLayout {
		unsigned lines;
		float totalHeight;
		float minWidth;
	};
	void layoutText( const char *text, TextLayout *layout = NULL );
	void drawText( TextAlignment align = LEFT );

	// Cursor text
	void setAltText( const char *text );

private:
	float x,y;
	unsigned colors[4];

	void textify( const char *text );
	void newLine( const char *txtStart, float lastW );
	bool actuallyDrawText;
	float textW, textH;
	float textDownV[2], textRightV[2], textVLen, textHLen;
	const char *textLines[64];
	float textLineWidths[64];
	float maxTextLineW;
	unsigned nTextLines;

	std::string alt;
};

#endif
