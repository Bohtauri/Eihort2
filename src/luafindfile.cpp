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


#include <lua.hpp>
#include <new>

#include "findfile.h"
#include "luafindfile.h"

#define LUAFINDFILE_META "LuaFindFile"

int luaFindFileNext( lua_State *L ) {
	FindFile *ff = (FindFile*)luaL_checkudata( L, 1, LUAFINDFILE_META );
	lua_pushboolean( L, !!ff->next() );
	return 1;
}

int luaFindFileFilename( lua_State *L ) {
	FindFile *ff = (FindFile*)luaL_checkudata( L, 1, LUAFINDFILE_META );
	lua_pushstring( L, ff->filename() );
	lua_pushboolean( L, ff->isdir() );
	return 2;
}

int luaFindFileClose( lua_State *L ) {
	FindFile *ff = (FindFile*)luaL_checkudata( L, 1, LUAFINDFILE_META );
	ff->close();
	return 0;
}

int luaFindFileCreate( lua_State *L ) {
	const char *path = luaL_checkstring( L, 1 );
	FindFile *ff = new( lua_newuserdata( L, sizeof(FindFile) ) ) FindFile( path );
	if( !ff->filename() ) {
		ff->close();
		lua_pushboolean( L, false );
	} else {
		luaL_newmetatable( L, LUAFINDFILE_META );
		lua_setmetatable( L, -2 );
	}
	return 1;
}

int luaFindFileDestroy( lua_State *L ) {
	FindFile *ff = (FindFile*)luaL_checkudata( L, 1, LUAFINDFILE_META );
	ff->~FindFile();
	return 0;
}

static const luaL_Reg LuaFindFile_functions[] = {
	{ "next", &luaFindFileNext },
	{ "filename", &luaFindFileFilename },
	{ "close", &luaFindFileClose },
	{ "__gc", &luaFindFileDestroy },
	{ NULL, NULL }
};

void FindFile_setupLua( lua_State *L ) {
	luaL_newmetatable( L, LUAFINDFILE_META );
	lua_pushvalue( L, -1 );
	lua_setfield( L, -2, "__index" );
	luaL_register( L, NULL, &LuaFindFile_functions[0] );
	lua_pop( L, 1 );

	lua_pushcfunction( L, &luaFindFileCreate );
	lua_setfield( L, -2, "findFile" );
}
