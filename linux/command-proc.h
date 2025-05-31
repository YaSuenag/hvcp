#ifndef COMMAND_PROC_H
#define COMMAND_PROC_H

#include <climits>
#include <tuple>

#include <sys/types.h>


#define BUF_SZ 2097152  // 2MB


enum HVCPServerErrorCode{
  ERR_RECV_STRING,
  ERR_USER_NOT_FOUND,
  ERR_INVALID_REMOTE_PATH,
  ERR_RECEIVE_FILE,
  ERR_SEND_FILE
};

class HVCPCommandProc{

  private:
    int sock;
    char path[PATH_MAX];
    uid_t target_uid;
    gid_t target_gid;

    void reset();
    void send_result(HVCPCommandResult result, char *data, size_t len);
    void recv_string(char *str, const size_t len);
    std::tuple<uid_t, gid_t> recv_user_info(const size_t len);
    void recv_remote_path(const size_t len);
    void recv_file(const size_t filelen);
    void send_file();

  public:
    HVCPCommandProc(const int clsock);
    virtual ~HVCPCommandProc(){};

    void command_loop();
};

#endif
