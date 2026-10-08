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

add_executable(RegistryDwordExecutorTest
    execution/RegistryDwordExecutorTest.cpp
    fakes/FakeRegistryBackend.cpp
    fakes/FakeRegistryBackend.h
)

target_include_directories(RegistryDwordExecutorTest PRIVATE ${PROJECT_SOURCE_DIR}/tests)
target_link_libraries(RegistryDwordExecutorTest PRIVATE Qt6::Test TweakopediaExecution TweakopediaPlatform)
add_test(NAME RegistryDwordExecutor COMMAND RegistryDwordExecutorTest)

add_executable(TransactionRunnerTest
    execution/TransactionRunnerTest.cpp
)

target_link_libraries(TransactionRunnerTest PRIVATE Qt6::Test TweakopediaExecution TweakopediaPersistence)
add_test(NAME TransactionRunner COMMAND TransactionRunnerTest)

add_executable(ExecutorIpcTest execution/ExecutorIpcTest.cpp)
target_compile_definitions(ExecutorIpcTest PRIVATE
    TWEAKOPEDIA_TEST_EXECUTOR="$<TARGET_FILE:TweakopediaExecutor>"
)
target_link_libraries(ExecutorIpcTest PRIVATE Qt6::Test Qt6::Network TweakopediaExecution)
add_dependencies(ExecutorIpcTest TweakopediaExecutor)
add_test(NAME ExecutorIpc COMMAND ExecutorIpcTest)

add_executable(ExecutorLauncherTest execution/ExecutorLauncherTest.cpp)
target_link_libraries(ExecutorLauncherTest PRIVATE Qt6::Test TweakopediaExecution)
add_test(NAME ExecutorLauncher COMMAND ExecutorLauncherTest)

add_executable(AppControllerTest app/AppControllerTest.cpp)
target_compile_definitions(AppControllerTest PRIVATE
    TWEAKOPEDIA_TEST_CONTENT_ROOT="${PROJECT_SOURCE_DIR}/content/tweaks"
)
target_link_libraries(AppControllerTest PRIVATE Qt6::Test TweakopediaApp TweakopediaContent)
add_test(NAME AppController COMMAND AppControllerTest)

add_executable(TweakListModelTest app/TweakListModelTest.cpp)
target_compile_definitions(TweakListModelTest PRIVATE
    TWEAKOPEDIA_TEST_CONTENT_ROOT="${PROJECT_SOURCE_DIR}/content/tweaks"
)
target_link_libraries(TweakListModelTest PRIVATE Qt6::Test TweakopediaApp TweakopediaContent)
add_test(NAME TweakListModel COMMAND TweakListModelTest)

add_executable(QmlComponentTests qml/QmlTestMain.cpp)
target_link_libraries(QmlComponentTests PRIVATE Qt6::QuickTest Qt6::QuickControls2)
add_test(NAME QmlPlanPreviewHistory COMMAND QmlComponentTests -input ${PROJECT_SOURCE_DIR}/tests/qml)
set_tests_properties(QmlPlanPreviewHistory PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_DISABLE_DISK_CACHE=1"
)

add_executable(VerticalSliceWorkflowTest
    integration/VerticalSliceWorkflowTest.cpp
    fakes/FakeRegistryBackend.cpp
    fakes/FakeRegistryBackend.h
)
target_include_directories(VerticalSliceWorkflowTest PRIVATE ${PROJECT_SOURCE_DIR}/tests)
target_compile_definitions(VerticalSliceWorkflowTest PRIVATE
    TWEAKOPEDIA_TEST_CONTENT_ROOT="${PROJECT_SOURCE_DIR}/content/tweaks"
)
target_link_libraries(VerticalSliceWorkflowTest PRIVATE
    Qt6::Test TweakopediaContent TweakopediaDetection TweakopediaExecution
    TweakopediaPersistence TweakopediaPlanning TweakopediaPlatform
)
add_test(NAME VerticalSliceWorkflow COMMAND VerticalSliceWorkflowTest)
