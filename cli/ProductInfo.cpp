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
#include "ProductInfo.h"

#include <stdexcept>


ProductInfo::ProductInfo(LPCWSTR module_name) {
	HMODULE hMod = GetModuleHandle(module_name);
    if (hMod == nullptr) {
        throw std::runtime_error("module not found");
    }

	WCHAR mod_path[MAX_PATH];
    GetModuleFileName(hMod, mod_path, sizeof(mod_path) / sizeof(WCHAR));
    if (GetLastError() == ERROR_SUCCESS) {
        LoadVersionInfo(mod_path);
    }
    else {
        data = nullptr;
        data_len = 0;
    }
}

ProductInfo::~ProductInfo() {
    if (data_len > 0) {
        free(data);
    }
}

void ProductInfo::LoadVersionInfo(LPCWSTR mod_path) {
    DWORD dummy = 0;
    data_len = GetFileVersionInfoSize(mod_path, &dummy);
    if (data_len == 0) {
        data = nullptr;
        return;
    }

    data = (unsigned char*)malloc(data_len);
    if (data == nullptr) {
        data_len = 0;
        return;
    }

    if (!GetFileVersionInfo(mod_path, 0, data_len, data)) {
        free(data);
        data = nullptr;
        data_len = 0;
        return;
    }
}

LPCWSTR ProductInfo::GetString(LPCWSTR query) {
    if (data_len > 0) {
        LPWSTR name = nullptr;
        UINT nameLen;
        if (VerQueryValue(data, query, reinterpret_cast<LPVOID*>(&name), &nameLen)) {
            return name;
        }
    }
    return nullptr;
}

LPCWSTR ProductInfo::GetProductName() {
    // Neutral codepage (000004b0)
    LPCWSTR query_result = GetString(L"\\StringFileInfo\\000004b0\\ProductName");
    return query_result ? query_result : L"";
}

LPCWSTR ProductInfo::GetProductVersion() {
    // Neutral codepage (000004b0)
    LPCWSTR query_result = GetString(L"\\StringFileInfo\\000004b0\\ProductVersion");
    return query_result ? query_result : L"";
}

LPCWSTR ProductInfo::GetLegalCopyright() {
    // Neutral codepage (000004b0)
    LPCWSTR query_result = GetString(L"\\StringFileInfo\\000004b0\\LegalCopyright");
    return query_result ? query_result : L"Copyright (C) Yasumasa Suenaga";
}