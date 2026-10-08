add_library(TweakopediaApp STATIC
    AppController.cpp
    AppController.h
    HistoryListModel.cpp
    HistoryListModel.h
    QueueListModel.cpp
    QueueListModel.h
    TweakListModel.cpp
    TweakListModel.h
)

target_include_directories(TweakopediaApp PUBLIC ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(TweakopediaApp PUBLIC
    Qt6::Core
    TweakopediaContent
    TweakopediaDetection
    TweakopediaPersistence
    TweakopediaPlanning
)
