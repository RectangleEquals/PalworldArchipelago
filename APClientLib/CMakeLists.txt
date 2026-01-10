project(APClientLib)

# Source files
set(SOURCES
    src/ap_client_lib.cpp
    src/ap_ipc_client.cpp
    src/lua_bindings.cpp
)

# Header files
set(HEADERS
    include/ap_client_lib.h
    include/ap_ipc_client.h
)

# Create shared library (DLL)
add_library(APClientLib SHARED ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(APClientLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/sol2/include
)

# Link libraries
target_link_libraries(APClientLib
    PRIVATE
        lua_static
)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(APClientLib PRIVATE
        _WIN32_WINNT=0x0601
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

# Export symbols for DLL
target_compile_definitions(APClientLib PRIVATE APCLIENTLIB_EXPORTS)

# Set output name
set_target_properties(APClientLib PROPERTIES
    OUTPUT_NAME "APClientLib"
    PREFIX ""
)