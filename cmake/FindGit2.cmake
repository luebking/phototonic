find_package(PkgConfig REQUIRED)
pkg_search_module(GIT2 REQUIRED libgit2)

#message("INC: ${GIT2_INCLUDE_DIRS}")
#message("LIB: ${GIT2_LIBRARIES}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GIT2 REQUIRED_VARS GIT2_LIBRARIES)


add_library(GIT2::GIT2 UNKNOWN IMPORTED)

set_target_properties(GIT2::GIT2 PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES "C"
  IMPORTED_LOCATION "${GIT2_LIBRARIES}"
)
