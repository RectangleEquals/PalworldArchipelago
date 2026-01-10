project(APFrameworkCore)

# Source files
set(SOURCES
    src/ap_manager.cpp
    src/ap_client.cpp
    src/ap_ipc_server.cpp
    src/ap_config.cpp
    src/ap_logger.cpp
    src/ap_debug_log.cpp
    src/ap_mod_registry.cpp
    src/ap_capabilities_generator.cpp
    src/ap_message_router.cpp
    src/ap_polling_thread.cpp
    src/lua_bindings.cpp
    src/main.cpp
)

# Header files (for IDE organization)
set(HEADERS
    include/ap_manager.h
    include/ap_client.h
    include/ap_ipc_server.h
    include/ap_config.h
    include/ap_logger.h
    include/ap_debug_log.h
    include/ap_mod_registry.h
    include/ap_capabilities_generator.h
    include/ap_message_router.h
    include/ap_polling_thread.h
    include/ap_types.h
)

# Create shared library (DLL)
add_library(APFrameworkCore SHARED ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(APFrameworkCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/nlohmann
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/sol2/include
)

# Link libraries
target_link_libraries(APFrameworkCore
    PRIVATE
        apclientpp
        lua_static
)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(APFrameworkCore PRIVATE
        _WIN32_WINNT=0x0601  # Windows 7+
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

# Export symbols for DLL
target_compile_definitions(APFrameworkCore PRIVATE APFRAMEWORK_EXPORTS)

# Set output name
set_target_properties(APFrameworkCore PROPERTIES
    OUTPUT_NAME "APFrameworkCore"
    PREFIX ""
)