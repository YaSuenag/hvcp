#include <cstring>
#include <iostream>

#include <libgen.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sendfile.h>

#include "../hvcp-common.h"
#include "command-proc.h"


HVCPCommandProc::HVCPCommandProc(const int clsock) : sock(clsock){
  reset();
}

void HVCPCommandProc::reset(){
  path[0] = '\0';
  target_uid = geteuid();
  target_gid = getegid();
}

void HVCPCommandProc::send_result(HVCPCommandResult result, char *data, size_t len){
  send(sock, reinterpret_cast<char*>(&result), sizeof(result), 0);
  send(sock, reinterpret_cast<char*>(&len), sizeof(len), 0);
  if(len > 0){
    send(sock, data, len, 0);
  }
}

void HVCPCommandProc::recv_string(char *str, const size_t len){
  if(recv(sock, str, len, MSG_WAITALL) != static_cast<ssize_t>(len)){
    throw ERR_RECV_STRING;
  }
  str[len] = '\0';
}

std::tuple<uid_t, gid_t> HVCPCommandProc::recv_user_info(const size_t len){
  if(len == 0){
    return { geteuid(), getegid() };
  }

  char *username = new char[len + 1];
  try{
    recv_string(username, len);
    username[len] = '\0';
  }
  catch(HVCPServerErrorCode &e){
    delete[] username;
    throw e;
  }
  
  long passwd_buflen = sysconf(_SC_GETPW_R_SIZE_MAX);
  char *passwd_buf = new char[passwd_buflen];
  struct passwd pwd, *pwd_result;
  int result = getpwnam_r(username, &pwd, passwd_buf, passwd_buflen, &pwd_result);
  delete[] username;
  if(result != 0){
    delete[] passwd_buf;
    throw ERR_USER_NOT_FOUND;
  }

  uid_t target_uid = pwd.pw_uid;
  uid_t target_gid = pwd.pw_gid;

  delete[] passwd_buf;
  return { target_uid, target_gid };
}

void HVCPCommandProc::recv_remote_path(const size_t len){
  if(len > PATH_MAX){
    throw ERR_INVALID_REMOTE_PATH;
  }

  recv_string(path, len);
  path[len] = '\0';

  /* Directory check */
  struct stat st_buf;
  if((stat(path, &st_buf) == 0) && S_ISDIR(st_buf.st_mode)){
    if(path[len - 1] != '/'){
      path[len] = '/';
      path[len + 1] = '\0';
    }
  }
  else{
    char path_for_dirname[PATH_MAX];
    strcpy(path_for_dirname, path);
    if(stat(dirname(path_for_dirname), &st_buf) == -1){
      throw ERR_INVALID_REMOTE_PATH;
    }
  }
}

void HVCPCommandProc::recv_file(const size_t len){
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  size_t remains = len;
  char buf[BUF_SZ];
  do{
    // Read data from socket even if fd == -1 to discard data in socket.
    long received = recv(sock, buf, std::min(sizeof(buf), remains), 0);
    if(received > 0){
      if(fd != -1){
        write(fd, buf, received);
      }
      remains -= received;
    }
    else if(received == -1){
      close(fd);
      throw ERR_RECEIVE_FILE;
    }
  } while(remains > 0);

  if(fd == -1){
    throw ERR_RECEIVE_FILE;
  }
  else{
    fchown(fd, target_uid, target_gid);
    close(fd);
  }
}

void HVCPCommandProc::send_file(){
  int fd = open(path, O_RDONLY, 0);
  if(fd == -1){
    throw ERR_SEND_FILE;
  }

  struct stat64 st_buf;
  if(fstat64(fd, &st_buf) == 0){
    auto result_cmd = HVCP_CMD_RESULT_OK;
    auto fsize = st_buf.st_size;
    send(sock, reinterpret_cast<char*>(&result_cmd), sizeof(result_cmd), 0);
    send(sock, reinterpret_cast<char*>(&fsize), sizeof(fsize), 0);

    // https://man7.org/linux/man-pages/man2/sendfile.2.html
    constexpr size_t sendfile_limit = 0x7ffff000;
    size_t remains = fsize;
    off64_t ofs = 0;
    do{
      size_t to_write = std::min(remains, sendfile_limit);
      ssize_t written = sendfile64(sock, fd, &ofs, to_write);
      if(written == -1){
        throw ERR_SEND_FILE;
      }
      remains -= written;
    } while (remains > 0);

    close(fd);
    return;
  }

  // Error occurred
  close(fd);
  throw ERR_SEND_FILE;
}

void HVCPCommandProc::command_loop(){
  while(1){
    HVCPCommand cmd;
    long len, recv_ret;
    recv_ret = recv(sock, &cmd, sizeof(cmd), MSG_WAITALL);
    if(recv_ret == 0){
      /* connection was closed by peer */
      break;
    }
    else if(recv_ret == -1){
      perror("Error: recv: ");
      break;
    }

    if(recv(sock, &len, sizeof(len), MSG_WAITALL) <= 0){
      perror("Error: recv: ");
      break;
    }

    switch(cmd){
      case HVCP_CMD_SET_USER:
        try{
          std::tie(target_uid, target_gid) = recv_user_info(len);
          std::cout << "uid=" << target_uid << ", gid=" << target_gid << std::endl;
          send_result(HVCP_CMD_RESULT_OK, nullptr, 0);
        }
        catch(HVCPServerErrorCode &e){
          send_result(HVCP_CMD_RESULT_UNKNOWN_USER, nullptr, 0);
        }
        break;

      case HVCP_CMD_SET_REMOTE_PATH:
        try{
          recv_remote_path(len);
          std::cout << "remote path: " << path << std::endl;
          send_result(HVCP_CMD_RESULT_OK, nullptr, 0);
        }
        catch(HVCPServerErrorCode &e){
          send_result(HVCP_CMD_RESULT_INVALID_PATH, nullptr, 0);
        }
        break;

      case HVCP_CMD_SET_FILE_NAME:
        try{
          if(len > PATH_MAX){
            throw ERR_INVALID_REMOTE_PATH;
          }
          char filename[PATH_MAX];
          recv_string(filename, len);
          std::cout << "filename: " << filename << std::endl;
          if(path[strlen(path) - 1] == '/'){
            strcat(path, filename);
            std::cout << "remote path: " << path << std::endl;
          }
          send_result(HVCP_CMD_RESULT_OK, nullptr, 0);
        }
        catch(HVCPServerErrorCode &e){
          send_result(HVCP_CMD_RESULT_INVALID_PATH, nullptr, 0);
        }
        break;

      case HVCP_CMD_COPY_FILE_TO_GUEST:
        try{
          recv_file(len);
          std::cout << "received: " << path << std::endl;
          send_result(HVCP_CMD_RESULT_OK, nullptr, 0);
        }
        catch(HVCPServerErrorCode &e){
          send_result(HVCP_CMD_RESULT_FILETRANSFER_ERROR, nullptr, 0);
        }
        break;

      case HVCP_CMD_COPY_FILE_FROM_GUEST:
        try{
          // Send response in send_file() if it succeeded
          send_file();
          std::cout << "sent: " << path << std::endl;
        }
        catch(HVCPServerErrorCode &e){
          send_result(HVCP_CMD_RESULT_FILETRANSFER_ERROR, nullptr, 0);
        }
        break;

      default:
        std::cerr << "Unknown command(" << cmd << "): client loop was aborted." << std::endl;
        return;

    }
  }
}
