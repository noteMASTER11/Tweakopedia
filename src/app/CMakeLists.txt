add_library(TweakopediaApp STATIC
    AppController.cpp
    AppController.h
    CategoryListModel.cpp
    CategoryListModel.h
    EncyclopediaArticleModel.cpp
    EncyclopediaArticleModel.h
    EncyclopediaController.cpp
    EncyclopediaController.h
    EncyclopediaTreeModel.cpp
    EncyclopediaTreeModel.h
    HistoryListModel.cpp
    HistoryListModel.h
    QueueListModel.cpp
    QueueListModel.h
    SessionLogger.cpp
    SessionLogger.h
    SettingsController.cpp
    SettingsController.h
    SystemOverviewPresenter.cpp
    SystemOverviewPresenter.h
    TweakListModel.cpp
    TweakListModel.h
    TweakFilterProxyModel.cpp
    TweakFilterProxyModel.h
)

target_include_directories(TweakopediaApp PUBLIC ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(TweakopediaApp PUBLIC
    Qt6::Core
    Qt6::Concurrent
    TweakopediaContent
    TweakopediaDetection
    TweakopediaPersistence
    TweakopediaPlanning
)
