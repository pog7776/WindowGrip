cmake_minimum_required(VERSION 4.2)
project(WindowGrip CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Build the DLL component (The Hook)
add_library(resizer_hook SHARED hook.cpp)
target_link_libraries(resizer_hook PRIVATE user32)

# 2. Build the EXE component (The Runner)
add_executable(WindowGrip main.cpp)
target_link_libraries(WindowGrip PRIVATE user32)

# 3. Automatically copy the DLL next to the EXE so the runner can find it
add_custom_command(TARGET WindowGrip POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:resizer_hook>
        $<TARGET_FILE_DIR:WindowGrip>
)