
#####################################
### Eihort makefile for gmake/gcc ###
#####################################

#	Debian Linux:
#		1. Install dependencies using the package manager (e.g. apt)
#		2. make
#	Mac OS X:
# 	1. Build dependencies as static libraries, install in "./macosx/contrib/"; remember to compile with appropriate flags (e.g. -arch and --sysroot)
#		2. PATH="$PATH:./macosx/contrib/bin/" make CXXFLAGS.EXTRA="-I./macosx/contrib/include/" LDFLAGS.EXTRA="-L./macosx/contrib/lib/" SDL_CONFIG="./macosx/contrib/bin/sdl-config --static-libs"

# Set up defaults for tools
CXX = g++
INSTALL = install
PYTHON := $(shell which python python3 python2 | head -1)
RM = rm -fr

# Set up project name and version
project := eihort
version := $(shell sed -n 's/^\#define VERSION "\(.*\)"/\1/p' src/main.cpp)

# Figure out system and machine
system  := $(shell uname -s | tr [A-Z] [a-z])
machine := $(shell uname -m | tr [A-Z] [a-z])

# Create package identifier
ifeq ($(system),darwin)
ident := $(project)-$(version)-macosx
else
ident := $(project)-$(version)-$(system)-$(machine)
endif

### Configure for GCC

# Add some useful flags
CXXFLAGS += -Wall -Wextra -ansi -pedantic $(CXXFLAGS.EXTRA) $(LDFLAGS.EXTRA)
  # NB. put here your extra C++ flags ------^^^^^^^^^^

# Add debugging flags
ifdef DEBUG
ifndef RELEASE
CXXFLAGS += -DDEBUG -O0
endif
CXXFLAGS += -g
endif # ifdef DEBUG

# Add optimization flags
ifdef RELEASE
CXXFLAGS += -DNDEBUG -O3 -fno-asynchronous-unwind-tables -fomit-frame-pointer
endif

### Default to Debian build

# Debian uses different names than Linux
ifeq ($(machine),x86_64)
debmachine := amd64
else
debmachine := $(subst i486,i386,$(subst i586,i386,$(subst i686,i386,$(machine))))
endif

# Debian target triple
debtriple = $(project)_$(version)_$(debmachine)

# Set up ZIP file
zipname = $(ident).zip
zipfiles = $(ident) $(luafiles) $(supfiles)

# Set targets
targets = control.$(debmachine) $(debtriple).debdir $(debtriple).deb $(zipname)

### Configure for Mac OS X
ifeq ($(system),darwin)

# Make universal binaries
CXXFLAGS += $(patsubst %,-arch %,$(machine) x86_64 i386)

# Set up framework paths
MACOSX_VERSION ?= 10.5
CXXFLAGS += -isysroot /Developer/SDKs/MacOSX$(MACOSX_VERSION).sdk \
  -mmacosx-version-min=$(MACOSX_VERSION)

# Add used frameworks
CXXFLAGS += $(patsubst %,-framework %,Cocoa CoreFoundation OpenGL)

# Set up app name & targets
appname = deploy/Eihort.app

# Set up ZIP parameters
zipname = $(ident).zip
zipfiles = $(appname) $(luafiles) $(supfiles)

# Set targets
targets = $(appname) $(zipname)

endif # ifeq ($(system),darwin)

### Configure dependent libraries

# For now SDL has special treatment, since the Mac version needs --static-libs
SDL_CONFIG = sdl-config
SDL_FLAGS ?= $(shell $(SDL_CONFIG) --cflags --libs)
CXXFLAGS += $(SDL_FLAGS)

# Add other libs
CXXFLAGS += $(shell $(PYTHON) depflags.py SDL_image glew libpng lua5.1 -or lua z)

# On Linux, get also GL and X11 in case indirect linking is disabled
ifeq ($(system),linux)
CXXFLAGS += $(shell $(PYTHON) depflags.py gl x11 xext)
endif

### Find input files

# C/C++ files
headers := $(wildcard src/*.h src/*.hpp)
sources := $(wildcard src/*.c src/*.cpp)

# Support files
luafiles := deploy/eihort.config $(wildcard deploy/*.lua)
resfiles := deploy/screen_font.bin
supfiles := deploy/readme.txt

# We don't have ODE
CXXFLAGS += -DNO_ODE

# Add triangle from lib/triangle/
CXXFLAGS += -Ilib/triangle/ lib/triangle/triangle.c

### Build rules

# Build the binary and the zip file
all: $(targets)

# Clean up
clean:
	$(RM) $(ident) $(targets)

# Build the binary
$(ident): $(headers) $(sources)
	$(CXX) -o "$@" $(sources) $(CXXFLAGS)

# Debian package

control.$(debmachine): debian/control.in
	sed "s/@package@/$(project)/;\
	s/@version@/$(version)/;\
	s/@arch@/$(debmachine)/" debian/control.in >"$@"

$(debtriple).debdir: control.$(debmachine) $(ident)
	$(INSTALL) -d -m 0755 "$@/" "$@/DEBIAN/" "$@/usr/games/" "$@/usr/share/games/" "$@/usr/share/games/eihort/"
	$(INSTALL) -m 0644 control.$(debmachine) "$@/DEBIAN/control"
	$(INSTALL) -m 0755 debian/eihort "$@/usr/games/"
	$(INSTALL) -m 0755 $(ident) "$@/usr/share/games/eihort/eihort"
	$(INSTALL) -m 0644 $(luafiles) $(resfiles) "$@/usr/share/games/eihort/"

$(debtriple).deb: $(debtriple).debdir
	sleep 1 && fakeroot dpkg-deb -b "$(debtriple).debdir" "$@"

# Make Mac OS X application bundle
$(appname): $(ident)
	$(INSTALL) -d -m 0755 "$@/" "$@/Contents/" "$@/Contents/MacOS/" "$@/Contents/Resources/"
	$(INSTALL) -m 0644 macosx/Info.plist "$@/Contents/"
	$(INSTALL) -m 0755 $(ident) "$@/Contents/MacOS/eihort"
	$(INSTALL) -m 0644 $(resfiles) "$@/Contents/Resources/"

# Make zip archive
$(zipname): $(zipfiles)
	$(PYTHON) makezip.py "$@" $(zipfiles)

.PHONY: all clean
.SUFFIXES:
