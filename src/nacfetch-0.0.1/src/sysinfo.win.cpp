#include "sysinfo.win.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <memory>
// Fix for MinGW WMI GUIDs
// #ifdef __MINGW32__

// // Define missing GUIDs for MinGW
// #ifndef _WBEMIDL_H_
// DEFINE_GUID(CLSID_WbemLocator, 0x4590f811, 0x1d3a, 0x11d0, 0x89, 0x1f, 0x00, 0xaa, 0x00, 0x4b, 0x2e, 0x24);
// DEFINE_GUID(IID_IWbemLocator, 0xdc12a687, 0x737f, 0x11cf, 0x88, 0x4d, 0x00, 0xaa, 0x00, 0x4b, 0x2e, 0x24);
// #endif

// #endif // __MINGW32__
namespace SystemInfo
{

#ifdef _WIN32

    static std::wstring to_wstring(const std::string &s)
    {
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (len <= 0) return L"";
        std::wstring ws(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, ws.data(), len);
        ws.pop_back();
        return ws;
    }

    // -------------------- WMI Query Helper --------------------

    WMIQuery::WMIQuery() : pLoc(nullptr), pSvc(nullptr), initialized(false)
    {
        HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (FAILED(hres))
            return;

        hres = CoInitializeSecurity(
            nullptr,
            -1,
            nullptr,
            nullptr,
            RPC_C_AUTHN_LEVEL_DEFAULT,
            RPC_C_IMP_LEVEL_IMPERSONATE,
            nullptr,
            EOAC_NONE,
            nullptr);

        if (FAILED(hres))
        {
            CoUninitialize();
            return;
        }

        hres = CoCreateInstance(
            CLSID_WbemLocator,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_IWbemLocator,
            reinterpret_cast<LPVOID *>(&pLoc));

        if (FAILED(hres))
        {
            CoUninitialize();
            return;
        }

        BSTR ns = SysAllocString(L"ROOT\\CIMV2");
        hres = pLoc->ConnectServer(
            ns,
            nullptr, nullptr, nullptr,
            0, nullptr, nullptr, &pSvc);
        SysFreeString(ns);

        if (SUCCEEDED(hres))
        {
            hres = CoSetProxyBlanket(
                pSvc,
                RPC_C_AUTHN_WINNT,
                RPC_C_AUTHZ_NONE,
                nullptr,
                RPC_C_AUTHN_LEVEL_CALL,
                RPC_C_IMP_LEVEL_IMPERSONATE,
                nullptr,
                EOAC_NONE);
            initialized = true;
        }
    }

    WMIQuery::~WMIQuery()
    {
        if (pSvc)
            pSvc->Release();
        if (pLoc)
            pLoc->Release();
        CoUninitialize();
    }

    bool WMIQuery::executeQuery(const std::string &query, std::vector<std::vector<std::pair<std::string, std::string>>> &results)
    {
        if (!initialized)
            return false;

        IEnumWbemClassObject *pEnumerator = nullptr;
        BSTR n1 = SysAllocString(L"WQL");
        std::wstring wquery = to_wstring(query);
        BSTR n2 = SysAllocString(wquery.c_str());

        HRESULT hres = pSvc->ExecQuery(
            n1,
            n2,
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr,
            &pEnumerator);

        SysFreeString(n1);
        SysFreeString(n2);
        if (FAILED(hres))
            return false;

        results.clear();
        IWbemClassObject *pclsObj = nullptr;
        ULONG uReturn = 0;

        while (pEnumerator)
        {
            // Use a reasonable timeout instead of WBEM_INFINITE
            hres = pEnumerator->Next(10000, 1, &pclsObj, &uReturn);
            if (uReturn == 0 || FAILED(hres))
                break;

            std::vector<std::pair<std::string, std::string>> row;

            pclsObj->BeginEnumeration(WBEM_FLAG_NONSYSTEM_ONLY);
            BSTR propName = nullptr;
            VARIANT propValue;
            CIMTYPE propType;
            LONG propFlavor;

            while (pclsObj->Next(0, &propName, &propValue, &propType, &propFlavor) == WBEM_S_NO_ERROR)
            {
                if (propName)
                {
                    std::wstring wPropName(propName);
                    std::string propNameStr(wPropName.begin(), wPropName.end());

                    std::string propValueStr;
                    
                    switch (propValue.vt)
                    {
                    case VT_BSTR:
                        if (propValue.bstrVal)
                        {
                            std::wstring wValue(propValue.bstrVal);
                            propValueStr = std::string(wValue.begin(), wValue.end());
                        }
                        break;
                    case VT_I4:
                        propValueStr = std::to_string(propValue.intVal);
                        break;
                    case VT_UI4:
                        propValueStr = std::to_string(propValue.uintVal);
                        break;
                    case VT_UI8:
                        propValueStr = std::to_string(propValue.ullVal);
                        break;
                    case VT_R8:
                        propValueStr = std::to_string(propValue.dblVal);
                        break;
                    case VT_BOOL:
                        propValueStr = propValue.boolVal ? "True" : "False";
                        break;
                    case VT_NULL:
                        propValueStr = "";
                        break;
                    default:
                        propValueStr = "[Unsupported Type]";
                        break;
                    }

                    row.emplace_back(propNameStr, propValueStr);
                    SysFreeString(propName);
                    VariantClear(&propValue);
                }
            }

            results.push_back(row);
            pclsObj->EndEnumeration();
            pclsObj->Release();
            pclsObj = nullptr;
        }

        if (pEnumerator)
        {
            pEnumerator->Release();
            pEnumerator = nullptr;
        }
        return true;
    }

    std::vector<std::string> WMIQuery::getPropertyValues(const std::string& query, const std::string& property)
    {
        std::vector<std::string> results;
        std::vector<std::vector<std::pair<std::string, std::string>>> queryResults;
        
        if (executeQuery(query, queryResults))
        {
            for (const auto& row : queryResults)
            {
                for (const auto& [key, value] : row)
                {
                    if (key == property)
                    {
                        results.push_back(value);
                    }
                }
            }
        }
        
        return results;
    }

    // -------------------- Fetcher --------------------

    Fetcher::Fetcher()
    {
        // Initialize info structure with default values
        info_ = Info();
        
        // Initialize Winsock for network functions
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    Fetcher::~Fetcher()
    {
        // Cleanup Winsock
        WSACleanup();
    }

    void Fetcher::fetchInfo(const Flags &flags)
    {
        fetchBasicInfo();
        fetchTimeInfo();
        fetchLocaleInfo();

        if (flags.os)
            fetchOSInfo();
        if (flags.kernel)
            fetchKernelInfo();
        if (flags.model)
            fetchHostInfo();
        if (flags.cpu)
            fetchCPUInfo();
        if (flags.gpu)
            fetchGPUInfo();
        if (flags.memory)
            fetchMemoryInfo();
        if (flags.swap)
            fetchSwapInfo();
        if (flags.disk)
            fetchDiskInfo();
        if (flags.display)
            fetchDisplayInfo();
        if (flags.network)
            fetchNetworkInfo();
        if (flags.battery)
            fetchBatteryInfo();
        if (flags.uptime)
            fetchUptimeInfo();
        if (flags.shell)
            fetchShellInfo();
        if (flags.terminal)
            fetchTerminalInfo();
        if (flags.de)
            fetchDesktopEnvironment();
    }

    const Info &Fetcher::getInfo() const { return info_; }

    // -------------------- Helper Methods --------------------

    std::string Fetcher::getRegistryString(HKEY hKey, const std::string &subKey, const std::string &value)
    {
        HKEY hSubKey;
        if (RegOpenKeyExA(hKey, subKey.c_str(), 0, KEY_READ, &hSubKey) != ERROR_SUCCESS)
        {
            return "";
        }

        DWORD dataSize = 0;
        if (RegQueryValueExA(hSubKey, value.c_str(), nullptr, nullptr, nullptr, &dataSize) != ERROR_SUCCESS)
        {
            RegCloseKey(hSubKey);
            return "";
        }

        std::vector<char> buffer(dataSize);
        if (RegQueryValueExA(hSubKey, value.c_str(), nullptr, nullptr, reinterpret_cast<LPBYTE>(buffer.data()), &dataSize) != ERROR_SUCCESS)
        {
            RegCloseKey(hSubKey);
            return "";
        }

        RegCloseKey(hSubKey);
        return std::string(buffer.data());
    }

    uint64_t Fetcher::getFileTimeToSeconds(FILETIME ft)
    {
        ULARGE_INTEGER ull;
        ull.LowPart = ft.dwLowDateTime;
        ull.HighPart = ft.dwHighDateTime;
        return ull.QuadPart / 10000000ULL;
    }

    std::string Fetcher::fileTimeToString(FILETIME ft)
    {
        SYSTEMTIME st;
        if (!FileTimeToSystemTime(&ft, &st))
            return "";
        
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond);
        return std::string(buffer);
    }

    std::string Fetcher::getCurrentTimeString()
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond);
        return std::string(buffer);
    }

    std::vector<std::string> Fetcher::getWmiProperty(const std::string& query, const std::string& property)
    {
        return wmi_.getPropertyValues(query, property);
    }

    // -------------------- BASIC --------------------

    void Fetcher::fetchBasicInfo()
    {
        // Username
        char username[256];
        DWORD usernameLen = sizeof(username);
        if (GetUserNameA(username, &usernameLen))
        {
            info_.username = username;
        }

        // Hostname
        char hostname[256];
        DWORD hostnameLen = sizeof(hostname);
        if (GetComputerNameA(hostname, &hostnameLen))
        {
            info_.hostname = hostname;
        }

        // Architecture
        SYSTEM_INFO sysInfo;
        GetNativeSystemInfo(&sysInfo);
        switch (sysInfo.wProcessorArchitecture)
        {
        case PROCESSOR_ARCHITECTURE_AMD64:
            info_.architecture = "x86_64";
            break;
        case PROCESSOR_ARCHITECTURE_ARM:
            info_.architecture = "ARM";
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            info_.architecture = "ARM64";
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            info_.architecture = "x86";
            break;
        default:
            info_.architecture = "Unknown";
        }
    }

    // -------------------- OS / KERNEL --------------------

    void Fetcher::fetchOSInfo()
    {
        std::vector<std::vector<std::pair<std::string, std::string>>> results;
        if (wmi_.executeQuery("SELECT * FROM Win32_OperatingSystem", results))
        {
            for (const auto &row : results)
            {
                for (const auto &[key, value] : row)
                {
                    if (key == "Caption")
                        info_.os_name = value;
                    else if (key == "Version")
                        info_.os_version = value;
                    else if (key == "CSDVersion")
                        info_.os_codename = value;
                    else if (key == "SerialNumber")
                        info_.os_id = value;
                }
                break;
            }
        }

        // Clean up Windows version name
        size_t pos = info_.os_name.find("Microsoft ");
        if (pos != std::string::npos)
        {
            info_.os_name = info_.os_name.substr(pos + 10);
        }
        
        if (info_.os_id.empty())
        {
            info_.os_id = "Windows_" + info_.os_version;
        }
    }

    void Fetcher::fetchKernelInfo()
    {
        info_.kernel = "Windows NT";
        info_.kernel_version = info_.os_version;
    }

    // -------------------- HOST --------------------

    void Fetcher::fetchHostInfo()
    {
        std::vector<std::vector<std::pair<std::string, std::string>>> results;

        if (wmi_.executeQuery("SELECT * FROM Win32_ComputerSystem", results))
        {
            for (const auto &row : results)
            {
                for (const auto &[key, value] : row)
                {
                    if (key == "Model")
                        info_.model = value;
                    else if (key == "Manufacturer")
                        info_.manufacturer = value;
                    else if (key == "ChassisSKUNumber")
                        info_.chassis_type = value;
                }
            }
        }

        results.clear();
        if (wmi_.executeQuery("SELECT * FROM Win32_BIOS", results))
        {
            for (const auto &row : results)
            {
                for (const auto &[key, value] : row)
                {
                    if (key == "Version")
                        info_.bios_version = value;
                }
            }
        }

        results.clear();
        if (wmi_.executeQuery("SELECT * FROM Win32_BaseBoard", results))
        {
            for (const auto &row : results)
            {
                for (const auto &[key, value] : row)
                {
                    if (key == "Product")
                        info_.board_name = value;
                }
            }
        }
        
        if (info_.chassis_type.empty())
        {
            results.clear();
            if (wmi_.executeQuery("SELECT * FROM Win32_SystemEnclosure", results))
            {
                for (const auto &row : results)
                {
                    for (const auto &[key, value] : row)
                    {
                        if (key == "ChassisTypes")
                        {
                            info_.chassis_type = value;
                            break;
                        }
                    }
                    if (!info_.chassis_type.empty())
                        break;
                }
            }
        }
    }

    // -------------------- CPU --------------------

    void Fetcher::fetchCPUInfo()
    {
        std::vector<std::vector<std::pair<std::string, std::string>>> results;

        if (wmi_.executeQuery("SELECT * FROM Win32_Processor", results))
        {
            for (const auto &row : results)
            {
                for (const auto &[key, value] : row)
                {
                    if (key == "Name")
                        info_.cpu.model = value;
                    else if (key == "Manufacturer")
                        info_.cpu.vendor = value;
                    else if (key == "NumberOfCores")
                        info_.cpu.core_count = std::stoi(value);
                    else if (key == "NumberOfLogicalProcessors")
                        info_.cpu.thread_count = std::stoi(value);
                    else if (key == "MaxClockSpeed")
                        info_.cpu.max_freq_ghz = std::stod(value) / 1000.0;
                    else if (key == "CurrentClockSpeed")
                        info_.cpu.current_freq_ghz = std::stod(value) / 1000.0;
                    else if (key == "Architecture")
                    {
                        int arch = std::stoi(value);
                        if (arch == 0)
                            info_.cpu.architecture = "x86";
                        else if (arch == 1)
                            info_.cpu.architecture = "MIPS";
                        else if (arch == 2)
                            info_.cpu.architecture = "Alpha";
                        else if (arch == 3)
                            info_.cpu.architecture = "PowerPC";
                        else if (arch == 5)
                            info_.cpu.architecture = "ARM";
                        else if (arch == 6)
                            info_.cpu.architecture = "Itanium";
                        else if (arch == 9)
                            info_.cpu.architecture = "x64";
                        else
                            info_.cpu.architecture = "Unknown";
                    }
                }
                break;
            }
        }
    }

    // -------------------- GPU --------------------

    void Fetcher::fetchGPUInfo()
    {
        info_.gpus.clear();
        std::vector<std::vector<std::pair<std::string, std::string>>> results;

        if (wmi_.executeQuery("SELECT * FROM Win32_VideoController", results))
        {
            for (const auto &row : results)
            {
                GPU gpu;
                for (const auto &[key, value] : row)
                {
                    if (key == "Name")
                        gpu.model = value;
                    else if (key == "AdapterCompatibility")
                        gpu.vendor = value;
                    else if (key == "DriverVersion")
                        gpu.driver = value;
                    else if (key == "AdapterRAM")
                    {
                        if (!value.empty() && value != "[Unsupported Type]")
                        {
                            try {
                                uint64_t bytes = std::stoull(value);
                                gpu.memory_mb = static_cast<int>(bytes / (1024 * 1024));
                            } catch (...) {
                                gpu.memory_mb = 0;
                            }
                        }
                    }
                    else if (key == "CurrentRefreshRate")
                    {
                        if (!value.empty() && value != "[Unsupported Type]")
                        {
                            try {
                                gpu.freq_ghz = std::stod(value) / 1000.0;
                            } catch (...) {
                                gpu.freq_ghz = 0.0;
                            }
                        }
                    }
                }

                std::string vendorLower = gpu.vendor;
                std::transform(vendorLower.begin(), vendorLower.end(), vendorLower.begin(), ::tolower);
                gpu.is_integrated = (vendorLower.find("intel") != std::string::npos) ||
                                    (gpu.model.find("APU") != std::string::npos) ||
                                    (gpu.model.find("Radeon Graphics") != std::string::npos);

                info_.gpus.push_back(gpu);
            }
        }
    }

    // -------------------- MEMORY / SWAP --------------------

    void Fetcher::fetchMemoryInfo()
    {
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);

        if (GlobalMemoryStatusEx(&memInfo))
        {
            info_.memory.total_bytes = memInfo.ullTotalPhys;
            info_.memory.available_bytes = memInfo.ullAvailPhys;
            info_.memory.free_bytes = info_.memory.available_bytes;
            info_.memory.used_bytes = info_.memory.total_bytes - info_.memory.available_bytes;
            info_.memory.usage_percent = memInfo.dwMemoryLoad;
        }
    }

    void Fetcher::fetchSwapInfo()
    {
        std::vector<std::vector<std::pair<std::string, std::string>>> results;

        if (wmi_.executeQuery("SELECT * FROM Win32_PageFileUsage", results))
        {
            uint64_t totalPagefile = 0;
            uint64_t usedPagefile = 0;

            for (const auto &row : results)
            {
                for (const auto &[key, value] : row)
                {
                    if (key == "AllocatedBaseSize")
                    {
                        if (!value.empty() && value != "[Unsupported Type]")
                        {
                            try {
                                uint64_t size = std::stoull(value) * 1024 * 1024;
                                totalPagefile += size;
                            } catch (...) {}
                        }
                    }
                    else if (key == "CurrentUsage")
                    {
                        if (!value.empty() && value != "[Unsupported Type]")
                        {
                            try {
                                uint64_t used = std::stoull(value) * 1024 * 1024;
                                usedPagefile += used;
                            } catch (...) {}
                        }
                    }
                }
            }

            info_.swap.total_bytes = totalPagefile;
            info_.swap.used_bytes = usedPagefile;
            info_.swap.free_bytes = totalPagefile > usedPagefile ? totalPagefile - usedPagefile : 0;
            info_.swap.usage_percent = (totalPagefile > 0) ? static_cast<int>((usedPagefile * 100) / totalPagefile) : 0;
        }
        else
        {
            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            
            if (GlobalMemoryStatusEx(&memInfo))
            {
                info_.swap.total_bytes = memInfo.ullTotalPageFile;
                info_.swap.free_bytes = memInfo.ullAvailPageFile;
                info_.swap.used_bytes = info_.swap.total_bytes - info_.swap.free_bytes;
                info_.swap.usage_percent = (info_.swap.total_bytes > 0) ? 
                    static_cast<int>((info_.swap.used_bytes * 100) / info_.swap.total_bytes) : 0;
            }
        }
    }

    // -------------------- DISK --------------------

    void Fetcher::fetchDiskInfo()
    {
        info_.disks.clear();

        DWORD drives = GetLogicalDrives();
        char driveLetter = 'A';

        for (int i = 0; i < 26; i++)
        {
            if (drives & (1 << i))
            {
                std::string rootPath = std::string(1, driveLetter + i) + ":\\";
                UINT driveType = GetDriveTypeA(rootPath.c_str());

                if (driveType == DRIVE_FIXED || driveType == DRIVE_REMOVABLE || driveType == DRIVE_REMOTE)
                {
                    ULARGE_INTEGER freeBytes, totalBytes, totalFreeBytes;

                    if (GetDiskFreeSpaceExA(rootPath.c_str(), &freeBytes, &totalBytes, &totalFreeBytes))
                    {
                        Disk disk;
                        disk.mount_point = rootPath;

                        char fsName[MAX_PATH];
                        if (GetVolumeInformationA(rootPath.c_str(), nullptr, 0, nullptr, nullptr, nullptr,
                                                  fsName, MAX_PATH))
                        {
                            disk.filesystem = fsName;
                        }

                        disk.total_bytes = totalBytes.QuadPart;
                        disk.free_bytes = freeBytes.QuadPart;
                        disk.available_bytes = totalFreeBytes.QuadPart;
                        disk.used_bytes = disk.total_bytes - disk.free_bytes;
                        disk.usage_percent = (disk.total_bytes > 0) ? static_cast<int>((disk.used_bytes * 100) / disk.total_bytes) : 0;

                        info_.disks.push_back(disk);
                    }
                }
            }
        }
    }

    // -------------------- DISPLAY --------------------

    void Fetcher::fetchDisplayInfo()
    {
        info_.displays.clear();

        DISPLAY_DEVICEA displayDevice;
        ZeroMemory(&displayDevice, sizeof(displayDevice));
        displayDevice.cb = sizeof(DISPLAY_DEVICEA);

        for (int i = 0; EnumDisplayDevicesA(nullptr, i, &displayDevice, 0); i++)
        {
            if (displayDevice.StateFlags & DISPLAY_DEVICE_ACTIVE)
            {
                Display display;
                display.name = displayDevice.DeviceName;
                display.output_name = displayDevice.DeviceString;

                display.is_builtin = (displayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0;

                DEVMODEA devMode;
                ZeroMemory(&devMode, sizeof(devMode));
                devMode.dmSize = sizeof(DEVMODEA);

                if (EnumDisplaySettingsA(displayDevice.DeviceName, ENUM_CURRENT_SETTINGS, &devMode))
                {
                    display.width = devMode.dmPelsWidth;
                    display.height = devMode.dmPelsHeight;
                    display.refresh_rate = devMode.dmDisplayFrequency;

                    std::stringstream ss;
                    ss << display.width << "x" << display.height << "@" << display.refresh_rate << "Hz";
                    display.current_mode = ss.str();
                }

                info_.displays.push_back(display);
            }
            
            ZeroMemory(&displayDevice, sizeof(displayDevice));
            displayDevice.cb = sizeof(DISPLAY_DEVICEA);
        }
    }

    // -------------------- NETWORK --------------------

    void Fetcher::fetchNetworkInfo()
    {
        info_.network_interfaces.clear();

        // Use older API that works better with MinGW
        PIP_ADAPTER_INFO pAdapterInfo = nullptr;
        PIP_ADAPTER_INFO pAdapter = nullptr;
        ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);

        pAdapterInfo = (IP_ADAPTER_INFO*)malloc(sizeof(IP_ADAPTER_INFO));
        if (pAdapterInfo == nullptr)
            return;

        if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW)
        {
            free(pAdapterInfo);
            pAdapterInfo = (IP_ADAPTER_INFO*)malloc(ulOutBufLen);
            if (pAdapterInfo == nullptr)
                return;
        }

        if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == NO_ERROR)
        {
            for (pAdapter = pAdapterInfo; pAdapter != nullptr; pAdapter = pAdapter->Next)
            {
                NetworkInterface nic;
                nic.name = pAdapter->AdapterName;
                nic.description = pAdapter->Description;

                // MAC address
                char mac[18];
                snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                        pAdapter->Address[0], pAdapter->Address[1], pAdapter->Address[2],
                        pAdapter->Address[3], pAdapter->Address[4], pAdapter->Address[5]);
                nic.mac = mac;

                // IPv4 address
                IP_ADDR_STRING *pIpAddrString = &pAdapter->IpAddressList;
                if (pIpAddrString != nullptr)
                {
                    nic.ipv4 = pIpAddrString->IpAddress.String;
                    nic.ipv4_addresses.push_back(nic.ipv4);

                    // Subnet mask
                    nic.subnet_mask = pIpAddrString->IpMask.String;
                }

                // Check if wireless (based on adapter type)
                nic.is_wireless = (pAdapter->Type == IF_TYPE_IEEE80211);

                // Check if up (simplified)
                nic.is_up = (pAdapter->DhcpEnabled || !nic.ipv4.empty());

                info_.network_interfaces.push_back(nic);
            }
        }

        free(pAdapterInfo);
    }

    // -------------------- BATTERY --------------------

    void Fetcher::fetchBatteryInfo()
    {
        info_.batteries.clear();

        std::vector<std::vector<std::pair<std::string, std::string>>> results;
        
        if (wmi_.executeQuery("SELECT * FROM Win32_Battery", results))
        {
            for (const auto& row : results)
            {
                Battery battery;
                battery.name = "Battery";
                
                for (const auto& [key, value] : row)
                {
                    if (key == "Name")
                        battery.name = value;
                    else if (key == "EstimatedChargeRemaining")
                    {
                        if (!value.empty() && value != "[Unsupported Type]")
                        {
                            try {
                                battery.percentage = std::stoi(value);
                            } catch (...) {
                                battery.percentage = -1;
                            }
                        }
                    }
                    else if (key == "BatteryStatus")
                    {
                        if (!value.empty() && value != "[Unsupported Type]")
                        {
                            try {
                                int status = std::stoi(value);
                                switch (status)
                                {
                                case 1:
                                    battery.status = "Discharging";
                                    battery.is_charging = false;
                                    break;
                                case 2:
                                    battery.status = "AC Connected";
                                    battery.ac_connected = true;
                                    battery.is_charging = false;
                                    break;
                                case 3:
                                    battery.status = "Fully Charged";
                                    battery.ac_connected = true;
                                    battery.is_charging = false;
                                    break;
                                case 4:
                                case 5:
                                    battery.status = "Charging";
                                    battery.ac_connected = true;
                                    battery.is_charging = true;
                                    break;
                                default:
                                    battery.status = "Unknown";
                                    break;
                                }
                            } catch (...) {
                                battery.status = "Unknown";
                            }
                        }
                    }
                    else if (key == "EstimatedRunTime")
                    {
                        if (!value.empty() && value != "[Unsupported Type]")
                        {
                            try {
                                battery.time_remaining_mins = std::stoi(value);
                            } catch (...) {
                                battery.time_remaining_mins = -1;
                            }
                        }
                    }
                    else if (key == "DesignVoltage")
                    {
                        if (!value.empty() && value != "[Unsupported Type]")
                        {
                            try {
                                battery.voltage = std::stod(value) / 1000.0;
                            } catch (...) {
                                battery.voltage = 0.0;
                            }
                        }
                    }
                }
                
                info_.batteries.push_back(battery);
            }
        }
        
        if (info_.batteries.empty())
        {
            SYSTEM_POWER_STATUS powerStatus;
            if (GetSystemPowerStatus(&powerStatus))
            {
                if (powerStatus.BatteryFlag != 128)
                {
                    Battery battery;
                    battery.name = "Battery";
                    battery.percentage = powerStatus.BatteryLifePercent;
                    
                    if (powerStatus.ACLineStatus == 1)
                    {
                        battery.status = "Charging";
                        battery.ac_connected = true;
                        battery.is_charging = (powerStatus.BatteryFlag & 8) != 0;
                    }
                    else
                    {
                        battery.status = "Discharging";
                        battery.ac_connected = false;
                        battery.is_charging = false;
                    }
                    
                    if (powerStatus.BatteryLifeTime != (DWORD)-1)
                    {
                        battery.time_remaining_mins = powerStatus.BatteryLifeTime / 60;
                    }
                    
                    info_.batteries.push_back(battery);
                }
            }
        }
    }

    // -------------------- SHELL / TERMINAL / DE --------------------

    void Fetcher::fetchShellInfo()
    {
        char *shell = getenv("COMSPEC");
        if (shell)
        {
            info_.shell = shell;
            size_t pos = info_.shell.find_last_of("\\/");
            if (pos != std::string::npos)
            {
                info_.shell = info_.shell.substr(pos + 1);
            }
            info_.shell_version = "";
        }
    }

    void Fetcher::fetchTerminalInfo()
    {
        char *term = getenv("WT_SESSION");
        if (term)
        {
            info_.terminal = "Windows Terminal";
            info_.terminal_version = "";
            return;
        }

        term = getenv("ConEmuANSI");
        if (term)
        {
            info_.terminal = "ConEmu";
            info_.terminal_version = "";
            return;
        }

        term = getenv("TERM_PROGRAM");
        if (term)
        {
            info_.terminal = term;
            info_.terminal_version = getenv("TERM_PROGRAM_VERSION") ? getenv("TERM_PROGRAM_VERSION") : "";
            return;
        }

        term = getenv("TERM");
        if (term)
        {
            info_.terminal = "Mintty/Cygwin";
            info_.terminal_version = "";
            return;
        }

        info_.terminal = "Windows Console";
        info_.terminal_version = "";
    }

    void Fetcher::fetchDesktopEnvironment()
    {
        info_.de.name = "Windows Desktop";

        DWORD value;
        DWORD dataSize = sizeof(DWORD);
        HKEY hKey;

        if (RegOpenKeyExA(HKEY_CURRENT_USER,
                          "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                          0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            if (RegQueryValueExA(hKey, "AppsUseLightTheme", nullptr, nullptr,
                                 reinterpret_cast<LPBYTE>(&value), &dataSize) == ERROR_SUCCESS)
            {
                info_.de.theme = (value == 0) ? "Dark" : "Light";
            }
            RegCloseKey(hKey);
        }
    }

    // -------------------- UPTIME / TIME / LOCALE --------------------

    void Fetcher::fetchUptimeInfo()
    {
        info_.uptime_seconds = GetTickCount64() / 1000;
        
        uint64_t bootTimeMs = GetTickCount64();
        FILETIME currentFileTime;
        GetSystemTimeAsFileTime(&currentFileTime);
        
        ULARGE_INTEGER currentTime;
        currentTime.LowPart = currentFileTime.dwLowDateTime;
        currentTime.HighPart = currentFileTime.dwHighDateTime;
        
        currentTime.QuadPart -= bootTimeMs * 10000;
        
        FILETIME bootFileTime;
        bootFileTime.dwLowDateTime = currentTime.LowPart;
        bootFileTime.dwHighDateTime = currentTime.HighPart;
        
        info_.boot_time = fileTimeToString(bootFileTime);
    }

    void Fetcher::fetchTimeInfo()
    {
        info_.current_time = getCurrentTimeString();
    }

    void Fetcher::fetchLocaleInfo()
    {
        // Get locale using older API that works with MinGW
        char localeName[LOCALE_NAME_MAX_LENGTH];
        if (GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SNAME, localeName, LOCALE_NAME_MAX_LENGTH) > 0)
        {
            info_.locale = localeName;
        }
        else
        {
            info_.locale = "en-US";
        }

        // Get timezone
        TIME_ZONE_INFORMATION tzInfo;
        if (GetTimeZoneInformation(&tzInfo) != TIME_ZONE_ID_INVALID)
        {
            char tzName[128];
            // Use wide char to multi-byte conversion
            int len = WideCharToMultiByte(CP_UTF8, 0, tzInfo.StandardName, -1, 
                                         tzName, sizeof(tzName), nullptr, nullptr);
            if (len > 0)
            {
                info_.timezone = tzName;
            }
            else
            {
                info_.timezone = "Unknown";
            }
        }
        else
        {
            info_.timezone = "Unknown";
        }
    }

#else // _WIN32 not defined

    // -------------------- Non-Windows stubs --------------------
    
    Fetcher::Fetcher()
    {
        // Initialize info structure with default values
        info_ = Info();
    }

    Fetcher::~Fetcher()
    {
        // Cleanup if needed
    }

    void Fetcher::fetchInfo(const Flags &flags)
    {
        // Stub implementation for non-Windows
        info_.os_name = "Unknown OS";
        info_.architecture = "Unknown";
    }

    const Info &Fetcher::getInfo() const { return info_; }

#endif // _WIN32

    // -------------------- UTILITY FUNCTIONS (Platform independent) --------------------

    std::string formatBytes(uint64_t bytes)
    {
        const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
        int unit = 0;
        double value = static_cast<double>(bytes);

        while (value >= 1024.0 && unit < 5)
        {
            value /= 1024.0;
            unit++;
        }

        char buffer[32];
        if (value < 10.0 && unit > 0)
        {
            snprintf(buffer, sizeof(buffer), "%.2f %s", value, units[unit]);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "%.1f %s", value, units[unit]);
        }

        return std::string(buffer);
    }

    std::string formatUptime(uint64_t seconds)
    {
        uint64_t days = seconds / 86400;
        uint64_t hours = (seconds % 86400) / 3600;
        uint64_t minutes = (seconds % 3600) / 60;
        uint64_t secs = seconds % 60;

        char buffer[64];
        if (days > 0)
        {
            snprintf(buffer, sizeof(buffer), "%llud %02lluh %02llum %02llus", 
                    days, hours, minutes, secs);
        }
        else if (hours > 0)
        {
            snprintf(buffer, sizeof(buffer), "%lluh %02llum %02llus", 
                    hours, minutes, secs);
        }
        else if (minutes > 0)
        {
            snprintf(buffer, sizeof(buffer), "%llum %02llus", minutes, secs);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "%llus", secs);
        }

        return std::string(buffer);
    }

    std::string formatMemory(uint64_t bytes)
    {
        return formatBytes(bytes);
    }

    std::string formatFrequency(double ghz)
    {
        char buffer[32];
        if (ghz >= 1.0)
        {
            snprintf(buffer, sizeof(buffer), "%.2f GHz", ghz);
        }
        else if (ghz > 0)
        {
            snprintf(buffer, sizeof(buffer), "%.0f MHz", ghz * 1000);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "N/A");
        }
        return std::string(buffer);
    }

    int calculatePercentage(uint64_t used, uint64_t total)
    {
        if (total == 0)
            return 0;
        return static_cast<int>((used * 100) / total);
    }

    double bytesToGiB(uint64_t bytes)
    {
        return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    }

    double bytesToMiB(uint64_t bytes)
    {
        return static_cast<double>(bytes) / (1024.0 * 1024.0);
    }

} // namespace SystemInfo