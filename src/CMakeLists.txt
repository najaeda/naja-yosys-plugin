set(CAPNPC_SRC_PREFIX "${PROJECT_SOURCE_DIR}/thirdparty/naja-if/schema" CACHE STRING "" FORCE)
set(CAPNPC_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

capnp_generate_cpp(najaCommonSources najaCommonHeaders ${CAPNPC_SRC_PREFIX}/naja_common.capnp)
capnp_generate_cpp(najaNLInterfaceSources najaNLInterfaceHeaders ${CAPNPC_SRC_PREFIX}/naja_nl_interface.capnp)
capnp_generate_cpp(najaNLImplementationSources najaNLImplementationHeaders ${CAPNPC_SRC_PREFIX}/naja_nl_implementation.capnp)

add_library(yosys-naja-if ${SHARED}
  ${najaCommonSources} ${najaNLInterfaceSources}
  ${najaNLImplementationSources}
  yosys_plugin.cpp
  yosys_debug.cpp
)

target_include_directories(yosys-naja-if PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(yosys-naja-if PRIVATE yosys::yosys CapnProto::capnp )

if (BUILD_AS_PLUGIN)
    set_target_properties(yosys-naja-if PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        # Yosys assumes `.so` to be the suffix on all platforms, so the following is required to make
        # `yosys -m slang` to work after installation on any platform.
        PREFIX ""
        OUTPUT_NAME "naja-if"
        SUFFIX ".so"
    )

    if (WIN32)
        # install .dll only
        install(TARGETS yosys-naja-if RUNTIME DESTINATION ${YOSYS_DATDIR}/plugins)
    else()
        # install .so/.dylib only
        install(TARGETS yosys-naja-if LIBRARY DESTINATION ${YOSYS_DATDIR}/plugins)
    endif()
endif()
