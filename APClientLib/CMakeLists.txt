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

# Post-build: Copy DLL to both mod directories
add_custom_command(TARGET APClientLib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:APClientLib>
        ${CMAKE_CURRENT_SOURCE_DIR}/../Mods/APFrameworkMod/Scripts/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:APClientLib>
        ${CMAKE_CURRENT_SOURCE_DIR}/../Mods/ExampleClientMod/Scripts/
    COMMENT "Copying APClientLib.dll to mod directories"
)

# Post-build: Copy APClient.lua wrapper to APFrameworkMod
add_custom_command(TARGET APClientLib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../Mods/ExampleClientMod/Scripts/APClient.lua
        ${CMAKE_CURRENT_SOURCE_DIR}/../Mods/APFrameworkMod/Scripts/APClient.lua
    COMMENT "Copying APClient.lua to APFrameworkMod"
)

# Post-build: Copy lunajson library to both mods
add_custom_command(TARGET APClientLib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lua/lunajson.lua
        ${CMAKE_CURRENT_SOURCE_DIR}/../Mods/APFrameworkMod/Scripts/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lua/lunajson.lua
        ${CMAKE_CURRENT_SOURCE_DIR}/../Mods/ExampleClientMod/Scripts/
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lua/lunajson
        ${CMAKE_CURRENT_SOURCE_DIR}/../Mods/APFrameworkMod/Scripts/lunajson
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lua/lunajson
        ${CMAKE_CURRENT_SOURCE_DIR}/../Mods/ExampleClientMod/Scripts/lunajson
    COMMENT "Copying lunajson library to mod directories"
)