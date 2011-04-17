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
    void slotGrubDefaultChanged();
    void slotGrubSavedefaultChanged();
    void slotGrubHiddenTimeoutChanged();
    void slotGrubHiddenTimeoutQuietChanged();
    void slotGrubTimeoutChanged();
    void slotGrubGfxmodeChanged();
    void slotGrubGfxpayloadLinuxChanged();
    void slotGrubColorNormalChanged();
    void slotGrubColorHighlightChanged();
    void slowGrubBackgroundChanged();
    void previewGrubBackground();
    void createGrubBackground();
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

    QString convertToGRUBFileName(const QString &fileName);
    QString convertToLocalFileName(const QString &grubFileName);

    QString readFile(const QString &fileName);
    bool saveFile(const QString &fileName, const QString &fileContents);
    bool updateGRUB(const QString &fileName);

    bool readResolutions();
    bool readDevices();
    bool readEntries();
    bool readSettings();

    void sortResolutions();
    void showResolutions();

    QString quoteWord(const QString &word);
    QString unquoteWord(const QString &word);
    void parseSettings(const QString &config);
    void parseEntries(const QString &config);

    Ui::KCMGRUB2 ui;

    enum {
        grubDefaultDirty,
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
        grubDisableOsProberDirty
    };
    QBitArray m_dirtyBits;
    QStringList m_resolutions;
    QHash<QString, QString> m_devices;
    QStringList m_entries;
    QHash<QString, QString> m_settings;
};

#endif
