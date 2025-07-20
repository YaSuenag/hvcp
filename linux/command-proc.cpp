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
#include <liburing.h>

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
  if(fd == -1){
    throw ERR_RECEIVE_FILE;
  }

  int pipe_fds[2];
  pipe(pipe_fds);
  int pipe_sz = fcntl(pipe_fds[1], F_GETPIPE_SZ);
  long page_sz = sysconf(_SC_PAGESIZE);

  // HV transfer size = 1 page (assumes)
  // num pages in pipes = pipe_sz / 4KB
  unsigned int queue_depth = pipe_sz / page_sz;
  struct io_uring ring;
  io_uring_queue_init(queue_depth, &ring, 0);
  struct io_uring_sqe *sqe;

  size_t num_requests = len / page_sz;
  size_t mod_size = len % page_sz;
  while(num_requests > 0){
    unsigned int n;
    // sock -> pipe
    for(n = 0; n < queue_depth && num_requests > 0; n++, num_requests--){
      sqe = io_uring_get_sqe(&ring);
      io_uring_prep_splice(sqe, sock, -1, pipe_fds[1], -1, page_sz, 0);
    }
    io_uring_submit_and_wait(&ring, n);
    io_uring_cq_advance(&ring, n);

    // pipe -> fd
    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_splice(sqe, pipe_fds[0], -1, fd, -1, n * page_sz, 0);
    io_uring_submit_and_wait(&ring, 1);
    io_uring_cq_advance(&ring, 1);
  }

  // Copy remaining data...
  if(mod_size > 0){
    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_splice(sqe, sock, -1, pipe_fds[1], -1, mod_size, 0);
    io_uring_sqe_set_flags(sqe, IOSQE_IO_LINK);

    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_splice(sqe, pipe_fds[0], -1, fd, -1, mod_size, 0);

    io_uring_submit_and_wait(&ring, 2);
    io_uring_cq_advance(&ring, 2);
  }

  io_uring_queue_exit(&ring);
  close(pipe_fds[0]);
  close(pipe_fds[1]);

  fchown(fd, target_uid, target_gid);
  close(fd);
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
        close(fd);
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
          shutdown(sock, SHUT_RD);
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
          shutdown(sock, SHUT_RD);
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
            if(strlen(path) + strlen(filename) < sizeof(path)){
              strncat(path, filename, sizeof(path) - strlen(path) - 1);
              std::cout << "remote path: " << path << std::endl;
            } else {
              throw ERR_INVALID_REMOTE_PATH;
            }
          }
          send_result(HVCP_CMD_RESULT_OK, nullptr, 0);
        }
        catch(HVCPServerErrorCode &e){
          shutdown(sock, SHUT_RD);
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
          shutdown(sock, SHUT_RD);
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
          shutdown(sock, SHUT_RD);
          send_result(HVCP_CMD_RESULT_FILETRANSFER_ERROR, nullptr, 0);
        }
        break;

      default:
        std::cerr << "Unknown command(" << cmd << "): client loop was aborted." << std::endl;
        return;

    }
  }
}
