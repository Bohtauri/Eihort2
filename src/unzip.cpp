/* Copyright (c) 2012, Antti Hakkinen
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


#include "unzip.h"
#include <cassert>
#include <zlib.h>

#include "endian.h"
#include "stdint.h"

namespace zip {

namespace {

template<class T>
inline static
T 
bswap_from_le_(T x)
{
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	return x;
#else
	return bswap(bswap_to_big(x));
#endif
}

static const uint32_t SIG_DIRREC_ = 0x02014b50ul,
	SIG_DIREND_ = 0x06054b50ul,
	SIG_FILEHEAD_ = 0x04034b50ul;

static const uint16_t METHOD_STORED_ = 0x00,
	METHOD_DEFLATED_ = 0x08;

} // <anonymous>

struct Unzip::Entry_ {

	Entry_ *next_;
	uint32_t head_pos_, pos_;
	std::string filename_;
	uint16_t method_;
	uint32_t comp_size_,
		uncomp_size_;

	// these were supposed to be methods of the class Unzip;
		// however, it would require that they were present in the
		// interface so i'd rather hide this crap here

	inline static bool error_(Unzip *self, const std::string& msg)
	{
		self->error_ = msg;
		return false;
	}

	template<class Type>
	inline static bool read_(Unzip *self, Type& value)
	{ return self->file_.read(reinterpret_cast<char *>(&value), sizeof(value)).good(); }

	static bool scan_endofdir_(Unzip *self, uint32_t size)
	{
		Entry_ **entries = &self->entries_;
		uint32_t sig;
		std::streampos lastpos;
		(void)size;
		for (;;)
		{
			lastpos = self->file_.tellg();
			Entry_ entry;
			uint16_t ver_creat,
				ver_need,
				flags,
				mod_time,
				mod_date;
			uint32_t crc32;
			uint16_t filename_size,
				extra_size,
				comment_size,
				start_disk,
				attr_in;
			uint32_t attr_ex,
				head_offset;
			if (!read_(self, sig) || bswap_from_le_(sig) != SIG_DIRREC_)
				return true;
			if (!read_(self, ver_creat) ||
					!read_(self, ver_need) ||
					!read_(self, flags) ||
					!read_(self, entry.method_) ||
					!read_(self, mod_time) ||
					!read_(self, mod_date) ||
					!read_(self, crc32) ||
					!read_(self, entry.comp_size_) ||
					!read_(self, entry.uncomp_size_) ||
					!read_(self, filename_size) ||
					!read_(self, extra_size) ||
					!read_(self, comment_size) ||
					!read_(self, start_disk) ||
					!read_(self, attr_in) ||
					!read_(self, attr_ex) ||
					!read_(self, head_offset))
				return error_(self, "read error");
			entry.filename_.assign(filename_size, '\0');
			if (!self->file_.read(&*entry.filename_.begin(), filename_size))
				return error_(self, "read error");
			if (!self->file_.seekg(bswap_from_le_(extra_size) + 
					bswap_from_le_(comment_size), std::ios::cur))
				return error_(self, "seek error");
			entry.next_ = *entries;
			entry.head_pos_ = bswap_from_le_(head_offset);
			entry.pos_ = 0;
			entry.method_ = bswap_from_le_(entry.method_);
			entry.comp_size_ = bswap_from_le_(entry.comp_size_);
			entry.uncomp_size_ = bswap_from_le_(entry.uncomp_size_);
			*entries = new Entry_(entry);
			entries = &(*entries)->next_;
		}
	}

	static bool find_endofdir_(Unzip *self)
	{
		uint32_t sig;
		if (!self->file_.seekg(-int(sizeof(sig)), std::ios_base::end))
			return error_(self, "failed to seek");
		do
			if (!read_(self, sig))
				return error_(self, "failed to read");
			else if (bswap_from_le_(sig) == SIG_DIREND_)
			{
				uint16_t this_disk,
					start_disk,
					this_rec,
					total_rec;
				uint32_t dir_size,
					dir_offset;
				if (!read_(self, this_disk) ||
						!read_(self, start_disk) ||
						!read_(self, this_rec) ||
						!read_(self, total_rec) || 
						!read_(self, dir_size) || 
						!read_(self, dir_offset))
					return error_(self, "failed to read");
				if (this_disk != start_disk ||
						this_rec != total_rec)
					return error_(self, "multipart archive - not supported");
				if (!self->file_.seekg(bswap_from_le_(dir_offset), std::ios_base::beg))
					return error_(self, "failed to seek");
				return scan_endofdir_(self, bswap_from_le_(dir_size));
			}
		while (self->file_.seekg(-int(sizeof(sig)) - 1, std::ios_base::cur));
		return error_(self, "central directory not found - not a zip file?");
	}

	static bool find_entry_(Unzip *self, Entry_ *entry)
	{
		if (!self->file_.seekg(entry->head_pos_, std::ios_base::beg))
			return error_(self, "failed to seek");
		uint32_t sig;
		if (!read_(self, sig) || bswap_from_le_(sig) != SIG_FILEHEAD_)
			return error_(self, "invalid file header");
		uint16_t ver_need,
			flags,
			method,
			mod_time,
			mod_date;
		uint32_t crc32,
			comp_size,
			uncomp_size;
		uint16_t filename_size,
			extra_size;
		if (!read_(self, ver_need) ||
				!read_(self, flags) ||
				!read_(self, method) ||
				!read_(self, mod_time) ||
				!read_(self, mod_date) ||
				!read_(self, crc32) ||
				!read_(self, comp_size) ||
				!read_(self, uncomp_size) ||
				!read_(self, filename_size) ||
				!read_(self, extra_size))
			return error_(self, "failed to read");
		if (!self->file_.seekg(bswap_from_le_(filename_size) +
				bswap_from_le_(extra_size), std::ios_base::cur))
			return error_(self, "failed to seek");
		entry->pos_ = (uint32_t)self->file_.tellg();
		return true;
	}

	static bool seek_entry_(Unzip *self, Entry_ *entry)
	{
		if (entry->pos_ == 0 && !find_entry_(self, entry))
			return false;
		else if (!self->file_.seekg(entry->pos_, std::ios_base::beg))
			return error_(self, "failed to seek");
		return true;
	}

	static bool uncompress_stored_(Unzip *self, Entry_ *entry, void *buffer)
	{
		assert(entry->comp_size_ == entry->uncomp_size_);
		if (!self->file_.read(reinterpret_cast<char *>(buffer), entry->comp_size_))
			return error_(self, "failed to read");
		return true;
	}

	static bool uncompress_deflated_(Unzip *self, Entry_ *entry, void *outbuf)
	{
		std::string inbuf(entry->comp_size_, '\0');
		if (!self->file_.read(&*inbuf.begin(), entry->comp_size_))
			return error_(self, "failed to read");
		z_stream z;
		z.avail_in = entry->comp_size_;
		z.next_in = reinterpret_cast<Bytef *>(&*inbuf.begin());
		z.zalloc = Z_NULL;
		z.zfree = Z_NULL;
		if (inflateInit2(&z, -MAX_WBITS) != Z_OK)
			return error_(self, "failed to initialize inflate stream");
		z.avail_out = entry->uncomp_size_;
		z.next_out = reinterpret_cast<Bytef *>(outbuf);
		int err = inflate(&z, Z_FINISH);
		if (inflateEnd(&z) != Z_OK || err != Z_STREAM_END)
			return error_(self, "failed to inflate file");
		return true;
	}

	static bool uncompress_(Unzip *self, Entry_ *entry, void *buffer)
	{
		if (!seek_entry_(self, entry))
			return false;
		if (entry->method_ == METHOD_STORED_)
			return uncompress_stored_(self, entry, buffer);
		else if (entry->method_ == METHOD_DEFLATED_)
			return uncompress_deflated_(self, entry, buffer);
		else
			return error_(self, "failed to decompress - method not supported");
	}

}; // Unzip::Entry_

Unzip::Iterator&
Unzip::Iterator::operator++()
{
	assert(entry_ != NULL);
	entry_ = entry_->next_;
	return *this;
}

const std::string&
Unzip::Iterator::filename() const
{
	assert(entry_ != NULL);
	return entry_->filename_;
}

std::size_t
Unzip::Iterator::uncompressed_size() const
{
	assert(entry_ != NULL);
	return std::size_t(entry_->uncomp_size_);
}

std::size_t
Unzip::Iterator::uncompress(void *buffer) const
{
	assert(parent_ != NULL && entry_ != NULL && buffer != NULL);
	if (!Unzip::Entry_::uncompress_(parent_, entry_, buffer))
		return 0;
	return entry_->uncomp_size_;
}

std::size_t
Unzip::Iterator::comp_size() const
{
	return entry_->comp_size_;
}

std::size_t
Unzip::Iterator::start() const
{
	if (entry_->pos_ == 0 && !Unzip::Entry_::find_entry_(parent_, entry_))
		return 0;
	return entry_->pos_;
}

bool
Unzip::Iterator::is_stored() const
{
	return entry_->method_ == METHOD_STORED_;
}

Unzip::Unzip(const char *filename)
	: file_(filename, std::ios::in | std::ios::binary)
	, entries_(NULL)
	, error_()
{
	if (!file_.good())
		error_ = (std::string("failed to open file ") += filename) += "\"";
	else
		Unzip::Entry_::find_endofdir_(this);
}

Unzip::~Unzip()
{
	for (Entry_ *it = entries_; it != NULL;)
	{
		Entry_ *next = it->next_;
		delete it;
		it = next;
	}
}

} // zip
