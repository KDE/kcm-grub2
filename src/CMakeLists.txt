include(KDE4Defaults)
include_directories(${QT_INCLUDES} ${KDE4_INCLUDES})

add_subdirectory(helper)

set(kcm_grub2_SRCS
    kcm_grub2.cpp
)

kde4_add_ui_files(kcm_grub2_SRCS
    ../ui/kcm_grub2.ui
)

kde4_add_kcfg_files(kcm_grub2_SRCS
    ../cfg/settings.kcfgc
)

kde4_add_plugin(kcm_grub2 ${kcm_grub2_SRCS})
target_link_libraries(kcm_grub2 ${KDE4_KDEUI_LIBS} ${KDE4_KIO_LIBS})
install(TARGETS kcm_grub2 DESTINATION ${PLUGIN_INSTALL_DIR})
