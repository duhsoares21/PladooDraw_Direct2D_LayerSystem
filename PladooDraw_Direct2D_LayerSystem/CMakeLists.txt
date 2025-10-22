cmake_minimum_required(VERSION 3.20)
project(PladooDraw_Direct2D_LayerSystem LANGUAGES CXX)

# Força arquitetura x86
set(CMAKE_GENERATOR_PLATFORM Win32)
set(CMAKE_SIZEOF_VOID_P 4)

# Define a DLL
file(GLOB CPP_HEADERS *.h)
file(GLOB CPP_SOURCES *.cpp)

add_library(PladooDraw_Direct2D_LayerSystem SHARED
        ${CPP_HEADERS}
        ${CPP_SOURCES}
)

# Inclui diretório de headers
target_include_directories(PladooDraw_Direct2D_LayerSystem PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(PladooDraw_Direct2D_LayerSystem
        d3d11.lib
        runtimeobject.lib
        CoreMessaging.lib
        kernel32.lib
        user32.lib
        gdi32.lib
        winspool.lib
        shell32.lib
        ole32.lib
        oleaut32.lib
        uuid.lib
        comdlg32.lib
        advapi32.lib
        dxgi.lib
        d2d1.lib
        windowsapp.lib
        runtimeobject.lib
)

# Caminhos de saída
set_target_properties(PladooDraw_Direct2D_LayerSystem PROPERTIES
        OUTPUT_NAME "PladooDraw_Direct2D_LayerSystem"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Força máquina x86
set_target_properties(PladooDraw_Direct2D_LayerSystem PROPERTIES
        LINK_FLAGS "/MACHINE:X86"
)
