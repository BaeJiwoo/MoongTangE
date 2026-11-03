include(FetchContent)
if(MOONGTANGE_BUILD_NETWORK)
  # cpp-httplib v0.61.0, MIT. All consumers use the same TLS configuration (ODR).
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
  # Mbed TLS 3.6.7 release commit 068ff080b369adfac81509f9b57b2afabaf82dc5.
  # The official archive includes its pinned framework submodule and generated files.
  set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
  set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(mbedtls
    URL https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2
    URL_HASH SHA256=a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(mbedtls)
  target_compile_definitions(MatchmakingHttp INTERFACE CPPHTTPLIB_MBEDTLS_SUPPORT)
  target_link_libraries(MatchmakingHttp INTERFACE mbedtls mbedx509 mbedcrypto)
  if(WIN32)
    target_link_libraries(MatchmakingHttp INTERFACE ws2_32 crypt32 bcrypt)
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
