cmake_minimum_required(VERSION 3.16)
project(tetrivibes LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Network)

add_executable(tetrivibes
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
    src/util/Scoreboard.cpp
)

target_include_directories(tetrivibes PRIVATE src)
target_link_libraries(tetrivibes PRIVATE Qt5::Widgets Qt5::Network)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tetrivibes PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_library(tetrivibes_testlib STATIC
        src/game/Piece.cpp
        src/game/Field.cpp
        src/game/Engine.cpp
        src/game/Bot.cpp
        src/net/Protocol.cpp
        src/net/Server.cpp
        src/net/Client.cpp
        src/net/ClassicProtocol.cpp
        src/net/ClassicClient.cpp
        src/session/GameSession.cpp
        src/session/NetSession.cpp
        src/session/ClassicSession.cpp
        src/util/Scoreboard.cpp
    )
    target_include_directories(tetrivibes_testlib PUBLIC src)
    target_link_libraries(tetrivibes_testlib PUBLIC Qt5::Network)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(tetrivibes_testlib PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    function(add_tetrivibes_test name)
        add_executable(${name} tests/${name}.cpp)
        target_link_libraries(${name} PRIVATE tetrivibes_testlib)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    add_tetrivibes_test(DecodeInvalidNumbers)
    add_tetrivibes_test(DecodeSpecial)
    add_tetrivibes_test(DecodeState)
    add_tetrivibes_test(EncodeChatStripsNewlines)
    add_tetrivibes_test(EncodeRejectsHugeFrame)
    add_tetrivibes_test(FieldValidEncoding)
    add_tetrivibes_test(NativeHostedBots)
    add_tetrivibes_test(HostedServerSurvivesLeave)
    add_tetrivibes_test(ClassicSessionTeardown)
    add_tetrivibes_test(ScoreboardWins)
endif()
