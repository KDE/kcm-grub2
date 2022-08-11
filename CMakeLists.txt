cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(kcm-grub2)

set(PROJECT_VERSION "0.6.4")
set(QT_MIN_VERSION "5.15.2")
set(KF5_MIN_VERSION "5.97")
find_package(ECM ${KF5_MIN_VERSION}  REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

include(ECMInstallIcons)
include(FeatureSummary)

set(KCM_GRUB2_VERSION ${PROJECT_VERSION})

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

include(GRUBPaths)

find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Gui Widgets)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
    Auth
    I18n
    KCMUtils
    KIO
    Solid
)

# Qt
add_definitions(-DQT_NO_KEYWORDS -DQT_NO_FOREACH -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_KEYWORDS -DQT_STRICT_ITERATORS -DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)
# ImageMagick
add_definitions(-DMAGICKCORE_HDRI_ENABLE=0 -DMAGICKCORE_QUANTUM_DEPTH=16)

find_package(ImageMagick COMPONENTS Magick++ MagickCore)
set_package_properties(ImageMagick PROPERTIES
                       DESCRIPTION "Create splash images compatible with GRUB2"
                       URL "http://www.imagemagick.org/"
                       TYPE OPTIONAL)
if(ImageMagick_FOUND)
    set(HAVE_IMAGEMAGICK 1)
else()
    set(HAVE_IMAGEMAGICK 0)
endif()

find_package(HD)
set_package_properties(hwinfo PROPERTIES
                       DESCRIPTION "Retrieve list of resolutions valid in GRUB2"
                       URL "http://www.opensuse.org/"
                       TYPE OPTIONAL)
if(HD_FOUND)
    set(HAVE_HD 1)
else()
    set(HAVE_HD 0)
endif()

find_package(QApt 3.0.0)
set_package_properties(QApt PROPERTIES
                       DESCRIPTION "Remove unneeded old entries (qapt backend)"
                       URL "https://commits.kde.org/libqapt"
                       TYPE OPTIONAL)
if(QApt_FOUND)
    set(HAVE_QAPT 1)
else()
    set(HAVE_QAPT 0)
endif()

find_package(PackageKitQt${QT_MAJOR_VERSION})
set_package_properties(PackageKitQt${QT_MAJOR_VERSION} PROPERTIES
                       DESCRIPTION "Remove unneeded old entries (qpackagekit backend)"
                       URL "http://www.packagekit.org/"
                       TYPE OPTIONAL)
if(PackageKitQt5_FOUND)
    set(HAVE_QPACKAGEKIT 1)
else()
    set(HAVE_QPACKAGEKIT 0)
endif()

configure_file(config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/src/config.h)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

add_subdirectory(src)

find_package(KF5I18n CONFIG REQUIRED)
ki18n_install(po)
