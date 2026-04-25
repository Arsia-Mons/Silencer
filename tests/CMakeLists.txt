# All test sources aggregate into one binary: keeps link+run simple,
# doctest handles discovery via its built-in main().
# doctest v2.4.11 vendored verbatim from https://github.com/doctest/doctest
set(TEST_SRC
    smoke_test.cpp
    updater_sha256_test.cpp
    updater_download_test.cpp
    updater_zip_test.cpp
    updater_sm_test.cpp
    ${CMAKE_SOURCE_DIR}/src/updatersha256.cpp
    ${CMAKE_SOURCE_DIR}/src/updaterdownload.cpp
    ${CMAKE_SOURCE_DIR}/src/updaterzip.cpp
    ${CMAKE_SOURCE_DIR}/src/updater.cpp
)

add_executable(silencer_tests ${TEST_SRC})
target_compile_features(silencer_tests PUBLIC cxx_std_14)
target_include_directories(silencer_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME silencer_tests COMMAND silencer_tests)

target_compile_definitions(silencer_tests PRIVATE
    ZIP_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

# Download wrapper needs curl headers even in tests (for the enum).
find_package(CURL REQUIRED)
target_link_libraries(silencer_tests PRIVATE CURL::libcurl)

find_package(ZLIB REQUIRED)
find_package(unofficial-minizip CONFIG QUIET)
if(NOT unofficial-minizip_FOUND)
    find_package(minizip CONFIG QUIET)
endif()
target_link_libraries(silencer_tests PRIVATE ${ZLIB_LIBRARY})
if(TARGET unofficial::minizip::minizip)
    target_link_libraries(silencer_tests PRIVATE unofficial::minizip::minizip)
elseif(TARGET minizip::minizip)
    target_link_libraries(silencer_tests PRIVATE minizip::minizip)
endif()
