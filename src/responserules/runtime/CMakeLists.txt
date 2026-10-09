add_library(responserules_runtime STATIC
    rrrlib.cpp
    response_types.cpp
    response_types_internal.cpp
    response_system.cpp
    rr_speechconcept.cpp
    rr_response.cpp
    criteriaset.cpp
)
target_link_libraries(responserules_runtime PRIVATE ks_common )
