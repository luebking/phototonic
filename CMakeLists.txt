cmake_minimum_required(VERSION 3.16)
project(phototonic
    VERSION 3.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 COMPONENTS Widgets OpenGLWidgets REQUIRED)
find_package(exiv2 REQUIRED)

set(HEADERS
    Phototonic.h ThumbsViewer.h ImageViewer.h CropRubberband.h SettingsDialog.h Settings.h InfoViewer.h
    FileSystemTree.h Bookmarks.h DirCompleter.h Tags.h MetadataCache.h ShortcutsTable.h CopyMoveDialog.h
    CopyMoveToDialog.h CropDialog.h ColorsDialog.h ResizeDialog.h ExternalAppsDialog.h
    ImageWidget.h RenameDialog.h MessageBox.h GuideWidget.h SmartCrop.h
)

set(SOURCES
    main.cpp Phototonic.cpp ThumbsViewer.cpp ImageViewer.cpp CropRubberband.cpp SettingsDialog.cpp
    Settings.cpp InfoViewer.cpp FileSystemTree.cpp Bookmarks.cpp DirCompleter.cpp Tags.cpp
    MetadataCache.cpp ShortcutsTable.cpp CopyMoveDialog.cpp CopyMoveToDialog.cpp CropDialog.cpp
    ExternalAppsDialog.cpp ColorsDialog.cpp ResizeDialog.cpp ImageWidget.cpp RenameDialog.cpp
    MessageBox.cpp GuideWidget.cpp IconProvider.cpp SmartCrop.cpp
)

qt_add_resources(RESOURCES phototonic.qrc)

add_executable(phototonic
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

target_link_libraries(phototonic
    PRIVATE
        Qt6::Widgets
        Qt6::OpenGLWidgets
        exiv2
)

file(GLOB TS_FILES "translations/*.ts")
find_package(Qt6 REQUIRED COMPONENTS LinguistTools)
qt6_add_lrelease(phototonic
    TS_FILES ${TS_FILES}
    QM_FILES_OUTPUT_VARIABLE QM_FILES
)

include(GNUInstallDirs)

if(UNIX AND NOT APPLE)
    install(TARGETS phototonic
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(FILES images/phototonic.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps
    )

    install(FILES images/icon16/phototonic.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/16x16/apps
    )

    install(FILES images/icon16/phototonic.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps
    )

    install(FILES phototonic.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )

    install(FILES phototonic.appdata.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
    )

    install(FILES ${QM_FILES}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/phototonic/lang
    )
endif()