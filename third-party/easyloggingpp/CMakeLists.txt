target_sources(${LRS_TARGET}
    PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/src/easylogging++.cc"
		"${CMAKE_CURRENT_LIST_DIR}/src/easylogging++.h"
)
hide_sources_symbols( "${CMAKE_CURRENT_LIST_DIR}/src/easylogging++.cc" )
