find_package(GTest CONFIG REQUIRED)


set(STV_TEST_SRCS
    test_sample.cpp
)


add_executable(simpleiptv_tests
    ${STV_TEST_SRCS}
    )

target_link_libraries(simpleiptv_tests PRIVATE GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)
target_include_directories(simpleiptv_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(simpleiptv_tests PRIVATE "STVTEST_DATA_FOLDER=\"${CMAKE_CURRENT_SOURCE_DIR}/data\"")

add_test(SimpleIPTVTests simpleiptv_tests)
