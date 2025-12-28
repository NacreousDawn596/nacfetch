#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace SystemInfo {

// Display information
struct Display {
    std::string name;
    int width = 0;
    int height = 0;
    int refresh_rate = 0;
    double size_inches = 0.0;
    bool is_builtin = false;
    std::string output_name;
    std::string current_mode;  // Added: current resolution/refresh rate
};

// Disk information
struct Disk {
    std::string mount_point;
    std::string filesystem;
    uint64_t total_bytes = 0;
    uint64_t used_bytes = 0;
    uint64_t available_bytes = 0;
    uint64_t free_bytes = 0;  // Added: for consistency with fetchDiskInfo
    int usage_percent = 0;
};

// Network interface information
struct NetworkInterface {
    std::string name;
    std::string ipv4;
    std::string ipv6;
    std::string mac;
    std::string subnet_mask;
    bool is_up = false;
    bool is_wireless = false;
    std::string operstate;  // Added: from /sys/class/net/*/operstate
    std::vector<std::string> ipv4_addresses;  // Added: multiple IPs possible
};

// Battery information
struct Battery {
    std::string name;
    int percentage = -1;
    std::string status;        // Charging, Discharging, Full, AC Connected
    bool is_charging = false;
    bool ac_connected = false;
    int time_remaining_mins = -1;   // -1 if unknown
    double voltage = 0.0;
    int capacity_mah = 0;
};

// Memory information
struct Memory {
    uint64_t total_bytes = 0;
    uint64_t used_bytes = 0;
    uint64_t available_bytes = 0;
    uint64_t free_bytes = 0;
    uint64_t cached_bytes = 0;
    uint64_t buffers_bytes = 0;
    int usage_percent = 0;
};

// Swap information
struct Swap {
    uint64_t total_bytes = 0;
    uint64_t used_bytes = 0;
    uint64_t free_bytes = 0;
    int usage_percent = 0;
};

// CPU information
struct CPU {
    std::string model;
    std::string vendor;
    int core_count = 0;
    int thread_count = 0;
    double max_freq_ghz = 0.0;
    double current_freq_ghz = 0.0;
    std::string architecture;
    std::vector<double> core_freqs;
    std::vector<int> core_temps;    // In Celsius
};

// GPU information
struct GPU {
    std::string model;
    std::string vendor;
    std::string driver;
    double freq_ghz = 0.0;
    int memory_mb = 0;
    bool is_integrated = false;
    int temperature = 0;
};

// Desktop Environment information
struct DesktopEnvironment {
    std::string name;
    std::string version;
    std::string wm_name;
    std::string wm_protocol;    // X11, Wayland, etc.
    std::string theme;
    std::string wm_theme;
    std::string icon_theme;
    std::string cursor_theme;
    int cursor_size = 0;
    std::string font_name;
    int font_size = 0;
};

// Package information - NOTE: We won't fetch this without subprocesses
struct PackageInfo {
    std::string manager_name;
    int count = 0;
};

// Main system information structure
struct Info {
    // Basic info
    std::string username;
    std::string hostname;
    std::string os_name;
    std::string os_version;
    std::string os_codename;
    std::string os_id;
    std::string kernel;
    std::string kernel_version;
    std::string architecture;
    
    // Hardware info
    std::string model;
    std::string manufacturer;
    std::string bios_version;
    std::string board_name;
    std::string chassis_type;
    
    // Shell info
    std::string shell;
    std::string shell_version;
    std::string terminal;
    std::string terminal_version;
    
    // Time info
    uint64_t uptime_seconds = 0;
    std::string boot_time;
    std::string current_time;
    
    // Locale info
    std::string locale;
    std::string timezone;
    
    // Complex structures
    CPU cpu;
    std::vector<GPU> gpus;
    Memory memory;
    Swap swap;
    std::vector<Display> displays;
    std::vector<Disk> disks;
    std::vector<NetworkInterface> network_interfaces;
    std::vector<Battery> batteries;
    DesktopEnvironment de;
    std::vector<PackageInfo> packages;
    
    // Totals
    int total_packages = 0;
    std::string package_managers;  // Formatted string
};

// Configuration flags
struct Flags {
    bool os = true;
    bool kernel = true;
    bool model = true;
    bool shell = true;
    bool terminal = true;
    bool cpu = true;
    bool gpu = true;
    bool memory = true;
    bool swap = true;
    bool disk = true;
    bool display = true;
    bool network = true;
    bool battery = true;
    bool de = true;
    bool packages = true;
    bool uptime = true;
};

// Main fetcher class
class Fetcher {
public:
    Fetcher();
    ~Fetcher() = default;
    
    void fetchInfo(const Flags& flags = Flags());
    const Info& getInfo() const;
    
private:
    Info info_;
    
    // Individual fetch methods - removed fetchPackageInfo since we can't get it
    void fetchBasicInfo();
    void fetchHostInfo();
    void fetchOSInfo();
    void fetchKernelInfo();
    void fetchShellInfo();
    void fetchTerminalInfo();
    void fetchCPUInfo();
    void fetchGPUInfo();
    void fetchMemoryInfo();
    void fetchSwapInfo();
    void fetchDiskInfo();
    void fetchDisplayInfo();
    void fetchNetworkInfo();
    void fetchBatteryInfo();
    void fetchDesktopEnvironment();
    void fetchUptimeInfo();
    void fetchLocaleInfo();
};

// Utility functions
std::string formatBytes(uint64_t bytes);
std::string formatUptime(uint64_t seconds);
std::string formatMemory(uint64_t bytes);
std::string formatFrequency(double ghz);
int calculatePercentage(uint64_t used, uint64_t total);
double bytesToGiB(uint64_t bytes);
double bytesToMiB(uint64_t bytes);

// Compatibility with old API
inline std::string formatRAM(uint64_t bytes) { return formatMemory(bytes); }
inline double getRAMPercentage(uint64_t used, uint64_t total) { 
    return calculatePercentage(used, total); 
}

} // namespace SystemInfo