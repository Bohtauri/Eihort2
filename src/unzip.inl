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
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies, 
either expressed or implied, of the FreeBSD Project. */


#include <algorithm>
#include <cstring>

namespace zip {

class Unzip::Iterator
	/* : public std::iterator<std::forward_iterator_tag, const Iterator> */ {

 public:

 	inline Iterator()
		: parent_(NULL)
		, entry_(NULL)
	{}

	inline Iterator(const Iterator& other)
		: parent_(other.parent_)
		, entry_(other.entry_)
	{}

 	inline explicit Iterator(Unzip& parent)
		: parent_(&parent)
		, entry_(parent.entries_)
	{}

	inline ~Iterator()
	{}

	inline Iterator& operator=(const Iterator& other)
	{
		Iterator(other).swap(*this);
		return *this;
	}

	inline void swap(Iterator& other)
	{
		std::swap(parent_, other.parent_);
		std::swap(entry_, other.entry_);
	}

	Iterator& operator++();

	inline Iterator operator++(int)
	{
		Iterator other(*this);
		++*this;
		return other;
	}

	inline friend bool operator==(const Iterator& left, const Iterator& right)
	{ return left.entry_ == right.entry_; }

	inline friend bool operator!=(const Iterator& left, const Iterator& right)
	{ return !(left == right); }

	inline const Iterator *operator->() const
	{ return this; }

	const std::string& filename() const;

	std::size_t uncompressed_size() const;
	std::size_t uncompress(void *) const;

	std::size_t comp_size() const;
	std::size_t start() const;
	bool is_stored() const;

 private:

 	Unzip *parent_;
	Unzip::Entry_ *entry_;

}; // Unzip::Iterator

inline
bool
Unzip::good() const
{ return error_.empty(); }

inline
const std::string&
Unzip::error() const
{ return error_; }

inline
Unzip::Iterator
Unzip::begin()
{ return Iterator(*this); }

inline
Unzip::Iterator
Unzip::end()
{ return Iterator(); }

inline
Unzip::Iterator
Unzip::find(const char *filename)
{
	for (Iterator it = begin(); it != end(); ++it)
		if (it->filename() == filename)
			return it;
	return end();
}

} // zip
