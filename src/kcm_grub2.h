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

#ifndef KCMGRUB2_H
#define KCMGRUB2_H

//Qt
#include <QtCore/QBitArray>

//KDE
#include <KCModule>

//Ui
#include "ui_kcm_grub2.h"

class KCMGRUB2 : public KCModule
{
    Q_OBJECT
public:
    KCMGRUB2(QWidget *parent = 0, const QVariantList &list = QVariantList());

    virtual void load();
    virtual void save();
private Q_SLOTS:
    void slotRemoveOldEntries();
    void slotGrubSavedefaultChanged();
    void slotGrubHiddenTimeoutChanged();
    void slotGrubHiddenTimeoutQuietChanged();
    void slotGrubTimeoutChanged();
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
    void slotGrubDisableRecoveryChanged();
    void slotGrubDisableOsProberChanged();

    void slotUpdateSuggestions();
    void slotTriggeredSuggestion(QAction *action);
private:
    void setupObjects();
    void setupConnections();

    //TODO: Maybe remove?
    QString convertToGRUBFileName(const QString &fileName);
    QString convertToLocalFileName(const QString &grubFileName);

    QString readFile(const QString &fileName);
    bool readResolutions();
    bool readDevices();
    bool readEntries();
    bool readSettings();
    bool readEnv();

    void sortResolutions();
    void showResolutions();

    QString quoteWord(const QString &word);
    QString unquoteWord(const QString &word);

    void parseEntries(const QString &config);
    void parseSettings(const QString &config);
    void parseEnv(const QString &config);

    Ui::KCMGRUB2 ui;

    enum {
        grubSavedefaultDirty,
        grubHiddenTimeoutDirty,
        grubHiddenTimeoutQuietDirty,
        grubTimeoutDirty,
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
        grubDisableRecoveryDirty,
        grubDisableOsProberDirty,
        lastDirtyBit
    };
    QBitArray m_dirtyBits;
    QStringList m_resolutions;
    QHash<QString, QString> m_devices;
    QStringList m_entries;
    QHash<QString, QString> m_kernels;
    QHash<QString, QString> m_settings;
    QHash<QString, QString> m_env;
};

#endif
