﻿/*******************************************************************************
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

#ifndef KCMGRUB2_H
#define KCMGRUB2_H

//Qt
#include <QBitArray>

//KDE
#include <KCModule>
namespace KAuth
{
    class ActionReply;
}
using namespace KAuth;

//Project
#include "config.h"
class Entry;

//Ui
namespace Ui
{
    class KCMGRUB2;
}

class KCMGRUB2 : public KCModule
{
    Q_OBJECT
public:
    explicit KCMGRUB2(QWidget *parent = 0, const QVariantList &list = QVariantList());
    virtual ~KCMGRUB2();

    virtual void defaults();
    virtual void load();
    virtual void save();
private Q_SLOTS:
    void slotRemoveOldEntries();
    void slotGrubSavedefaultChanged();
    void slotGrubHiddenTimeoutToggled(bool checked);
    void slotGrubHiddenTimeoutChanged();
    void slotGrubHiddenTimeoutQuietChanged();
    void slotGrubTimeoutToggled(bool checked);
    void slotGrubTimeoutChanged();
    void slotGrubDisableRecoveryChanged();
    void slotMemtestChanged();
    void slotGrubDisableOsProberChanged();
    void slotInstallBootloader();
    void slotGrubGfxmodeChanged();
    void slotGrubGfxpayloadLinuxChanged();
    void slotGrubColorNormalChanged();
    void slotGrubColorHighlightChanged();
    void slowGrubBackgroundChanged();
    void slotPreviewGrubBackground();
    void slotCreateGrubBackground();
    void slotGrubThemeChanged();
    void slotGrubCmdlineLinuxDefaultChanged();
    void slotGrubCmdlineLinuxChanged();
    void slotGrubTerminalChanged();
    void slotGrubTerminalInputChanged();
    void slotGrubTerminalOutputChanged();
    void slotGrubDistributorChanged();
    void slotGrubSerialCommandChanged();
    void slotGrubInitTuneChanged();
    void slotGrubDisableLinuxUuidChanged();

    void slotUpdateSuggestions();
    void slotTriggeredSuggestion(QAction *action);
private:
    void setupObjects();
    void setupConnections();

    //TODO: Maybe remove?
    QString convertToGRUBFileName(const QString &fileName);
    QString convertToLocalFileName(const QString &grubFileName);

    ActionReply loadFile(GrubFile grubFile);
    QString readFile(GrubFile grubFile);
    void readEntries();
    void readSettings();
    void readEnv();
    void readMemtest();
    void readDevices();
    void readResolutions();

    void sortResolutions();
    void showResolutions();

    void processReply(ActionReply &reply);
    QString parseTitle(const QString &line);
    void parseEntries(const QString &config);
    void parseSettings(const QString &config);
    void parseEnv(const QString &config);

    Ui::KCMGRUB2 *ui;

    enum {
        grubSavedefaultDirty,
        grubHiddenTimeoutDirty,
        grubHiddenTimeoutQuietDirty,
        grubTimeoutDirty,
        grubDisableRecoveryDirty,
        memtestDirty,
        grubDisableOsProberDirty,
        grubGfxmodeDirty,
        grubGfxpayloadLinuxDirty,
        grubColorNormalDirty,
        grubColorHighlightDirty,
        grubBackgroundDirty,
        grubThemeDirty,
        grubCmdlineLinuxDefaultDirty,
        grubCmdlineLinuxDirty,
        grubTerminalDirty,
        grubTerminalInputDirty,
        grubTerminalOutputDirty,
        grubDistributorDirty,
        grubSerialCommandDirty,
        grubInitTuneDirty,
        grubDisableLinuxUuidDirty,
        lastDirtyBit
    };
    QBitArray m_dirtyBits;

    QList<Entry> m_entries;
    QHash<QString, QString> m_settings;
    QHash<QString, QString> m_env;
    bool m_memtest;
    bool m_memtestOn;
    QHash<QString, QString> m_devices;
    QStringList m_resolutions;
};

#endif
