/*******************************************************************************
 * Copyright (C) 2008-2011 Konstantinos Smanis <konstantinos.smanis@gmail.com> *
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

//Own
#include "helper.h"

//Qt
#include <QtCore/QDir>
#include <QtCore/QTextCodec>
#include <QtCore/QTextStream>

//KDE
#include <KGlobal>
#include <KLocale>
#include <KProcess>
#include <KShell>
#include <KAuth/HelperSupport>

//Project
#include "../config.h"
#if HAVE_HD
#undef slots
#include <hd.h>
#endif

Helper::Helper()
{
    KGlobal::locale()->insertCatalog("kcm-grub2");
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
}

ActionReply Helper::defaults(QVariantMap args)
{
    QString configFileName = args.value("configFileName").toString();
    QString originalConfigFileName = configFileName + ".original";
    return (QFile::exists(originalConfigFileName) && QFile::remove(configFileName) && QFile::copy(originalConfigFileName, configFileName) ? ActionReply::SuccessReply : ActionReply::HelperErrorReply);
}
ActionReply Helper::install(QVariantMap args)
{
    ActionReply reply;
    QString installExePath = args.value("installExePath").toString();
    QString partition = args.value("partition").toString();
    QString mountPoint = args.value("mountPoint").toString();

    if (mountPoint.isEmpty()) {
        for (int i = 0; QDir(mountPoint = QString("%1/kcm-grub2-%2").arg(QDir::tempPath(), QString::number(i))).exists(); i++);
        if (QDir().mkpath(mountPoint)) {
            KProcess mount;
            mount.setShellCommand(QString("mount %1 %2").arg(KShell::quoteArg(partition), KShell::quoteArg(mountPoint)));
            mount.setOutputChannelMode(KProcess::MergedChannels);
            if (mount.execute() != 0) {
                reply = ActionReply::HelperErrorReply;
                reply.addData("output", mount.readAll());
                return reply;
            }
        } else {
            reply = ActionReply::HelperErrorReply;
            reply.addData("output", i18nc("@info", "Failed to create temporary mount point."));
            return reply;
        }
    }

    KProcess grub_install;
    grub_install.setShellCommand(QString("%1 --root-directory=%2 %3").arg(installExePath, KShell::quoteArg(mountPoint), KShell::quoteArg(partition.remove(QRegExp("\\d+")))));
    grub_install.setOutputChannelMode(KProcess::MergedChannels);
    if (grub_install.execute() != 0) {
        reply = ActionReply::HelperErrorReply;
        reply.addData("output", grub_install.readAll());
        return reply;
    }

    reply.addData("output", grub_install.readAll());
    return reply;
}
ActionReply Helper::load(QVariantMap args)
{
    ActionReply reply;
    QString fileName = args.value("fileName").toString();

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        reply = ActionReply::HelperErrorReply;
        reply.addData("output", file.errorString());
        return reply;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    reply.addData("fileContents", stream.readAll());
    return reply;
}
ActionReply Helper::probe(QVariantMap args)
{
    ActionReply reply;
    QString probeExePath = args.value("probeExePath").toString();
    QStringList mountPoints = args.value("mountPoints").toStringList();

    KProcess grub_probe;
    QStringList grubPartitions;
    HelperSupport::progressStep(0);
    for (int i = 0; i < mountPoints.size(); i++) {
        grub_probe.setShellCommand(QString("%1 -t drive %2").arg(probeExePath, KShell::quoteArg(mountPoints.at(i))));
        grub_probe.setOutputChannelMode(KProcess::MergedChannels);
        if (grub_probe.execute() != 0) {
            reply = ActionReply::HelperErrorReply;
            reply.addData("output", grub_probe.readAll());
            return reply;
        }
        grubPartitions.append(grub_probe.readAll().trimmed());
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
    qputenv("PATH", args.value("PATH").toByteArray());
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
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        stream << configFileContents;
        file.close();
    } else {
        reply = ActionReply::HelperErrorReply;
        reply.addData("output", file.errorString());
        return reply;
    }

    if (args.contains("memtest")) {
        QFile::Permissions permissions = QFile::permissions(memtestFileName);
        if (memtest) {
            permissions |= (QFile::ExeOwner | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther);
        } else {
            permissions &= ~(QFile::ExeOwner | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther);
        }
        QFile::setPermissions(memtestFileName, permissions);
    }

    KProcess grub_mkconfig;
    grub_mkconfig.setShellCommand(QString("%1 -o %2").arg(mkconfigExePath, KShell::quoteArg(menuFileName)));
    grub_mkconfig.setOutputChannelMode(KProcess::MergedChannels);
    if (grub_mkconfig.execute() != 0) {
        reply = ActionReply::HelperErrorReply;
        reply.addData("output", grub_mkconfig.readAll());
        return reply;
    }

    KProcess grub_set_default;
    grub_set_default.setShellCommand(QString("%1 %2").arg(set_defaultExePath, defaultEntry));
    grub_set_default.setOutputChannelMode(KProcess::MergedChannels);
    if (grub_set_default.execute() != 0) {
        reply = ActionReply::HelperErrorReply;
        reply.addData("output", grub_set_default.readAll());
        return reply;
    }

    reply.addData("output", grub_mkconfig.readAll());
    return reply;
}

KDE4_AUTH_HELPER_MAIN("org.kde.kcontrol.kcmgrub2", Helper)
