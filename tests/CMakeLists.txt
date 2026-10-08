add_executable(BuildSmokeTest
    smoke/BuildSmokeTest.cpp
)

target_link_libraries(BuildSmokeTest
    PRIVATE
        Qt6::Test
)

add_test(NAME BuildSmoke COMMAND BuildSmokeTest)
