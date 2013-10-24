/*******************************************************************************
 * Copyright (C) 2008-2013 Konstantinos Smanis <konstantinos.smanis@gmail.com> *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify it     *
 * under the terms of the GNU General Public License as published by the Free  *
 * Software Foundation, either version 3 of the License, or (at your option)   *
 * any later version.                                                          *
 *                                                                             *
 * This program is distributed in the hope that it will be useful, but WITHOUT *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for    *
 * more details.                                                               *
 *                                                                             *
 * You should have received a copy of the GNU General Public License along     *
 * with this program. If not, see <http://www.gnu.org/licenses/>.              *
 *******************************************************************************/

//Krazy
//krazy:excludeall=cpp

//Own
#include "helper.h"

//Qt
#include <QDir>

//KDE
#include <KDebug>
#include <KGlobal>
#include <KLocale>
#include <KProcess>
#include <KAuth/HelperSupport>

//Project
#include "../common.h"
#include "../config.h"
#if HAVE_HD
#undef slots
#include <hd.h>
#endif

//The $PATH environment variable is emptied by D-Bus activation,
//so let's provide a sane default. Needed for os-prober to work.
static const QLatin1String path("/usr/sbin:/usr/bin:/sbin:/bin");

Helper::Helper()
{
    KGlobal::locale()->insertCatalog(QLatin1String("kcm-grub2"));
    qputenv("PATH", path.latin1());
}

ActionReply Helper::executeCommand(const QStringList &command)
{
    KProcess process;
    process.setProgram(command);
    process.setOutputChannelMode(KProcess::MergedChannels);

    kDebug() << "Executing" << command.join(QLatin1String(" "));
    int exitCode = process.execute();

    ActionReply reply;
    if (exitCode != 0) {
        reply = ActionReply::HelperErrorReply;
        reply.setErrorCode(exitCode);
    }
    reply.addData(QLatin1String("command"), command);
    reply.addData(QLatin1String("output"), process.readAll());
    return reply;
}
bool Helper::setLang(const QString &lang)
{
    QFile file(QString::fromUtf8(GRUB_MENU));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        kError() << "Failed to open file for reading:" << GRUB_MENU;
        kError() << "Error code:" << file.error();
        kError() << "Error description:" << file.errorString();
        return false;
    }
    QString fileContents = QString::fromUtf8(file.readAll().constData());
    file.close();
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        kError() << "Failed to open file for writing:" << GRUB_MENU;
        kError() << "Error code:" << file.error();
        kError() << "Error description:" << file.errorString();
        return false;
    }
    fileContents.replace(QRegExp(QLatin1String("(\\n\\s*set\\s+lang=)\\S*\\n")), QString(QLatin1String("\\1%1\n")).arg(lang));
    if (file.write(fileContents.toUtf8()) == -1) {
        kError() << "Failed to write data to file:" << GRUB_MENU;
        kError() << "Error code:" << file.error();
        kError() << "Error description:" << file.errorString();
        return false;
    }
    file.close();
    return true;
}

ActionReply Helper::defaults(QVariantMap args)
{
    Q_UNUSED(args)
    ActionReply reply;
    QString configFileName = QString::fromUtf8(GRUB_CONFIG);
    QString originalConfigFileName = configFileName + QLatin1String(".original");

    if (!QFile::exists(originalConfigFileName)) {
        reply = ActionReply::HelperErrorReply;
        reply.addData(QLatin1String("errorDescription"), i18nc("@info", "Original configuration file <filename>%1</filename> does not exist.", originalConfigFileName));
        return reply;
    }
    if (!QFile::remove(configFileName)) {
        reply = ActionReply::HelperErrorReply;
        reply.addData(QLatin1String("errorDescription"), i18nc("@info", "Cannot remove current configuration file <filename>%1</filename>.", configFileName));
        return reply;
    }
    if (!QFile::copy(originalConfigFileName, configFileName)) {
        reply = ActionReply::HelperErrorReply;
        reply.addData(QLatin1String("errorDescription"), i18nc("@info", "Cannot copy original configuration file <filename>%1</filename> to <filename>%2</filename>.", originalConfigFileName, configFileName));
        return reply;
    }
    return reply;
}
ActionReply Helper::install(QVariantMap args)
{
    ActionReply reply;
    QString partition = args.value(QLatin1String("partition")).toString();
    QString mountPoint = args.value(QLatin1String("mountPoint")).toString();
    bool mbrInstall = args.value(QLatin1String("mbrInstall")).toBool();

    if (mountPoint.isEmpty()) {
        for (int i = 0; QDir(mountPoint = QString(QLatin1String("%1/kcm-grub2-%2")).arg(QDir::tempPath(), QString::number(i))).exists(); i++);
        if (!QDir().mkpath(mountPoint)) {
            reply = ActionReply::HelperErrorReply;
            reply.addData(QLatin1String("errorDescription"), i18nc("@info", "Failed to create temporary mount point."));
            return reply;
        }
        ActionReply mountReply = executeCommand(QStringList() << QLatin1String("mount") << partition << mountPoint);
        if (mountReply.failed()) {
            return mountReply;
        }
    }

    QStringList grub_installCommand;
    grub_installCommand << QString::fromUtf8(GRUB_INSTALL_EXE) << QLatin1String("--root-directory") << mountPoint;
    if (mbrInstall) {
        grub_installCommand << partition.remove(QRegExp(QLatin1String("\\d+")));
    } else {
        grub_installCommand << QLatin1String("--force") << partition;
    }
    return executeCommand(grub_installCommand);
}
ActionReply Helper::load(QVariantMap args)
{
    ActionReply reply;
    LoadOperations operations = (LoadOperations)(args.value(QLatin1String("operations")).toInt());

    if (operations.testFlag(MenuFile)) {
        QFile file(QString::fromUtf8(GRUB_MENU));
        bool ok = file.open(QIODevice::ReadOnly | QIODevice::Text);
        reply.addData(QLatin1String("menuSuccess"), ok);
        if (ok) {
            reply.addData(QLatin1String("menuContents"), file.readAll());
        } else {
            reply.addData(QLatin1String("menuError"), file.error());
            reply.addData(QLatin1String("menuErrorString"), file.errorString());
        }
    }
    if (operations.testFlag(ConfigurationFile)) {
        QFile file(QString::fromUtf8(GRUB_CONFIG));
        bool ok = file.open(QIODevice::ReadOnly | QIODevice::Text);
        reply.addData(QLatin1String("configSuccess"), ok);
        if (ok) {
            reply.addData(QLatin1String("configContents"), file.readAll());
        } else {
            reply.addData(QLatin1String("configError"), file.error());
            reply.addData(QLatin1String("configErrorString"), file.errorString());
        }
    }
    if (operations.testFlag(EnvironmentFile)) {
        QFile file(QString::fromUtf8(GRUB_ENV));
        bool ok = file.open(QIODevice::ReadOnly | QIODevice::Text);
        reply.addData(QLatin1String("envSuccess"), ok);
        if (ok) {
            reply.addData(QLatin1String("envContents"), file.readAll());
        } else {
            reply.addData(QLatin1String("envError"), file.error());
            reply.addData(QLatin1String("envErrorString"), file.errorString());
        }
    }
    if (operations.testFlag(MemtestFile)) {
        bool memtest = QFile::exists(QString::fromUtf8(GRUB_MEMTEST));
        reply.addData(QLatin1String("memtest"), memtest);
        if (memtest) {
            reply.addData(QLatin1String("memtestOn"), (bool)(QFile::permissions(QString::fromUtf8(GRUB_MEMTEST)) & (QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther)));
        }
    }
#if HAVE_HD
    if (operations.testFlag(Vbe)) {
        QStringList gfxmodes;
        hd_data_t hd_data;
        memset(&hd_data, 0, sizeof(hd_data));
        hd_t *hd = hd_list(&hd_data, hw_framebuffer, 1, NULL);
        for (hd_res_t *res = hd->res; res; res = res->next) {
            if (res->any.type == res_framebuffer) {
                gfxmodes += QString(QLatin1String("%1x%2x%3")).arg(QString::number(res->framebuffer.width), QString::number(res->framebuffer.height), QString::number(res->framebuffer.colorbits));
            }
        }
        hd_free_hd_list(hd);
        hd_free_hd_data(&hd_data);
        reply.addData(QLatin1String("gfxmodes"), gfxmodes);
    }
#endif
    if (operations.testFlag(Locales)) {
        reply.addData(QLatin1String("locales"), QDir(QString::fromUtf8(GRUB_LOCALE)).entryList(QStringList() << QLatin1String("*.mo"), QDir::Files).replaceInStrings(QRegExp(QLatin1String("\\.mo$")), QString()));
    }
    return reply;
}
ActionReply Helper::save(QVariantMap args)
{
    ActionReply reply;
    QString configFileName = QString::fromUtf8(GRUB_CONFIG);
    QByteArray rawConfigFileContents = args.value(QLatin1String("rawConfigFileContents")).toByteArray();
    QByteArray rawDefaultEntry = args.value(QLatin1String("rawDefaultEntry")).toByteArray();
    bool memtest = args.value(QLatin1String("memtest")).toBool();

    QFile::copy(configFileName, configFileName + QLatin1String(".original"));

    QFile file(configFileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        reply = ActionReply::HelperErrorReply;
        reply.addData(QLatin1String("errorDescription"), file.errorString());
        return reply;
    }
    file.write(rawConfigFileContents);
    file.close();

    if (args.contains(QLatin1String("memtest"))) {
        QFile::Permissions permissions = QFile::permissions(QString::fromUtf8(GRUB_MEMTEST));
        if (memtest) {
            permissions |= (QFile::ExeOwner | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther);
        } else {
            permissions &= ~(QFile::ExeOwner | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther);
        }
        QFile::setPermissions(QString::fromUtf8(GRUB_MEMTEST), permissions);
    }

    if (args.contains(QLatin1String("LANG"))) {
        qputenv("LANG", args.value(QLatin1String("LANG")).toByteArray());
    }
    ActionReply grub_mkconfigReply = executeCommand(QStringList() << QString::fromUtf8(GRUB_MKCONFIG_EXE) << QLatin1String("-o") << QString::fromUtf8(GRUB_MENU));
    if (grub_mkconfigReply.failed()) {
        return grub_mkconfigReply;
    }
    if (args.contains(QLatin1String("LANGUAGE"))) {
        if (!setLang(args.value(QLatin1String("LANGUAGE")).toString())) {
            kError() << "An error occured while setting the language for the GRUB menu.";
            kError() << "The GRUB menu will not be properly translated!";
        }
    }

    ActionReply grub_set_defaultReply = executeCommand(QStringList() << QString::fromUtf8(GRUB_SET_DEFAULT_EXE) << QString::fromUtf8(rawDefaultEntry.constData()));
    if (grub_set_defaultReply.failed()) {
        return grub_set_defaultReply;
    }

    return grub_mkconfigReply;
}

KDE4_AUTH_HELPER_MAIN("org.kde.kcontrol.kcmgrub2", Helper)
