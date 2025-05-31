#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "../hvcp-common.h"
#include "command-proc.h"


static void accept_client(const int server_sock){
  int cl_sock;
  struct sockaddr_vm addr;
  socklen_t addrlen = sizeof(addr);

  std::cout << "Ready" << std::endl;

  while((cl_sock = accept(server_sock, reinterpret_cast<struct sockaddr*>(&addr), &addrlen)) != -1){
    std::cout << "Accepted from " << std::hex << std::showbase << addr.svm_cid << std::endl;

    HVCPCommandProc cmdproc(cl_sock);
    cmdproc.command_loop();
    close(cl_sock);
  }

  perror("Error: accept: ");
}

int main(){
  int sock;
  struct sockaddr_vm addr = {0};
  socklen_t addrlen = sizeof(struct sockaddr_vm);
  int result;

  addr.svm_family = AF_VSOCK;
  addr.svm_cid = VMADDR_CID_ANY;
  addr.svm_port = 50500;

  sock = socket(AF_VSOCK, SOCK_STREAM, 0);
  if(sock == -1){
    perror("socket");
    return -1;
  }

  result = bind(sock, (const struct sockaddr*)&addr, addrlen);
  if(result == -1){
    perror("bind");
    return -2;
  }

  result = listen(sock, 1);
  if(result == -1){
    perror("listen");
    return -3;
  }

  unsigned int cid;
  ioctl(sock, IOCTL_VM_SOCKETS_GET_LOCAL_CID, &cid);
  std::cout << "CID: " << std::hex << std::showbase << cid << std::endl;

  accept_client(sock);

  close(sock);
  return 1;
}
