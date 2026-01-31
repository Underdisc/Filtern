target_compile_features(${targetName} PRIVATE cxx_std_20)
if(MSVC AND ("${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel"))
  target_link_options(${targetName} PRIVATE /MANIFEST:NO)
endif()

target_sources(${targetName} PRIVATE
  Main.cc)
