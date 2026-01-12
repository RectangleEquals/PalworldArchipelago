#pragma once
#include <string>
#include <filesystem>
#include <optional>

namespace APFramework {

/**
 * Static utility class for path resolution and DLL location discovery
 *
 * This class provides robust path handling for the framework, ensuring
 * all file paths are correctly resolved relative to the running DLL,
 * UE4SS installation, or absolute paths.
 */
class APPathUtility {
public:
    // Delete instantiation - static utility class only
    APPathUtility() = delete;
    APPathUtility(const APPathUtility&) = delete;
    APPathUtility& operator=(const APPathUtility&) = delete;

    /**
     * Check if a path is absolute
     * @param path Path to check (string or filesystem::path)
     * @return true if path is absolute, false if relative
     */
    static bool is_absolute(const std::string& path);
    static bool is_absolute(const std::filesystem::path& path);

    /**
     * Check if a path exists as a file
     * @param path Path to check
     * @return true if file exists
     */
    static bool file_exists(const std::string& path);
    static bool file_exists(const std::filesystem::path& path);

    /**
     * Check if a path exists as a directory
     * @param path Path to check
     * @return true if directory exists
     */
    static bool directory_exists(const std::string& path);
    static bool directory_exists(const std::filesystem::path& path);

    /**
     * Convert a relative path to absolute
     * If path is already absolute, returns it unchanged
     * If relative, resolves from the DLL directory
     * @param path Relative or absolute path
     * @return Absolute path
     */
    static std::filesystem::path to_absolute(const std::string& path);
    static std::filesystem::path to_absolute(const std::filesystem::path& path);

    /**
     * Get the absolute path of the currently running DLL
     * Uses GetModuleFileName with __ImageBase trick
     * @return Path to the DLL file (e.g., "E:\...\Win64\ue4ss\Mods\APFrameworkMod\Scripts\APFrameworkCore.dll")
     */
    static std::filesystem::path get_dll_path();

    /**
     * Get the directory containing the currently running DLL
     * @return Directory path (e.g., "E:\...\Win64\ue4ss\Mods\APFrameworkMod\Scripts")
     */
    static std::filesystem::path get_dll_directory();

    /**
     * Find the UE4SS installation folder
     * Searches upward from DLL path for "ue4ss" folder within a Binaries directory
     * @return Path to ue4ss folder (e.g., "E:\...\Win64\ue4ss"), or empty if not found
     */
    static std::optional<std::filesystem::path> find_ue4ss_folder();

    /**
     * Find the UE4SS Mods folder
     * Searches for ue4ss/Mods folder relative to DLL location
     * @return Path to Mods folder (e.g., "E:\...\Win64\ue4ss\Mods"), or empty if not found
     */
    static std::optional<std::filesystem::path> find_mods_folder();

    /**
     * Get the game's Binaries directory (e.g., "E:\...\Palworld\Pal\Binaries\Win64")
     * Searches upward from DLL path for a Binaries folder
     * @return Path to Binaries directory, or empty if not found
     */
    static std::optional<std::filesystem::path> find_binaries_folder();

    /**
     * Resolve a path relative to the Mods folder if it's relative
     * If absolute, validates it exists and returns it
     * @param path Path to resolve
     * @return Resolved absolute path, or empty if cannot resolve or doesn't exist
     */
    static std::optional<std::filesystem::path> resolve_relative_to_mods(const std::string& path);
    static std::optional<std::filesystem::path> resolve_relative_to_mods(const std::filesystem::path& path);

    /**
     * Resolve a path with multiple fallback strategies:
     * 1. If absolute and exists, return it
     * 2. If relative, try resolving from DLL directory
     * 3. If relative, try resolving from ue4ss folder
     * 4. If relative, try resolving from Mods folder
     * @param path Path to resolve
     * @return Resolved absolute path, or empty if cannot resolve
     */
    static std::optional<std::filesystem::path> resolve_path(const std::string& path);
    static std::optional<std::filesystem::path> resolve_path(const std::filesystem::path& path);

private:
    // Cached paths to avoid repeated expensive lookups
    static std::filesystem::path cached_dll_path_;
    static std::filesystem::path cached_dll_directory_;
    static std::optional<std::filesystem::path> cached_ue4ss_folder_;
    static std::optional<std::filesystem::path> cached_mods_folder_;
    static std::optional<std::filesystem::path> cached_binaries_folder_;
    static bool cache_initialized_;

    // Initialize cache (called once on first use)
    static void initialize_cache();
};

} // namespace APFramework
