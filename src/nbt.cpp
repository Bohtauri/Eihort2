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


#include <zlib.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "nbt.h"
#include "endian.h"

namespace nbt {

void Tag::destroyPayload() {
	switch( type ) {
	case TAG_End:
	case TAG_Byte:
	case TAG_Short:
	case TAG_Int:
	case TAG_Long:
	case TAG_Float:
	case TAG_Double: break;
	case TAG_Byte_Array: free( data.bytes );   break;
	case TAG_String:     delete data.str;  break;
	case TAG_List:       delete data.list; break;
	case TAG_Compound:   delete data.comp; break;
	case TAG_Int_Array:  delete[] data.ia; break;
	default: assert( false );
	}
	type = TAG_Int;
}

Compound::~Compound() {
	for( iterator it = begin(); it != end(); ++it ) 
		it->second.destroyPayload();
}

void Compound::eraseTag( const std::wstring &tagName ) {
	iterator it = find( tagName );
	if( it != end() ) {
		it->second.destroyPayload();
		erase( it );
	}
}

void Compound::replaceTag( const std::wstring &tagName, const Tag &newTag ) {
	eraseTag( tagName );
	(*this)[tagName] = newTag;
}

void Compound::printReadable( std::ostream &out, const char *pre ) const {
	if( size() == 0 ) {
		out << "{ }";
		return;
	}

	out << "{" << std::endl;
	for( const_iterator it = begin(); it != end(); ++it ) {
		out << pre << '\t';
		std::string name( it->first.begin(), it->first.end() );
		switch( it->second.type ) {
		case TAG_Byte:
			out << "Byte " << name << " = " << (unsigned)it->second.data.b << std::endl;
			break;
		case TAG_Short:
			out << "Short " << name << " = " << it->second.data.s << std::endl;
			break;
		case TAG_Int:
			out << "Int " << name << " = " << it->second.data.i << std::endl;
			break;
		case TAG_Long:
			out << "Long " << name << " = " << it->second.data.l << std::endl;
			break;
		case TAG_Float:
			out << "Float " << name << " = " << it->second.data.f << std::endl;
			break;
		case TAG_Double:
			out << "Double " << name << " = " << it->second.data.d << std::endl;
			break;
		case TAG_Byte_Array:
			out << "Data " << name << ", size = " << it->second.getArraySize() << std::endl;
			break;
		case TAG_String: {
				std::string str( it->second.data.str->begin(), it->second.data.str->end() );
				out << "String " << name << " = " << str << std::endl;
			} break;
		case TAG_List:
			out << "List " << name << std::endl;
			break;
		case TAG_Compound: {
				char newPre[32];
				strcpy( newPre, pre );
				strcat( newPre, "\t" );
				out << "Compound " << name << " ";
				it->second.data.comp->printReadable( out, newPre );
				out << std::endl;
			} break;
		case TAG_Int_Array:
			out << "IntArray " << name << ", size = " << it->second.getArraySize() << std::endl;
			break;
		default: assert( false );
		}
	}
	out << pre << "}";
}


List::~List() {
	Tag tag;
	for( iterator it = begin(); it != end(); ++it ) {
		tag.type = type;
		tag.data = *it;
		tag.destroyPayload();
	}
}

class gzistream {
public:
	gzistream( const char *fn, unsigned idx ) {
		// Read an NBT from an MCRegion file
		in = Z_NULL;
		fileNotFound = false;
		cmpBuffer = NULL;

		uint32_t position, len;

		FILE *f = fopen( fn, "rb" );
		if( !f ) {
			fileNotFound = true;
			return;
		}
		if( idx < 1024 ) {
			fseek( f, (long)(idx<<2), SEEK_SET );
			fread( &position, 4, 1, f );
			position = (bswap_from_big(position) >> 8) << 12;
		} else {
			position = idx;
		}
		if( position == 0 ) {
			fileNotFound = true;
			return;
		}
		fseek( f, (long)position, SEEK_SET );
		fread( &len, 4, 1, f );
		len = bswap_from_big(len);

		unsigned char version;
		fread( &version, 1, 1, f );
		void *fileBuf = malloc( len );
		fread( fileBuf, len-1, 1, f );
		fclose( f );

		uLongf bytesAvailable = 512*1024;
		cmpBuffer = (unsigned char*)malloc( bytesAvailable );
		uncompress( cmpBuffer, &bytesAvailable, (unsigned char*)fileBuf, len-1 );
		free( fileBuf );

		bufferLeft = bytesAvailable;
		cursor = &cmpBuffer[0];
	}
	gzistream( const char *fn )
		: bufferLeft(0)
	{
		in = gzopen( const_cast<char*>(fn), "rb" );
		fileNotFound = (in == Z_NULL);
		cmpBuffer = fileNotFound ? NULL : (unsigned char*)malloc( BUFFER_SIZE );
	}
	~gzistream() {
		if( in != Z_NULL )
			gzclose( in );
		free( cmpBuffer );
	}

	char get() { return read<char>(); }
	template< typename T >
	T read() {
		T data;
		read( &data, sizeof(T) );
		return data;
	}
	template< typename T, typename Tui >
	T readlli() { // endian swap for integer types
    return bswap_from_big(read<T>());
	}

	void read( void *dest, size_t sz ) {
		while( sz > bufferLeft ) {
			assert( in );

			memcpy( dest, cursor, bufferLeft );
			sz -= bufferLeft;
			dest = (char*)dest + bufferLeft;
			bufferLeft = (unsigned)gzread( in, cmpBuffer, BUFFER_SIZE );
			cursor = &cmpBuffer[0];
		}

		memcpy( dest, cursor, sz );
		cursor += sz;
		bufferLeft -= (unsigned)sz;
	}

	bool fileFound() { return !fileNotFound; }

private:
	enum { BUFFER_SIZE = 1024 };
	unsigned char *cmpBuffer;
	unsigned bufferLeft;
	unsigned char *cursor;
	bool fileNotFound;
	gzFile in;
};

class nbtstream : private gzistream {
public:
	nbtstream( const char *fn, unsigned idx )
		: gzistream( fn, idx )
	{ }
	explicit nbtstream( const char *filename )
		: gzistream( filename )
	{ }
	~nbtstream() { }

	void readNamedTag( std::wstring &name, Tag &tag ) {
		tag.type = (TagType)get();
		if( tag.type == TAG_End ) {
			name = L"";
		} else {
			readString( name );
			readTagPayload( tag );
		}
	}

	bool fileFound() { return gzistream::fileFound(); }

private:
	void readTagPayload( Tag &tag ) {
		switch( tag.type ) {
		case TAG_Byte:   tag.data.b = get(); break;
		case TAG_Short:  tag.data.s = readlli<int16_t, uint16_t>(); break;
		case TAG_Float:
		case TAG_Int:    tag.data.i = readlli<int32_t, uint32_t>(); break;
		case TAG_Double:
		case TAG_Long:   tag.data.l = readlli<int64_t, uint64_t>(); break;
		case TAG_Byte_Array:
			tag.extra = readlli<uint32_t,uint32_t>();
			tag.data.bytes = malloc( tag.extra );
			read( tag.data.bytes, tag.extra );
			break;
		case TAG_String:
			tag.data.str = new std::wstring;
			readString( *tag.data.str );
			break;
		case TAG_List:
			tag.data.list = readList();
			break;
		case TAG_Compound:
			tag.data.comp = readCompound();
			break;
		case TAG_Int_Array:
			tag.extra = readlli<uint32_t,uint32_t>();
			tag.data.ia = new int[tag.extra];
			for( unsigned i = 0; i < tag.extra; i++ )
				tag.data.ia[i] = readlli<int32_t, uint32_t>();
			break;
		default:
			assert( false );
		}
	}

	void readString( std::wstring &str ) {
		unsigned len = (unsigned)readlli<int16_t,uint16_t>();
		if( len == 0 ) {
			str = L"";
			return;
		}

		char *b = new char[len+1];
		read( b, len );
		b[len] = '\0';
		wchar_t *d = new wchar_t[len+1];
		mbstowcs( d, b, len+1 );
		str = d;
		delete[] d;
		delete[] b;
	}

	List *readList() {
		Tag tag;
		tag.type = (TagType)get();
		assert( tag.type != TAG_Byte_Array && tag.type < TAG_Count );
		unsigned len = readlli<uint32_t,uint32_t>();
		List *l = new List( tag.type );

		for( unsigned i = 0; i < len; i++ ) {
			readTagPayload( tag );
			l->push_back( tag.data );
		}
		return l;
	}

	Compound *readCompound() {
		Compound *comp = new Compound;
		std::wstring name;
		Tag tag;

		while( true ) {
			readNamedTag( name, tag );
			if( tag.type == TAG_End )
				break;
			(*comp)[name] = tag;
		}

		return comp;
	}
};

Compound *readNBT( const char *filename, std::wstring *outerName ) {
	nbtstream is( filename );
	if( is.fileFound() ) {
		Tag tag;
		std::wstring name;
		is.readNamedTag( name, tag );
		assert( tag.type == TAG_Compound );
		if( outerName )
			*outerName = name;
		return tag.data.comp;
	}
	return NULL;
}

Compound *readFromRegionFile( const char *filename, unsigned idx ) {
	nbtstream is( filename, idx );
	if( is.fileFound() ) {
		Tag tag;
		std::wstring name;
		is.readNamedTag( name, tag );
		assert( tag.type == TAG_Compound );
		return tag.data.comp;
	}
	return NULL;
}

Compound *readFromRegionFileSector( const char *filename, unsigned sector ) {
	return readFromRegionFile( filename, sector<<12 );
}

class gzostream {
public:
	gzostream( const char *fn )
	{
		out = gzopen( const_cast<char*>(fn), "wb" );
	}
	~gzostream() {
		if( out )
			gzclose( out );
	}

	void put(char ch) { write(ch); }
	template< typename T >
	void write( T data ) {
		write( &data, sizeof(T) );
	}
	template< typename T >
	void writelli( T data ) { // endian swap for integer types
    write(bswap_to_big(data));
	}

	void write( void *src, size_t sz ) {
		gzwrite( out, src, (unsigned)sz );
	}

private:
	gzFile out;
};


class nbtostream : public gzostream {
public:
	explicit nbtostream( const char *fn )
		: gzostream(fn)
	{
	}
	~nbtostream() { }

	void writeNamedTag( const std::wstring &name, const Tag &tag ) {
		write( (unsigned char)tag.type );
		if( tag.type != TAG_End ) {
			//asdf
			writeString( name );
			writeTagPayload( tag );
		}
	}

private:
	void writeTagPayload( const Tag &tag ) {
		switch( tag.type ) {
		case TAG_Byte:   write( (uint8_t)tag.data.b ); break;
		case TAG_Short:  writelli( (uint16_t)tag.data.s ); break;
		case TAG_Float:
		case TAG_Int:    writelli( (uint32_t)tag.data.i ); break;
		case TAG_Double:
		case TAG_Long:   writelli( (uint64_t)tag.data.l ); break;
		case TAG_Byte_Array:
			writelli( (int32_t)tag.extra );
			write( tag.data.bytes, tag.extra );
			break;
		case TAG_String:
			writeString( *tag.data.str );
			break;
		case TAG_List:
			writeList( tag.data.list );
			break;
		case TAG_Compound:
			writeCompound( tag.data.comp );
			break;
		case TAG_Int_Array:
			writelli( (int32_t)tag.extra );
			for( unsigned i = 0; i < tag.extra; i++ )
				writelli( (uint32_t)tag.data.ia[i] );
			break;
		default:
			assert( false );
		}
	}

	void writeString( const std::wstring &str ) {
		if( str.empty() ) {
			writelli( (uint16_t)0 );
			return;
		}

		char *b = new char[str.length()*4+1];
		size_t len = wcstombs( b, str.c_str(), str.length()*4+1 );
		writelli( (uint16_t)len );
		write( b, len );
		delete[] b;
	}

	void writeList( const List *l ) {
		write( (unsigned char)l->getType() );
		writelli( (uint32_t)l->size() );

		Tag tag;
		tag.type = l->getType();
		for( List::const_iterator it = l->begin(); it != l->end(); ++it ) {
			tag.data = *it;
			writeTagPayload( tag );
		}
	}

	void writeCompound( const Compound *comp ) {
		for( Compound::const_iterator it = comp->begin(); it != comp->end(); ++it )
			writeNamedTag( it->first, it->second );
		Tag endTag;
		endTag.type = TAG_End;
		writeNamedTag( L"", endTag );
	}
};

void Compound::write( const char *filename, const std::wstring &outerName ) {
	nbtostream nbtOut( filename );
	Tag me;
	me.type = TAG_Compound;
	me.data.comp = this;
	nbtOut.writeNamedTag( outerName, me );
}

} //namespace nbt
