set(LIBRARY_OUTPUT_PATH ../../../lib/)

include_directories(${PGROUTING_INCLUDE_DIRECTORIES} ${POSTGRESQL_INCLUDE_DIR})
if(WIN32)
    include_directories(${POSTGRESQL_INCLUDE_DIR}/port/win32)
endif(WIN32)

if(APPLE)
    set(LIBRARY_MODE_TARGET "MODULE")
else(APPLE)
    set(LIBRARY_MODE_TARGET "SHARED")
endif(APPLE)

add_library(routing_atsp ${LIBRARY_MODE_TARGET} TSPEngine.c tsp.h  tspInterface.c tspSupport.cpp envSupport.c )
if(WIN32)
  if(MSVC)
    set_target_properties(routing_atsp PROPERTIES COMPILE_FLAGS "-DHAVE_GETHOSTNAME")
  endif(MSVC)
endif(WIN32)

if(APPLE)
  set_target_properties(routing_atsp
    PROPERTIES
    LINK_FLAGS "-bundle_loader ${POSTGRESQL_EXECUTABLE} -bundle")
endif(APPLE)

install(TARGETS routing_atsp DESTINATION ${LIBRARY_INSTALL_PATH})
