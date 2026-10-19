include(FetchContent)
if(MOONGTANGE_BUILD_NETWORK)
  # cpp-httplib v0.61.0, MIT; HTTP only (TLS termination belongs to the deployment proxy).
  FetchContent_Declare(httplib
    URL https://codeload.github.com/yhirose/cpp-httplib/tar.gz/c419c904da4ed0d057f169c225c58cc795c07e4d
    URL_HASH SHA256=2a37947f7933d7bccbee420cee1ccf3bac6f1631ecebdc23625730df1452248e
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR moongtange-unused)
  FetchContent_MakeAvailable(httplib)
  add_library(MatchmakingHttp INTERFACE)
  target_include_directories(MatchmakingHttp SYSTEM INTERFACE "${httplib_SOURCE_DIR}")
  target_compile_definitions(MatchmakingHttp INTERFACE
    CPPHTTPLIB_REQUEST_URI_MAX_LENGTH=256 CPPHTTPLIB_HEADER_MAX_LENGTH=1024
    CPPHTTPLIB_HEADER_MAX_COUNT=32)
  target_link_libraries(MatchmakingHttp INTERFACE Threads::Threads)
  if(WIN32)
    target_link_libraries(MatchmakingHttp INTERFACE ws2_32)
  endif()

  FetchContent_Declare(asio
    URL https://codeload.github.com/chriskohlhoff/asio/tar.gz/8806a6803cde7054c3049d3666d3ec36786568c5
    URL_HASH SHA256=ca7f6c14f2bf91e61c7e81fb693f2f8fc86f93e85520d5fc7fd035d0f666bb35
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(asio)
  add_library(AsioStandalone INTERFACE)
  target_include_directories(AsioStandalone SYSTEM INTERFACE "${asio_SOURCE_DIR}/include")
  target_compile_definitions(AsioStandalone INTERFACE ASIO_STANDALONE)
  target_link_libraries(AsioStandalone INTERFACE Threads::Threads)
  if(WIN32)
    target_compile_definitions(AsioStandalone INTERFACE _WIN32_WINNT=0x0A00)
    target_link_libraries(AsioStandalone INTERFACE ws2_32 mswsock)
  endif()
endif()
if(MOONGTANGE_BUILD_CLIENT)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(raylib
    URL https://codeload.github.com/raysan5/raylib/tar.gz/dbc56a87da87d973a9c5baa4e7438a9d20121d28
    URL_HASH SHA256=81b06ce7c19cf3b634b0271c23c361ba6ad8bf45fb8b036abbfeb4260ec1e126
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(raylib)
endif()
