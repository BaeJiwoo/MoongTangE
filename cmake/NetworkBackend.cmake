set(MOONGTANGE_NETWORK_BACKEND "AUTO" CACHE STRING
  "Network implementation: AUTO, ASIO, IOCP, or EPOLL")
set_property(CACHE MOONGTANGE_NETWORK_BACKEND PROPERTY STRINGS AUTO ASIO IOCP EPOLL)

string(TOUPPER "${MOONGTANGE_NETWORK_BACKEND}" moongtange_requested_network_backend)
set(moongtange_supported_network_backends AUTO ASIO IOCP EPOLL)
if(NOT moongtange_requested_network_backend IN_LIST moongtange_supported_network_backends)
  message(FATAL_ERROR
    "MOONGTANGE_NETWORK_BACKEND must be AUTO, ASIO, IOCP, or EPOLL; got "
    "'${MOONGTANGE_NETWORK_BACKEND}'")
endif()

if(WIN32)
  set(moongtange_native_network_backend IOCP)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(moongtange_native_network_backend EPOLL)
else()
  message(FATAL_ERROR
    "MoongTangE networking supports only Windows and Linux; target system is "
    "'${CMAKE_SYSTEM_NAME}'.")
endif()

if(moongtange_requested_network_backend STREQUAL "AUTO")
  set(moongtange_resolved_network_backend "${moongtange_native_network_backend}")
elseif(moongtange_requested_network_backend STREQUAL "ASIO")
  set(moongtange_resolved_network_backend ASIO)
elseif(NOT moongtange_requested_network_backend STREQUAL moongtange_native_network_backend)
  message(FATAL_ERROR
    "MOONGTANGE_NETWORK_BACKEND=${moongtange_requested_network_backend} is unavailable on "
    "${CMAKE_SYSTEM_NAME}; select ASIO or ${moongtange_native_network_backend}")
else()
  set(moongtange_resolved_network_backend "${moongtange_requested_network_backend}")
endif()

set(MOONGTANGE_RESOLVED_NETWORK_BACKEND "${moongtange_resolved_network_backend}")

function(moongtange_configure_network_backend target)
  target_compile_definitions(${target} PUBLIC
    "MOONGTANGE_NETWORK_BACKEND_${MOONGTANGE_RESOLVED_NETWORK_BACKEND}")

  if(MOONGTANGE_RESOLVED_NETWORK_BACKEND STREQUAL "ASIO")
    target_compile_definitions(${target} PRIVATE
      "MOONGTANGE_EXPECT_ASIO_${moongtange_native_network_backend}")
  elseif(MOONGTANGE_RESOLVED_NETWORK_BACKEND STREQUAL "IOCP")
    target_link_libraries(${target} PRIVATE ws2_32 mswsock)
  endif()

  message(STATUS
    "MoongTangE network backend: ${MOONGTANGE_RESOLVED_NETWORK_BACKEND} "
    "(${CMAKE_SYSTEM_NAME})")
endfunction()
