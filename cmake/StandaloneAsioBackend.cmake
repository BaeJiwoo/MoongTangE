set(MOONGTANGE_STANDALONE_ASIO_BACKEND "AUTO" CACHE STRING
  "Expected Standalone Asio backend: AUTO, IOCP, or EPOLL")
set_property(CACHE MOONGTANGE_STANDALONE_ASIO_BACKEND PROPERTY STRINGS AUTO IOCP EPOLL)

function(moongtange_configure_standalone_asio_backend target)
  string(TOUPPER "${MOONGTANGE_STANDALONE_ASIO_BACKEND}"
    moongtange_expected_standalone_asio_backend)
  set(moongtange_supported_standalone_asio_backends AUTO IOCP EPOLL)
  if(NOT moongtange_expected_standalone_asio_backend IN_LIST
     moongtange_supported_standalone_asio_backends)
    message(FATAL_ERROR
      "MOONGTANGE_STANDALONE_ASIO_BACKEND must be AUTO, IOCP, or EPOLL; got "
      "'${MOONGTANGE_STANDALONE_ASIO_BACKEND}'")
  endif()

  if(WIN32)
    set(moongtange_native_standalone_asio_backend IOCP)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(moongtange_native_standalone_asio_backend EPOLL)
  else()
    message(FATAL_ERROR
      "MoongTangE networking supports only Windows and Linux; target system is "
      "'${CMAKE_SYSTEM_NAME}'")
  endif()

  if(moongtange_expected_standalone_asio_backend STREQUAL "AUTO")
    set(moongtange_expected_standalone_asio_backend
      "${moongtange_native_standalone_asio_backend}")
  elseif(NOT "${moongtange_expected_standalone_asio_backend}" STREQUAL
         "${moongtange_native_standalone_asio_backend}")
    message(FATAL_ERROR
      "MOONGTANGE_STANDALONE_ASIO_BACKEND=${moongtange_expected_standalone_asio_backend} "
      "does not match ${CMAKE_SYSTEM_NAME}; expected "
      "${moongtange_native_standalone_asio_backend}")
  endif()

  target_compile_definitions(${target} PRIVATE
    "MOONGTANGE_EXPECT_STANDALONE_ASIO_${moongtange_expected_standalone_asio_backend}")
  message(STATUS
    "MoongTangE Standalone Asio backend: ${moongtange_expected_standalone_asio_backend} "
    "(${CMAKE_SYSTEM_NAME})")
endfunction()
