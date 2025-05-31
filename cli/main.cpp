#include <iostream>
#include <regex>

#include "../hvcp.h"


int wmain(int argc, wchar_t* argv[])
{
	if (argc != 3) {
		std::wcout << L"Usage:" << std::endl;
		std::wcout << argv[0] << L" [src] [dest]" << std::endl;
		return 1;
	}

    PWSTR src = argv[1];
	PWSTR dest = argv[2];
	bool src_is_remote = false;
	bool dest_is_remote = false;

	std::wstring user;
	std::wstring vm;
	std::wstring remote_path;
	std::wregex remote_pattern(L"^((.+?)@)?(.+?):([^\\\\]+)$");
	std::wcmatch match;
	if (std::regex_match(src, match, remote_pattern)) {
		src_is_remote = true;
		user = match[2].str();
		vm = match[3].str();
		remote_path = match[4].str();
	}
	if (std::regex_match(dest, match, remote_pattern)) {
		dest_is_remote = true;
		user = match[2].str();
		vm = match[3].str();
		remote_path = match[4].str();
	}

	bool result;
	if (src_is_remote && dest_is_remote) {
		std::wcerr << L"Error: Cannot copy between two remote paths." << std::endl;
		return 2;
	}
	else if (src_is_remote) {
		result = CopyFileFromGuest(vm.c_str(), remote_path.c_str(), dest);
	}
	else if (dest_is_remote) {
		result = CopyFileToGuest(user.c_str(), vm.c_str(), src, remote_path.c_str());
	}
	else {
		std::wcerr << L" Error: Cannot copy between two local paths." << std::endl;
		return 3;
	}

	if (!result) {
		LARGE_INTEGER last_error = GetHVCPLastError();
		std::wcerr << L"Error: Failed: error code = " << last_error.LowPart << std::endl;
#ifdef _DEBUG
		std::wcerr << L"  error details = " << last_error.HighPart << std::endl;
#endif
		return 4;
	}

	return 0;
}
