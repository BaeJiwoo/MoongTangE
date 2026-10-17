include(FetchContent)
if(MOONGTANGE_BUILD_NETWORK)
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
