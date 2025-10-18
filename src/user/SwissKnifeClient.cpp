#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <cstdlib>
#include <cstring>

#include "../../include/SwissKnifeCommon.h"

namespace
{
    template <typename T>
    bool SendSimpleRequest(HANDLE device, DWORD code, T &data)
    {
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(device, code, nullptr, 0, &data, sizeof(T), &bytesReturned, nullptr);
        if (!ok)
        {
            std::wcerr << L"DeviceIoControl failed with error " << GetLastError() << std::endl;
            return false;
        }

        if (bytesReturned < sizeof(T))
        {
            std::wcerr << L"Incomplete response from driver." << std::endl;
            return false;
        }

        return true;
    }

    bool SendBufferRequest(HANDLE device, DWORD code, const void *inBuffer, DWORD inSize, std::vector<BYTE> &outBuffer)
    {
        DWORD bytesReturned = 0;
        while (true)
        {
            BOOL ok = DeviceIoControl(device, code, const_cast<void *>(inBuffer), inSize,
                                      outBuffer.data(), static_cast<DWORD>(outBuffer.size()),
                                      &bytesReturned, nullptr);
            if (ok)
            {
                outBuffer.resize(bytesReturned);
                return true;
            }

            DWORD error = GetLastError();
            if (error == ERROR_INSUFFICIENT_BUFFER || error == ERROR_MORE_DATA)
            {
                if (bytesReturned > 0)
                {
                    outBuffer.resize(bytesReturned);
                    continue;
                }
                outBuffer.resize(outBuffer.size() * 2);
                continue;
            }

            std::wcerr << L"DeviceIoControl failed with error " << error << std::endl;
            return false;
        }
    }

    std::wstring ReadWideLine()
    {
        std::wstring value;
        std::getline(std::wcin, value);
        if (!value.empty() && value.back() == L'\r')
        {
            value.pop_back();
        }
        return value;
    }

    bool TryParseUnsigned64(const std::wstring &text, unsigned long long &value)
    {
        if (text.empty())
        {
            return false;
        }

        wchar_t *end = nullptr;
        value = _wcstoui64(text.c_str(), &end, 0);
        return end != text.c_str() && *end == L'\0';
    }

    bool TryParseUnsigned(const std::wstring &text, unsigned long &value)
    {
        unsigned long long temp = 0;
        if (!TryParseUnsigned64(text, temp))
        {
            return false;
        }
        value = static_cast<unsigned long>(temp);
        return true;
    }

    bool TryParseUnsigned(const std::wstring &text, unsigned long long &value)
    {
        return TryParseUnsigned64(text, value);
    }

    void PrintDriverInfo(HANDLE device)
    {
        SWISSKNIFE_DRIVER_INFO info = {};
        if (!SendSimpleRequest(device, IOCTL_SWISSKNIFE_GET_DRIVER_INFO, info))
        {
            return;
        }

        std::wcout << L"Driver: " << info.FriendlyName << std::endl;
        std::wcout << L"Description: " << info.Description << std::endl;
        std::wcout << L"Version: " << info.VersionMajor << L'.' << info.VersionMinor << L" (build " << info.BuildNumber << L")" << std::endl;
    }

    void PrintSystemOverview(HANDLE device)
    {
        SWISSKNIFE_SYSTEM_OVERVIEW overview = {};
        if (!SendSimpleRequest(device, IOCTL_SWISSKNIFE_GET_SYSTEM_OVERVIEW, overview))
        {
            return;
        }

        std::wcout << L"Active processor mask: 0x" << std::hex << overview.ActiveProcessorMask << std::dec << std::endl;
        std::wcout << L"Number of processors: " << overview.NumberOfProcessors << std::endl;
        std::wcout << L"Total physical pages: " << overview.TotalPhysicalPages << std::endl;
        std::wcout << L"Available pages: " << overview.AvailablePages << std::endl;
        std::wcout << L"Page size: " << overview.PageSize << std::endl;
        std::wcout << L"System uptime: " << overview.SystemUptimeMilliseconds / 1000 << L" seconds" << std::endl;
    }

    void ListProcesses(HANDLE device)
    {
        std::vector<BYTE> buffer(sizeof(SWISSKNIFE_PROCESS_ENTRY) * 512);
        if (!SendBufferRequest(device, IOCTL_SWISSKNIFE_ENUMERATE_PROCESSES, nullptr, 0, buffer))
        {
            return;
        }

        size_t count = buffer.size() / sizeof(SWISSKNIFE_PROCESS_ENTRY);
        const SWISSKNIFE_PROCESS_ENTRY *entries = reinterpret_cast<const SWISSKNIFE_PROCESS_ENTRY *>(buffer.data());

        std::wcout << L"PID     PPID    Threads  Image" << std::endl;
        for (size_t i = 0; i < count; ++i)
        {
            std::wcout << std::setw(6) << entries[i].ProcessId << L"  "
                       << std::setw(6) << entries[i].ParentProcessId << L"  "
                       << std::setw(7) << entries[i].ThreadCount << L"  "
                       << entries[i].ImageName << std::endl;
        }
    }

    void ListModules(HANDLE device)
    {
        std::vector<BYTE> buffer(sizeof(SWISSKNIFE_MODULE_ENTRY) * 256);
        if (!SendBufferRequest(device, IOCTL_SWISSKNIFE_ENUMERATE_MODULES, nullptr, 0, buffer))
        {
            return;
        }

        size_t count = buffer.size() / sizeof(SWISSKNIFE_MODULE_ENTRY);
        const SWISSKNIFE_MODULE_ENTRY *entries = reinterpret_cast<const SWISSKNIFE_MODULE_ENTRY *>(buffer.data());

        std::wcout << L"Base Address        Size    Flags  Module" << std::endl;
        for (size_t i = 0; i < count; ++i)
        {
            std::wcout << L"0x" << std::hex << std::setw(16) << std::setfill(L'0') << entries[i].BaseAddress
                       << std::dec << std::setfill(L' ')
                       << L"  " << std::setw(7) << entries[i].ImageSize
                       << L"  0x" << std::hex << std::setw(4) << std::setfill(L'0') << entries[i].Flags
                       << std::dec << std::setfill(L' ')
                       << L"  " << entries[i].ImageName << std::endl;
        }
    }

    void ReadFileFromDriver(HANDLE device)
    {
        std::wcout << L"Enter full NT path (e.g. \\??\\C:\\Windows\\System32\\drivers\\etc\\hosts):" << std::endl;
        std::wstring path = ReadWideLine();
        if (path.empty())
        {
            std::wcout << L"Path cannot be empty." << std::endl;
            return;
        }

        std::wcout << L"Offset (decimal or 0x..): ";
        std::wstring offsetText = ReadWideLine();
        std::wcout << L"Number of bytes to read: ";
        std::wstring lengthText = ReadWideLine();

        unsigned long long offset = 0;
        unsigned long length = 0;
        if (!TryParseUnsigned(offsetText, offset) || !TryParseUnsigned(lengthText, length))
        {
            std::wcout << L"Invalid numeric input." << std::endl;
            return;
        }

        size_t pathChars = path.size() + 1;
        size_t requestSize = FIELD_OFFSET(SWISSKNIFE_FILE_REQUEST, Path) + pathChars * sizeof(WCHAR);
        std::vector<BYTE> requestBuffer(requestSize);
        auto *request = reinterpret_cast<SWISSKNIFE_FILE_REQUEST *>(requestBuffer.data());
        request->PathLength = static_cast<ULONG>(pathChars * sizeof(WCHAR));
        request->Offset = offset;
        request->ReadLength = length;
        memcpy(request->Path, path.c_str(), request->PathLength);

        std::vector<BYTE> responseBuffer(sizeof(SWISSKNIFE_FILE_RESPONSE) + length);
        if (!SendBufferRequest(device, IOCTL_SWISSKNIFE_READ_FILE, request, static_cast<DWORD>(requestSize), responseBuffer))
        {
            return;
        }

        const auto *response = reinterpret_cast<const SWISSKNIFE_FILE_RESPONSE *>(responseBuffer.data());
        std::wcout << L"Bytes read: " << response->BytesRead << std::endl;
        const BYTE *data = response->Data;
        for (ULONG i = 0; i < response->BytesRead; ++i)
        {
            if (i % 16 == 0)
            {
                std::wcout << std::endl << std::setw(4) << i << L": ";
            }
            std::wcout << std::hex << std::setfill(L'0') << std::setw(2) << static_cast<int>(data[i]) << L' ';
        }
        std::wcout << std::dec << std::setfill(L' ') << std::endl;
    }

    void QueryRegistryValue(HANDLE device)
    {
        std::wcout << L"Enter registry key path (NT format, e.g. \\Registry\\Machine\\SOFTWARE\\Microsoft):" << std::endl;
        std::wstring keyPath = ReadWideLine();
        std::wcout << L"Enter value name:" << std::endl;
        std::wstring valueName = ReadWideLine();

        if (keyPath.empty() || valueName.empty())
        {
            std::wcout << L"Key path and value name must be provided." << std::endl;
            return;
        }

        size_t keyChars = keyPath.size() + 1;
        size_t valueChars = valueName.size() + 1;
        size_t requestSize = FIELD_OFFSET(SWISSKNIFE_REGISTRY_REQUEST, Buffer) + (keyChars + valueChars) * sizeof(WCHAR);
        std::vector<BYTE> requestBuffer(requestSize);
        auto *request = reinterpret_cast<SWISSKNIFE_REGISTRY_REQUEST *>(requestBuffer.data());
        request->KeyPathLength = static_cast<ULONG>(keyChars * sizeof(WCHAR));
        request->ValueNameLength = static_cast<ULONG>(valueChars * sizeof(WCHAR));
        memcpy(request->Buffer, keyPath.c_str(), request->KeyPathLength);
        memcpy(reinterpret_cast<BYTE *>(request->Buffer) + request->KeyPathLength, valueName.c_str(), request->ValueNameLength);

        std::vector<BYTE> responseBuffer(sizeof(SWISSKNIFE_REGISTRY_RESPONSE) + SWISSKNIFE_MAX_REG_VALUE);
        if (!SendBufferRequest(device, IOCTL_SWISSKNIFE_QUERY_REGISTRY, request, static_cast<DWORD>(requestSize), responseBuffer))
        {
            return;
        }

        const auto *response = reinterpret_cast<const SWISSKNIFE_REGISTRY_RESPONSE *>(responseBuffer.data());
        std::wcout << L"Type: " << response->Type << L" Length: " << response->DataLength << std::endl;

        if (response->DataLength > 0)
        {
            std::wcout << L"Hex data:" << std::endl;
            for (ULONG i = 0; i < response->DataLength; ++i)
            {
                if (i % 16 == 0)
                {
                    std::wcout << std::endl << std::setw(4) << i << L": ";
                }
                std::wcout << std::hex << std::setfill(L'0') << std::setw(2) << static_cast<int>(response->Data[i]) << L' ';
            }
            std::wcout << std::dec << std::setfill(L' ') << std::endl;
        }
    }

    void GenerateRandomBytes(HANDLE device)
    {
        std::wcout << L"Enter seed (0 for automatic):" << std::endl;
        std::wstring seedText = ReadWideLine();
        std::wcout << L"Number of bytes:" << std::endl;
        std::wstring lengthText = ReadWideLine();

        unsigned long seed = 0;
        unsigned long length = 0;
        if (!TryParseUnsigned(seedText, seed) || !TryParseUnsigned(lengthText, length))
        {
            std::wcout << L"Invalid numeric input." << std::endl;
            return;
        }

        SWISSKNIFE_RANDOM_REQUEST request = {};
        request.Seed = seed;
        request.Length = length;

        std::vector<BYTE> response(sizeof(SWISSKNIFE_RANDOM_RESPONSE) + length);
        if (!SendBufferRequest(device, IOCTL_SWISSKNIFE_RANDOM_BYTES, &request, sizeof(request), response))
        {
            return;
        }

        const auto *randomResponse = reinterpret_cast<const SWISSKNIFE_RANDOM_RESPONSE *>(response.data());
        std::wcout << L"Seed used: " << randomResponse->SeedUsed << std::endl;
        std::wcout << L"Bytes generated: " << randomResponse->BytesGenerated << std::endl;

        for (ULONG i = 0; i < randomResponse->BytesGenerated; ++i)
        {
            if (i % 16 == 0)
            {
                std::wcout << std::endl << std::setw(4) << i << L": ";
            }
            std::wcout << std::hex << std::setfill(L'0') << std::setw(2) << static_cast<int>(randomResponse->Data[i]) << L' ';
        }
        std::wcout << std::dec << std::setfill(L' ') << std::endl;
    }
}

int wmain()
{
    HANDLE device = CreateFileW(SWISSKNIFE_USER_SYMBOLIC_NAME, GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (device == INVALID_HANDLE_VALUE)
    {
        std::wcerr << L"Failed to open SwissKnife device. Error: " << GetLastError() << std::endl;
        return 1;
    }

    while (true)
    {
        std::wcout << L"\nSwissKnife Control Panel" << std::endl;
        std::wcout << L"1. Driver information" << std::endl;
        std::wcout << L"2. System overview" << std::endl;
        std::wcout << L"3. Enumerate processes" << std::endl;
        std::wcout << L"4. Enumerate kernel modules" << std::endl;
        std::wcout << L"5. Read file" << std::endl;
        std::wcout << L"6. Query registry value" << std::endl;
        std::wcout << L"7. Generate random bytes" << std::endl;
        std::wcout << L"0. Exit" << std::endl;
        std::wcout << L"Select option: ";

        std::wstring choiceText = ReadWideLine();
        if (choiceText.empty())
        {
            continue;
        }

        int choice = std::stoi(choiceText);
        switch (choice)
        {
        case 1:
            PrintDriverInfo(device);
            break;
        case 2:
            PrintSystemOverview(device);
            break;
        case 3:
            ListProcesses(device);
            break;
        case 4:
            ListModules(device);
            break;
        case 5:
            ReadFileFromDriver(device);
            break;
        case 6:
            QueryRegistryValue(device);
            break;
        case 7:
            GenerateRandomBytes(device);
            break;
        case 0:
            CloseHandle(device);
            return 0;
        default:
            std::wcout << L"Unknown option." << std::endl;
            break;
        }
    }
}

