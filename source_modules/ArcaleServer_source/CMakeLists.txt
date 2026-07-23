cmake_minimum_required(VERSION 3.10)
project(ArcaleServer_source)

# Flags de compilation
if(MSVC)
    add_compile_options(/O2 /std:c++17 /EHsc /utf-8)
else()
    add_compile_options(-O3 -std=c++17 -fpermissive)
endif()

# Sources
file(GLOB SRC "src/*.cpp")

add_library(ArcaleServer_source SHARED ${SRC})
target_include_directories(ArcaleServer_source PRIVATE src/)

# D'autres librairies
target_link_libraries(ArcaleServer_source PRIVATE sdrpp_core)

# Intégration de tinyxml2
find_package(tinyxml2 REQUIRED)
target_link_libraries(ArcaleServer_source PRIVATE tinyxml2::tinyxml2)

# Sur Windows, ajout de wsock32, ws2_32
if(WIN32)
    target_link_libraries(ArcaleServer_source PRIVATE wsock32 ws2_32)
endif()

# Supprimer le préfixe lib si besoin
set_target_properties(ArcaleServer_source PROPERTIES PREFIX "")
