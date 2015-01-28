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

#ifndef UNZIP_H_
#define UNZIP_H_

#include <cstddef>
#include <fstream>
#include <string>

namespace zip {

	class Unzip {

	 public:

	 	class Iterator; /* {
		 public:
			const std::string& filename() const;
			std::size_t uncompressed_size() const;
			std::size_t uncompress(void *) const;

			std::size_t comp_size() const;
			std::size_t start() const;
			bool is_stored() const;
		}; */

	 	explicit Unzip(const char *);
		~Unzip();

		bool good() const;
		const std::string& error() const;

		Iterator begin();
		Iterator end();

		Iterator find(const char *);

	 private:

	 	Unzip(const Unzip&);
	 	Unzip& operator=(const Unzip&);

	 	struct Entry_;

	 	std::ifstream file_;
		Entry_ *entries_;
		std::string error_;

	}; // Unzip

} // zip
using namespace zip;

#include "unzip.inl"

#endif // UNZIP_H_
