#ifndef CONFIG_H
#define CONFIG_H

#define KCM_GRUB2_VERSION "@KCM_GRUB2_VERSION@"

#define HAVE_IMAGEMAGICK @HAVE_IMAGEMAGICK@
#define HAVE_HD @HAVE_HD@
#define HAVE_QAPT @HAVE_QAPT@
#define HAVE_QPACKAGEKIT @HAVE_QPACKAGEKIT@
#define QAPT_VERSION_MAJOR @QAPT_VERSION_MAJOR@

//Qt
#include <QFile>

const QString & grubInstallExePath()
{
    static const QString str = QFile::decodeName("@GRUB_INSTALL_EXE@");
    return str;
}
const QString & grubMkconfigExePath()
{
    static const QString str = QFile::decodeName("@GRUB_MKCONFIG_EXE@");
    return str;
}
const QString & grubProbeExePath()
{
    static const QString str = QFile::decodeName("@GRUB_PROBE_EXE@");
    return str;
}
const QString & grubSetDefaultExePath()
{
    static const QString str = QFile::decodeName("@GRUB_SET_DEFAULT_EXE@");
    return str;
}
const QString & grubMenuPath()
{
    static const QString str = QFile::decodeName("@GRUB_MENU@");
    return str;
}
const QString & grubConfigPath()
{
    static const QString str = QFile::decodeName("@GRUB_CONFIG@");
    return str;
}
const QString & grubEnvPath()
{
    static const QString str = QFile::decodeName("@GRUB_ENV@");
    return str;
}
const QString & grubMemtestPath()
{
    static const QString str = QFile::decodeName("@GRUB_MEMTEST@");
    return str;
}
const QString & grubLocalePath()
{
    static const QString str = QFile::decodeName("@GRUB_LOCALE@");
    return str;
}

#endif
