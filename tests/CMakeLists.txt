add_executable(BuildSmokeTest
    smoke/BuildSmokeTest.cpp
)

target_link_libraries(BuildSmokeTest
    PRIVATE
        Qt6::Test
)

add_test(NAME BuildSmoke COMMAND BuildSmokeTest)

add_executable(DomainTweakDefinitionTest
    domain/TweakDefinitionTest.cpp
)

target_link_libraries(DomainTweakDefinitionTest
    PRIVATE
        Qt6::Test
        TweakopediaDomain
)

add_test(NAME DomainTweakDefinition COMMAND DomainTweakDefinitionTest)

add_executable(TweakCatalogLoaderTest
    content/TweakCatalogLoaderTest.cpp
)

target_compile_definitions(TweakCatalogLoaderTest
    PRIVATE
        TWEAKOPEDIA_TEST_FIXTURES="${PROJECT_SOURCE_DIR}/tests/fixtures/content"
        TWEAKOPEDIA_CONTENT_ROOT="${PROJECT_SOURCE_DIR}/content"
)

target_link_libraries(TweakCatalogLoaderTest
    PRIVATE
        Qt6::Test
        TweakopediaContent
)

add_test(NAME TweakCatalogLoader COMMAND TweakCatalogLoaderTest)
add_test(NAME ContentCatalog COMMAND TweakCatalogLoaderTest loadsRealCatalog)

add_executable(CompatibilityEvaluatorTest
    detection/CompatibilityEvaluatorTest.cpp
)

target_link_libraries(CompatibilityEvaluatorTest
    PRIVATE
        Qt6::Test
        TweakopediaDetection
)

add_test(NAME CompatibilityEvaluator COMMAND CompatibilityEvaluatorTest)

add_executable(WindowsSystemProfileProviderTest
    platform/WindowsSystemProfileProviderTest.cpp
)

target_link_libraries(WindowsSystemProfileProviderTest
    PRIVATE
        Qt6::Test
        TweakopediaPlatform
        advapi32
)

add_test(NAME WindowsSystemProfile COMMAND WindowsSystemProfileProviderTest)

add_executable(RegistryDwordStateDetectorTest
    detection/RegistryDwordStateDetectorTest.cpp
    fakes/FakeRegistryBackend.cpp
    fakes/FakeRegistryBackend.h
)

target_include_directories(RegistryDwordStateDetectorTest
    PRIVATE
        ${PROJECT_SOURCE_DIR}/tests
)

target_link_libraries(RegistryDwordStateDetectorTest
    PRIVATE
        Qt6::Test
        TweakopediaDetection
        TweakopediaPlatform
)

add_test(NAME RegistryDwordStateDetector COMMAND RegistryDwordStateDetectorTest)

add_executable(WindowsRegistryBackendTest
    platform/WindowsRegistryBackendTest.cpp
)

target_link_libraries(WindowsRegistryBackendTest
    PRIVATE
        Qt6::Test
        TweakopediaPlatform
        advapi32
)

add_test(NAME WindowsRegistryBackend COMMAND WindowsRegistryBackendTest)

add_executable(TweakQueueTest
    planning/TweakQueueTest.cpp
    fakes/FakeRegistryBackend.cpp
    fakes/FakeRegistryBackend.h
)

target_include_directories(TweakQueueTest
    PRIVATE
        ${PROJECT_SOURCE_DIR}/tests
)

target_link_libraries(TweakQueueTest
    PRIVATE
        Qt6::Test
        TweakopediaPlanning
        TweakopediaPlatform
)

add_test(NAME TweakQueue COMMAND TweakQueueTest)

add_executable(PlanBuilderTest
    planning/PlanBuilderTest.cpp
)

target_link_libraries(PlanBuilderTest
    PRIVATE
        Qt6::Test
        TweakopediaPlanning
)

add_test(NAME PlanBuilder COMMAND PlanBuilderTest)

add_executable(AppPathsTest
    persistence/AppPathsTest.cpp
)

target_link_libraries(AppPathsTest
    PRIVATE
        Qt6::Test
        TweakopediaPersistence
)

add_test(NAME AppPaths COMMAND AppPathsTest)

add_executable(TransactionRepositoryTest
    persistence/TransactionRepositoryTest.cpp
)

target_link_libraries(TransactionRepositoryTest
    PRIVATE
        Qt6::Test
        TweakopediaPersistence
)

add_test(NAME TransactionRepository COMMAND TransactionRepositoryTest)

add_executable(TransactionFilesTest
    persistence/TransactionFilesTest.cpp
)

target_link_libraries(TransactionFilesTest
    PRIVATE
        Qt6::Test
        TweakopediaPersistence
)

add_test(NAME TransactionFiles COMMAND TransactionFilesTest)

add_executable(ExecutionProtocolTest
    execution/ExecutionProtocolTest.cpp
)

target_link_libraries(ExecutionProtocolTest
    PRIVATE
        Qt6::Test
        TweakopediaExecution
)

add_test(NAME ExecutionProtocol COMMAND ExecutionProtocolTest)

add_executable(PlanValidatorTest
    execution/PlanValidatorTest.cpp
)

target_link_libraries(PlanValidatorTest
    PRIVATE
        Qt6::Test
        TweakopediaExecution
)

add_test(NAME PlanValidator COMMAND PlanValidatorTest)
