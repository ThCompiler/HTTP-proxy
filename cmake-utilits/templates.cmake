macro( connect_lib lib_name project_name)
    set(LIBRARY_HEADERS_DIR include)
    file(GLOB_RECURSE LIBRARY_HEADERS ${LIBRARY_HEADERS_DIR}/*)

    set(LIBRARY_SOURCE_DIR src)
    file(GLOB LIBRARY_SOURCE ${LIBRARY_SOURCE_DIR}/*.*)

    project(${project_name})

    add_library(${lib_name} STATIC ${LIBRARY_HEADERS} ${LIBRARY_SOURCE})

    target_include_directories(${lib_name} PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
            )

    target_include_directories(${lib_name} PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<INSTALL_INTERFACE:include>
            )

endmacro( connect_lib )