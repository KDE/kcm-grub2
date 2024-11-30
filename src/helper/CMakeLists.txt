# Set Include Directories
if(HAVE_HD)
    set(kcmgrub2helper_INCLUDE_DIRS ${kcmgrub2helper_INCLUDE_DIRS} ${HD_INCLUDE_DIR})
endif(HAVE_HD)

# Set Sources
set(kcmgrub2helper_SRCS helper.cpp)

# Set Link Libraries
if(HAVE_HD)
    set(kcmgrub2helper_LINK_LIBS ${kcmgrub2helper_LINK_LIBS} ${HD_LIBRARY})
endif(HAVE_HD)

# Build & Link
include_directories(${kcmgrub2helper_INCLUDE_DIRS})
add_executable(kcmgrub2helper ${kcmgrub2helper_SRCS})
target_link_libraries(kcmgrub2helper ${kcmgrub2helper_LINK_LIBS} KF6::AuthCore KF6::I18n)

# Install
install(TARGETS kcmgrub2helper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})
kauth_install_helper_files(kcmgrub2helper org.kde.kcontrol.kcmgrub2 root)
kauth_install_actions(org.kde.kcontrol.kcmgrub2 kcmgrub2.actions)
