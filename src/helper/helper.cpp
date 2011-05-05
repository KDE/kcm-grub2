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
#include <QtCore/QFile>
#include <QtCore/QTextStream>

//KDE
#include <KProcess>
#include <KShell>
#include <KAuth/HelperSupport>

//Project
#include "../config.h"
#if HAVE_HD
#undef slots
#include <hd.h>
#endif

ActionReply Helper::defaults(QVariantMap args)
{
    ActionReply reply;
    QString configFileName = args.value("configFileName").toString();
    return ((QFile::exists(configFileName + ".original") && QFile::remove(configFileName) && QFile::copy(configFileName + ".original", configFileName)) ? reply : ActionReply::HelperErrorReply);
}
ActionReply Helper::install(QVariantMap args)
{
    ActionReply reply;
    QString partition = args.value("partition").toString();
    QString mountPoint = args.value("mountPoint").toString();

    if (mountPoint.isEmpty()) {
        if (QDir::temp().mkdir("kcm-grub2")) {
            mountPoint = QDir::tempPath() + "/kcm-grub2";

            KProcess mount;
            mount.setShellCommand(QString("mount %1 %2").arg(partition, mountPoint)); // Run through a shell. For some reason $PATH is empty for the helper. KAuth bug?
            mount.setOutputChannelMode(KProcess::MergedChannels);
            if (mount.execute() != 0) {
                reply.addData("output", mount.readAll());
                reply = ActionReply::HelperErrorReply;
                return reply;
            }
        } else {
            reply = ActionReply::HelperErrorReply;
            return reply;
        }
    }

    KProcess grub_install;
    grub_install.setShellCommand(QString("grub-install --root-directory=%1 %2").arg(mountPoint, partition.remove(QRegExp("\\d+")))); // Run through a shell. For some reason $PATH is empty for the helper. KAuth bug?
    grub_install.setOutputChannelMode(KProcess::MergedChannels);
    if (grub_install.execute() != 0) {
        reply.addData("output", grub_install.readAll());
        reply = ActionReply::HelperErrorReply;
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
        reply.setErrorCode(file.error());
        reply.setErrorDescription(file.errorString()); //Caller gets empty error description. KAuth bug?
        return reply;
    }

    QTextStream stream(&file);
    reply.addData("fileContents", stream.readAll());
    return reply;
}
ActionReply Helper::probe(QVariantMap args)
{
    ActionReply reply;
    QStringList mountPoints = args.value("mountPoints").toStringList(), grubPartitions;
    KProcess grub_probe;
    HelperSupport::progressStep(0);
    for (int i = 0; i < mountPoints.size(); i++) {
        grub_probe.setShellCommand(QString("grub-probe -t drive %1").arg(KShell::quoteArg(mountPoints.at(i)))); // Run through a shell. For some reason $PATH is empty for the helper. KAuth bug?
        grub_probe.setOutputChannelMode(KProcess::MergedChannels);
        int ret = grub_probe.execute();
        if (ret != 0) {
            reply = ActionReply::HelperErrorReply;
            reply.setErrorCode(ret);
            reply.setErrorDescription(grub_probe.readAll());
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
    QString configFileName = args.value("configFileName").toString();
    QFile::copy(configFileName, configFileName + ".original");
    QFile file(configFileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << args.value("configFileContents").toString();
        file.close();
    } else {
        reply.addData("output", file.errorString());
        reply = ActionReply::HelperErrorReply;
        return reply;
    }

    if (args.contains("memtest")) {
        QString memtestFileName = args.value("memtestFileName").toString();
        QFile::Permissions permissions = QFile::permissions(memtestFileName);
        if (args.value("memtest").toBool()) {
            permissions |= (QFile::ExeOwner | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther);
        } else {
            permissions &= ~(QFile::ExeOwner | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther);
        }
        QFile::setPermissions(memtestFileName, permissions);
    }

    KProcess grub_mkconfig;
    grub_mkconfig.setShellCommand(QString("grub-mkconfig -o %1").arg(KShell::quoteArg(args.value("menuFileName").toString()))); // Run through a shell. For some reason $PATH is empty for the helper. KAuth bug?
    grub_mkconfig.setOutputChannelMode(KProcess::MergedChannels);
    if (grub_mkconfig.execute() != 0) {
        reply.addData("output", grub_mkconfig.readAll());
        reply = ActionReply::HelperErrorReply;
        return reply;
    }

    KProcess grub_set_default;
    grub_set_default.setShellCommand(QString("grub-set-default %1").arg(args.value("defaultEntry").toString())); // Run through a shell. For some reason $PATH is empty for the helper. KAuth bug?
    grub_set_default.setOutputChannelMode(KProcess::MergedChannels);
    if (grub_set_default.execute() != 0) {
        reply.addData("output", grub_set_default.readAll());
        reply = ActionReply::HelperErrorReply;
        return reply;
    }

    reply.addData("output", grub_mkconfig.readAll());
    return reply;
}

KDE4_AUTH_HELPER_MAIN("org.kde.kcontrol.kcmgrub2", Helper)
