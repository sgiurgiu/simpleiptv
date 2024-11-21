cmake_minimum_required(VERSION 3.15)
project(simpleiptv_tests)

find_package(GTest CONFIG REQUIRED)


set(STV_TEST_SRCS
    test_sample.cpp
)


add_executable(${PROJECT_NAME}
    ${STV_TEST_SRCS}
    )

target_link_libraries(${PROJECT_NAME} PRIVATE GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)
target_link_libraries(${PROJECT_NAME} PRIVATE ${STV_LIBARIES})
target_compile_definitions(${PROJECT_NAME} PRIVATE "STVTEST_DATA_FOLDER=\"${CMAKE_CURRENT_SOURCE_DIR}/data\"")

add_test(SimpleIPTVTests ${PROJECT_NAME})
