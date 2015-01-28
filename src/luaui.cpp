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


#include <new>
#include <lua.hpp>
#include <algorithm>

#include "uitheme.h"
#include "luaui.h"

LuaUIRect::LuaUIRect() {
	rect.set( 0.f, 0.f, 1.f, 1.f );
	setBorder( false, BUTTON_COL );
	setBackground( false, BUTTON_COL );
}

LuaUIRect::~LuaUIRect() {
}

void LuaUIRect::draw( UIDrawContext *ctx ) {
	// Background
	if( drawBkg ) {
		ctx->moveTo( rect.x, rect.y );
		ctx->color4( bkgColors );
		ctx->filledRectTo( rect.x+rect.w, rect.y+rect.h );
	}

	// Border
	if( drawBorder ) {
		ctx->moveTo( rect.x, rect.y );
		ctx->color4( borderColors );
		ctx->lineRectTo( rect.x+rect.w, rect.y+rect.h );
	}
}

void LuaUIRect::drawTextIn( UIDrawContext *ctx, const char *text, float vScale, float hScale, UIDrawContext::TextAlignment align, float vBorder, float hBorder ) {
	//ctx->color( fontColor );
	ctx->white();
	ctx->moveTo( rect.x + hBorder, rect.y + vBorder );
	ctx->setFontVectorsAA( hScale, vScale );
	ctx->setWidth( rect.w - 2 * hBorder );
	ctx->setHeight( rect.h - 2 * vBorder );
	ctx->layoutText( text );
	ctx->drawText( align );
}

void LuaUIRect::setRect( const UIRect *r ) {
	rect = *r;
}

void LuaUIRect::setBackground( bool on, const unsigned *bkgCols ) {
	drawBkg = on;
	if( on ) {
		bkgColors[0] = bkgCols[0];
		bkgColors[1] = bkgCols[1];
		bkgColors[2] = bkgCols[2];
		bkgColors[3] = bkgCols[3];
	}
}

void LuaUIRect::setBorder( bool on, const unsigned *borderCols ) {
	drawBorder = on;
	if( on ) {
		borderColors[0] = borderCols[0];
		borderColors[1] = borderCols[1];
		borderColors[2] = borderCols[2];
		borderColors[3] = borderCols[3];
	}
}

int LuaUIRect::lua_draw( lua_State *L ) {
	LuaUIRect *rect = (LuaUIRect*)luaL_checkudata( L, 1, LUAUIRECT_META );
	UIDrawContext *ctx = (UIDrawContext*)luaL_checkudata( L, 2, LUAUIDRAWCONTEXT_META );
	rect->draw( ctx );
	return 0;
}

int LuaUIRect::lua_drawTextIn( lua_State *L ) {
	LuaUIRect *rect = (LuaUIRect*)luaL_checkudata( L, 1, LUAUIRECT_META );
	UIDrawContext *ctx = (UIDrawContext*)luaL_checkudata( L, 2, LUAUIDRAWCONTEXT_META );
	const char *text = luaL_checkstring( L, 3 );
	float vScale = (float)luaL_checknumber( L, 4 );
	float hScale = (float)luaL_checknumber( L, 5 );
	UIDrawContext::TextAlignment align = (UIDrawContext::TextAlignment)(unsigned)luaL_optnumber( L, 6, 0.0 );
	float vBorder = (float)luaL_optnumber( L, 7, 0.0 );
	float hBorder = (float)luaL_optnumber( L, 8, 0.0 );
	rect->drawTextIn( ctx, text, vScale, hScale, align, vBorder, hBorder );
	return 0;
}

int LuaUIRect::lua_contains( lua_State *L ) {
	LuaUIRect *rect = (LuaUIRect*)luaL_checkudata( L, 1, LUAUIRECT_META );
	float x = (float)luaL_checknumber( L, 2 );
	float y = (float)luaL_checknumber( L, 3 );

	lua_pushboolean( L, rect->getRect()->isInside( x, y ) );
	return 1;
}

int LuaUIRect::lua_getRect( lua_State *L ) {
	LuaUIRect *rect = (LuaUIRect*)luaL_checkudata( L, 1, LUAUIRECT_META );
	const UIRect *r = rect->getRect();
	lua_pushnumber( L, r->x );
	lua_pushnumber( L, r->y );
	lua_pushnumber( L, r->w );
	lua_pushnumber( L, r->h );
	return 4;
}

int LuaUIRect::lua_setRect( lua_State *L ) {
	LuaUIRect *rect = (LuaUIRect*)luaL_checkudata( L, 1, LUAUIRECT_META );
	UIRect r;
	r.set( (float)luaL_checknumber( L, 2 ), (float)luaL_checknumber( L, 3 ),
		   (float)luaL_checknumber( L, 4 ), (float)luaL_checknumber( L, 5 ) );
	rect->setRect( &r );
	return 0;
}

static unsigned luaRGBToUint( lua_State *L, int baseArg ) {
	return 0xff000000u
		| ((unsigned)(std::max(std::min(luaL_checknumber(L,baseArg), 1.0), 0.0)*255.0) << 16)
		| ((unsigned)(std::max(std::min(luaL_checknumber(L,baseArg+1), 1.0), 0.0)*255.0) << 8)
		| ((unsigned)(std::max(std::min(luaL_checknumber(L,baseArg+2), 1.0), 0.0)*255.0));
}

int LuaUIRect::lua_setBorder( lua_State *L ) {
	LuaUIRect *rect = (LuaUIRect*)luaL_checkudata( L, 1, LUAUIRECT_META );
	bool on = !!lua_toboolean( L, 2 );
	if( on ) {
		unsigned colors[4];
		colors[0] = luaRGBToUint( L, 3 );
		if( !lua_isnone( L, 6 ) ) {
			colors[1] = luaRGBToUint( L, 6 );
			if( !lua_isnone( L, 9 ) ) {
				colors[2] = luaRGBToUint( L, 9 );
				colors[3] = luaRGBToUint( L, 12 );
			} else {
				colors[2] = colors[0];
				colors[3] = colors[1];
			}
		} else {
			colors[1] = colors[2] = colors[3] = colors[0];
		}
		rect->setBorder( true, &colors[0] );
	} else {
		rect->setBorder( false, NULL );
	}
	return 0;
}

int LuaUIRect::lua_setBackground( lua_State *L ) {
	LuaUIRect *rect = (LuaUIRect*)luaL_checkudata( L, 1, LUAUIRECT_META );
	bool on = !!lua_toboolean( L, 2 );
	if( on ) {
		unsigned colors[4];
		colors[0] = luaRGBToUint( L, 3 );
		if( !lua_isnoneornil( L, 6 ) ) {
			colors[1] = luaRGBToUint( L, 6 );
			if( !lua_isnoneornil( L, 9 ) ) {
				colors[2] = luaRGBToUint( L, 9 );
				colors[3] = luaRGBToUint( L, 12 );
			} else {
				colors[2] = colors[0];
				colors[3] = colors[1];
			}
		} else {
			colors[1] = colors[2] = colors[3] = colors[0];
		}
		rect->setBackground( true, &colors[0] );
	} else {
		rect->setBackground( false, NULL );
	}
	return 0;
}

int LuaUIRect::lua_destroy( lua_State *L ) {
	LuaUIRect *rect = (LuaUIRect*)luaL_checkudata( L, 1, LUAUIRECT_META );
	rect->~LuaUIRect();
	return 0;
}

int LuaUIRect::lua_create( lua_State *L ) {
	LuaUIRect *rect = new( lua_newuserdata( L, sizeof(LuaUIRect) ) ) LuaUIRect;
	luaL_newmetatable( L, LUAUIRECT_META );
	lua_setmetatable( L, -2 );

	if( !lua_isnoneornil( L, 1 ) ) {
		UIRect r;
		r.set( (float)luaL_checknumber( L, 1 ), (float)luaL_checknumber( L, 2 ),
			   (float)luaL_checknumber( L, 3 ), (float)luaL_checknumber( L, 4 ) );
		rect->setRect( &r );
	}
	return 1;
}

static const luaL_Reg LuaUIRect_functions[] = {
	{ "draw", &LuaUIRect::lua_draw },
	{ "drawTextIn", &LuaUIRect::lua_drawTextIn },
	{ "contains", &LuaUIRect::lua_contains },
	{ "getRect", &LuaUIRect::lua_getRect },
	{ "setRect", &LuaUIRect::lua_setRect },
	{ "setBorder", &LuaUIRect::lua_setBorder },
	{ "setBkg", &LuaUIRect::lua_setBackground },
	{ "__gc", &LuaUIRect::lua_destroy },
	{ NULL, NULL }
};

static int luaCtxCreate( lua_State *L ) {
	new( lua_newuserdata( L, sizeof(UIDrawContext) ) ) UIDrawContext;
	luaL_newmetatable( L, LUAUIDRAWCONTEXT_META );
	lua_setmetatable( L, -2 );
	return 1;
}

static int luaCtxTextSize( lua_State *L ) {
	UIDrawContext *ctx = (UIDrawContext*)luaL_checkudata( L, 1, LUAUIDRAWCONTEXT_META );
	const char *text = luaL_checkstring( L, 2 );
	float maxWidth = (float)luaL_checknumber( L, 3 );

	ctx->setWidth( maxWidth );
	ctx->setFontVectorsAA( 1.0f, 1.0f );
	UIDrawContext::TextLayout ly;
	ctx->layoutText( text, &ly );

	lua_pushnumber( L, ly.minWidth );
	lua_pushnumber( L, ly.totalHeight );
	return 2;
}

static int luaCtxDestroy( lua_State *L ) {
	UIDrawContext *ctx = (UIDrawContext*)luaL_checkudata( L, 1, LUAUIDRAWCONTEXT_META );
	ctx->~UIDrawContext();
	return 0;
}

static const luaL_Reg LuaUIDrawContext_functions[] = {
	{ "textSize", &luaCtxTextSize },
	{ "destroy", &luaCtxDestroy },
	{ NULL, NULL }
};

void LuaUIRect::setupLua( lua_State *L ) {
	luaL_newmetatable( L, LUAUIRECT_META );
	lua_pushvalue( L, -1 );
	lua_setfield( L, -2, "__index" );
	luaL_register( L, NULL, &LuaUIRect_functions[0] );
	lua_pop( L, 1 );

	lua_pushcfunction( L, &LuaUIRect::lua_create );
	lua_setfield( L, -2, "newUIRect" );

	luaL_newmetatable( L, LUAUIDRAWCONTEXT_META );
	lua_pushvalue( L, -1 );
	lua_setfield( L, -2, "__index" );
	luaL_register( L, NULL, &LuaUIDrawContext_functions[0] );
	lua_pop( L, 1 );

	lua_pushcfunction( L, &luaCtxCreate );
	lua_setfield( L, -2, "newUIContext" );
	lua_pushnumber( L, UIDrawContext::LEFT );
	lua_setfield( L, -2, "TextAlignLeft" );
	lua_pushnumber( L, UIDrawContext::RIGHT );
	lua_setfield( L, -2, "TextAlignRight" );
	lua_pushnumber( L, UIDrawContext::CENTER );
	lua_setfield( L, -2, "TextAlignCenter" );
	lua_pushnumber( L, UIDrawContext::TOP );
	lua_setfield( L, -2, "TextAlignTop" );
	lua_pushnumber( L, UIDrawContext::BOTTOM );
	lua_setfield( L, -2, "TextAlignBottom" );
	lua_pushnumber( L, UIDrawContext::V_CENTER );
	lua_setfield( L, -2, "TextAlignVCenter" );
}

