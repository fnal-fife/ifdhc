#ifndef CHECKSUM_HPP
#define CHECKSUM_HPP

#include <string>

namespace checksum {

  class ChecksumError : public std::exception {
  public:
    ChecksumError(const std::string& msg="");
    virtual const char* what() const throw();
    virtual ~ChecksumError() throw();
  protected:
    std::string _msg;
  };

  class InvalidChecksum : public ChecksumError {
  public:
    InvalidChecksum(const std::string& msg) : ChecksumError(msg) {}
  };

  const int charbuffersize = 11; // largest buffer needed to hold the checksum

  unsigned long get_adler32(const std::string& filename);

  int get_adler32(const std::string& filename, char* buf, size_t len); // return number of bytes written
  std::string get_adler32_str(const std::string& filename);

  bool compare_crcs(const std::string& source, const std::string& dest);
  bool compare_crcs(const char* source, const char* dest);
  bool compare_crcs(const char* source, size_t slen, const char* dest, size_t dlen);
  
  bool compare_adler32(const std::string& filename, const std::string& crcvalue);
  bool compare_adler32(const std::string& filename, const char* adler);
  bool compare_adler32(const std::string& filename, const char* adler, size_t len);

}

#endif
