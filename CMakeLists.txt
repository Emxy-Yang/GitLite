cmake_minimum_required(VERSION 3.28)
project(gitlite)

set(CMAKE_CXX_STANDARD 14)

add_executable(gitlite
        src/GitliteException.cpp
        src/Utils.cpp
        main.cpp
        src/Objects.cpp
        include/Objects.hpp
        include/Def.hpp
        src/Repository.cpp
        include/Repository.hpp
        src/ObjectDataBase.cpp
        include/ObjectDataBase.hpp
        src/RefManager.cpp
        include/RefManager.hpp
        src/index.cpp
        include/index.hpp
        src/RemoteManager.cpp
        include/RemoteManager.hpp)


target_include_directories(gitlite
        PUBLIC
        include)

target_compile_options(gitlite
        PRIVATE
        -g)