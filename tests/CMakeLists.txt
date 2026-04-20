# All test sources aggregate into one binary: keeps link+run simple,
# doctest handles discovery via its built-in main().
# doctest v2.4.11 vendored verbatim from https://github.com/doctest/doctest
set(TEST_SRC
    smoke_test.cpp
    updater_sha256_test.cpp
    updater_download_test.cpp
    ${CMAKE_SOURCE_DIR}/src/updatersha256.cpp
    ${CMAKE_SOURCE_DIR}/src/updaterdownload.cpp
)

add_executable(zsilencer_tests ${TEST_SRC})
target_compile_features(zsilencer_tests PUBLIC cxx_std_14)
target_include_directories(zsilencer_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME zsilencer_tests COMMAND zsilencer_tests)

# Download wrapper needs curl headers even in tests (for the enum).
find_package(CURL REQUIRED)
target_link_libraries(zsilencer_tests PRIVATE CURL::libcurl)
