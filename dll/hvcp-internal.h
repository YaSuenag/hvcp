#pragma once

#include "../hvcp.h"


extern __declspec(thread) HVCPErrorCode errcode;
extern __declspec(thread) int errdetails;


SOCKET ConnectToGuest(LPCWSTR VMName);
bool SetUserName(const SOCKET sock, LPCWSTR UserName);
bool CopyFileToGuestInternal(const SOCKET sock, LPCWSTR LocalSrcPath, LPCWSTR RemoteDestPath);
bool CopyFileFromGuestInternal(const SOCKET sock, LPCWSTR RemoteSrcPath, LPWSTR LocalDestPath);
void Disconnect(SOCKET sock);
