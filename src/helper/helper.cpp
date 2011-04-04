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
#include <QtCore/QFile>
#include <QtCore/QTextStream>

//KDE
#include <KAuth/HelperSupport>
#include <KProcess>
#include <KShell>

//Project
#include "../config.h"
#ifdef HAVE_HD
#undef slots
#include <hd.h>
#endif

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
#ifdef HAVE_HD
    QStringList gfxmodes;
    hd_data_t hd_data;
    memset(&hd_data, 0, sizeof(hd_data));
    hd_t *hd = hd_list(&hd_data, hw_framebuffer, 1, NULL);
    for (hd_res_t *res = hd->res; res; res = res->next) {
        if (res->any.type == res_framebuffer) {
            QString width = QString::number(res->framebuffer.width);
            if (res->framebuffer.width / 1000 == 0) {
                width.prepend('0');
            }
            QString height = QString::number(res->framebuffer.height);
            if (res->framebuffer.height / 1000 == 0) {
                height.prepend('0');
            }
            QString colorbits = QString::number(res->framebuffer.colorbits);
            if (res->framebuffer.colorbits / 10 == 0) {
                colorbits.prepend('0');
            }
            gfxmodes << width + 'x' + height + 'x' + colorbits;
        }
    }
    gfxmodes.sort();
    for (int i = 0; i < gfxmodes.size(); i++) {
        if (gfxmodes.at(i).startsWith('0')) {
            gfxmodes[i].remove(0, 1);
        }
        gfxmodes[i].replace("x0", "x");
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
    QString fileName = args.value("fileName").toString();
    QString fileContents = args.value("fileContents").toString();

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        reply = ActionReply::HelperErrorReply;
        reply.setErrorCode(file.error());
        reply.setErrorDescription(file.errorString()); //Caller gets empty error description. KAuth bug?
        return reply;
    }

    QTextStream stream(&file);
    stream << fileContents;
    return reply;
}
ActionReply Helper::update(QVariantMap args)
{
    ActionReply reply;
    KProcess grub_mkconfig;
    grub_mkconfig.setShellCommand(QString("grub-mkconfig -o %1").arg(KShell::quoteArg(args.value("fileName").toString()))); // Run through a shell. For some reason $PATH is empty for the helper. KAuth bug?
    grub_mkconfig.setOutputChannelMode(KProcess::MergedChannels);

    if (grub_mkconfig.execute() != 0) {
        reply = ActionReply::HelperErrorReply;
    }

    reply.addData("output", grub_mkconfig.readAll());
    return reply;
}

KDE4_AUTH_HELPER_MAIN("org.kde.kcontrol.kcmgrub2", Helper)
