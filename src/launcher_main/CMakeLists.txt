add_executable(srceng
    main.cpp
)
target_link_libraries(srceng PRIVATE ks_common )
set_target_properties(srceng PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
