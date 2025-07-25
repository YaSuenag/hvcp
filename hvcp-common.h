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
#ifndef HVCP_COMMON_H
#define HVCP_COMMON_H

// Commands to communicate with the guest
//  [CMD (4 bytes)][LENGTH (8 bytes)][DATA (length bytes)]
enum HVCPCommand {
	HVCP_CMD_SET_USER = 1,     // User name should be encoded in UTF-8
	HVCP_CMD_SET_REMOTE_PATH,  // Remote path should be encoded in UTF-8
	HVCP_CMD_SET_FILE_NAME,    // File name should be encoded in UTF-8
	HVCP_CMD_COPY_FILE_TO_GUEST,
	HVCP_CMD_COPY_FILE_FROM_GUEST,
	HVCP_CMD_COPY_FILE_TO_GUEST_RECURSIVE,
	HVCP_CMD_COPY_FILE_TO_HOST_RECURSIVE
};

enum HVCPCommandResult {
	HVCP_CMD_RESULT_OK = 0,
	HVCP_CMD_RESULT_UNKNOWN_USER,
	HVCP_CMD_RESULT_INVALID_PATH,
	HVCP_CMD_RESULT_FILETRANSFER_ERROR
};

#define TRANSMIT_BLOCK_SZ 16384 /* 16KB */

#endif
