
#include <vector>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
//#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <zlib.h>
#include "../util/Checksum.h"
#include <stdio.h>

namespace checksum {

  class ChecksumSysError : public ChecksumError {
  public:
    ChecksumSysError(const std::string& msg, int err)
      : ChecksumError(msg) {
      char buf[128];
      if (strerror_r(err,buf,sizeof(buf)) == 0) {
	_msg += ": ";
	_msg += buf;
      }
    }
  };

  ChecksumError::ChecksumError(const std::string& msg) : _msg(msg) {}
  const char* ChecksumError::what() const throw() { return _msg.c_str(); }
  ChecksumError::~ChecksumError() throw() {}

  unsigned long get_adler32(const std::string& filename) {
    const size_t buffersize=4096*256;

    int fd = open(filename.c_str(),O_RDONLY);
    if (fd < 0) {
      std::string msg("Unable to open file ");
      msg+=filename;
      throw ChecksumSysError(msg,errno);
    }
#if defined(POSIX_FADV_SEQUENTIAL)
    posix_fadvise(fd,0,0,POSIX_FADV_SEQUENTIAL); // may speed up reading sequentially
#endif
    std::vector<Bytef> buffer(buffersize);
    ssize_t length;
    unsigned long adler = 0L; // seed value for enstore compatibility
    while ( (length = read(fd,&buffer[0],buffer.size())) > 0 ) {
      if (length < 0) {
        std::string msg("Error reading file ");
        msg+=filename;
        close(fd);
        throw ChecksumSysError(msg,errno);
      }
      adler = adler32(adler,&buffer[0],length);
    }
    close(fd);
    return adler;
  }

  /* mmap based alternative implementation - I'm not sure there's really much benefit to this
  unsigned long get_adler32(const std::string& filename) {

    int fd = open(filename.c_str(),O_RDONLY);
    if (fd < 0) {
      std::string msg("Unable to open file ");
      msg+=filename;
      throw ChecksumSysError(msg,errno);
    }

    unsigned long adler = 0L; // seed value for enstore compatibility
    struct stat st;
    if (fstat(fd,&st) != 0) {
      std::string msg("Unable to stat file ");
      msg+=filename;
      close(fd);
      throw ChecksumSysError(msg,errno);
    }
    off_t filesize = st.st_size;
    off_t offset = 0;
    // find the size in pages, rounding up
    size_t page_size = (size_t) sysconf (_SC_PAGESIZE);
    off_t mapchunksize = ((filesize+1)/page_size)*page_size ;
    // if the chunksize won't fit in the virtual memory, shrink it
    while (mapchunksize > std::numeric_limits<size_t>::max()/4) mapchunksize/=2;
    while (offset < filesize) {
      off_t mapsize = std::min(filesize - offset,mapchunksize);
      void* buffer = mmap(0,(size_t)mapsize,PROT_READ,MAP_SHARED,fd,offset);
      if (buffer == MAP_FAILED) {
	if (errno == ENOMEM) {
	  // won't fit in address space
	  mapchunksize/=2;
	  continue;
	} else {
	  std::string msg("Unable to mmap file ");
	  msg+=filename;
	  close(fd);
	  throw ChecksumSysError(msg,errno);
	}
      }
      madvise(buffer, (size_t)mapsize, MADV_SEQUENTIAL);
      adler = adler32(adler,(Bytef*)buffer,mapsize);
      munmap(buffer,mapsize);
      offset+=mapsize;
    }
    close(fd);
    return adler;
  }
  */

  int get_adler32(const std::string& filename, char* buf, size_t len) {
    unsigned long adler = get_adler32(filename);
    return snprintf(buf, len, "%lu", adler);
  }

  std::string get_adler32_str(const std::string& filename) {
    std::string s(charbuffersize, char());
    int n = get_adler32(filename, &s[0], s.size());
    s.resize(n);
    return s;
  }

  bool compare_adler32(const std::string& filename, const char* adler) {
    size_t len = strlen(adler);
    return compare_adler32(filename, adler, len);
  }
  
  bool compare_adler32(const std::string& filename, const char* adler, size_t len) {
    std::string newadler = get_adler32_str(filename);
    return compare_crcs(adler, len, newadler.c_str(), newadler.size());
  }

  bool compare_adler32(const std::string& filename, const std::string& adlervalue) {
    return compare_adler32(filename, adlervalue.c_str(), adlervalue.size());
  }

  bool compare_crcs(const std::string& source, const std::string& dest) {
    return compare_crcs(source.c_str(), source.size(), dest.c_str(), dest.size());
  }
  
  bool compare_crcs(const char* source, const char* dest) {
    size_t slen = strlen(source);
    size_t dlen = strlen(dest);
    return compare_crcs(source, slen, dest, dlen);
  }

  bool compare_crcs(const char* source, size_t slen, const char* dest, size_t dlen) {
    if (source==0 || slen==0) throw InvalidChecksum("Source checksum is empty");
    if (dest==0 || dlen==0) throw InvalidChecksum("Destination checksum is empty");
    if (source[slen-1] == 'L') --slen; // ignore stupid trailing L
    if (dest[dlen-1] == 'L') --dlen;
    if (slen != dlen) return false;
    return memcmp(source, dest, slen) == 0;
  }
}
