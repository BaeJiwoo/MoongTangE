# Compile each project header as the first and only include in a translation unit.
file(GLOB_RECURSE moongtange_headers CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/Common/*.h" "${PROJECT_SOURCE_DIR}/Client/*.h"
  "${PROJECT_SOURCE_DIR}/Server/*.h" "${PROJECT_SOURCE_DIR}/Network/*.h"
  "${PROJECT_SOURCE_DIR}/Tests/*.h")
set(header_check_sources)
foreach(header IN LISTS moongtange_headers)
  file(RELATIVE_PATH relative_header "${PROJECT_SOURCE_DIR}" "${header}")
  if(NOT MOONGTANGE_BUILD_CLIENT AND relative_header MATCHES "(Client/Input/|GraphicsWindow.h)")
    continue()
  endif()
  if(NOT MOONGTANGE_BUILD_NETWORK AND relative_header MATCHES "(^Network/|^Server/Network/)")
    continue()
  endif()
  string(MAKE_C_IDENTIFIER "${relative_header}" check_name)
  set(check_source "${PROJECT_BINARY_DIR}/header-checks/${check_name}.cpp")
  file(GENERATE OUTPUT "${check_source}" CONTENT "#include \"${relative_header}\"\n")
  list(APPEND header_check_sources "${check_source}")
endforeach()
add_library(HeaderChecks OBJECT ${header_check_sources})
target_link_libraries(HeaderChecks PRIVATE Common Threads::Threads)
if(MOONGTANGE_BUILD_CLIENT)
  target_link_libraries(HeaderChecks PRIVATE raylib)
endif()
if(MOONGTANGE_BUILD_NETWORK)
  target_compile_definitions(HeaderChecks PRIVATE
    "MOONGTANGE_NETWORK_BACKEND_${MOONGTANGE_RESOLVED_NETWORK_BACKEND}")
endif()
