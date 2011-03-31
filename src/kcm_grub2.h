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

//KDE
#include <KCModule>
class KSplashScreen;

//Ui
#include "ui_kcm_grub2.h"

class KCMGRUB2 : public KCModule
{
    Q_OBJECT
public:
    KCMGRUB2(QWidget *parent = 0, const QVariantList &list = QVariantList());
    virtual ~KCMGRUB2();

    virtual void load();
    virtual void save();
private Q_SLOTS:
    void updateGrubDefault();
    void updateGrubSavedefault(bool checked);
    void updateGrubHiddenTimeout();
    void updateGrubHiddenTimeoutQuiet(bool checked);
    void updateGrubTimeout();
    void updateGrubGfxmode(const QString &text);
    void updateGrubGfxpayloadLinux();
    void updateGrubColorNormal();
    void updateGrubColorHighlight();
    void updateGrubBackground(const QString &text);
    void previewGrubBackground();
    void createGrubBackground();
    void updateGrubTheme(const QString &text);
    void updateGrubCmdlineLinuxDefault(const QString &text);
    void updateGrubCmdlineLinux(const QString &text);
    void updateGrubTerminal(const QString &text);
    void updateGrubTerminalInput(const QString &text);
    void updateGrubTerminalOutput(const QString &text);
    void updateGrubDistributor(const QString &text);
    void updateGrubSerialCommand(const QString &text);
    void updateGrubInitTune(const QString &text);
    void updateGrubDisableLinuxUUID(bool checked);
    void updateGrubDisableRecovery(bool checked);
    void updateGrubDisableOsProber(bool checked);

    void updateSuggestions();
    void triggeredSuggestion(QAction *action);
private:
    void setupObjects();
    void setupConnections();

    QString convertToGRUBFileName(const QString &fileName);
    QString convertToLocalFileName(const QString &grubFileName);

    QString readFile(const QString &fileName);
    bool saveFile(const QString &fileName, const QString &fileContents);
    bool updateGRUB(const QString &fileName);

    bool readDevices();
    bool readEntries();
    bool readSettings();

    QString unquoteWord(const QString &word);
    void parseSettings(const QString &config);
    void parseEntries(const QString &config);

    KSplashScreen *splashScreen;
    Ui::KCMGRUB2 ui;

    QHash<QString, QString> m_settings;
    QStringList m_entries;
    QHash<QString, QString> m_devices;
};

#endif
