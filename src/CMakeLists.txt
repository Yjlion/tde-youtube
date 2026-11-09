include_directories(
  ${CMAKE_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${TDE_INCLUDE_DIR}
  ${TQT_INCLUDE_DIRS}
)

link_directories(
  ${TQT_LIBRARY_DIRS}
  ${TDE_LIB_DIR}
)


##### tde-youtube (executable)

tde_add_executable( tde-youtube AUTOMOC

  SOURCES
    main.cpp
    mainwindow.cpp
    settings.cpp
    settingsdialog.cpp
    searchbackend.cpp
    thumbnailloader.cpp
    videolist.cpp
    detailspane.cpp
    mpvplayer.cpp
    helperupdater.cpp
    playercontrols.cpp
    library.cpp

  LINK
    tdecore-shared
    tdeui-shared
    tdeio-shared

  DESTINATION ${BIN_INSTALL_DIR}
)


##### other data

install( FILES tde-youtubeui.rc DESTINATION ${DATA_INSTALL_DIR}/tde-youtube )
tde_create_translated_desktop( tde-youtube.desktop )
