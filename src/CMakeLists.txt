add_definitions(-DTRANSLATION_DOMAIN=\"kcm-grub2\")

add_subdirectory(helper)

# Set Include Directories
if(HAVE_IMAGEMAGICK)
    set(kcm_grub2_INCLUDE_DIRS ${kcm_grub2_INCLUDE_DIRS} ${ImageMagick_INCLUDE_DIRS})
endif(HAVE_IMAGEMAGICK)

# Set Sources
set(kcm_grub2_SRCS common.cpp entry.cpp installDlg.cpp kcm_grub2.cpp textinputdialog.cpp)
ki18n_wrap_ui(kcm_grub2_SRCS ../ui/installDlg.ui ../ui/kcm_grub2.ui)
if(HAVE_IMAGEMAGICK)
    set(kcm_grub2_SRCS ${kcm_grub2_SRCS} convertDlg.cpp)
    ki18n_wrap_ui(kcm_grub2_SRCS ../ui/convertDlg.ui)
endif(HAVE_IMAGEMAGICK)
if(HAVE_QAPT OR HAVE_QPACKAGEKIT)
    set(kcm_grub2_SRCS ${kcm_grub2_SRCS} removeDlg.cpp)
    ki18n_wrap_ui(kcm_grub2_SRCS ../ui/removeDlg.ui)
endif(HAVE_QAPT OR HAVE_QPACKAGEKIT)
if(HAVE_QAPT)
    set(kcm_grub2_SRCS ${kcm_grub2_SRCS} qaptBackend.cpp)
elseif(HAVE_QPACKAGEKIT)
    set(kcm_grub2_SRCS ${kcm_grub2_SRCS} qPkBackend.cpp)
endif(HAVE_QAPT)

# Set Link Libraries
if(HAVE_IMAGEMAGICK)
    set(kcm_grub2_LINK_LIBS ${kcm_grub2_LINK_LIBS} ${ImageMagick_LIBRARIES})
endif(HAVE_IMAGEMAGICK)
if(HAVE_QAPT)
    set(kcm_grub2_LINK_LIBS ${kcm_grub2_LINK_LIBS} QApt::Main)
elseif(HAVE_QPACKAGEKIT)
    set(kcm_grub2_LINK_LIBS ${kcm_grub2_LINK_LIBS} ${PackageKitQt5_LIBRARIES})
endif(HAVE_QAPT)

# Definitions
if(HAVE_IMAGEMAGICK)
    add_definitions(-fexceptions)
endif(HAVE_IMAGEMAGICK)

# Build & Link
include_directories(${kcm_grub2_INCLUDE_DIRS})
add_library(kcm_grub2 MODULE ${kcm_grub2_SRCS})
target_link_libraries(kcm_grub2 ${kcm_grub2_LINK_LIBS} Qt5::Core Qt5::Widgets KF5::Auth KF5::I18n KF5::KCMUtils KF5::KIOCore KF5::KIOWidgets KF5::Solid)

# Install
install(TARGETS kcm_grub2 DESTINATION ${PLUGIN_INSTALL_DIR})
