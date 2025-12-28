#if defined(_WIN32) || defined(_WIN64)
    #include "sysinfo.win.hpp"
#else
    #include "sysinfo.hpp"
#endif
#include <iostream>
#include <iomanip>

using namespace SystemInfo;
using namespace std;

int main() {
    cout << "=== Nacfetch Library Test Suite ===\n\n";

    // Test 1: Basic fetch with all flags enabled
    cout << "Test 1: Full system information fetch\n";
    cout << "--------------------------------------\n";
    // Fetcher::Fetcher() = default;
    // Fetcher::~Fetcher() = default;

    Fetcher fetcher1;
    fetcher1.fetchInfo();
    const Info& info = fetcher1.getInfo();
    
    cout << "User: " << info.username << "@" << info.hostname << "\n";
    cout << "OS: " << info.os_name << "\n";
    cout << "Kernel: " << info.kernel << "\n";
    
    if (!info.cpu.model.empty()) {
        cout << "CPU: " << info.cpu.model << " (" << info.cpu.thread_count << " threads)\n";
    }
    
    if (info.memory.total_bytes > 0) {
        cout << "RAM: " << formatMemory(info.memory.used_bytes) << " / " 
             << formatMemory(info.memory.total_bytes) << " (" 
             << info.memory.usage_percent << "%)\n";
    }
    
    cout << "\n";

    // Test 2: Selective fetching (skip GPU and display)
    cout << "Test 2: Selective information fetch (no GPU, no display)\n";
    cout << "--------------------------------------------------------\n";
    
    Flags custom_flags;
    custom_flags.gpu = false;
    custom_flags.display = false;
    
    Fetcher fetcher2;
    fetcher2.fetchInfo(custom_flags);
    const Info& info2 = fetcher2.getInfo();
    
    cout << "Hostname: " << info2.hostname << "\n";
    cout << "OS: " << info2.os_name << "\n";
    
    if (info2.uptime_seconds > 0) {
        cout << "Uptime: " << formatUptime(info2.uptime_seconds) << "\n";
    }
    
    cout << "\n";

    // Test 3: Memory-focused test
    cout << "Test 3: Memory information\n";
    cout << "--------------------------\n";
    
    Flags mem_flags;
    mem_flags.cpu = false;
    mem_flags.gpu = false;
    mem_flags.disk = false;
    mem_flags.display = false;
    mem_flags.network = false;
    mem_flags.battery = false;
    mem_flags.packages = false;
    
    Fetcher fetcher3;
    fetcher3.fetchInfo(mem_flags);
    const Info& info3 = fetcher3.getInfo();
    
    if (info3.memory.total_bytes > 0) {
        cout << "Memory Statistics:\n";
        cout << "  Total:     " << formatMemory(info3.memory.total_bytes) << "\n";
        cout << "  Used:      " << formatMemory(info3.memory.used_bytes) << "\n";
        cout << "  Free:      " << formatMemory(info3.memory.free_bytes) << "\n";
        cout << "  Available: " << formatMemory(info3.memory.available_bytes) << "\n";
        cout << "  Cached:    " << formatMemory(info3.memory.cached_bytes) << "\n";
        cout << "  Usage:     " << info3.memory.usage_percent << "%\n";
    }
    
    if (info3.swap.total_bytes > 0) {
        cout << "\nSwap Statistics:\n";
        cout << "  Total: " << formatMemory(info3.swap.total_bytes) << "\n";
        cout << "  Used:  " << formatMemory(info3.swap.used_bytes) << "\n";
        cout << "  Free:  " << formatMemory(info3.swap.free_bytes) << "\n";
        cout << "  Usage: " << info3.swap.usage_percent << "%\n";
    }
    
    cout << "\n";

    // Test 4: Hardware information
    cout << "Test 4: Hardware information\n";
    cout << "----------------------------\n";
    
    Flags hw_flags;
    hw_flags.os = false;
    hw_flags.shell = false;
    hw_flags.terminal = false;
    hw_flags.memory = false;
    hw_flags.swap = false;
    hw_flags.disk = false;
    hw_flags.network = false;
    hw_flags.battery = false;
    hw_flags.de = false;
    hw_flags.packages = false;
    hw_flags.uptime = false;
    
    Fetcher fetcher4;
    fetcher4.fetchInfo(hw_flags);
    const Info& info4 = fetcher4.getInfo();
    
    if (!info4.model.empty()) {
        cout << "Model: " << info4.model << "\n";
    }
    
    if (!info4.cpu.model.empty()) {
        cout << "CPU: " << info4.cpu.model << "\n";
        cout << "  Cores: " << info4.cpu.core_count << "\n";
        cout << "  Threads: " << info4.cpu.thread_count << "\n";
        if (info4.cpu.max_freq_ghz > 0) {
            cout << "  Max Freq: " << fixed << setprecision(2) 
                 << info4.cpu.max_freq_ghz << " GHz\n";
        }
    }
    
    if (!info4.gpus.empty()) {
        cout << "GPUs:\n";
        for (size_t i = 0; i < info4.gpus.size(); ++i) {
            cout << "  [" << i + 1 << "] " << info4.gpus[i].model;
            if (info4.gpus[i].is_integrated) {
                cout << " (Integrated)";
            }
            cout << "\n";
        }
    }
    
    cout << "\n";

    // Test 5: Network and storage
    cout << "Test 5: Network and Storage\n";
    cout << "---------------------------\n";
    
    Flags net_flags;
    net_flags.cpu = false;
    net_flags.gpu = false;
    net_flags.memory = false;
    net_flags.swap = false;
    net_flags.display = false;
    net_flags.battery = false;
    net_flags.de = false;
    net_flags.packages = false;
    
    Fetcher fetcher5;
    fetcher5.fetchInfo(net_flags);
    const Info& info5 = fetcher5.getInfo();
    
    if (!info5.network_interfaces.empty()) {
        cout << "Network Interfaces:\n";
        for (const auto& net : info5.network_interfaces) {
            cout << "  " << net.name;
            if (net.is_wireless) cout << " (Wireless)";
            cout << ": ";
            if (!net.ipv4.empty()) cout << net.ipv4;
            if (!net.ipv6.empty()) {
                if (!net.ipv4.empty()) cout << ", ";
                cout << net.ipv6;
            }
            cout << "\n";
        }
    }
    
    if (!info5.disks.empty()) {
        cout << "\nDisk Usage:\n";
        for (const auto& disk : info5.disks) {
            cout << "  " << disk.mount_point << " (" << disk.filesystem << "): ";
            cout << formatBytes(disk.used_bytes) << " / " 
                 << formatBytes(disk.total_bytes);
            cout << " (" << disk.usage_percent << "%)\n";
        }
    }
    
    cout << "\n";

    // Test 6: Package information
    cout << "Test 6: Package Management\n";
    cout << "--------------------------\n";
    
    Flags pkg_flags;
    pkg_flags.cpu = false;
    pkg_flags.gpu = false;
    pkg_flags.memory = false;
    pkg_flags.swap = false;
    pkg_flags.disk = false;
    pkg_flags.display = false;
    pkg_flags.network = false;
    pkg_flags.battery = false;
    pkg_flags.de = false;
    
    Fetcher fetcher6;
    fetcher6.fetchInfo(pkg_flags);
    const Info& info6 = fetcher6.getInfo();

    // Test 7: Utility functions
    cout << "Test 7: Utility Functions\n";
    cout << "-------------------------\n";
    
    cout << "formatBytes(1024): " << formatBytes(1024) << "\n";
    cout << "formatBytes(1048576): " << formatBytes(1048576) << "\n";
    cout << "formatBytes(1073741824): " << formatBytes(1073741824) << "\n";
    cout << "formatUptime(3665): " << formatUptime(3665) << "\n";
    cout << "formatUptime(90061): " << formatUptime(90061) << "\n";
    cout << "calculatePercentage(8192, 16384): " << calculatePercentage(8192, 16384) << "%\n";
    cout << "bytesToGiB(1073741824): " << fixed << setprecision(2) 
         << bytesToGiB(1073741824) << " GiB\n";
    
    cout << "\n";

    // Test 8: Display and Desktop Environment
    cout << "Test 8: Display & Desktop Environment\n";
    cout << "-------------------------------------\n";
    
    Flags de_flags;
    de_flags.cpu = false;
    de_flags.gpu = false;
    de_flags.memory = false;
    de_flags.swap = false;
    de_flags.disk = false;
    de_flags.network = false;
    de_flags.battery = false;
    de_flags.packages = false;
    
    Fetcher fetcher8;
    fetcher8.fetchInfo(de_flags);
    const Info& info8 = fetcher8.getInfo();
    
    if (!info8.displays.empty()) {
        cout << "Displays:\n";
        for (const auto& display : info8.displays) {
            cout << "  " << display.width << "x" << display.height;
            if (display.refresh_rate > 0) {
                cout << " @ " << display.refresh_rate << "Hz";
            }
            if (display.is_builtin) {
                cout << " (Built-in)";
            }
            cout << "\n";
        }
    }
    
    if (!info8.de.name.empty()) {
        cout << "\nDesktop Environment:\n";
        cout << "  DE: " << info8.de.name << "\n";
        if (!info8.de.wm_name.empty()) {
            cout << "  WM: " << info8.de.wm_name;
            if (!info8.de.wm_protocol.empty()) {
                cout << " " << info8.de.wm_protocol;
            }
            cout << "\n";
        }
        if (!info8.de.theme.empty()) {
            cout << "  Theme: " << info8.de.theme << "\n";
        }
        if (!info8.de.icon_theme.empty()) {
            cout << "  Icons: " << info8.de.icon_theme << "\n";
        }
    }
    
    cout << "\n";

    // Test 9: Battery information
    cout << "Test 9: Battery Status\n";
    cout << "----------------------\n";
    
    Flags bat_flags;
    bat_flags.cpu = false;
    bat_flags.gpu = false;
    bat_flags.memory = false;
    bat_flags.swap = false;
    bat_flags.disk = false;
    bat_flags.display = false;
    bat_flags.network = false;
    bat_flags.de = false;
    bat_flags.packages = false;
    
    Fetcher fetcher9;
    fetcher9.fetchInfo(bat_flags);
    const Info& info9 = fetcher9.getInfo();
    
    if (!info9.batteries.empty()) {
        for (const auto& battery : info9.batteries) {
            cout << "Battery (" << battery.name << "):\n";
            cout << "  Level: " << battery.percentage << "%\n";
            cout << "  Status: " << battery.status << "\n";
            cout << "  Charging: " << (battery.is_charging ? "Yes" : "No") << "\n";
            cout << "  AC Connected: " << (battery.ac_connected ? "Yes" : "No") << "\n";
        }
    } else {
        cout << "No batteries detected (desktop system?)\n";
    }
    
    cout << "\n=== All Tests Complete ===\n";
    
    return 0;
}