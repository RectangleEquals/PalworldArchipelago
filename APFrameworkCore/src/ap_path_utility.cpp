#include "ap_path_utility.h"
#include <windows.h>
#include <algorithm>

namespace APFramework {

// Static member initialization
std::filesystem::path APPathUtility::cached_dll_path_;
std::filesystem::path APPathUtility::cached_dll_directory_;
std::optional<std::filesystem::path> APPathUtility::cached_ue4ss_folder_;
std::optional<std::filesystem::path> APPathUtility::cached_mods_folder_;
std::optional<std::filesystem::path> APPathUtility::cached_binaries_folder_;
bool APPathUtility::cache_initialized_ = false;

// Declare __ImageBase as external linker symbol (defined by MSVC linker)
// This points to the base address of the current module (DLL)
extern "C" IMAGE_DOS_HEADER __ImageBase;

bool APPathUtility::is_absolute(const std::string& path) {
    return std::filesystem::path(path).is_absolute();
}

bool APPathUtility::is_absolute(const std::filesystem::path& path) {
    return path.is_absolute();
}

bool APPathUtility::file_exists(const std::string& path) {
    return file_exists(std::filesystem::path(path));
}

bool APPathUtility::file_exists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec);
}

bool APPathUtility::directory_exists(const std::string& path) {
    return directory_exists(std::filesystem::path(path));
}

bool APPathUtility::directory_exists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec);
}

std::filesystem::path APPathUtility::to_absolute(const std::string& path) {
    return to_absolute(std::filesystem::path(path));
}

std::filesystem::path APPathUtility::to_absolute(const std::filesystem::path& path) {
    if (path.is_absolute()) {
        return path;
    }

    // Resolve relative paths from DLL directory
    return get_dll_directory() / path;
}

std::filesystem::path APPathUtility::get_dll_path() {
    if (!cache_initialized_) {
        initialize_cache();
    }
    return cached_dll_path_;
}

std::filesystem::path APPathUtility::get_dll_directory() {
    if (!cache_initialized_) {
        initialize_cache();
    }
    return cached_dll_directory_;
}

std::optional<std::filesystem::path> APPathUtility::find_ue4ss_folder() {
    if (!cache_initialized_) {
        initialize_cache();
    }
    return cached_ue4ss_folder_;
}

std::optional<std::filesystem::path> APPathUtility::find_mods_folder() {
    if (!cache_initialized_) {
        initialize_cache();
    }
    return cached_mods_folder_;
}

std::optional<std::filesystem::path> APPathUtility::find_binaries_folder() {
    if (!cache_initialized_) {
        initialize_cache();
    }
    return cached_binaries_folder_;
}

std::optional<std::filesystem::path> APPathUtility::resolve_relative_to_mods(const std::string& path) {
    return resolve_relative_to_mods(std::filesystem::path(path));
}

std::optional<std::filesystem::path> APPathUtility::resolve_relative_to_mods(const std::filesystem::path& path) {
    // If absolute, validate and return
    if (path.is_absolute()) {
        if (directory_exists(path) || file_exists(path)) {
            return path;
        }
        return std::nullopt;
    }

    // Try resolving from Mods folder
    auto mods_folder = find_mods_folder();
    if (mods_folder.has_value()) {
        auto resolved = mods_folder.value() / path;
        if (directory_exists(resolved) || file_exists(resolved)) {
            return resolved;
        }
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> APPathUtility::resolve_path(const std::string& path) {
    return resolve_path(std::filesystem::path(path));
}

std::optional<std::filesystem::path> APPathUtility::resolve_path(const std::filesystem::path& path) {
    // Strategy 1: If absolute and exists, return it
    if (path.is_absolute()) {
        if (directory_exists(path) || file_exists(path)) {
            return path;
        }
        return std::nullopt;
    }

    // Strategy 2: Try resolving from DLL directory
    auto dll_dir_path = get_dll_directory() / path;
    if (directory_exists(dll_dir_path) || file_exists(dll_dir_path)) {
        return dll_dir_path;
    }

    // Strategy 3: Try resolving from ue4ss folder
    auto ue4ss_folder = find_ue4ss_folder();
    if (ue4ss_folder.has_value()) {
        auto ue4ss_path = ue4ss_folder.value() / path;
        if (directory_exists(ue4ss_path) || file_exists(ue4ss_path)) {
            return ue4ss_path;
        }
    }

    // Strategy 4: Try resolving from Mods folder
    auto mods_folder = find_mods_folder();
    if (mods_folder.has_value()) {
        auto mods_path = mods_folder.value() / path;
        if (directory_exists(mods_path) || file_exists(mods_path)) {
            return mods_path;
        }
    }

    // Could not resolve
    return std::nullopt;
}

void APPathUtility::initialize_cache() {
    if (cache_initialized_) {
        return;
    }

    // Get DLL path using GetModuleFileName with __ImageBase trick
    HMODULE hm = reinterpret_cast<HMODULE>(&__ImageBase);
    char path_buffer[MAX_PATH];
    DWORD result = GetModuleFileNameA(hm, path_buffer, MAX_PATH);

    if (result > 0 && result < MAX_PATH) {
        cached_dll_path_ = std::filesystem::path(path_buffer);
        cached_dll_directory_ = cached_dll_path_.parent_path();
    } else {
        // Fallback: use current working directory (should not happen)
        std::error_code ec;
        cached_dll_directory_ = std::filesystem::current_path(ec);
        cached_dll_path_ = cached_dll_directory_ / "APFrameworkCore.dll";
    }

    // Find UE4SS folder by searching upward for "ue4ss" within a Binaries directory
    // Expected structure: <game>/Binaries/<platform>/ue4ss/
    std::filesystem::path current = cached_dll_directory_;
    std::filesystem::path ue4ss_candidate;

    for (int i = 0; i < 10; ++i) {  // Limit search depth to avoid infinite loop
        // Check if current directory is named "ue4ss"
        if (current.filename() == "ue4ss") {
            ue4ss_candidate = current;

            // Verify parent contains "Binaries" somewhere in the path
            std::filesystem::path parent = current.parent_path();
            std::string parent_str = parent.string();
            std::string parent_lower = parent_str;
            std::transform(parent_lower.begin(), parent_lower.end(), parent_lower.begin(), ::tolower);

            if (parent_lower.find("binaries") != std::string::npos) {
                cached_ue4ss_folder_ = ue4ss_candidate;
                break;
            }
        }

        // Also check if current directory has a "ue4ss" subdirectory
        auto ue4ss_subdir = current / "ue4ss";
        if (directory_exists(ue4ss_subdir)) {
            // Verify we're within a Binaries directory
            std::string current_str = current.string();
            std::string current_lower = current_str;
            std::transform(current_lower.begin(), current_lower.end(), current_lower.begin(), ::tolower);

            if (current_lower.find("binaries") != std::string::npos) {
                cached_ue4ss_folder_ = ue4ss_subdir;
                break;
            }
        }

        // Move up one directory
        if (current == current.parent_path()) {
            break;  // Reached filesystem root
        }
        current = current.parent_path();
    }

    // Find Mods folder (ue4ss/Mods)
    if (cached_ue4ss_folder_.has_value()) {
        auto mods_path = cached_ue4ss_folder_.value() / "Mods";
        if (directory_exists(mods_path)) {
            cached_mods_folder_ = mods_path;
        }
    }

    // Find Binaries folder by searching upward
    current = cached_dll_directory_;
    for (int i = 0; i < 10; ++i) {
        std::string current_str = current.string();
        std::string current_lower = current_str;
        std::transform(current_lower.begin(), current_lower.end(), current_lower.begin(), ::tolower);

        if (current_lower.find("binaries") != std::string::npos) {
            // Check if this directory has typical game structure (Win64, etc.)
            bool has_platform_dir = false;
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(current, ec)) {
                if (entry.is_directory()) {
                    std::string dir_name = entry.path().filename().string();
                    std::string dir_name_lower = dir_name;
                    std::transform(dir_name_lower.begin(), dir_name_lower.end(), dir_name_lower.begin(), ::tolower);

                    if (dir_name_lower == "win64" || dir_name_lower == "win32" ||
                        dir_name_lower.find("linux") != std::string::npos) {
                        has_platform_dir = true;
                        break;
                    }
                }
            }

            if (has_platform_dir) {
                cached_binaries_folder_ = current;
                break;
            }
        }

        if (current == current.parent_path()) {
            break;
        }
        current = current.parent_path();
    }

    cache_initialized_ = true;
}

} // namespace APFramework