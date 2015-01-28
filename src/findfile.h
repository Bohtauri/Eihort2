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


#ifndef FINDFILE_H_
#define FINDFILE_H_

#include "platform.h"

# ifdef _WINDOWS
  // Windows

namespace {

  class FindFile {

   public:

    inline explicit FindFile(const char *dirname)
      : handle_(FindFirstFileA(dirname, &data_))
    {}

    inline ~FindFile()
    { close(); }

    void close()
    {
      if (handle_ == INVALID_HANDLE_VALUE)
        return;
      FindClose(handle_);
      handle_ = INVALID_HANDLE_VALUE;
    }

	inline bool isdir() const
	{ return (data_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }

    inline const char *filename() const
    { return handle_ != INVALID_HANDLE_VALUE ? data_.cFileName : NULL; }

    inline const char *next()
    {
      if (FindNextFileA(handle_, &data_) != FALSE)
        return data_.cFileName;
      close();
      return NULL;
    }

   private:

    FindFile(const FindFile&);
    FindFile& operator=(const FindFile&);

    WIN32_FIND_DATAA data_;
    HANDLE handle_;

  }; // FindFile

} // <anonymous>

#elif defined(_POSIX_VERSION) // !_WINDOWS
  // POSIX
# include <glob.h>
# include <sys/stat.h>
# include <unistd.h>

namespace {

  class FindFile {

   public:

    explicit FindFile(const char *dirname)
      : index_(0)
    {
      if (glob(dirname, 0, NULL, &globbuf_) != 0)
      {
        globbuf_.gl_pathc = 0;
        globbuf_.gl_pathv = NULL;
      }
    }

    inline ~FindFile()
    { close(); }

    void close()
    {
      if (globbuf_.gl_pathv != NULL)
        globfree(&globbuf_);
    }

    inline const char *fullname() const
    { return globbuf_.gl_pathv != NULL ? globbuf_.gl_pathv[index_] : NULL; }

    bool isdir() const
    {
      struct stat statbuf;
      if (fullname() != NULL && stat(fullname(), &statbuf) == 0)
        return (statbuf.st_mode & S_IFDIR) != 0;
      return false;
    }

    inline const char *filename()
    {
      const char *result = fullname();
      if (result == NULL)
        return NULL;
      for (const char *it = result; *it != '\0'; ++it)
        if (*it == '/')
          result = &it[1];
      return result;
    }

    inline const char *next()
    {
      if (index_ < globbuf_.gl_pathc)
        ++index_;
      return index_ < globbuf_.gl_pathc ? filename() : NULL;
    }

   private:
    
    FindFile(const FindFile&);
    FindFile& operator=(const FindFile&);
    
    glob_t globbuf_;
    size_t index_;

  }; // FindFile

} // <anonymous>

#else // !_POSIX_VERSION
  // Others
# error: Failed to implement directory listing.

#endif

#endif // FINDFILE_H_
