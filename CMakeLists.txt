cmake_minimum_required(VERSION 3.10)

project(phototonic LANGUAGES CXX VERSION 3.1)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

IF(MSVC)
    SET(OPTIONS WIN32)
ENDIF(MSVC)

include(version)

add_executable(${PROJECT_NAME} ${OPTIONS}
    ShortcutsTable.cpp
    ThumbsViewer.h
    Phototonic.cpp
    Bookmarks.cpp
    FileSystemTree.h
    MetadataCache.cpp
    SettingsDialog.h
    SmartCrop.cpp
    RenameDialog.h
    ResizeDialog.h
    ResizeDialog.cpp
    Phototonic.h
    ColorsDialog.cpp
    SettingsDialog.cpp
    ColorsDialog.h
    GuideWidget.cpp
    DirCompleter.h
    Bookmarks.h
    MessageBox.h
    ImageWidget.h
    InfoViewer.cpp
    CopyMoveDialog.h
    CopyMoveToDialog.cpp
    DirCompleter.cpp
    CropRubberband.h
    GuideWidget.h
    ImageViewer.cpp
    CopyMoveToDialog.h
    CropRubberband.cpp
    CropDialog.cpp
    Settings.cpp
    Tags.h
    CropDialog.h
    ExternalAppsDialog.cpp
    RenameDialog.cpp
    Tags.cpp
    ThumbsViewer.cpp
    FileSystemTree.cpp
    ExternalAppsDialog.h
    MetadataCache.h
    Settings.h
    ImageWidget.cpp
    ImageViewer.h
    MessageBox.cpp
    main.cpp
    IconProvider.h
    CopyMoveDialog.cpp
    SmartCrop.h
    InfoViewer.h
    ShortcutsTable.h
    IconProvider.cpp
    ${TS_FILES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets OpenGLWidgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets OpenGLWidgets LinguistTools)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt::OpenGLWidgets
        Qt::Widgets
        Qt::Core
)

# translations
set(TRANSLATION_FILES
    translations/phototonic.ts
    translations/phototonic_de.ts
    translations/phototonic_ka.ts
    translations/phototonic_it.ts
    translations/phototonic_sr.ts
    translations/phototonic_uk.ts
    translations/phototonic_cs.ts
    translations/phototonic_fr.ts
    translations/phototonic_es.ts
    translations/phototonic_et.ts
    translations/phototonic_zh.ts
    translations/phototonic_zh_TW.ts
    translations/phototonic_bs.ts
    translations/phototonic_nl.ts
    translations/phototonic_sv.ts
    translations/phototonic_fi.ts
    translations/phototonic_pt.ts
    translations/phototonic_pl.ts
    translations/phototonic_hr.ts
    translations/phototonic_ru.ts
)


set_source_files_properties(${TRANSLATION_FILES}
    PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/translations")

qt_add_translations(${PROJECT_NAME}
    TS_FILES ${TRANSLATION_FILES}
)

# exiv2
find_package(exiv2)
target_link_libraries(${PROJECT_NAME} PRIVATE exiv2lib)

# installation
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    install(FILES images/phototonic.png DESTINATION share/icons/hicolor/512x512/apps)

    install(FILES images/phototonic.png DESTINATION share/icons/hicolor/48x48/apps)
    install(FILES images/icon16/phototonic.png DESTINATION share/icons/hicolor/16x16/apps)
    install(FILES images/icon16/phototonic.png DESTINATION share/pixmaps)

    install(FILES phototonic.desktop DESTINATION share/applications)
    install(FILES phototonic.appdata.xml DESTINATION share/metainfo)

endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

# Auto. CCACHE
if(UNIX)
    find_program(CCACHE_FOUND ccache)

    if(CCACHE_FOUND)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    endif(CCACHE_FOUND)
endif()

# Auto. distcc
if(UNIX)
    find_program(DISTCC_FOUND distcc)

    if(DISTCC_FOUND)
        set(ENV{CCACHE_PREFIX} distcc)
    endif(DISTCC_FOUND)
endif()


option(USE_ASAN "Build with address sanitizer" OFF)
if(USE_ASAN)
    target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
    target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
endif()
