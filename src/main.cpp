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



#include <GL/glew.h>
#include <SDL.h>
#include <SDL_keyboard.h>
#include <SDL_mouse.h>
#include <stdlib.h>
#include <sstream>
#include <climits>
#include <ctime>

#include <lua.hpp>

#include "nbt.h"
#include "jmath.h"
#include "mcregionmap.h"
#include "mcworldmesh.h"
#include "worker.h"
#include "worldqtree.h"
#include "lightmodel.h"
#include "eihortshader.h"
#include "sky.h"
#include "mcbiome.h"
#include "platform.h"
#include "luaui.h"
#include "luafindfile.h"
#include "luanbt.h"
#include "luaimage.h"
#include "unzip.h"

#if defined(__APPLE__) && defined(__MACH__)
# include <CoreFoundation/CoreFoundation.h>
# include <OpenGL/OpenGL.h>
#endif

#if defined(__linux)
#	include <GL/glx.h>
#	include <X11/Xlib.h>
#endif

#if defined(_POSIX_VERSION)
#	include <sys/stat.h> // for mkdir()
#endif

#define VERSION "0.3.14"

// ---------------------------------------------------------------------------
lua_State *g_L;
unsigned g_width, g_height;
SDL_Window *g_window;
SDL_GLContext g_glContext;

bool g_needRefresh = true;
bool g_keepUpdated = false;

Worker *g_workers[MAX_WORKERS];
unsigned g_nWorkers;
EihortShader *g_shader;

char g_programRoot[MAX_PATH];
const char *g_worldRoot;

extern const luaL_Reg BaseEihort_functions[];

// ---------------------------------------------------------------------------
// Event handlers
void initLua();
void initLowLevel(const char *progname);
int runLuaMain( int argc, const char **argv );

// Helpers
static const char *SDLKeyToString( SDL_Keycode key );
static const char *GLErrorToString( GLenum err );

bool errorDlg( const char *context, const char *error, bool yesno = false ) {
#ifdef _WINDOWS
	return IDYES == MessageBoxA( GetActiveWindow(), error, context, (yesno ? MB_YESNO : MB_OK) | MB_ICONERROR );
#elif defined(__APPLE__) && defined(__MACH__)
  CFStringRef alertHeader = CFStringCreateWithCString(NULL, context, CFStringGetSystemEncoding());
  CFStringRef alertMessage = CFStringCreateWithCString(NULL, error, CFStringGetSystemEncoding());
  CFOptionFlags result;
	if (!yesno)
		CFUserNotificationDisplayAlert(0, kCFUserNotificationStopAlertLevel, NULL, NULL, NULL, alertHeader, alertMessage, NULL, NULL, NULL, &result);
	else
		CFUserNotificationDisplayAlert(0, kCFUserNotificationStopAlertLevel, NULL, NULL, NULL, alertHeader, alertMessage, CFSTR("Yes"), CFSTR("No"), NULL, &result);
  CFRelease(alertHeader);
  CFRelease(alertMessage);
	return result == kCFUserNotificationDefaultResponse;
#else
	(void)context, (void)yesno;
	std::cerr << error << '\n';
	return false;
#endif
}

void onError( const char *context, const char *error ) {
	errorDlg( context, error );
	exit (1);
}

void errorOut( const char *error ) {
	onError( "Error", error );
}

int luaErrorDlgYesNo( lua_State *L ) {
	lua_pushboolean( L, errorDlg( luaL_checkstring( L, 1 ), luaL_checkstring( L, 2 ), true ) );
	return 1;
}

int luaErrorDlg( lua_State *L ) {
	errorDlg( luaL_checkstring( L, 1 ), luaL_checkstring( L, 2 ), false );
	return 0;
}

// ---------------------------------------------------------------------------
int main( int argc, char **argv ) {
	initLowLevel(*argv);
	initLua();

	return runLuaMain( argc, (const char**)argv );
}

#if defined(_MSC_VER) && (defined(_M_IX86_FP) && _M_IX86_FP != 0)
#include <intrin.h>
inline static
void
check_sse(void)
{
	int info[4];
	// NB. CPUID is available since Pentium. While it is possible to 
		// to also test this, we won't bother.
	__cpuid(info, 1);
#	if _M_IX86_FP == 1
	if ((info[3] & (1 << 25)) == 0)
		onError("No SSE", "Compiled with /arch:SSE, but no SSE\n");
#	elif _M_IX86_FP == 2
	if ((info[3] & (1 << 26)) == 0)
		onError("No SSE2", "Compiled with /arch:SSE2, but no SSE2\n");
#	endif
}
#else // _MSC_VER && _M_IX86_FP != 0
inline static
void
check_sse(void)
{}
#endif

// ---------------------------------------------------------------------------
void initLua() {
	lua_State *L = g_L = luaL_newstate();
	luaL_openlibs( L );

	lua_newtable( L );

	char path[MAX_PATH];
	snprintf( path, MAX_PATH, ROOT_FOLDER_FORMAT, g_programRoot );
	lua_pushstring( L, path );
	lua_setfield( L, -2, "ProgramPath" );
	snprintf( path, MAX_PATH, MINECRAFT_PATH_FORMAT, gethomedir() );
	lua_pushstring( L, path );
	lua_setfield( L, -2, "MinecraftPath" );
	lua_pushstring( L, VERSION );
	lua_setfield( L, -2, "Version" );

	luaL_register( L, NULL, BaseEihort_functions );

	MCRegionMap::setupLua( L );
	WorldQTree::setupLua( L );
	LightModel::setupLua( L );
	Sky::setupLua( L );
	FindFile_setupLua( L );
	LuaUIRect::setupLua( L );
	LuaNBT_setupLua( L );
	LuaImage_setupLua( L );
	mcgeom::BlockGeometry::setupLua( L );
	MCBlockDesc::setupLua( L );

	lua_setglobal( L, "eihort" );
}

// ---------------------------------------------------------------------------
int runLuaMain( int argc, const char **argv ) {
	// Run eihort.lua
	char filename[MAX_PATH];
	snprintf( filename, MAX_PATH, ROOT_FOLDER_FORMAT "eihort.lua" , g_programRoot );
	int ret = -1;
	if( 0 == luaL_loadfile( g_L, filename ) ) {
		for( int i = 1; i < argc; i++ )
			lua_pushstring( g_L, argv[i] );
		
		if( 0 != lua_pcall( g_L, argc - 1, 1, 0 ) ) {
			errorDlg( "Fatal Error", lua_tostring( g_L, -1 ) );
			lua_pop( g_L, 1 );
		} else {
			ret = (int)luaL_optnumber( g_L, -1, 0 );
			lua_pop( g_L, 1 );
		}
	} else {
		errorDlg( "Fatal Error", lua_tostring( g_L, -1 ) );
		lua_pop( g_L, 1 );
	}
	return ret;
}

// ---------------------------------------------------------------------------
inline static
void
resizeContext()
{
	// NB. SDL 1.2 is broken, hopefully the newer versions will be sane
		// calling SDL_SetVideoMode() (as suggested by the documentation)
		// is not the right thing, since it recreates (!) the GL context;
		// all we need to do is update its size

#if defined(__APPLE__) && defined(__MACH__)
		// CoreGL
	GLint size[2] = { g_width, g_height };
	CGLSetParameter(CGLGetCurrentContext(), kCGLCPSurfaceBackingSize, size);
#elif defined(__linux)
		// GLX
	XResizeWindow(glXGetCurrentDisplay(), glXGetCurrentDrawable(), g_width, g_height);
#endif
}

// ---------------------------------------------------------------------------
static int luaPollEvent( lua_State *L ) {
	SDL_Event e;
	while( SDL_PollEvent( &e ) ) {
		switch( e.type ) {
		case SDL_WINDOWEVENT:
			switch( e.window.type ) {
			case SDL_WINDOWEVENT_EXPOSED:
				g_needRefresh = true;
				lua_pushstring( L, "expose" );
				return 1;
			case SDL_WINDOWEVENT_RESIZED:
				g_width = e.window.data1;
				g_height = e.window.data2;
				g_needRefresh = true;
				resizeContext();
				lua_pushstring( L, "resize" );
				lua_pushnumber( L, g_width );
				lua_pushnumber( L, g_height );
				return 3;
			case SDL_WINDOWEVENT_ENTER:
			case SDL_WINDOWEVENT_LEAVE:
				lua_pushstring( L, "active" );
				lua_pushstring( L, "mouse" );
				lua_pushboolean( L, e.window.type == SDL_WINDOWEVENT_ENTER );
				return 3;
			case SDL_WINDOWEVENT_FOCUS_GAINED:
			case SDL_WINDOWEVENT_FOCUS_LOST:
				lua_pushstring( L, "active" );
				lua_pushstring( L, "input" );
				lua_pushboolean( L, e.window.type == SDL_WINDOWEVENT_FOCUS_GAINED );
				return 3;
			case SDL_WINDOWEVENT_CLOSE:
				lua_pushstring( L, "quit" );
				return 1;
			}
			break;
		case SDL_KEYDOWN:
			if( !e.key.repeat ) {
				lua_pushstring( L, "keydown" );
				lua_pushstring( L, SDLKeyToString( e.key.keysym.sym ) );
				return 2;
			}
			break;
		case SDL_KEYUP:
			lua_pushstring( L, "keyup" );
			lua_pushstring( L, SDLKeyToString( e.key.keysym.sym ) );
			return 2;
		case SDL_MOUSEMOTION:
			lua_pushstring( L, "mousemove" );
			lua_pushnumber( L, e.motion.x );
			lua_pushnumber( L, e.motion.y );
			return 3;
		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEBUTTONDOWN:
			switch (e.button.button) {
				case 2: e.button.button = 3; break;
				case 3: e.button.button = 2; break;
			}
			lua_pushstring( L, e.type == SDL_MOUSEBUTTONUP ? "mouseup" : "mousedown" );
			lua_pushnumber( L, e.button.button );
			lua_pushnumber( L, e.button.x );
			lua_pushnumber( L, e.button.y );
			return 4;
		case SDL_QUIT:
			lua_pushstring( L, "quit" );
			return 1;
		}
	}

	return 0;
}

// ---------------------------------------------------------------------------
void initLowLevel(const char *progname) {
	g_nWorkers = 1;

#ifdef _WINDOWS
	(void)progname;

	// Check SSE
	check_sse();

	// Get program path
	GetModuleFileNameA(0, g_programRoot, sizeof(g_programRoot) - 1);
	char *pch = &g_programRoot[0], *lastSlash = NULL;
	while( *pch ) {
		switch( *pch ) {
		case '\\':
			*pch = '/';
		case '/':
			lastSlash = pch;
			break;
		}
		pch++;
	}
	*lastSlash = '\0';
#else
	strcpy( g_programRoot, progname );
	char *pch = &g_programRoot[0], *lastSlash = NULL;
	while( *pch ) {
		if( *pch == '/' )
			lastSlash = pch;
		pch++;
	}
	*lastSlash = '\0';
#endif

	for( unsigned i = 0; i < g_nWorkers; i++ )
		g_workers[i] = new Worker;

	// Initialize SDL
	if ( SDL_Init( SDL_INIT_VIDEO ) < 0 )
		onError( "SDL Initialization Error", SDL_GetError() );
}

static int luaGetProcessorCount( lua_State *L ) {
	// Get number of cores for workers
#ifdef _WINDOWS
#ifdef NDEBUG
	SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    sysinfo.dwNumberOfProcessors;
	lua_pushnumber( L, sysinfo.dwNumberOfProcessors );
#else
	lua_pushnumber( L, 1 );
#endif
#else
# if defined(NDEBUG) && defined(_POSIX_VERSION) && defined(_SC_NPROCESSORS_ONLN)
    // This works on most unices; however, it's not POSIX
	lua_pushnumber( L, sysconf(_SC_NPROCESSORS_ONLN) );
# elif defined(NDEBUG) && defined(_POSIX_VERSION) && defined(_SC_NPROC_ONLN)
	lua_pushnumber( L, sysconf(_SC_NPROC_ONLN) );
#else
	lua_pushnumber( L, 1 );
#endif
#endif
	return 1;
}

static int luaInitWorkers( lua_State *L ) {
	unsigned oldWorkerCount = g_nWorkers;
	int newWorkerCount = (int)luaL_checknumber( L, 1 );
	if( newWorkerCount < 0 || (unsigned)newWorkerCount < oldWorkerCount )
		return 0;
	newWorkerCount = std::min( newWorkerCount, MAX_WORKERS );
	for( unsigned i = oldWorkerCount; i < (unsigned)newWorkerCount; i++ )
		g_workers[i] = new Worker;
	g_nWorkers = newWorkerCount;
	return 0;
}

static int luaInitializeVideo( lua_State *L ) {
	int width = (int)luaL_checknumber( L, 1 );
	luaL_argcheck( L, width > 20, 1, "Width too small" );
	int height = (int)luaL_checknumber( L, 2 );
	luaL_argcheck( L, height > 20, 2, "Height too small" );
	bool fullscreen = !!lua_toboolean( L, 3 );
	int msaa = (int)luaL_checknumber( L, 4 );
	luaL_argcheck( L, msaa >= 0, 4, "MSAA too small" );

	g_width = width;
	g_height = height;

	//SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 2 );
	//SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
	//SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );

	Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
	if( fullscreen )
		flags |= SDL_WINDOW_FULLSCREEN;

	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 0 );
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
	SDL_GL_SetAttribute( SDL_GL_ACCUM_RED_SIZE, 0 );
	SDL_GL_SetAttribute( SDL_GL_ACCUM_GREEN_SIZE, 0 );
	SDL_GL_SetAttribute( SDL_GL_ACCUM_BLUE_SIZE, 0 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
#ifndef __linux
	if( msaa > 0 ) {
		// Linux builds will ship w/o anti-aliasing until we figure this out
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, msaa );
	}
#endif
	
	// Create the window
	g_window = SDL_CreateWindow( "Eihort v" VERSION,
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		width = 1024, height = 768, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL );

	if( !g_window ) {
		lua_pushboolean( L, false );
		lua_pushstring( L, SDL_GetError() );
		return 2;
	}

	// Initialize the GL context
	g_glContext = SDL_GL_CreateContext( g_window );
	if( !g_glContext ) {
		lua_pushboolean( L, false );
		lua_pushstring( L, SDL_GetError() );
		return 2;
	}

	// Initialize GLEW
	GLenum err = glewInit();
	if (GLEW_OK != err) {
		lua_pushboolean( L, false );
		lua_pushstring( L, (const char*)glewGetErrorString(err) );
		return 2;
	}

	// Did we get GL 2.0?
	if( !GLEW_VERSION_2_0 ) {
		lua_pushboolean( L, false );
		lua_pushstring( L,  "Eihort requires OpenGL version 2.0 or newer to function." );
		return 2;
	}

	// GL setup
	SDL_GL_SetSwapInterval( 1 );
	glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	glClearDepth( 1.0 );

	// Load shaders
	g_shader = new EihortShader;

	lua_pushboolean( L, true );
	return 1;
}

// ---------------------------------------------------------------------------
static int luaGetWindowDims( lua_State *L ) {
	int w, h;
	SDL_GL_GetDrawableSize( g_window, &w, &h );
	lua_pushnumber( L, w );
	lua_pushnumber( L, h );
	return 2;
}

// ---------------------------------------------------------------------------
static int luaSetWindowCaption( lua_State *L ) {
	SDL_SetWindowTitle( g_window, luaL_checklstring( L, 1, NULL ) );
	return 0;
}

// ---------------------------------------------------------------------------
static int luaShowCursor( lua_State *L ) {
	SDL_ShowCursor( lua_toboolean( L, 1 ) ? 1 : 0 );
	return 0;
}

// ---------------------------------------------------------------------------
static int luaGetMousePos( lua_State *L ) {
	int x, y;
	SDL_GetMouseState( &x, &y );
	lua_pushnumber( L, x );
	lua_pushnumber( L, y );
	return 2;
}

// ---------------------------------------------------------------------------
static int luaWarpMouse( lua_State *L ) {
	SDL_WarpMouseInWindow( g_window, (Uint16)luaL_checknumber( L, 1 ), (Uint16)luaL_checknumber( L, 2 ) );
	return 0;
}

// ---------------------------------------------------------------------------
static int luaGetVideoMem( lua_State *L ) {
	if( GLEW_NVX_gpu_memory_info ) {
		GLint mem;
		glGetIntegerv( GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &mem );
		lua_pushnumber( L, mem * 1024.0 );
		return 1;
	} else if( GLEW_ATI_meminfo ) {
		GLint mem[4];
		glGetIntegerv( GL_TEXTURE_FREE_MEMORY_ATI, mem );
		lua_pushnumber( L, mem[0] * 1024.0 );
		return 1;
	}
	return 0;
}

// ---------------------------------------------------------------------------
static int luaYield( lua_State* ) {
	SDL_Delay( 1 );
	return 0;
}

// ---------------------------------------------------------------------------
static int luaShouldRedraw( lua_State *L ) {
	bool newVal = !!lua_toboolean( L, 1 );
	lua_pushboolean( L, g_needRefresh );
	g_needRefresh = newVal;
	return 1;
}

// ---------------------------------------------------------------------------
static int luaIntAnd( lua_State *L ) {
	unsigned i1 = (unsigned)luaL_checknumber( L, 1 );
	unsigned i2 = (unsigned)luaL_checknumber( L, 2 );
	lua_pushnumber( L, i1 & i2 );
	return 1;
}

// ---------------------------------------------------------------------------
static int luaIntOr( lua_State *L ) {
	unsigned i1 = (unsigned)luaL_checknumber( L, 1 );
	unsigned i2 = (unsigned)luaL_checknumber( L, 2 );
	lua_pushnumber( L, i1 | i2 );
	return 1;
}

// ---------------------------------------------------------------------------
static int luaAzimuthPitch( lua_State *L ) {
	float azimuth = (float)luaL_checknumber( L, 1 );
	float pitch = (float)luaL_checknumber( L, 2 );

	jMatrix tempAzimuth, tempPitch, final;

	jMatrixSetRotationAxis( &tempAzimuth, 0.0f, 0.0f, 1.0f, azimuth );
	jMatrixSetRotationAxis( &tempPitch, 1.0f, 0.0f, 0.0f, pitch );
	jMatrixMultiply( &final, &tempAzimuth, &tempPitch );

	// Eihort --> Minecraft coordinate conversion here
	lua_pushnumber( L, final.fwd.y );
	lua_pushnumber( L, final.fwd.z );
	lua_pushnumber( L, final.fwd.x );
	lua_pushnumber( L, final.up.y );
	lua_pushnumber( L, final.up.z );
	lua_pushnumber( L, final.up.x );
	lua_pushnumber( L, final.right.y );
	lua_pushnumber( L, final.right.z );
	lua_pushnumber( L, final.right.x );
	return 9;
}

// ---------------------------------------------------------------------------
static int luaBeginRender( lua_State *L ) {
	int vpX = (int)luaL_optnumber( L, 1, 0 );
	int vpY = (int)luaL_optnumber( L, 2, 0 );
	int vpW = (int)luaL_optnumber( L, 3, g_width );
	int vpH = (int)luaL_optnumber( L, 4, g_height );

	glViewport( vpX, vpY, vpW, vpH );

	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LESS );

	glClear( GL_DEPTH_BUFFER_BIT );
	glUseProgram( 0 );

	glDisable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glEnable( GL_ALPHA_TEST );
	glAlphaFunc( GL_GREATER, 0.4f );
	glDisable( GL_LIGHTING );
	glEnable( GL_CULL_FACE );
	//glEnable(GL_COLOR_MATERIAL);
	glColorMaterial( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE );
	float white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glMaterialfv( GL_FRONT, GL_AMBIENT_AND_DIFFUSE, &white[0] );
	
	return 0;
}

static int luaSetClearColor( lua_State *L ) {
	glClearColor( (float)luaL_checknumber( L, 1 ), (float)luaL_checknumber( L, 2 ), (float)luaL_checknumber( L, 3 ), 1.0f );
	return 0;
}

static int luaClearScreen( lua_State* ) {
	glClear( GL_COLOR_BUFFER_BIT );
	return 0;
}

static int luaEndRender( lua_State *L ) {
	SDL_GL_SwapWindow( g_window );

	GLenum err = glGetError();
	if( err == GL_NO_ERROR ) {
		lua_pushboolean( L, true );
		return 1;
	}

	lua_pushboolean( L, false );
	lua_pushstring( L, GLErrorToString( err ) );
	return 2;
}

static int luaGetTime( lua_State *L ) {
	lua_pushnumber( L, SDL_GetTicks() / 1000.0f );
	return 1;
}

static int luaCreateDirectory( lua_State *L ) {
#ifdef _WINDOWS
	lua_pushboolean( L, CreateDirectoryA( luaL_checkstring( L, 1 ), NULL ) );
#elif defined(_POSIX_VERSION)
	lua_pushboolean( L, mkdir( luaL_checkstring( L, 1 ), S_IRWXU | S_IRWXG | S_IRWXO ) == 0 );
#else
#error implement me
#endif
	return 1;
}

static int luaIndexZip( lua_State *L ) {
	const char *path = luaL_checkstring( L, 1 );
	Unzip zipfile( path );
	if( !zipfile.good() )
		return 0;
	lua_newtable( L );
	for( auto it = zipfile.begin(); it != zipfile.end(); ++it ) {
		lua_pushstring( L, it->filename().c_str() );
		stringstream ss;
		if( it->is_stored() ) {
			ss << it->start() << ':' << it->uncompressed_size();
		} else {
			ss << it->start() << ':' << it->comp_size() << '>' << it->uncompressed_size();
		}
		lua_pushstring( L, ss.str().c_str() );
		lua_settable( L, -3 );
	}
	return 1;
}

// ---------------------------------------------------------------------------
const luaL_Reg BaseEihort_functions[] = {
	{ "pollEvent", &luaPollEvent },
	{ "yield", &luaYield },
	{ "getTime", &luaGetTime },

	{ "getProcessorCount", &luaGetProcessorCount },
	{ "initWorkers", &luaInitWorkers },
	{ "initializeVideo", &luaInitializeVideo },
	{ "getWindowDims", &luaGetWindowDims },
	{ "errorDialogYesNo", &luaErrorDlgYesNo },
	{ "errorDialog", &luaErrorDlg },
	{ "setWindowCaption", &luaSetWindowCaption },
	{ "showCursor", &luaShowCursor },
	{ "getMousePos", &luaGetMousePos },
	{ "warpMouse", &luaWarpMouse },

	{ "fwdUpRightFromAzPitch", &luaAzimuthPitch },

	{ "getVideoMem", &luaGetVideoMem },
	{ "shouldRedraw", &luaShouldRedraw },
	{ "beginRender", &luaBeginRender },
	{ "setClearColor", &luaSetClearColor },
	{ "clearScreen", &luaClearScreen },
	{ "endRender", &luaEndRender },

	{ "intAnd", &luaIntAnd },
	{ "intOr", &luaIntOr },

	{ "createDirectory", luaCreateDirectory },
	{ "indexZip", luaIndexZip },

	{ NULL, NULL }
};

// ---------------------------------------------------------------------------
inline
static const char *SDLKeyToString( SDL_Keycode key ) {
	switch( key ) {
		case SDLK_BACKSPACE: return "backspace";
		case SDLK_TAB: return "tab";
		case SDLK_CLEAR: return "clear";
		case SDLK_RETURN: return "return";
		case SDLK_PAUSE: return "pause";
		case SDLK_ESCAPE: return "escape";
		case SDLK_SPACE: return "space";
		case SDLK_EXCLAIM: return "!";
		case SDLK_QUOTEDBL: return "\"";
		case SDLK_HASH: return "#";
		case SDLK_DOLLAR: return "$";
		case SDLK_AMPERSAND: return "&";
		case SDLK_QUOTE: return "\'";
		case SDLK_LEFTPAREN: return "(";
		case SDLK_RIGHTPAREN: return ")";
		case SDLK_ASTERISK: return "*";
		case SDLK_PLUS: return "+";
		case SDLK_COMMA: return ",";
		case SDLK_MINUS: return "-";
		case SDLK_PERIOD: return ".";
		case SDLK_SLASH: return "/";
		case SDLK_0: return "0";
		case SDLK_1: return "1";
		case SDLK_2: return "2";
		case SDLK_3: return "3";
		case SDLK_4: return "4";
		case SDLK_5: return "5";
		case SDLK_6: return "6";
		case SDLK_7: return "7";
		case SDLK_8: return "8";
		case SDLK_9: return "9";
		case SDLK_COLON: return ":";
		case SDLK_SEMICOLON: return ";";
		case SDLK_LESS: return "<";
		case SDLK_EQUALS: return "=";
		case SDLK_GREATER: return ">";
		case SDLK_QUESTION: return "?";
		case SDLK_AT: return "@";
		case SDLK_LEFTBRACKET: return "[";
		case SDLK_BACKSLASH: return "\\";
		case SDLK_RIGHTBRACKET: return "]";
		case SDLK_CARET: return "^";
		case SDLK_UNDERSCORE: return "_";
		case SDLK_BACKQUOTE: return "`";
		case SDLK_a: return "a";
		case SDLK_b: return "b";
		case SDLK_c: return "c";
		case SDLK_d: return "d";
		case SDLK_e: return "e";
		case SDLK_f: return "f";
		case SDLK_g: return "g";
		case SDLK_h: return "h";
		case SDLK_i: return "i";
		case SDLK_j: return "j";
		case SDLK_k: return "k";
		case SDLK_l: return "l";
		case SDLK_m: return "m";
		case SDLK_n: return "n";
		case SDLK_o: return "o";
		case SDLK_p: return "p";
		case SDLK_q: return "q";
		case SDLK_r: return "r";
		case SDLK_s: return "s";
		case SDLK_t: return "t";
		case SDLK_u: return "u";
		case SDLK_v: return "v";
		case SDLK_w: return "w";
		case SDLK_x: return "x";
		case SDLK_y: return "y";
		case SDLK_z: return "z";
		case SDLK_DELETE: return "delete";
		case SDLK_KP_0: return "kp_0";
		case SDLK_KP_1: return "kp_1";
		case SDLK_KP_2: return "kp_2";
		case SDLK_KP_3: return "kp_3";
		case SDLK_KP_4: return "kp_4";
		case SDLK_KP_5: return "kp_5";
		case SDLK_KP_6: return "kp_6";
		case SDLK_KP_7: return "kp_7";
		case SDLK_KP_8: return "kp_8";
		case SDLK_KP_9: return "kp_9";
		case SDLK_KP_PERIOD: return "kp_period";
		case SDLK_KP_DIVIDE: return "kp_divide";
		case SDLK_KP_MULTIPLY: return "kp_multiply";
		case SDLK_KP_MINUS: return "kp_minus";
		case SDLK_KP_PLUS: return "kp_plus";
		case SDLK_KP_ENTER: return "kp_enter";
		case SDLK_KP_EQUALS: return "kp_equals";
		case SDLK_UP: return "up";
		case SDLK_DOWN: return "down";
		case SDLK_RIGHT: return "right";
		case SDLK_LEFT: return "left";
		case SDLK_INSERT: return "insert";
		case SDLK_HOME: return "home";
		case SDLK_END: return "end";
		case SDLK_PAGEUP: return "pageup";
		case SDLK_PAGEDOWN: return "pagedown";
		case SDLK_F1: return "f1";
		case SDLK_F2: return "f2";
		case SDLK_F3: return "f3";
		case SDLK_F4: return "f4";
		case SDLK_F5: return "f5";
		case SDLK_F6: return "f6";
		case SDLK_F7: return "f7";
		case SDLK_F8: return "f8";
		case SDLK_F9: return "f9";
		case SDLK_F10: return "f10";
		case SDLK_F11: return "f11";
		case SDLK_F12: return "f12";
		case SDLK_F13: return "f13";
		case SDLK_F14: return "f14";
		case SDLK_F15: return "f15";
		case SDLK_NUMLOCKCLEAR: return "numlockclear";
		case SDLK_CAPSLOCK: return "capslock";
		case SDLK_SCROLLLOCK: return "scrolllock";
		case SDLK_RSHIFT: return "rshift";
		case SDLK_LSHIFT: return "lshift";
		case SDLK_RCTRL: return "rctrl";
		case SDLK_LCTRL: return "lctrl";
		case SDLK_RALT: return "ralt";
		case SDLK_LALT: return "lalt";
		case SDLK_LGUI: return "lwin";
		case SDLK_RGUI: return "rwin";
		case SDLK_MODE: return "mode";
		case SDLK_HELP: return "help";
		case SDLK_SYSREQ: return "sysreq";
		case SDLK_MENU: return "menu";
		case SDLK_POWER: return "power";
		default:
			static char kee[16];
			snprintf (kee, 16, "unk%d", key);
			return kee;
	}
}

// ---------------------------------------------------------------------------
static const char *GLErrorToString( GLenum err ) {
	switch( err ) {
	case GL_NO_ERROR: return "GL_NO_ERROR";
	case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
	case GL_STACK_OVERFLOW: return "GL_INVALID_OPERATION";
	case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW";
	case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
	case GL_TABLE_TOO_LARGE: return "GL_TABLE_TOO_LARGE";
	default: return "Unknown GL error";
	}
}

