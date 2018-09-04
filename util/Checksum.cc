
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
#include "md5.h"

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

  unsigned long convert_0_adler32_to_1_adler32(long int crc, long int filesize) {
      const long int BASE = 65521;
      long int size;
      long int s1, s2;
      
      size = filesize % BASE;
      s1 = crc & 0xffff;
      s2 = ((crc >> 16) & 0xffff);
      s1 = (s1 + 1) % BASE;
      s2 = (size + s2 ) % BASE;
      return (s2 << 16) + s1;
  }

  void
  get_sums(const std::string& filename, unsigned long* adler_32_0, unsigned long* adler_32_1, unsigned char *md5digest) {
    const size_t buffersize=4096*256;
    long int filesize;
    md5_state_t ms;

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
    md5_init(&ms);
    filesize = 0;
    while ( (length = read(fd,&buffer[0],buffer.size())) > 0 ) {
      if (length < 0) {
        std::string msg("Error reading file ");
        msg+=filename;
        close(fd);
        throw ChecksumSysError(msg,errno);
      }
      filesize += length;
      md5_append(&ms, &buffer[0], length);
      adler = adler32(adler,&buffer[0],length);
    }
    close(fd);
    if (md5digest) {
        md5_finish(&ms, md5digest);
    }
    if (adler_32_0) {
        *adler_32_0 = adler;
    }
    if (adler_32_1) {
        *adler_32_1 = convert_0_adler32_to_1_adler32(adler, filesize);
    }
    return;
  }


  int get_adler32(const std::string& filename, char* buf, size_t len) {
    unsigned long adler;
    get_sums(filename, &adler, 0, 0);
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
