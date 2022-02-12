macro( connect_lib lib_name project_name)
    set(LIBRARY_HEADERS_DIR include/${connect_lib})
    file(GLOB LIBRARY_HEADERS ${LIBRARY_HEADERS_DIR}/*.hpp)

    set(LIBRARY_SOURCE_DIR src)
    file(GLOB LIBRARY_SOURCE ${LIBRARY_SOURCE_DIR}/*.cpp)

    project(${project_name})

    add_library(${lib_name} STATIC ${LIBRARY_HEADERS} ${LIBRARY_SOURCE})

    target_include_directories(${lib_name} PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/${include_lib_name}>
            $<INSTALL_INTERFACE:include/${include_lib_name}>
            )

    target_include_directories(${lib_name} PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
            )

endmacro( connect_lib )