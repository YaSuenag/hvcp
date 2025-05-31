#include "pch.h"

#include "hvcp-internal.h"


__declspec(thread) HVCPErrorCode errcode;
__declspec(thread) int errdetails;


#define HCS_QUERY_FIRST LR"({ "Names": [ ")"
#define HCS_QUERY_LAST  LR"(" ] })"
static CLSID GetVMID(LPCWSTR VMName)
{
    PWSTR query_result;

    /* Get Virtual Machine information via HCS API */
    {
        HCS_OPERATION operation = HcsCreateOperation(nullptr, nullptr);
        if (operation == nullptr) {
            return CLSID_NULL;
        }

        size_t querylen = wcslen(HCS_QUERY_FIRST) + wcslen(VMName) + wcslen(HCS_QUERY_LAST) + 1;
        PWSTR query = new WCHAR[querylen];
        wsprintf(query, HCS_QUERY_FIRST L"%s" HCS_QUERY_LAST, VMName);
        HRESULT hr = HcsEnumerateComputeSystems(query, operation);
        if (FAILED(hr)) {
            errcode = ERR_GETVMID_ENUM_SYSTEMS;
            errdetails = 0;
            delete[] query;
            HcsCloseOperation(operation);
            return CLSID_NULL;
        }

        hr = HcsWaitForOperationResult(operation, INFINITE, &query_result);
        if (FAILED(hr)) {
            errcode = ERR_GETVMID_HCS_RESULT;
            errdetails = 0;
            delete[] query;
            HcsCloseOperation(operation);
            return CLSID_NULL;
        }

        delete[] query;
        HcsCloseOperation(operation);
    }

	/* Extract VMID from HCS query result */
    {
        int query_result_str_len = WideCharToMultiByte(CP_UTF8, 0, query_result, -1, nullptr, 0, nullptr, nullptr) - 1;  // Exclude NUL char
        std::string query_result_str(query_result_str_len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, query_result, -1, &query_result_str[0], query_result_str_len, nullptr, nullptr);
        LocalFree(query_result);

        simdjson::dom::parser parser;
        simdjson::dom::element doc;
        auto err = parser.parse(query_result_str).get(doc);
        if (err) {
            errcode = ERR_GETVMID_JSON_PARSE;
            errdetails = 0;
            return CLSID_NULL;
        }

        auto vmid = simdjson::dom::array(doc).at(0)["Id"].get_string();
        int vmidlen = MultiByteToWideChar(CP_UTF8, 0, vmid.value().data(), vmid.value().size(), nullptr, 0) + 3; // Ensure bracket and NUL char
		PWSTR vmid_w = new WCHAR[vmidlen];
        MultiByteToWideChar(CP_UTF8, 0, vmid.value().data(), vmid.value().size(), &vmid_w[1], vmidlen);
        vmid_w[0] = L'{';
        vmid_w[vmidlen - 2] = L'}';
        vmid_w[vmidlen - 1] = L'\0';

        GUID vmid_guid;
        HRESULT hr = CLSIDFromString(vmid_w, &vmid_guid);
		delete[] vmid_w;
        if (hr != S_OK) {
            errcode = ERR_GETVMID_CLSID;
			errdetails = 0;
            return CLSID_NULL;
        }

        return vmid_guid;
    }
}

SOCKET ConnectToGuest(LPCWSTR VMName) {
    WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        errcode = ERR_CONNECT_WSA_STARTUP;
        errdetails = result;
		return INVALID_SOCKET;
    }

    SOCKET sock = socket(AF_HYPERV, SOCK_STREAM, HV_PROTOCOL_RAW);
    if (sock == INVALID_SOCKET) {
        errcode = ERR_CONNECT_SOCKET;
        errdetails = WSAGetLastError();
        WSACleanup();
		return INVALID_SOCKET;
    }

    __try {
        auto vmid = GetVMID(VMName);
        if (IsEqualCLSID(vmid, CLSID_NULL)) {
            RaiseException(errcode /* errcode should be set in GetVMID() */, 0, 0, nullptr);
        }

        SOCKADDR_HV clientService = { 0 };
        clientService.Family = AF_HYPERV;
        clientService.VmId = vmid;
        clientService.ServiceId = HVCP_SERVICE_ID;
        int result = connect(sock, (SOCKADDR*)&clientService, sizeof(clientService));
        if (result == SOCKET_ERROR) {
            RaiseException(ERR_CONNECT_CONNECT, 0, 0, nullptr);
        }

        return sock;
    }
	__except (EXCEPTION_EXECUTE_HANDLER) {
		errcode = static_cast<HVCPErrorCode>(GetExceptionCode());
		if (errcode == ERR_CONNECT_CONNECT) {
			errdetails = WSAGetLastError();
		}

		closesocket(sock);
		WSACleanup();
		return INVALID_SOCKET;
	}
}

void Disconnect(SOCKET sock) {
	closesocket(sock);
	WSACleanup();
}

static void SendData(const SOCKET sock, HVCPCommand cmd, const char* data, long long data_len) {
    if (send(sock, reinterpret_cast<char*>(&cmd), sizeof(HVCPCommand), 0) == SOCKET_ERROR) {
        RaiseException(ERR_SEND_DATA, 0, 0, nullptr);
    }
    if (send(sock, reinterpret_cast<char*>(&data_len), sizeof(long long), 0) == SOCKET_ERROR) {
        RaiseException(ERR_SEND_DATA, 0, 0, nullptr);
    }
    
    if (data_len > 0) {
        int sent = send(sock, data, data_len, 0);
        if (sent == SOCKET_ERROR) {
            RaiseException(ERR_SEND_DATA, 0, 0, nullptr);
        }
    }
}

static std::tuple<int, long long> GetCommandResult(const SOCKET sock) {
    /* Receive command result */
    int cmd_result;
    int result = recv(sock, reinterpret_cast<char*>(&cmd_result), sizeof(int), MSG_WAITALL);
	if (result == SOCKET_ERROR) {
		RaiseException(ERR_GET_CMD_RESULT, 0, 0, nullptr);
	}
    else if (result != 4) {
		RaiseException(ERR_GET_CMD_RESULT_INSUFFICIENT_DATA, 0, 0, nullptr);
    }

    /* Receive length of result */
    long long len;
    result = recv(sock, reinterpret_cast<char*>(&len), sizeof(long long), MSG_WAITALL);
    if (result == SOCKET_ERROR) {
        RaiseException(ERR_GET_CMD_RESULT, 0, 0, nullptr);
    }
    else if (result != 8) {
        RaiseException(ERR_GET_CMD_RESULT_INSUFFICIENT_DATA, 0, 0, nullptr);
    }

    return { cmd_result, len };
}

bool SetUserName(const SOCKET sock, LPCWSTR UserName) {
    long long len = (UserName == nullptr) ? 0 : WideCharToMultiByte(CP_UTF8, 0, UserName, -1, nullptr, 0, nullptr, nullptr);
	PSTR username_c = nullptr;
    if (len > 0) {
        username_c = new CHAR[len];
        WideCharToMultiByte(CP_UTF8, 0, UserName, -1, username_c, len, nullptr, nullptr);
    }

    bool result = false;
    __try {
		SendData(sock, HVCP_CMD_SET_USER, username_c, len - 1 /* Eliminate NUL char */);
        result = std::get<0>(GetCommandResult(sock)) == HVCP_CMD_RESULT_OK;
    }
	__except (EXCEPTION_EXECUTE_HANDLER) {
        errcode = static_cast<HVCPErrorCode>(GetExceptionCode());
        errdetails = (errcode == ERR_COPY_FILE_TO_GUEST_SRC_OPEN) ? GetLastError() : WSAGetLastError();
	}

    delete[] username_c;
    return result;
}

bool CopyFileToGuestInternal(const SOCKET sock, LPCWSTR LocalSrcPath, LPCWSTR RemoteDestPath) {
    __try {
        /* Send remote path */
        long long len = WideCharToMultiByte(CP_UTF8, 0, RemoteDestPath, -1, nullptr, 0, nullptr, nullptr);
        PSTR remote_dest_path_c = new CHAR[len];
        WideCharToMultiByte(CP_UTF8, 0, RemoteDestPath, -1, remote_dest_path_c, len, nullptr, nullptr);
        __try {
			SendData(sock, HVCP_CMD_SET_REMOTE_PATH, remote_dest_path_c, len - 1 /* Eliminate NUL char */);
			if (std::get<0>(GetCommandResult(sock)) != HVCP_CMD_RESULT_OK) {
				RaiseException(ERR_INALID_REMOETE_PATH, 0, 0, nullptr);
			}
        }
        __finally {
			delete[] remote_dest_path_c;
        }

        /* Send file name */
        len = WideCharToMultiByte(CP_UTF8, 0, LocalSrcPath, -1, nullptr, 0, nullptr, nullptr);
        PSTR local_src_path_c = new CHAR[len];
        WideCharToMultiByte(CP_UTF8, 0, LocalSrcPath, -1, local_src_path_c, len, nullptr, nullptr);
		PSTR fname = PathFindFileNameA(local_src_path_c);
        __try {
            SendData(sock, HVCP_CMD_SET_FILE_NAME, fname, strlen(fname));
            if (std::get<0>(GetCommandResult(sock)) != HVCP_CMD_RESULT_OK) {
                RaiseException(ERR_COPY_FILE_TO_GUEST, 0, 0, nullptr);
            }
        }
        __finally {
            delete[] local_src_path_c;
        }

        /* Send file */
        HANDLE src = CreateFile(LocalSrcPath, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (src == INVALID_HANDLE_VALUE) {
            RaiseException(ERR_COPY_FILE_TO_GUEST_SRC_OPEN, 0, 0, nullptr);
        }
        __try {
            LARGE_INTEGER fileSize;
			GetFileSizeEx(src, &fileSize);
			HVCPCommand cmd = HVCP_CMD_COPY_FILE_TO_GUEST;
            if (send(sock, reinterpret_cast<char*>(&cmd), sizeof(HVCPCommand), 0) == SOCKET_ERROR) {
                RaiseException(ERR_COPY_FILE_TO_GUEST, 0, 0, nullptr);
            }
            if (send(sock, reinterpret_cast<char*>(&fileSize.QuadPart), sizeof(long long), 0) == SOCKET_ERROR) {
                RaiseException(ERR_COPY_FILE_TO_GUEST, 0, 0, nullptr);
            }

            // https://learn.microsoft.com/ja-jp/windows/win32/api/winsock/nf-winsock-transmitfile
            constexpr DWORD transmitfile_limit = 2147483646;
            long long remains = fileSize.QuadPart;
			LARGE_INTEGER offset = { 0 };
            do {
                DWORD to_write = min(remains, static_cast<long long>(transmitfile_limit));
                if (!TransmitFile(sock, src, to_write, 0, nullptr, nullptr, 0)) {
                    RaiseException(ERR_COPY_FILE_TO_GUEST, 0, 0, nullptr);
                }
                remains -= to_write;
				offset.QuadPart += to_write;
				SetFilePointerEx(src, offset, nullptr, FILE_BEGIN);
            } while (remains > 0);
			if (std::get<0>(GetCommandResult(sock)) != HVCP_CMD_RESULT_OK) {
                RaiseException(ERR_COPY_FILE_TO_GUEST_SEND_FILE, 0, 0, nullptr);
			}
        }
        __finally {
            CloseHandle(src);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        errcode = static_cast<HVCPErrorCode>(GetExceptionCode());
        errdetails = (errcode == ERR_COPY_FILE_TO_GUEST_SRC_OPEN) ? GetLastError() : WSAGetLastError();
        return false;
    }

    return true;
}

#define RCVBUF_SZ 4096
bool CopyFileFromGuestInternal(const SOCKET sock, LPCWSTR RemoteSrcPath, LPWSTR LocalDestPath) {
    __try {
        /* Send remote path */
        long long len = WideCharToMultiByte(CP_UTF8, 0, RemoteSrcPath, -1, nullptr, 0, nullptr, nullptr);
        PSTR remote_src_path_c = new CHAR[len];
        WideCharToMultiByte(CP_UTF8, 0, RemoteSrcPath, -1, remote_src_path_c, len, nullptr, nullptr);
        __try {
            SendData(sock, HVCP_CMD_SET_REMOTE_PATH, remote_src_path_c, len - 1 /* Eliminate NUL char */);
            if (std::get<0>(GetCommandResult(sock)) != HVCP_CMD_RESULT_OK) {
                RaiseException(ERR_INALID_REMOETE_PATH, 0, 0, nullptr);
            }
        }
        __finally {
            delete[] remote_src_path_c;
        }

        /* Check local file name */
		DWORD attr = GetFileAttributes(LocalDestPath);
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
#pragma warning(push)
#pragma warning(disable: 4996)
			wcscat(LocalDestPath, L"\\");
            wcscat(LocalDestPath, PathFindFileName(RemoteSrcPath));
#pragma warning(pop)
        }

        /* Receive file */
        HANDLE dest = CreateFile(LocalDestPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        __try {
			SendData(sock, HVCP_CMD_COPY_FILE_FROM_GUEST, nullptr, 0);
			auto [cmd_result, file_size] = GetCommandResult(sock);
			if (cmd_result != HVCP_CMD_RESULT_OK) {
				RaiseException(ERR_COPY_FILE_FROM_GUEST, 0, 0, nullptr);
			}
			if (file_size > 0) {
				long long remaining = file_size;
				char rcvbuf[RCVBUF_SZ];
				while (remaining > 0) {
					int recv_size = recv(sock, rcvbuf, min(remaining, sizeof(rcvbuf)), 0);
					if (recv_size == SOCKET_ERROR) {
						RaiseException(ERR_COPY_FILE_FROM_GUEST, 0, 0, nullptr);
					}
					if (recv_size == 0) {
						break;
					}
                    if (dest != INVALID_HANDLE_VALUE) {
                        DWORD written;
                        if (!WriteFile(dest, rcvbuf, recv_size, &written, nullptr)) {
                            RaiseException(ERR_COPY_FILE_FROM_GUEST_WRITE_FILE, 0, 0, nullptr);
                        }
                    }
					remaining -= recv_size;
				}
			}
        }
        __finally {
            if (dest == INVALID_HANDLE_VALUE) {
				RaiseException(ERR_COPY_FILE_FROM_GUEST_DEST_CREATE, 0, 0, nullptr);
			}
			else {
                CloseHandle(dest);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        errcode = static_cast<HVCPErrorCode>(GetExceptionCode());
        errdetails = (errcode == ERR_COPY_FILE_FROM_GUEST_DEST_CREATE) ? GetLastError() : WSAGetLastError();
        return false;
    }

    return true;
}