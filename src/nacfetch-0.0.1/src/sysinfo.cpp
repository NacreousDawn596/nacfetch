#include "sysinfo.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <pwd.h>

namespace fs = std::filesystem;
namespace SystemInfo {

// -------------------- helpers --------------------

static std::string trim(std::string s) {
    s.erase(0, s.find_first_not_of(" \n\r\t"));
    s.erase(s.find_last_not_of(" \n\r\t") + 1);
    return s;
}

static std::string readFirstLine(const fs::path& p) {
    std::ifstream f(p);
    std::string s;
    std::getline(f, s);
    return trim(s);
}

// -------------------- Fetcher --------------------

Fetcher::Fetcher() = default;

void Fetcher::fetchInfo(const Flags& flags) {
    fetchBasicInfo();

    if (flags.os)        fetchOSInfo();
    if (flags.kernel)    fetchKernelInfo();
    if (flags.model)     fetchHostInfo();
    if (flags.cpu)       fetchCPUInfo();
    if (flags.gpu)       fetchGPUInfo();
    if (flags.memory)    fetchMemoryInfo();
    if (flags.swap)      fetchSwapInfo();
    if (flags.disk)      fetchDiskInfo();
    if (flags.display)   fetchDisplayInfo();
    if (flags.network)   fetchNetworkInfo();
    if (flags.battery)   fetchBatteryInfo();
    if (flags.uptime)    fetchUptimeInfo();
    if (flags.shell)     fetchShellInfo();
    if (flags.terminal)  fetchTerminalInfo();
    if (flags.de)        fetchDesktopEnvironment();

    fetchLocaleInfo();
}

const Info& Fetcher::getInfo() const { return info_; }

// -------------------- BASIC --------------------

void Fetcher::fetchBasicInfo() {
    if (passwd* pw = getpwuid(getuid()))
        info_.username = pw->pw_name;

    char host[256];
    if (gethostname(host, sizeof(host)) == 0)
        info_.hostname = host;

    struct utsname uts;
    if (uname(&uts) == 0)
        info_.architecture = uts.machine;
}

// -------------------- OS / KERNEL --------------------

void Fetcher::fetchOSInfo() {
    const fs::path os_release = "/etc/os-release";
    if (!fs::exists(os_release)) return;

    std::ifstream f(os_release);
    std::string line;
    while (std::getline(f, line)) {
        if (line.starts_with("NAME="))
            info_.os_name = line.substr(5);
        else if (line.starts_with("VERSION="))
            info_.os_version = line.substr(8);
        else if (line.starts_with("VERSION_CODENAME="))
            info_.os_codename = line.substr(17);
    }
    // Remove quotes
    auto remove_quotes = [](std::string& s) {
        s.erase(std::remove(s.begin(), s.end(), '"'), s.end());
    };
    remove_quotes(info_.os_name);
    remove_quotes(info_.os_version);
}

void Fetcher::fetchKernelInfo() {
    struct utsname uts;
    if (uname(&uts) == 0) {
        info_.kernel = std::string(uts.sysname) + " " + uts.release;
        info_.kernel_version = uts.release;
    }
}

// -------------------- HOST --------------------

void Fetcher::fetchHostInfo() {
    const fs::path dmi_path = "/sys/devices/virtual/dmi/id";
    if (!fs::exists(dmi_path)) return;

    info_.model = readFirstLine(dmi_path / "product_family");
    info_.manufacturer = readFirstLine(dmi_path / "sys_vendor");
    info_.bios_version = readFirstLine(dmi_path / "bios_version");
    info_.board_name = readFirstLine(dmi_path / "board_name");
}

// -------------------- CPU --------------------

void Fetcher::fetchCPUInfo() {
    std::ifstream f("/proc/cpuinfo");
    std::string line;
    bool found_model = false;
    bool found_vendor = false;
    
    while (std::getline(f, line)) {
        if (!found_model && line.starts_with("model name")) {
            info_.cpu.model = trim(line.substr(line.find(':') + 1));
            found_model = true;
        }
        else if (!found_vendor && line.starts_with("vendor_id")) {
            std::string vendor = trim(line.substr(line.find(':') + 1));
            if (vendor == "GenuineIntel") info_.cpu.vendor = "Intel";
            else if (vendor == "AuthenticAMD") info_.cpu.vendor = "AMD";
            else info_.cpu.vendor = vendor;
            found_vendor = true;
        }
        else if (line.starts_with("processor")) {
            info_.cpu.thread_count++;
        }
        else if (line.starts_with("cpu cores")) {
            info_.cpu.core_count = std::stoi(trim(line.substr(line.find(':') + 1)));
        }
        else if (line.starts_with("cpu MHz")) {
            std::string freq = trim(line.substr(line.find(':') + 1));
            info_.cpu.current_freq_ghz = std::stod(freq) / 1000.0;
        }
        
        // Stop after first processor block if we have model and vendor
        if (found_model && found_vendor && line.empty()) break;
    }

    const fs::path freq_path = "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq";
    if (fs::exists(freq_path)) {
        std::string freq = readFirstLine(freq_path);
        if (!freq.empty())
            info_.cpu.max_freq_ghz = std::stod(freq) / 1e6;
    }
    
    // Get core frequencies
    for (int i = 0; ; i++) {
        const fs::path core_freq_path = "/sys/devices/system/cpu/cpu" + std::to_string(i) + "/cpufreq/scaling_cur_freq";
        if (!fs::exists(core_freq_path)) break;
        
        std::string freq = readFirstLine(core_freq_path);
        if (!freq.empty()) {
            info_.cpu.core_freqs.push_back(std::stod(freq) / 1e6);
        }
    }
    
    info_.cpu.architecture = info_.architecture;
}

// -------------------- GPU --------------------

void Fetcher::fetchGPUInfo() {
    info_.gpus.clear();
    const fs::path drm_path = "/sys/class/drm";
    if (!fs::exists(drm_path)) return;

    // Map vendor IDs to names
    auto get_vendor_name = [](const std::string& vid) -> std::pair<std::string, bool> {
        if (vid == "0x8086") return {"Intel", true};
        if (vid == "0x10de") return {"NVIDIA", false};
        if (vid == "0x1002") return {"AMD", false};
        if (vid == "0x106b") return {"Apple", true};
        return {"Unknown", false};
    };

    for (const auto& entry : fs::directory_iterator(drm_path)) {
        std::string name = entry.path().filename().string();
        // Skip connectors (those with dash) and control devices
        if (!name.starts_with("card") || name.find('-') != std::string::npos)
            continue;

        GPU gpu;
        const fs::path device_path = entry.path() / "device";
        
        // Vendor
        std::string vid = readFirstLine(device_path / "vendor");
        auto [vendor, is_integrated] = get_vendor_name(vid);
        gpu.vendor = vendor;
        gpu.is_integrated = is_integrated;
        
        // Model - try multiple possible locations
        const std::vector<fs::path> model_paths = {
            device_path / "product_name",
            device_path / "model",
            device_path / "device"  // device ID file
        };
        
        for (const auto& model_path : model_paths) {
            if (fs::exists(model_path)) {
                std::string model = readFirstLine(model_path);
                if (!model.empty()) {
                    gpu.model = model;
                    break;
                }
            }
        }
        
        // Fallback to vendor + "GPU"
        if (gpu.model.empty()) {
            gpu.model = gpu.vendor + " GPU";
        }
        
        // Try to get driver
        const fs::path driver_path = entry.path() / "device" / "uevent";
        if (fs::exists(driver_path)) {
            std::ifstream uevent(driver_path);
            std::string line;
            while (std::getline(uevent, line)) {
                if (line.starts_with("DRIVER=")) {
                    gpu.driver = line.substr(7);
                    break;
                }
            }
        }
        
        // Avoid duplicates by checking if we already have this GPU
        auto it = std::find_if(info_.gpus.begin(), info_.gpus.end(),
            [&gpu](const GPU& existing) {
                return existing.vendor == gpu.vendor && 
                       existing.model == gpu.model;
            });
        
        if (it == info_.gpus.end()) {
            info_.gpus.push_back(gpu);
        }
    }
}

// -------------------- MEMORY / SWAP --------------------

void Fetcher::fetchMemoryInfo() {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info_.memory.total_bytes = si.totalram * si.mem_unit;
        info_.memory.free_bytes  = si.freeram * si.mem_unit;
        info_.memory.used_bytes  = (info_.memory.total_bytes - info_.memory.free_bytes)/2;
        info_.memory.available_bytes = info_.memory.free_bytes; // Simplified
        info_.memory.usage_percent = (info_.memory.total_bytes > 0) ? 
            static_cast<int>((info_.memory.used_bytes * 100) / info_.memory.total_bytes) : 0;
    }
}

void Fetcher::fetchSwapInfo() {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info_.swap.total_bytes = si.totalswap * si.mem_unit;
        info_.swap.free_bytes  = si.freeswap * si.mem_unit;
        info_.swap.used_bytes  = info_.swap.total_bytes - info_.swap.free_bytes;
        info_.swap.usage_percent = (info_.swap.total_bytes > 0) ? 
            static_cast<int>((info_.swap.used_bytes * 100) / info_.swap.total_bytes) : 0;
    }
}

// -------------------- DISK --------------------

void Fetcher::fetchDiskInfo() {
    info_.disks.clear();
    
    // Read mount points from /proc/mounts
    std::ifstream mounts("/proc/mounts");
    std::string line;
    
    while (std::getline(mounts, line)) {
        std::istringstream iss(line);
        std::string device, mount_point, type;
        iss >> device >> mount_point >> type;
        
        // Skip non-physical filesystems
        if (device.find("/dev/") != 0) continue;
        if (type == "tmpfs" || type == "proc" || type == "sysfs" || 
            type == "devtmpfs" || type == "cgroup" || type == "overlay") continue;
        
        struct statvfs st;
        if (statvfs(mount_point.c_str(), &st) == 0) {
            Disk disk;
            disk.mount_point = mount_point;
            disk.filesystem = type;
            disk.total_bytes = st.f_blocks * st.f_frsize;
            disk.free_bytes = st.f_bfree * st.f_frsize;
            disk.available_bytes = st.f_bavail * st.f_frsize;
            disk.used_bytes = disk.total_bytes - disk.free_bytes;
            disk.usage_percent = (disk.total_bytes > 0) ? 
                static_cast<int>((disk.used_bytes * 100) / disk.total_bytes) : 0;
            info_.disks.push_back(disk);
        }
    }
}

// -------------------- DISPLAY --------------------

void Fetcher::fetchDisplayInfo() {
    info_.displays.clear();
    const fs::path drm_path = "/sys/class/drm";
    if (!fs::exists(drm_path)) return;

    for (const auto& entry : fs::directory_iterator(drm_path)) {
        std::string name = entry.path().filename().string();
        // Look for connector directories (contain dash)
        if (name.find('-') == std::string::npos) continue;
        
        const fs::path status_path = entry.path() / "status";
        if (fs::exists(status_path)) {
            std::string status = readFirstLine(status_path);
            if (status == "connected") {
                Display display;
                display.output_name = name;
                display.name = name;
                
                // Check if it's built-in (eDP typically is)
                if (name.find("eDP") != std::string::npos) {
                    display.is_builtin = true;
                }
                
                // Try to get mode from modes file
                const fs::path modes_path = entry.path() / "modes";
                if (fs::exists(modes_path)) {
                    std::string mode = readFirstLine(modes_path);
                    if (!mode.empty()) {
                        display.current_mode = mode;
                        // Parse resolution from mode (e.g., "1920x1080")
                        size_t x_pos = mode.find('x');
                        if (x_pos != std::string::npos) {
                            std::string width_str = mode.substr(0, x_pos);
                            std::string rest = mode.substr(x_pos + 1);
                            size_t at_pos = rest.find('@');
                            std::string height_str = (at_pos != std::string::npos) ? 
                                rest.substr(0, at_pos) : rest;
                            
                            try {
                                display.width = std::stoi(width_str);
                                display.height = std::stoi(height_str);
                            } catch (...) {
                                // Ignore conversion errors
                            }
                        }
                    }
                }
                
                info_.displays.push_back(display);
            }
        }
    }
}

// -------------------- NETWORK --------------------

void Fetcher::fetchNetworkInfo() {
    info_.network_interfaces.clear();
    const fs::path net_path = "/sys/class/net";
    if (!fs::exists(net_path)) return;

    for (const auto& entry : fs::directory_iterator(net_path)) {
        std::string ifname = entry.path().filename().string();
        if (ifname == "lo") continue;
        
        NetworkInterface nic;
        nic.name = ifname;
        nic.mac = readFirstLine(entry.path() / "address");
        
        // Get operational state
        const fs::path operstate_path = entry.path() / "operstate";
        if (fs::exists(operstate_path)) {
            nic.operstate = readFirstLine(operstate_path);
            nic.is_up = (nic.operstate == "up");
        }
        
        // Check if wireless
        const fs::path wireless_path = entry.path() / "wireless";
        if (fs::exists(wireless_path)) {
            nic.is_wireless = true;
        }
        
        // Get IP addresses from /proc/net/fib_trie (simplified)
        std::ifstream fib("/proc/net/fib_trie");
        std::string line;
        while (std::getline(fib, line)) {
            if (line.find(ifname) != std::string::npos) {
                size_t pos = line.find("/32 host");
                if (pos != std::string::npos) {
                    std::string ip = line.substr(0, pos);
                    // Simple IPv4 check
                    if (ip.find('.') != std::string::npos && ip != "127.0.0.1") {
                        if (nic.ipv4.empty()) {
                            nic.ipv4 = ip;  // Set primary IPv4
                        }
                        nic.ipv4_addresses.push_back(ip);
                    }
                }
            }
        }
        
        info_.network_interfaces.push_back(nic);
    }
}

// -------------------- BATTERY --------------------

void Fetcher::fetchBatteryInfo() {
    info_.batteries.clear();
    const fs::path power_path = "/sys/class/power_supply";
    if (!fs::exists(power_path)) return;

    for (const auto& entry : fs::directory_iterator(power_path)) {
        const fs::path type_path = entry.path() / "type";
        if (!fs::exists(type_path)) continue;
        
        std::string type = readFirstLine(type_path);
        if (type != "Battery") continue;
        
        Battery battery;
        battery.name = entry.path().filename().string();
        
        // Capacity
        const fs::path capacity_path = entry.path() / "capacity";
        if (fs::exists(capacity_path)) {
            std::string cap = readFirstLine(capacity_path);
            if (!cap.empty()) battery.percentage = std::stoi(cap);
        }
        
        // Status
        const fs::path status_path = entry.path() / "status";
        if (fs::exists(status_path)) {
            battery.status = readFirstLine(status_path);
            battery.is_charging = (battery.status == "Charging");
            battery.ac_connected = (battery.status == "Charging" || battery.status == "Full");
        }
        
        // Voltage
        const fs::path voltage_path = entry.path() / "voltage_now";
        if (fs::exists(voltage_path)) {
            std::string volt = readFirstLine(voltage_path);
            if (!volt.empty()) {
                double volts = std::stod(volt);
                battery.voltage = volts / 1e6;  // Convert µV to V
            }
        }
        
        // Capacity in mAh
        const fs::path energy_full_path = entry.path() / "energy_full";
        const fs::path charge_full_path = entry.path() / "charge_full";
        if (fs::exists(energy_full_path)) {
            std::string energy = readFirstLine(energy_full_path);
            if (!energy.empty() && battery.voltage > 0) {
                double energy_wh = std::stod(energy) / 1e6;  // µWh to Wh
                battery.capacity_mah = static_cast<int>((energy_wh * 1000) / battery.voltage);
            }
        } else if (fs::exists(charge_full_path)) {
            std::string charge = readFirstLine(charge_full_path);
            if (!charge.empty()) {
                battery.capacity_mah = std::stoi(charge) / 1000;  // µAh to mAh
            }
        }
        
        info_.batteries.push_back(battery);
    }
}

// -------------------- SHELL / TERMINAL / DE --------------------

void Fetcher::fetchShellInfo() {
    if (const char* s = getenv("SHELL"))
        info_.shell = fs::path(s).filename().string();
}

void Fetcher::fetchTerminalInfo() {
    // Try multiple environment variables
    const char* env_vars[] = {"TERM_PROGRAM", "TERMINAL_EMULATOR", "TERM", nullptr};
    for (const char** var = env_vars; *var; ++var) {
        if (const char* t = getenv(*var)) {
            info_.terminal = t;
            break;
        }
    }
}

void Fetcher::fetchDesktopEnvironment() {
    // Try multiple environment variables
    const char* env_vars[] = {"XDG_CURRENT_DESKTOP", "DESKTOP_SESSION", "GDMSESSION", nullptr};
    for (const char** var = env_vars; *var; ++var) {
        if (const char* d = getenv(*var)) {
            info_.de.name = d;
            break;
        }
    }
}

// -------------------- UPTIME / LOCALE --------------------

void Fetcher::fetchUptimeInfo() {
    // Read from /proc/uptime for more precision
    std::ifstream uptime_file("/proc/uptime");
    if (uptime_file) {
        double uptime_seconds;
        uptime_file >> uptime_seconds;
        info_.uptime_seconds = static_cast<long>(uptime_seconds);
    } else {
        // Fallback to sysinfo
        struct sysinfo si;
        if (sysinfo(&si) == 0)
            info_.uptime_seconds = si.uptime;
    }
}

void Fetcher::fetchLocaleInfo() {
    // Try multiple locale variables
    const char* env_vars[] = {"LC_ALL", "LC_MESSAGES", "LANG", nullptr};
    for (const char** var = env_vars; *var; ++var) {
        if (const char* l = getenv(*var)) {
            info_.locale = l;
            break;
        }
    }
}

// -------------------- UTILITY FUNCTIONS --------------------

std::string formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    int unit = 0;
    double value = static_cast<double>(bytes);
    
    while (value >= 1024.0 && unit < 5) {
        value /= 1024.0;
        unit++;
    }
    
    char buffer[32];
    if (value < 10.0 && unit > 0) {
        snprintf(buffer, sizeof(buffer), "%.2f %s", value, units[unit]);
    } else {
        snprintf(buffer, sizeof(buffer), "%.1f %s", value, units[unit]);
    }
    
    return std::string(buffer);
}

std::string formatUptime(uint64_t seconds) {
    uint64_t days = seconds / 86400;
    uint64_t hours = (seconds % 86400) / 3600;
    uint64_t minutes = (seconds % 3600) / 60;
    
    char buffer[64];
    if (days > 0) {
        snprintf(buffer, sizeof(buffer), "%lud %02luh %02lum", days, hours, minutes);
    } else if (hours > 0) {
        snprintf(buffer, sizeof(buffer), "%luh %02lum", hours, minutes);
    } else {
        snprintf(buffer, sizeof(buffer), "%lum", minutes);
    }
    
    return std::string(buffer);
}

std::string formatMemory(uint64_t bytes) {
    return formatBytes(bytes);
}

std::string formatFrequency(double ghz) {
    char buffer[32];
    if (ghz >= 1.0) {
        snprintf(buffer, sizeof(buffer), "%.2f GHz", ghz);
    } else {
        snprintf(buffer, sizeof(buffer), "%.0f MHz", ghz * 1000);
    }
    return std::string(buffer);
}

int calculatePercentage(uint64_t used, uint64_t total) {
    if (total == 0) return 0;
    return static_cast<int>((used * 100) / total);
}

double bytesToGiB(uint64_t bytes) {
    return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
}

double bytesToMiB(uint64_t bytes) {
    return static_cast<double>(bytes) / (1024.0 * 1024.0);
}

} // namespace SystemInfo