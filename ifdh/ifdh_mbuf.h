
namespace ifdh_ns {

class ifdh_op_msg {
public:
   ifdh &handle;
   std::string operation; // cp, rm, ls, etc.
   uuid_t uuid;           // uuid for this message set
   int ctime;            // start time
   std::string src;     // path 
   std::string dst;     // second path, for copy
   std::string proto;    // protocol
   std::string door;     // door
   int count;            // bytes transferred/files listed
   ifdh_op_msg(std::string op, class ifdh &ih);        
   void log_start();
   void log_retry();
   void log_failure();
   void log_success();
   void log_common(std::stringstream &message);
};

}
