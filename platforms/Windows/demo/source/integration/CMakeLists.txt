# Compiled by the consumer: no C++ ABI crosses the HiFace DLL boundary.
# 由接入方编译，HiFace DLL 边界不传递 C++ ABI。
if(NOT DEFINED HIFACE_CPP_INCLUDE_DIR)
    get_filename_component(HIFACE_CPP_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../include" ABSOLUTE)
endif()
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/client.cpp")
    set(hiface_cpp_sources "${CMAKE_CURRENT_LIST_DIR}")
else()
    get_filename_component(hiface_cpp_sources "${CMAKE_CURRENT_LIST_DIR}/../src" ABSOLUTE)
endif()
add_library(hiface_windows_cpp STATIC
    "${hiface_cpp_sources}/client.cpp" "${hiface_cpp_sources}/database.cpp"
    "${hiface_cpp_sources}/frame.cpp" "${hiface_cpp_sources}/error.cpp")
add_library(HiFace::Cpp ALIAS hiface_windows_cpp)
target_compile_features(hiface_windows_cpp PUBLIC cxx_std_17)
target_include_directories(hiface_windows_cpp PUBLIC "${HIFACE_CPP_INCLUDE_DIR}")
target_link_libraries(hiface_windows_cpp PUBLIC HiFace::HiFace)
if(MSVC)
    target_compile_options(hiface_windows_cpp PUBLIC /utf-8 /EHsc)
else()
    target_compile_options(hiface_windows_cpp PRIVATE -Wall -Wextra -Werror)
endif()
