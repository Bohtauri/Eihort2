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

#ifndef NBT_H
#define NBT_H

#include <list>
#include <map>
#include <string>
#include <iostream>
#include "stdint.h"

namespace nbt {

	enum TagType {
		TAG_End			= 0,
		TAG_Byte		= 1,
		TAG_Short		= 2,
		TAG_Int			= 3,
		TAG_Long		= 4,
		TAG_Float		= 5,
		TAG_Double		= 6,
		TAG_Byte_Array	= 7,
		TAG_String		= 8,
		TAG_List		= 9,
		TAG_Compound	= 10,
		TAG_Int_Array   = 11,
		TAG_Count       = 12
	};

	union TagData;
	class Compound;
	class List;
	struct Tag;
	struct Array;

	union TagData {
		int8_t b;
		int16_t s;
		int32_t i;
		int64_t l;
		float f;
		double d;
		void *bytes;
		std::wstring *str;
		List *list;
		Compound *comp;
		int *ia;
	};

	struct Tag {
		inline Tag() { }
		inline Tag( int8_t b )
			: type(TAG_Byte) { data.b = b; }
		inline Tag( int16_t s )
			: type(TAG_Short) { data.s = s; }
		inline Tag( int32_t i )
			: type(TAG_Int) { data.i = i; }
		inline Tag( int64_t l )
			: type(TAG_Long) { data.l = l; }
		inline Tag( float f )
			: type(TAG_Float) { data.f = f; }
		inline Tag( double d )
			: type(TAG_Double) { data.d = d; }
		inline Tag( void *bytes, unsigned size )
			: type(TAG_Byte_Array) { data.bytes = bytes; extra = size; }
		inline Tag( wchar_t *str )
			: type(TAG_String) { data.str = new std::wstring(str); }
		inline Tag( const std::wstring &str )
			: type(TAG_String) { data.str = new std::wstring(str); }
		inline Tag( std::wstring *str )
			: type(TAG_String) { data.str = str; }
		inline Tag( List *l )
			: type(TAG_List) { data.list = l; }
		inline Tag( Compound *c )
			: type(TAG_Compound) { data.comp = c; }
		inline Tag( int *ints, unsigned n )
			: type(TAG_Int_Array) { data.ia = ints; extra = n; }

		TagType type;
		unsigned extra;
		TagData data;

		unsigned getArraySize() const { return extra; }

		void destroyPayload();
	};

	class Compound : public std::map< std::wstring, Tag > {
	public:
		inline Compound() { }
		~Compound();

		inline bool has( const std::wstring &what ) const { return find(what)!=end(); }
		void eraseTag( const std::wstring &tagName );
		void replaceTag( const std::wstring &tagName, const Tag &newTag );
		void write( const char *filename, const std::wstring &outerName );
		void printReadable( std::ostream &out, const char *pre = "" ) const;
	};

	class List : public std::list< TagData > {
	public:
		inline List( TagType type )
			: std::list<TagData>(), type(type) { }
		~List();

		inline TagType getType() const { return type; }

	private:
		TagType type;
	};

	Compound *readNBT( const char *filename, std::wstring *outerName = NULL );
	Compound *readFromRegionFile( const char *filename, unsigned idx );
	Compound *readFromRegionFileSector( const char *filename, unsigned idx );
	inline Compound *readFromRegionFile( const char *filename, unsigned x, unsigned y ) {
		return readFromRegionFile( filename, x+(y<<5) );
	}
}

#endif
