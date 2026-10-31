add_executable(openrct2-core-bench bench.cpp)
target_compile_features(openrct2-core-bench PRIVATE cxx_std_20)
target_include_directories(openrct2-core-bench PRIVATE "${ROOT_DIR}/src")
target_link_libraries(openrct2-core-bench PRIVATE OpenRCT2::libopenrct2 Threads::Threads)

add_executable(openrct2-core-probe probe.cpp)
target_compile_features(openrct2-core-probe PRIVATE cxx_std_20)
target_include_directories(openrct2-core-probe PRIVATE "${ROOT_DIR}/src")
target_compile_definitions(openrct2-core-probe PRIVATE $<$<BOOL:${OPENRCT2_SMALL_MEMORY}>:OPENRCT2_SMALL_MEMORY>)
