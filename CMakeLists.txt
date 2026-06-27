cmake_minimum_required(VERSION 3.10)
project(BlackHoleSim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_OSX_ARCHITECTURES "arm64")

# Chemins explicites pour les bibliothèques installées avec Homebrew sur Apple Silicon
include_directories(/opt/homebrew/include)
link_directories(/opt/homebrew/lib)

add_executable(BlackHoleSim main.cpp Camera.cpp)

# Lier les bibliothèques
target_link_libraries(BlackHoleSim
    glew
    glfw
    "-framework OpenGL"
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo"
)
