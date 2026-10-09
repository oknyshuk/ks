add_executable(srcds
    main.cpp
)
target_link_libraries(srcds PRIVATE ks_common )
set_target_properties(srcds PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
