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


#include "findfile.h"
#include "mcregionmap.h"
#include "mcbiome.h"
#include "worldqtree.h"
#include "platform.h"
#include "endian.h"

#define MCREGIONMAP_META "MCRegionMap"

#if defined(__APPLE__) && defined(__MACH__)
  // Needed for FSEvent api
# include <CoreServices/CoreServices.h>
#endif

#if defined(__linux) || defined(linux)
  // inotify(7)
# include <sys/inotify.h>
# include <unistd.h>
#endif

inline int toRegionCoord( int x ) {
	return shift_right( x, 5 );
}

MCRegionMap::MCRegionMap( const char *rootPath, bool anvil )
: root(""), anvil(anvil)
, minRgX(0), maxRgX(0), minRgY(0), maxRgY(0)
, watchUpdates(false)
{
	changeRoot( rootPath, anvil );

	rgDescMutex = SDL_CreateMutex();
	changeThread = SDL_CreateThread( updateScanner, "Eihort File Scanner", this );
}

MCRegionMap::~MCRegionMap() {
}

void MCRegionMap::changeRoot( const char *newRoot, bool anvil ) {
	root = newRoot;
	this->anvil = anvil;
	if( root[this->root.length()-1] == '/' || root[this->root.length()-1] == '\\' )
		this->root = this->root.substr( 0, this->root.length()-1 );
	
	flushRegionSectors();
	exploreDirectories();
}

void MCRegionMap::checkForRegionChanges() {
	exploreDirectories();
}

MCRegionMap::ChangeListener::ChangeListener() {
}

MCRegionMap::ChangeListener::~ChangeListener() {
}

void MCRegionMap::getWorldChunkExtents( int &minx, int &maxx, int &miny, int &maxy ) {
	const unsigned rgShift = 5; // 32 chunks/region
	miny = minRgX << rgShift;
	maxy = ((maxRgX+1) << rgShift) - 1;
	minx = minRgY << rgShift;
	maxx = ((maxRgY+1) << rgShift) - 1;
}

void MCRegionMap::getWorldBlockExtents( int &minx, int &maxx, int &miny, int &maxy ) {
	const unsigned rgShift = 5+4; // 32 chunks/region, 16 blocks/chunk
	miny = minRgX << rgShift;
	maxy = ((maxRgX+1) << rgShift) - 1;
	minx = minRgY << rgShift;
	maxx = ((maxRgY+1) << rgShift) - 1;
}

nbt::Compound *MCRegionMap::readChunk( int x, int y ) {
	unsigned t;
	if( getChunkInfo( x, y, t ) ) {
		char regionfn[MAX_PATH];
		snprintf( regionfn, MAX_PATH, "%s/region/r.%d.%d.%s", root.c_str(), toRegionCoord(x), toRegionCoord(y), getRegionExt() );
		return nbt::readFromRegionFile( regionfn, ((unsigned)x&31) + (((unsigned)y&31)<<5) );
	}
	return NULL;
}

void MCRegionMap::exploreDirectories() {
	//unsigned *chunkSectors = new unsigned[1024];

	char dirname[MAX_PATH];
	snprintf( dirname, MAX_PATH, "%s/region/*.%s", root.c_str(), getRegionExt() );

  FindFile find(dirname);

  const char *file = find.filename();
	if( file != NULL ) {
		do {
			int regionX, regionY;
			if( file[0] != 'r' || file[1] != '.' )
				continue;
			char *s;
			regionX = strtol( &file[2], &s, 10 );
			if( s[0] != '.' )
				continue;
			s++;
			regionY = strtol( s, &s, 10 );
			if( *s != '.' || 0 != strcmp( s+1, getRegionExt() ) )
				continue;

			if( abs(regionX) > 0x0001ffff || abs(regionY) > 0x0001ffff )
				continue;
			Coords2D rgCoords = { regionX, regionY };

			RegionMap::iterator it = regions.find( rgCoords );
			if( it != regions.end() ) {
				// Reloading regions after level.dat update
				if( it->second.chunkTimes ) {
					// The region is loaded - check it for changes
					checkRegionForChanges( regionX, regionY, &it->second );
				}
			} else {
				// New undiscovered region
				RegionDesc rg;
				rg.chunkTimes = NULL;
				rg.sectors = NULL;
				regions[rgCoords] = rg;

				if( rgCoords.x < minRgX )
					minRgX = rgCoords.x;
				if( rgCoords.x > maxRgX )
					maxRgX = rgCoords.x;
				if( rgCoords.y < minRgY )
					minRgY = rgCoords.y;
				if( rgCoords.y > maxRgY )
					maxRgY = rgCoords.y;
			}
		} while((file = find.next()) != NULL);
	}
}

void MCRegionMap::flushRegionSectors() {
	for( RegionMap::iterator it = regions.begin(); it != regions.end(); ++it ) {
		delete[] it->second.chunkTimes;
		it->second.chunkTimes = NULL;
		delete[] it->second.sectors;
		it->second.sectors = NULL;
	}
	regions.clear();
}

void MCRegionMap::checkRegionForChanges( int x, int y, RegionDesc *region ) {
	Coords2D c = { x, y };

	char regionfn[MAX_PATH];
	snprintf( regionfn, MAX_PATH, "%s/region/r.%d.%d.%s", root.c_str(), c.x, c.y, getRegionExt() );
	FILE *f = fopen( regionfn, "rb" );

	if( !f ) {
		// ... what just happened?
		delete[] region->chunkTimes;
		region->chunkTimes = NULL;
		delete[] region->sectors;
		region->sectors = NULL;
		return;
	}

	fseek( f, 0, SEEK_SET );
	fread( region->sectors, 4, 1024, f );

	for( unsigned i = 0; i < 1024; i++ ) {
		unsigned newTime;
		fread( &newTime, 4, 1, f );
		newTime = bswap_from_big( newTime );
		region->sectors[i] = bswap_from_big( region->sectors[i] );
		if( region->chunkTimes[i] < newTime ) {
			// Updated chunk!
			region->chunkTimes[i] = newTime;
			if( listener ) {
				int x = (c.x<<5)+(int)(i&31);
				int y = (c.y*32)+(int)(i>>5);
				listener->chunkChanged( y, x );
			}
		}
	}

	fclose( f );
}

bool MCRegionMap::getChunkInfo( int x, int y, unsigned &updTime ) {
	SDL_mutexP( rgDescMutex );

	Coords2D c = { toRegionCoord(x), toRegionCoord(y) };
	RegionMap::iterator it = regions.find( c );
	if( it == regions.end() ) {
		SDL_mutexV( rgDescMutex );
		return false;
	}

	if( !it->second.chunkTimes ) {
		char regionfn[MAX_PATH];
		snprintf( regionfn, MAX_PATH, "%s/region/r.%d.%d.%s", root.c_str(), c.x, c.y, getRegionExt() );
		FILE *f = fopen( regionfn, "rb" );

		if( !f ) {
			SDL_mutexV( rgDescMutex );
			return false;
		}

		it->second.chunkTimes = new uint32_t[1024];
		it->second.sectors = new uint32_t[1024];
		fseek( f, 0, SEEK_SET );
		fread( it->second.sectors, 4, 1024, f );
		fread( it->second.chunkTimes, 4, 1024, f );
		fclose( f );

		for( unsigned i = 0; i < 1024; i++ ) {
			it->second.chunkTimes[i] = bswap_from_big( it->second.chunkTimes[i] );
			it->second.sectors[i] = bswap_from_big( it->second.sectors[i] );
		}
	}

	unsigned i = ((unsigned)x&31) + (((unsigned)y&31)<<5);
	updTime = it->second.chunkTimes[i];

	SDL_mutexV( rgDescMutex );
	return it->second.sectors[i] != 0;
	//return updTime != 0; // Apparently the timestamps are unreliable. This punches holes in the world.
}

int MCRegionMap::updateScanner( void *rgMapCookie ) {
	MCRegionMap *rgMap = static_cast<MCRegionMap*>( rgMapCookie );

	//rgMap->exploreDirectories();
	//SDL_mutexV( rgMap->rgDescMutex );

#ifdef _WINDOWS
	unsigned lastFullScan = SDL_GetTicks();

	// PS-WIN32: Directory change signals
	char notifyInfo[16*1024];
	snprintf( notifyInfo, 16*1024, "%s/", rgMap->root.c_str() );
	HANDLE dirHandle = CreateFileA( notifyInfo, FILE_LIST_DIRECTORY, 7, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
	//HANDLE changeHandle = FindFirstChangeNotificationA( rgMap->root.c_str(), false, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE );

	while( true ) {
	
		//DWORD waitResult = WaitForSingleObject( changeHandle, INFINITE );
		//FILE_NOTIFY_INFORMATION &info = *(FILE_NOTIFY_INFORMATION*)&notifyInfo[0];
		DWORD len = 0;

		if( ReadDirectoryChangesW( dirHandle, &notifyInfo[0], sizeof(notifyInfo), TRUE,
			FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
			&len, NULL, NULL ) ) {

			if( rgMap->watchUpdates ) {//&& (wcsncmp( L"level.dat", info.FileName, info.FileNameLength>>1 ) ||
				//(info.FileName[0] == L'r' && info.FileName[1] == L'.')) ) {

				unsigned now = SDL_GetTicks();
				if( now - lastFullScan < 1000 ) {
					SDL_Delay( 1000 ); // At least 1s between updates
					lastFullScan = now;
				}

				SDL_mutexP( rgMap->rgDescMutex );
				rgMap->exploreDirectories();
				SDL_mutexV( rgMap->rgDescMutex );
			}

			/*
			unsigned i = 0;
			char *fn = (char*)&info.FileName[0];
			for( ; i < info.FileNameLength>>1; i++ )
				fn[i] = (char)info.FileName[i];
			fn[i] = 0;

			Coords2D c;
			char *s = (char*)&info.FileName[0];
			if( s[0] != 'r' || s[1] != '.' )
				continue;
			s += 2;
			c.x = strtol( s, &s, 10 );
			if( s[0] != '.' )
				continue;
			s++;
			c.y = strtol( s, &s, 10 );
			if( 0 != strcmp( s, ".mcr" ) )
				continue;

			SDL_mutexP( rgMap->rgDescMutex );
			
			RegionMap::iterator it = rgMap->regions.find( c );
			if( it == rgMap->regions.end() ) {
				// New region! ignore for now....
			} else if( it->second.chunkSectors ) {
				// Already-loaded region - check for new changes

				char regionfn[MAX_PATH];
				snprintf( regionfn, MAX_PATH, "%s/region/r.%d.%d.mcr", rgMap->root.c_str(), c.x, c.y );
				FILE *f = fopen( regionfn, "rb" );
				//FILE *f = fopen( (char*)&info.FileName[0], "rb" );

				if( !f ) {
					// ... what just happened?
					delete[] it->second.chunkSectors;
					it->second.chunkSectors = NULL;
					it->second.chunkTimes = NULL;
					SDL_mutexV( rgMap->rgDescMutex );
					continue;
				}

				fread( it->second.chunkSectors, 4, 1024, f );

				for( unsigned i = 0; i < 1024; i++ ) {
					it->second.chunkSectors[i] = bswap_from_big( it->second.chunkSectors[i] );
					unsigned newTime;
					fread( &newTime, 4, 1, f );
					if( (it->second.chunkSectors[i] & 0xff) == 0 ) {
						it->second.chunkSectors[i] = 0;
						it->second.chunkTimes[i] = 0;
					} else {
						it->second.chunkSectors[i] >>= 8;
						newTime = bswap_from_big( newTime );
						if( it->second.chunkTimes[i] < newTime ) {
							// Updated chunk!
							it->second.chunkTimes[i] = newTime;
							if( rgMap->listener )
								rgMap->listener->chunkChanged( (c.y<<5)+(i>>5), (c.x<<5)+(i&31) );
						}
					}
				}

				fclose( f );
			}

			SDL_mutexV( rgMap->rgDescMutex );
			*/
		}
	}// while( FindNextChangeNotification( changeHandle ) );

#elif defined(__APPLE__) && defined(__MACH__)
  // Mac OS X FSEvent API
    // I'd prefer kqueue(2), but it does not allow detection of file changes
    // w/o registering the files. Ask Jason what's the actual use scenario.

  struct Callback {
    static void callback(ConstFSEventStreamRef, void *cookie, size_t, void *, const FSEventStreamEventFlags *, const FSEventStreamEventId *)
    {
			MCRegionMap *rgMap = static_cast<MCRegionMap*>( cookie );
      if (rgMap->watchUpdates)
      {
        SDL_mutexP( rgMap->rgDescMutex );
        rgMap->exploreDirectories();
        SDL_mutexV( rgMap->rgDescMutex );
      }
    }
  };

  CFStringRef path = CFStringCreateWithCString(NULL, rgMap->root.c_str(), CFStringGetSystemEncoding());
  CFArrayRef paths = CFArrayCreate(NULL, (const void **)&path, 1, NULL);
  FSEventStreamContext callback_data = { 0, (void *)rgMap, NULL, NULL, NULL };
  FSEventStreamRef stream = FSEventStreamCreate(NULL,
    &Callback::callback,
    &callback_data,
    paths,
    kFSEventStreamEventIdSinceNow,
    CFAbsoluteTime(1.0),
    kFSEventStreamCreateFlagNone);
  FSEventStreamScheduleWithRunLoop(stream, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
  FSEventStreamStart(stream);
  CFRunLoopRun();
  FSEventStreamStop(stream);
  FSEventStreamRelease(stream);
  CFRelease(path);
  CFRelease(paths);

#else
# if defined(__linux) || defined(linux)
  // Linux inotify(7)

  char buf[BUFSIZ];
  int fd = inotify_init();
  if (inotify_add_watch(fd, rgMap->root.c_str(), IN_CREATE | IN_DELETE | IN_MODIFY) != -1 &&
      inotify_add_watch(fd, (rgMap->root + "/region/").c_str(), IN_CREATE | IN_DELETE | IN_MODIFY) != -1)
    while (0 < read(fd, buf, BUFSIZ))
# else
  // Far from optimal..
  for (;;)

# endif
    if (rgMap->watchUpdates)
    {
      SDL_mutexP( rgMap->rgDescMutex );
      rgMap->exploreDirectories();
      SDL_mutexV( rgMap->rgDescMutex );
      SDL_Delay(1000);
    }
#endif

	return 0;
}

int MCRegionMap::lua_create( lua_State *L ) {
	const char *root = luaL_checklstring( L, 1, NULL );
	luaL_argcheck( L, lua_isboolean( L, 2 ) || lua_isnoneornil( L, 2 ), 2, "Expected Anvil flag" );
	MCRegionMap *regions = new MCRegionMap( root, !!lua_toboolean( L, 2 ) );
	regions->setupLuaObject( L, MCREGIONMAP_META );
	regions->lua_push();
	return 1;
}

int MCRegionMap::lua_getRegionCount( lua_State *L ) {
	lua_pushnumber( L, getLuaObjectArg<MCRegionMap>( L, 1, MCREGIONMAP_META )->getTotalRegionCount() );
	return 1;
}

int MCRegionMap::lua_getRootPath( lua_State *L ) {
	char path[MAX_PATH];
	strcpy( path, getLuaObjectArg<MCRegionMap>( L, 1, MCREGIONMAP_META )->getRoot().c_str() );
	strcat( path, "/" );
	lua_pushstring( L, &path[0] );
	return 1;
}

int MCRegionMap::lua_changeRootPath( lua_State *L ) {
	MCRegionMap *regions = getLuaObjectArg<MCRegionMap>( L, 1, MCREGIONMAP_META );
	regions->changeRoot( luaL_checklstring( L, 2, NULL ), regions->anvil );
	return 0;
}

int MCRegionMap::lua_setMonitorState( lua_State *L ) {
	luaL_argcheck( L, lua_isboolean( L, 2 ), 2, "Expected monitor state" );
	getLuaObjectArg<MCRegionMap>( L, 1, MCREGIONMAP_META )->watchUpdates = !!lua_toboolean( L, 2 );
	return 0;
}

int MCRegionMap::lua_createView( lua_State *L ) {
	MCRegionMap *regions = getLuaObjectArg<MCRegionMap>( L, 1, MCREGIONMAP_META );
	MCBlockDesc *blocks = getLuaObjectArg<MCBlockDesc>( L, 2, MCBLOCKDESC_META );
	unsigned leafShift = (unsigned)luaL_optnumber( L, 3, 7.0 );
	luaL_argcheck( L, leafShift >= 2 && leafShift <= 12, 2, "The QTree leaf size must be at least 2 and no more than 12." );
	WorldQTree::createNew( L, regions, blocks, leafShift );
	return 1;
}

int MCRegionMap::lua_destroy( lua_State *L ) {
	delete getLuaObjectArg<MCRegionMap>( L, 1, MCREGIONMAP_META );
	return 0;
}

static const luaL_Reg MCRegionMap_functions[] = {
	{ "getRegionCount", &MCRegionMap::lua_getRegionCount },
	{ "getRootPath", &MCRegionMap::lua_getRootPath },
	{ "changeRootPath", &MCRegionMap::lua_changeRootPath },
	{ "setMonitorState", &MCRegionMap::lua_setMonitorState },
	{ "createView", &MCRegionMap::lua_createView },
	{ "destroy", &MCRegionMap::lua_destroy },
	{ NULL, NULL }
};

void MCRegionMap::setupLua( lua_State *L ) {
	luaL_newmetatable( L, MCREGIONMAP_META );
	lua_pushvalue( L, -1 );
	lua_setfield( L, -2, "__index" );
	luaL_register( L, NULL, &MCRegionMap_functions[0] );
	lua_pop( L, 1 );

	lua_pushcfunction( L, &MCRegionMap::lua_create );
	lua_setfield( L, -2, "loadWorld" );
}
