include_directories (
    "${CMAKE_CURRENT_LIST_DIR}"
    )

target_sources(jasterix
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/pcapreader.h"
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/pcapreader.cpp"
)
