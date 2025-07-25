/*
 * Copyright 2025 Yasumasa Suenaga
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "pch.h"

#define DECLSPEC dllexport
#include "hvcp-internal.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

BOOL CopyFileToGuest(LPCWSTR UserName, LPCWSTR VMName, LPCWSTR LocalSrcPath, LPCWSTR RemoteDestPath)
{
	auto sock = ConnectToGuest(VMName);
    if (sock == INVALID_SOCKET) {
        return FALSE;
    }
    
    BOOL result = FALSE;
    __try {
        if (SetUserName(sock, UserName)) {
            result = CopyFileToGuestInternal(sock, LocalSrcPath, RemoteDestPath);
        }
    }
    __finally {
        Disconnect(sock);
    }

    return result;
}

BOOL CopyFileFromGuest(LPCWSTR VMName, LPCWSTR RemoteSrcPath, LPWSTR LocalDestPath)
{
    auto sock = ConnectToGuest(VMName);
    if (sock == INVALID_SOCKET) {
        return FALSE;
    }

	BOOL result = CopyFileFromGuestInternal(sock, RemoteSrcPath, LocalDestPath);
	Disconnect(sock);
	return result;
}

LARGE_INTEGER GetHVCPLastError()  
{
    LARGE_INTEGER result;
    result.LowPart = errcode;  
    result.HighPart = errdetails;  
    return result;  
}