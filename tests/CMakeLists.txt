add_executable(BuildSmokeTest
    smoke/BuildSmokeTest.cpp
)

target_link_libraries(BuildSmokeTest
    PRIVATE
        Qt6::Test
)
target_compile_definitions(BuildSmokeTest PRIVATE
    TWEAKOPEDIA_TEST_GUI="$<TARGET_FILE:Tweakopedia>"
    TWEAKOPEDIA_TEST_EXECUTOR="$<TARGET_FILE:TweakopediaExecutor>"
)
add_dependencies(BuildSmokeTest Tweakopedia TweakopediaExecutor)

add_test(NAME BuildSmoke COMMAND BuildSmokeTest)

add_executable(PackageFooterTest package/PackageFooterTest.cpp)
target_link_libraries(PackageFooterTest PRIVATE Qt6::Test TweakopediaPackage)
add_test(NAME PackageFooter COMMAND PackageFooterTest)

add_executable(PayloadManifestTest package/PayloadManifestTest.cpp)
target_link_libraries(PayloadManifestTest PRIVATE Qt6::Test TweakopediaPackage)
add_test(NAME PayloadManifest COMMAND PayloadManifestTest)

add_executable(ZipPayloadTest package/ZipPayloadTest.cpp)
target_link_libraries(ZipPayloadTest PRIVATE Qt6::Test TweakopediaPackage TweakopediaMiniz)
add_test(NAME ZipPayload COMMAND ZipPayloadTest)

add_executable(RuntimeCacheProbe package/RuntimeCacheProbe.cpp)
target_link_libraries(RuntimeCacheProbe PRIVATE TweakopediaPackage)
if(MINGW)
    target_link_options(RuntimeCacheProbe PRIVATE -municode)
endif()

add_executable(RuntimeCacheTest package/RuntimeCacheTest.cpp)
target_compile_definitions(RuntimeCacheTest PRIVATE
    TWEAKOPEDIA_RUNTIME_CACHE_PROBE="$<TARGET_FILE:RuntimeCacheProbe>"
)
target_link_libraries(RuntimeCacheTest PRIVATE Qt6::Test TweakopediaPackage)
add_dependencies(RuntimeCacheTest RuntimeCacheProbe)
add_test(NAME RuntimeCache COMMAND RuntimeCacheTest)

add_executable(LegacyDataMigratorTest package/LegacyDataMigratorTest.cpp)
target_link_libraries(LegacyDataMigratorTest PRIVATE Qt6::Test TweakopediaPackage)
add_test(NAME LegacyDataMigrator COMMAND LegacyDataMigratorTest)

add_executable(BootstrapArgumentProbe package/BootstrapArgumentProbe.cpp)
if(MINGW)
    target_link_options(BootstrapArgumentProbe PRIVATE -municode)
endif()
add_test(NAME BootstrapArgumentProbe COMMAND BootstrapArgumentProbe
    "обычный" "два слова" "кавычка \" внутри" "хвост\\"
)

add_executable(BootstrapApplicationTest package/BootstrapApplicationTest.cpp)
target_compile_definitions(BootstrapApplicationTest PRIVATE
    TWEAKOPEDIA_BOOTSTRAP_ARGUMENT_PROBE="$<TARGET_FILE:BootstrapArgumentProbe>"
)
target_link_libraries(BootstrapApplicationTest PRIVATE
    Qt6::Test TweakopediaBootstrapCore TweakopediaPackage
)
add_dependencies(BootstrapApplicationTest BootstrapArgumentProbe)
add_test(NAME BootstrapApplication COMMAND BootstrapApplicationTest)

add_executable(PackagerCliTest package/PackagerCliTest.cpp)
target_compile_definitions(PackagerCliTest PRIVATE
    TWEAKOPEDIA_TEST_PACKAGER="$<TARGET_FILE:TweakopediaPackager>"
)
target_link_libraries(PackagerCliTest PRIVATE Qt6::Test TweakopediaPackage)
add_dependencies(PackagerCliTest TweakopediaPackager)
add_test(NAME PackagerCli COMMAND PackagerCliTest)

add_executable(DomainTweakDefinitionTest
    domain/TweakDefinitionTest.cpp
)

target_link_libraries(DomainTweakDefinitionTest
    PRIVATE
        Qt6::Test
        TweakopediaDomain
)

add_test(NAME DomainTweakDefinition COMMAND DomainTweakDefinitionTest)

add_executable(TweakInputTest domain/TweakInputTest.cpp)
target_link_libraries(TweakInputTest PRIVATE Qt6::Test TweakopediaDomain)
add_test(NAME TweakInput COMMAND TweakInputTest)

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

add_executable(CompleteTweakImportAuditTest
    content/CompleteTweakImportAuditTest.cpp
)
target_compile_definitions(CompleteTweakImportAuditTest PRIVATE
    TWEAKOPEDIA_IMPORT_AUDIT="${PROJECT_SOURCE_DIR}/docs/audits/complete-tweak-import.json"
    TWEAKOPEDIA_CONTENT_ROOT="${PROJECT_SOURCE_DIR}/content"
)
target_link_libraries(CompleteTweakImportAuditTest PRIVATE Qt6::Test TweakopediaContent)
add_test(NAME CompleteTweakImportAudit COMMAND CompleteTweakImportAuditTest)

add_executable(AppRemovalCatalogLoaderTest
    content/AppRemovalCatalogLoaderTest.cpp
)
target_compile_definitions(AppRemovalCatalogLoaderTest PRIVATE
    TWEAKOPEDIA_CONTENT_ROOT="${PROJECT_SOURCE_DIR}/content"
)
target_link_libraries(AppRemovalCatalogLoaderTest PRIVATE Qt6::Test TweakopediaContent)
add_test(NAME AppRemovalCatalogLoader COMMAND AppRemovalCatalogLoaderTest)

add_executable(CategoryCatalogLoaderTest
    content/CategoryCatalogLoaderTest.cpp
)

target_compile_definitions(CategoryCatalogLoaderTest
    PRIVATE
        TWEAKOPEDIA_CATEGORY_FIXTURES="${PROJECT_SOURCE_DIR}/tests/fixtures/categories"
        TWEAKOPEDIA_CONTENT_ROOT="${PROJECT_SOURCE_DIR}/content"
)

target_link_libraries(CategoryCatalogLoaderTest
    PRIVATE
        Qt6::Test
        TweakopediaContent
)

add_test(NAME CategoryCatalogLoader COMMAND CategoryCatalogLoaderTest)

add_executable(CategoryMembershipValidatorTest
    content/CategoryMembershipValidatorTest.cpp
)
target_compile_definitions(CategoryMembershipValidatorTest PRIVATE
    TWEAKOPEDIA_CONTENT_ROOT="${PROJECT_SOURCE_DIR}/content"
)
target_link_libraries(CategoryMembershipValidatorTest PRIVATE
    Qt6::Test TweakopediaContent
)
add_test(NAME CategoryMembershipValidator COMMAND CategoryMembershipValidatorTest)

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

add_executable(WindowsSystemOverviewProviderTest
    platform/WindowsSystemOverviewProviderTest.cpp
)
target_link_libraries(WindowsSystemOverviewProviderTest
    PRIVATE
        Qt6::Test
        TweakopediaPlatform
        dxgi
)
add_test(NAME WindowsSystemOverviewProvider COMMAND WindowsSystemOverviewProviderTest)

add_executable(WindowsAppxPackageProviderTest
    platform/WindowsAppxPackageProviderTest.cpp
)
target_link_libraries(WindowsAppxPackageProviderTest PRIVATE Qt6::Test TweakopediaPlatform)
add_test(NAME WindowsAppxPackageProvider COMMAND WindowsAppxPackageProviderTest)

add_executable(WindowsFeatureStoreBackendTest platform/WindowsFeatureStoreBackendTest.cpp)
target_link_libraries(WindowsFeatureStoreBackendTest PRIVATE Qt6::Test TweakopediaPlatform)
add_test(NAME WindowsFeatureStoreBackend COMMAND WindowsFeatureStoreBackendTest)

add_executable(WindowsScheduledTaskBackendTest platform/WindowsScheduledTaskBackendTest.cpp)
target_link_libraries(WindowsScheduledTaskBackendTest PRIVATE
    Qt6::Test TweakopediaPlatform ole32 oleaut32 taskschd)
add_test(NAME WindowsScheduledTaskBackend COMMAND WindowsScheduledTaskBackendTest)

add_executable(WindowsBcdBackendTest platform/WindowsBcdBackendTest.cpp)
target_link_libraries(WindowsBcdBackendTest PRIVATE
    Qt6::Test TweakopediaPlatform ole32 oleaut32 wbemuuid)
add_test(NAME WindowsBcdBackend COMMAND WindowsBcdBackendTest)

add_executable(WindowsPowerSettingBackendTest platform/WindowsPowerSettingBackendTest.cpp)
target_link_libraries(WindowsPowerSettingBackendTest PRIVATE
    Qt6::Test TweakopediaPlatform powrprof)
add_test(NAME WindowsPowerSettingBackend COMMAND WindowsPowerSettingBackendTest)

add_executable(WindowsComponentBackendTest platform/WindowsComponentBackendTest.cpp)
target_link_libraries(WindowsComponentBackendTest PRIVATE
    Qt6::Test TweakopediaPlatform)
add_test(NAME WindowsComponentBackend COMMAND WindowsComponentBackendTest)

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

add_executable(RegistryValueStateDetectorTest
    detection/RegistryValueStateDetectorTest.cpp
    fakes/FakeRegistryBackend.cpp
    fakes/FakeRegistryBackend.h
)
target_include_directories(RegistryValueStateDetectorTest PRIVATE ${PROJECT_SOURCE_DIR}/tests)
target_link_libraries(RegistryValueStateDetectorTest PRIVATE Qt6::Test TweakopediaDetection TweakopediaPlatform)
add_test(NAME RegistryValueStateDetector COMMAND RegistryValueStateDetectorTest)

add_executable(RegistryTreeStateDetectorTest detection/RegistryTreeStateDetectorTest.cpp)
target_link_libraries(RegistryTreeStateDetectorTest PRIVATE
    Qt6::Test TweakopediaDetection TweakopediaPlatform)
add_test(NAME RegistryTreeStateDetector COMMAND RegistryTreeStateDetectorTest)

add_executable(AppxPackageStateDetectorTest
    detection/AppxPackageStateDetectorTest.cpp
)
target_link_libraries(AppxPackageStateDetectorTest PRIVATE Qt6::Test TweakopediaDetection TweakopediaPlatform)
add_test(NAME AppxPackageStateDetector COMMAND AppxPackageStateDetectorTest)

add_executable(FeatureStateDetectorTest detection/FeatureStateDetectorTest.cpp)
target_link_libraries(FeatureStateDetectorTest PRIVATE Qt6::Test TweakopediaDetection TweakopediaPlatform)
add_test(NAME FeatureStateDetector COMMAND FeatureStateDetectorTest)

add_executable(ScheduledTaskStateDetectorTest detection/ScheduledTaskStateDetectorTest.cpp)
target_link_libraries(ScheduledTaskStateDetectorTest PRIVATE
    Qt6::Test TweakopediaDetection TweakopediaPlatform)
add_test(NAME ScheduledTaskStateDetector COMMAND ScheduledTaskStateDetectorTest)

add_executable(BcdStateDetectorTest detection/BcdStateDetectorTest.cpp)
target_link_libraries(BcdStateDetectorTest PRIVATE
    Qt6::Test TweakopediaDetection TweakopediaPlatform)
add_test(NAME BcdStateDetector COMMAND BcdStateDetectorTest)

add_executable(PowerSettingStateDetectorTest detection/PowerSettingStateDetectorTest.cpp)
target_link_libraries(PowerSettingStateDetectorTest PRIVATE
    Qt6::Test TweakopediaDetection TweakopediaPlatform)
add_test(NAME PowerSettingStateDetector COMMAND PowerSettingStateDetectorTest)

add_executable(WindowsComponentStateDetectorTest detection/WindowsComponentStateDetectorTest.cpp)
target_link_libraries(WindowsComponentStateDetectorTest PRIVATE
    Qt6::Test TweakopediaDetection)
add_test(NAME WindowsComponentStateDetector COMMAND WindowsComponentStateDetectorTest)

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

add_executable(AppSettingsTest
    persistence/AppSettingsTest.cpp
)

target_link_libraries(AppSettingsTest
    PRIVATE
        Qt6::Test
        TweakopediaPersistence
)

add_test(NAME AppSettings COMMAND AppSettingsTest)

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

add_executable(PendingInputStoreTest persistence/PendingInputStoreTest.cpp)
target_link_libraries(PendingInputStoreTest PRIVATE Qt6::Test TweakopediaPersistence)
add_test(NAME PendingInputStore COMMAND PendingInputStoreTest)

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

add_executable(RegistryValueExecutorTest
    execution/RegistryValueExecutorTest.cpp
    fakes/FakeRegistryBackend.cpp
    fakes/FakeRegistryBackend.h
)
target_include_directories(RegistryValueExecutorTest PRIVATE ${PROJECT_SOURCE_DIR}/tests)
target_link_libraries(RegistryValueExecutorTest PRIVATE Qt6::Test TweakopediaExecution TweakopediaPlatform)
add_test(NAME RegistryValueExecutor COMMAND RegistryValueExecutorTest)

add_executable(RegistryTreeExecutorTest execution/RegistryTreeExecutorTest.cpp)
target_link_libraries(RegistryTreeExecutorTest PRIVATE Qt6::Test TweakopediaExecution TweakopediaPlatform)
add_test(NAME RegistryTreeExecutor COMMAND RegistryTreeExecutorTest)

add_executable(FileOperationExecutorTest execution/FileOperationExecutorTest.cpp)
target_link_libraries(FileOperationExecutorTest PRIVATE Qt6::Test TweakopediaExecution)
add_test(NAME FileOperationExecutor COMMAND FileOperationExecutorTest)

add_executable(ScheduledTaskExecutorTest execution/ScheduledTaskExecutorTest.cpp)
target_link_libraries(ScheduledTaskExecutorTest PRIVATE Qt6::Test TweakopediaExecution)
add_test(NAME ScheduledTaskExecutor COMMAND ScheduledTaskExecutorTest)

add_executable(BcdElementExecutorTest execution/BcdElementExecutorTest.cpp)
target_link_libraries(BcdElementExecutorTest PRIVATE Qt6::Test TweakopediaExecution)
add_test(NAME BcdElementExecutor COMMAND BcdElementExecutorTest)

add_executable(PowerSettingExecutorTest execution/PowerSettingExecutorTest.cpp)
target_link_libraries(PowerSettingExecutorTest PRIVATE Qt6::Test TweakopediaExecution)
add_test(NAME PowerSettingExecutor COMMAND PowerSettingExecutorTest)

add_executable(WindowsComponentExecutorTest execution/WindowsComponentExecutorTest.cpp)
target_link_libraries(WindowsComponentExecutorTest PRIVATE Qt6::Test TweakopediaExecution)
add_test(NAME WindowsComponentExecutor COMMAND WindowsComponentExecutorTest)

add_executable(AppxPackageExecutorTest
    execution/AppxPackageExecutorTest.cpp
)
target_link_libraries(AppxPackageExecutorTest PRIVATE Qt6::Test TweakopediaExecution TweakopediaPlatform)
add_test(NAME AppxPackageExecutor COMMAND AppxPackageExecutorTest)

add_executable(FeatureStateExecutorTest execution/FeatureStateExecutorTest.cpp)
target_link_libraries(FeatureStateExecutorTest PRIVATE Qt6::Test TweakopediaExecution TweakopediaPlatform)
add_test(NAME FeatureStateExecutor COMMAND FeatureStateExecutorTest)

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

add_executable(SessionLoggerTest app/SessionLoggerTest.cpp)
target_link_libraries(SessionLoggerTest PRIVATE Qt6::Test TweakopediaApp)
add_test(NAME SessionLogger COMMAND SessionLoggerTest)

add_executable(SettingsControllerTest app/SettingsControllerTest.cpp)
target_link_libraries(SettingsControllerTest PRIVATE Qt6::Test TweakopediaApp)
add_test(NAME SettingsController COMMAND SettingsControllerTest)

add_executable(SystemOverviewPresenterTest app/SystemOverviewPresenterTest.cpp)
target_link_libraries(SystemOverviewPresenterTest PRIVATE Qt6::Test TweakopediaApp)
add_test(NAME SystemOverviewPresenter COMMAND SystemOverviewPresenterTest)

add_executable(TweakListModelTest app/TweakListModelTest.cpp)
target_compile_definitions(TweakListModelTest PRIVATE
    TWEAKOPEDIA_TEST_CONTENT_ROOT="${PROJECT_SOURCE_DIR}/content/tweaks"
)
target_link_libraries(TweakListModelTest PRIVATE Qt6::Test TweakopediaApp TweakopediaContent)
add_test(NAME TweakListModel COMMAND TweakListModelTest)

add_executable(TweakFilterProxyModelTest app/TweakFilterProxyModelTest.cpp)
target_compile_definitions(TweakFilterProxyModelTest PRIVATE
    TWEAKOPEDIA_TEST_CONTENT_ROOT="${PROJECT_SOURCE_DIR}/content/tweaks"
)
target_link_libraries(TweakFilterProxyModelTest PRIVATE Qt6::Test TweakopediaApp TweakopediaContent)
add_test(NAME TweakFilterProxyModel COMMAND TweakFilterProxyModelTest)

add_executable(TweakGroupListModelTest app/TweakGroupListModelTest.cpp)
target_link_libraries(TweakGroupListModelTest PRIVATE
    Qt6::Test TweakopediaApp TweakopediaContent
)
add_test(NAME TweakGroupListModel COMMAND TweakGroupListModelTest)

add_executable(EncyclopediaTreeModelTest app/EncyclopediaTreeModelTest.cpp)
target_link_libraries(EncyclopediaTreeModelTest PRIVATE Qt6::Test TweakopediaApp)
add_test(NAME EncyclopediaTreeModel COMMAND EncyclopediaTreeModelTest)

add_executable(EncyclopediaArticleModelTest app/EncyclopediaArticleModelTest.cpp)
target_link_libraries(EncyclopediaArticleModelTest PRIVATE Qt6::Test TweakopediaApp)
add_test(NAME EncyclopediaArticleModel COMMAND EncyclopediaArticleModelTest)

add_executable(EncyclopediaControllerTest app/EncyclopediaControllerTest.cpp)
target_link_libraries(EncyclopediaControllerTest PRIVATE Qt6::Test TweakopediaApp)
add_test(NAME EncyclopediaController COMMAND EncyclopediaControllerTest)

add_executable(QmlComponentTests qml/QmlTestMain.cpp)
target_link_libraries(QmlComponentTests PRIVATE Qt6::QuickTest Qt6::QuickControls2)
add_test(NAME QmlPlanPreviewHistory COMMAND QmlComponentTests -input ${PROJECT_SOURCE_DIR}/tests/qml)
set_tests_properties(QmlPlanPreviewHistory PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_DISABLE_DISK_CACHE=1;QT_QUICK_CONTROLS_STYLE=Basic"
)

add_executable(UiFontLoaderTest
    ui/UiFontLoaderTest.cpp
    ${PROJECT_SOURCE_DIR}/apps/tweakopedia/UiFontLoader.cpp
    ${PROJECT_SOURCE_DIR}/apps/tweakopedia/UiFontLoader.h
    ${PROJECT_SOURCE_DIR}/apps/tweakopedia/fonts.qrc
)
target_include_directories(UiFontLoaderTest PRIVATE ${PROJECT_SOURCE_DIR}/apps/tweakopedia)
target_link_libraries(UiFontLoaderTest PRIVATE Qt6::Gui Qt6::Test)
add_test(NAME UiFontLoader COMMAND UiFontLoaderTest)
set_tests_properties(UiFontLoader PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(BrandingAssetsTest
    ui/BrandingAssetsTest.cpp
    ${PROJECT_SOURCE_DIR}/apps/tweakopedia/resources.qrc
)
target_compile_definitions(BrandingAssetsTest PRIVATE
    TWEAKOPEDIA_TEST_CONTAINER="$<TARGET_FILE:TweakopediaBootstrap>"
    TWEAKOPEDIA_TEST_GUI="$<TARGET_FILE:Tweakopedia>"
    TWEAKOPEDIA_TEST_EXECUTOR="$<TARGET_FILE:TweakopediaExecutor>"
)
target_link_libraries(BrandingAssetsTest PRIVATE Qt6::Gui Qt6::Test)
add_dependencies(BrandingAssetsTest TweakopediaBootstrap Tweakopedia TweakopediaExecutor)
add_test(NAME BrandingAssets COMMAND BrandingAssetsTest)
set_tests_properties(BrandingAssets PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(TweakopediaVisualCapture
    visual/VisualCaptureMain.cpp
    ${PROJECT_SOURCE_DIR}/apps/tweakopedia/UiFontLoader.cpp
    ${PROJECT_SOURCE_DIR}/apps/tweakopedia/UiFontLoader.h
    ${PROJECT_SOURCE_DIR}/apps/tweakopedia/fonts.qrc
    ${PROJECT_SOURCE_DIR}/apps/tweakopedia/resources.qrc
)
target_include_directories(TweakopediaVisualCapture PRIVATE ${PROJECT_SOURCE_DIR}/apps/tweakopedia)
target_link_libraries(TweakopediaVisualCapture PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
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
    Qt6::Test TweakopediaApp TweakopediaContent TweakopediaDetection TweakopediaExecution
    TweakopediaPersistence TweakopediaPlanning TweakopediaPlatform
)
add_test(NAME VerticalSliceWorkflow COMMAND VerticalSliceWorkflowTest)
