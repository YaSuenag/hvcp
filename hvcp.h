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
#pragma once

#include <Windows.h>

#include "hvcp-common.h"

#ifndef DECLSPEC
#define DECLSPEC dllimport
#endif

// Error codes
enum HVCPErrorCode {
	ERR_GETVMID_ENUM_SYSTEMS = 1,
	ERR_GETVMID_HCS_RESULT,
	ERR_GETVMID_JSON_PARSE,
	ERR_GETVMID_CLSID,

	ERR_CONNECT_WSA_STARTUP,
	ERR_CONNECT_SOCKET,
	ERR_CONNECT_CONNECT,

	ERR_SEND_DATA,
	ERR_INALID_REMOETE_PATH,
	ERR_COPY_FILE_TO_GUEST_SRC_OPEN,
	ERR_COPY_FILE_TO_GUEST,
	ERR_COPY_FILE_TO_GUEST_SEND_FILE,
	ERR_COPY_FILE_FROM_GUEST_DEST_CREATE,
	ERR_COPY_FILE_FROM_GUEST,
	ERR_COPY_FILE_FROM_GUEST_WRITE_FILE,

	ERR_GET_CMD_RESULT,
	ERR_GET_CMD_RESULT_INSUFFICIENT_DATA
};


extern "C" __declspec(DECLSPEC) BOOL WINAPI CopyFileToGuest(LPCWSTR UserName, LPCWSTR VMName, LPCWSTR LocalSrcPath, LPCWSTR RemoteDestPath);
extern "C" __declspec(DECLSPEC) BOOL WINAPI CopyFileFromGuest(LPCWSTR VMName, LPCWSTR RemoteSrcPath, LPWSTR LocalDestPath);

extern "C" __declspec(DECLSPEC) LARGE_INTEGER WINAPI GetHVCPLastError();