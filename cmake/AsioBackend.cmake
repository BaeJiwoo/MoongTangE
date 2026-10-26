set(MOONGTANGE_ASIO_BACKEND "AUTO" CACHE STRING
  "Expected Asio backend: AUTO, IOCP, EPOLL, or KQUEUE")
set_property(CACHE MOONGTANGE_ASIO_BACKEND PROPERTY STRINGS AUTO IOCP EPOLL KQUEUE)

function(moongtange_configure_asio_backend target)
  string(TOUPPER "${MOONGTANGE_ASIO_BACKEND}" moongtange_expected_asio_backend)
  set(moongtange_supported_asio_backends AUTO IOCP EPOLL KQUEUE)
  if(NOT moongtange_expected_asio_backend IN_LIST moongtange_supported_asio_backends)
    message(FATAL_ERROR
      "MOONGTANGE_ASIO_BACKEND must be AUTO, IOCP, EPOLL, or KQUEUE; got "
      "'${MOONGTANGE_ASIO_BACKEND}'")
  endif()

  if(WIN32)
    set(moongtange_native_asio_backend IOCP)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(moongtange_native_asio_backend EPOLL)
  elseif(APPLE)
    set(moongtange_native_asio_backend KQUEUE)
  else()
    message(FATAL_ERROR
      "MoongTangE networking supports Windows, Linux, and macOS; target system is "
      "'${CMAKE_SYSTEM_NAME}'")
  endif()

  if(moongtange_expected_asio_backend STREQUAL "AUTO")
    set(moongtange_expected_asio_backend "${moongtange_native_asio_backend}")
  elseif(NOT "${moongtange_expected_asio_backend}" STREQUAL
         "${moongtange_native_asio_backend}")
    message(FATAL_ERROR
      "MOONGTANGE_ASIO_BACKEND=${moongtange_expected_asio_backend} does not match "
      "${CMAKE_SYSTEM_NAME}; expected ${moongtange_native_asio_backend}")
  endif()

  target_compile_definitions(${target} PRIVATE
    "MOONGTANGE_EXPECT_ASIO_${moongtange_expected_asio_backend}")
  message(STATUS
    "MoongTangE Asio backend: ${moongtange_expected_asio_backend} (${CMAKE_SYSTEM_NAME})")
endfunction()
