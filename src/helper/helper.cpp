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
#include <QTextCodec>
#include <QTextStream>

//KDE
#include <KDebug>
#include <kdeversion.h>
#include <KGlobal>
#include <KLocale>
#include <KProcess>
#include <KStandardDirs>
#include <KAuth/HelperSupport>

//Project
#include "../config.h"
#if HAVE_HD
#undef slots
#include <hd.h>
#endif

//The $PATH environment variable is emptied by D-Bus activation,
//so let's provide a sane default. To be used as a fallback.
static const QString path = QLatin1String("/usr/sbin:/usr/bin:/sbin:/bin");

Helper::Helper()
{
    KGlobal::locale()->insertCatalog("kcm-grub2");
    //-l stands for --login. A login shell is needed in order to properly
    //source /etc/profile, ~/.profile and/or other shell-specific login
    //scripts (such as ~/.bash_profile for Bash).
    //Not all shells implement this option, in which case we have the fallback.
    //Bash, DASH, ksh and Zsh seem to work properly.
    ActionReply pathReply = executeCommand(QStringList() << findShell() << "-l" << "-c" << "echo $PATH");
    if (pathReply.succeeded()) {
        qputenv("PATH", pathReply.data().value("output").toByteArray().trimmed());
    } else {
        qputenv("PATH", path.toLatin1());
    }
    //TODO: system encoding should be sent from the core application
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
}

ActionReply Helper::executeCommand(const QStringList &command)
{
    ActionReply reply;
    reply.addData("command", command);

    KProcess process;
    process.setProgram(command);
    process.setOutputChannelMode(KProcess::MergedChannels);

    kDebug() << "Executing" << command.join(" ");
    int exitCode = process.execute();
    reply.addData("exitCode", exitCode);
    QString output = QString::fromUtf8(process.readAll());
    reply.addData("output", output);

    if (exitCode == 0) {
        return reply;
    }

    QString errorMessage;
    switch (exitCode) {
    case -2:
        errorMessage = i18nc("@info", "The process could not be started.");
        break;
    case -1:
        errorMessage = i18nc("@info", "The process crashed.");
        break;
    default:
        errorMessage = output;
        break;
    }
    QString errorDescription = i18nc("@info", "Command: <command>%1</command><nl/>Error code: <numid>%2</numid><nl/>Error message:<nl/><message>%3</message>", command.join(" "), exitCode, errorMessage);

    reply = ActionReply::HelperErrorReply;
    reply.setErrorCode(exitCode);
    reply.addData("errorMessage", errorMessage);
#if KDE_IS_VERSION(4,7,0)
    reply.setErrorDescription(errorDescription);
#else
    reply.addData("errorDescription", errorDescription);
#endif
    return reply;
}
QString Helper::findShell()
{
    QString shell = QFile::symLinkTarget(QLatin1String("/bin/sh"));
    if (shell.isEmpty()) {
        shell = KStandardDirs::findExe(QLatin1String("bash"), path);
        if (shell.isEmpty()) {
            shell = KStandardDirs::findExe(QLatin1String("dash"), path);
            if (shell.isEmpty()) {
                shell = KStandardDirs::findExe(QLatin1String("ksh"), path);
                if (shell.isEmpty()) {
                    shell = KStandardDirs::findExe(QLatin1String("zsh"), path);
                    if (shell.isEmpty()) {
                        shell = QLatin1String("/bin/sh");
                    }
                }
            }
        }
    }
    return shell;
}

ActionReply Helper::defaults(QVariantMap args)
{
    ActionReply reply;
    QString configFileName = args.value("configFileName").toString();
    QString originalConfigFileName = configFileName + ".original";

    if (!QFile::exists(originalConfigFileName)) {
        reply = ActionReply::HelperErrorReply;
        QString errorDescription = i18nc("@info", "Original configuration file <filename>%1</filename> does not exist.", originalConfigFileName);
#if KDE_IS_VERSION(4,7,0)
        reply.setErrorDescription(errorDescription);
#else
        reply.addData("errorDescription", errorDescription);
#endif
        return reply;
    }
    if (!QFile::remove(configFileName)) {
        reply = ActionReply::HelperErrorReply;
        QString errorDescription = i18nc("@info", "Cannot remove current configuration file <filename>%1</filename>.", configFileName);
#if KDE_IS_VERSION(4,7,0)
        reply.setErrorDescription(errorDescription);
#else
        reply.addData("errorDescription", errorDescription);
#endif
        return reply;
    }
    if (!QFile::copy(originalConfigFileName, configFileName)) {
        reply = ActionReply::HelperErrorReply;
        QString errorDescription = i18nc("@info", "Cannot copy original configuration file <filename>%1</filename> to <filename>%2</filename>.", originalConfigFileName, configFileName);
#if KDE_IS_VERSION(4,7,0)
        reply.setErrorDescription(errorDescription);
#else
        reply.addData("errorDescription", errorDescription);
#endif
        return reply;
    }
    return reply;
}
ActionReply Helper::install(QVariantMap args)
{
    ActionReply reply;
    QString installExePath = args.value("installExePath").toString();
    QString partition = args.value("partition").toString();
    QString mountPoint = args.value("mountPoint").toString();
    bool mbrInstall = args.value("mbrInstall").toBool();

    if (mountPoint.isEmpty()) {
        for (int i = 0; QDir(mountPoint = QString("%1/kcm-grub2-%2").arg(QDir::tempPath(), QString::number(i))).exists(); i++);
        if (!QDir().mkpath(mountPoint)) {
            reply = ActionReply::HelperErrorReply;
            QString errorDescription = i18nc("@info", "Failed to create temporary mount point.");
#if KDE_IS_VERSION(4,7,0)
            reply.setErrorDescription(errorDescription);
#else
            reply.addData("errorDescription", errorDescription);
#endif
            return reply;
        }
        ActionReply mountReply = executeCommand(QStringList() << "mount" << partition << mountPoint);
        if (mountReply.failed()) {
            return mountReply;
        }
    }

    QStringList grub_installCommand;
    grub_installCommand << installExePath << "--root-directory" << mountPoint;
    if (mbrInstall) {
        grub_installCommand << partition.remove(QRegExp("\\d+"));
    } else {
        grub_installCommand << "--force" << partition;
    }
    return executeCommand(grub_installCommand);
}
ActionReply Helper::load(QVariantMap args)
{
    ActionReply reply;
    QString fileName = args.value("fileName").toString();

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        reply = ActionReply::HelperErrorReply;
#if KDE_IS_VERSION(4,7,0)
        reply.setErrorDescription(file.errorString());
#else
        reply.addData("errorDescription", file.errorString());
#endif
        return reply;
    }

    QTextStream stream(&file);
    reply.addData("fileContents", stream.readAll());
    return reply;
}
ActionReply Helper::probe(QVariantMap args)
{
    ActionReply reply;
    QString probeExePath = args.value("probeExePath").toString();
    QStringList mountPoints = args.value("mountPoints").toStringList();

    QStringList grubPartitions;
    HelperSupport::progressStep(0);
    for (int i = 0; i < mountPoints.size(); i++) {
        ActionReply grub_probeReply = executeCommand(QStringList() << probeExePath << "-t" << "drive" << mountPoints.at(i));
        if (grub_probeReply.failed()) {
            return grub_probeReply;
        }
        grubPartitions.append(grub_probeReply.data().value("output").toString().trimmed());
        HelperSupport::progressStep((i + 1) * 100. / mountPoints.size());
    }

    reply.addData("grubPartitions", grubPartitions);
    return reply;
}
ActionReply Helper::probevbe(QVariantMap args)
{
    Q_UNUSED(args)
    ActionReply reply;

#if HAVE_HD
    QStringList gfxmodes;
    hd_data_t hd_data;
    memset(&hd_data, 0, sizeof(hd_data));
    hd_t *hd = hd_list(&hd_data, hw_framebuffer, 1, NULL);
    for (hd_res_t *res = hd->res; res; res = res->next) {
        if (res->any.type == res_framebuffer) {
            gfxmodes += QString("%1x%2x%3").arg(QString::number(res->framebuffer.width), QString::number(res->framebuffer.height), QString::number(res->framebuffer.colorbits));
        }
    }
    hd_free_hd_list(hd);
    hd_free_hd_data(&hd_data);
    reply.addData("gfxmodes", gfxmodes);
#else
    reply = ActionReply::HelperErrorReply;
#endif

    return reply;
}
ActionReply Helper::save(QVariantMap args)
{
    ActionReply reply;
    QString mkconfigExePath = args.value("mkconfigExePath").toString();
    QString set_defaultExePath = args.value("set_defaultExePath").toString();
    QString configFileName = args.value("configFileName").toString();
    QString configFileContents = args.value("configFileContents").toString();
    QString menuFileName = args.value("menuFileName").toString();
    QString defaultEntry = args.value("defaultEntry").toString();
    QString memtestFileName = args.value("memtestFileName").toString();
    bool memtest = args.value("memtest").toBool();

    QFile::copy(configFileName, configFileName + ".original");

    QFile file(configFileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        reply = ActionReply::HelperErrorReply;
#if KDE_IS_VERSION(4,7,0)
        reply.setErrorDescription(file.errorString());
#else
        reply.addData("errorDescription", file.errorString());
#endif
        return reply;
    }
    QTextStream stream(&file);
    stream << configFileContents;
    file.close();

    if (args.contains("memtest")) {
        QFile::Permissions permissions = QFile::permissions(memtestFileName);
        if (memtest) {
            permissions |= (QFile::ExeOwner | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther);
        } else {
            permissions &= ~(QFile::ExeOwner | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther);
        }
        QFile::setPermissions(memtestFileName, permissions);
    }

    ActionReply grub_mkconfigReply = executeCommand(QStringList() << mkconfigExePath << "-o" << menuFileName);
    if (grub_mkconfigReply.failed()) {
        return grub_mkconfigReply;
    }

    ActionReply grub_set_defaultReply = executeCommand(QStringList() << set_defaultExePath << defaultEntry);
    if (grub_set_defaultReply.failed()) {
        return grub_set_defaultReply;
    }

    return grub_mkconfigReply;
}

KDE4_AUTH_HELPER_MAIN("org.kde.kcontrol.kcmgrub2", Helper)
