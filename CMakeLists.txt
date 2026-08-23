cmake_minimum_required(VERSION 3.16)
project(tetrinet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Network)

add_executable(tetrinet
    src/main.cpp
    src/game/Piece.cpp
    src/game/Field.cpp
    src/game/Engine.cpp
    src/game/Bot.cpp
    src/net/Protocol.cpp
    src/net/Server.cpp
    src/net/Client.cpp
    src/net/StatusQuery.cpp
    src/net/ClassicProtocol.cpp
    src/net/ClassicClient.cpp
    src/session/ClassicSession.cpp
    src/session/GameSession.cpp
    src/session/LocalSession.cpp
    src/session/NetSession.cpp
    src/ui/FieldWidget.cpp
    src/ui/InventoryBar.cpp
    src/ui/LobbyWidget.cpp
    src/ui/GameView.cpp
    src/ui/MainWindow.cpp
)

target_include_directories(tetrinet PRIVATE src)
target_link_libraries(tetrinet PRIVATE Qt5::Widgets Qt5::Network)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tetrinet PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(tetrinet_tests
        tests/ProtocolTest.cpp
        src/game/Piece.cpp
        src/game/Field.cpp
        src/game/Engine.cpp
        src/net/Protocol.cpp
        src/net/Server.cpp
        src/net/Client.cpp
        src/net/ClassicProtocol.cpp
        src/net/ClassicClient.cpp
        src/session/GameSession.cpp
        src/session/NetSession.cpp
        src/session/ClassicSession.cpp
    )
    target_include_directories(tetrinet_tests PRIVATE src)
    target_link_libraries(tetrinet_tests PRIVATE Qt5::Network)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(tetrinet_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME tetrinet_tests COMMAND tetrinet_tests)
endif()
