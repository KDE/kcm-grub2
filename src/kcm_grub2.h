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

/**
 * @short Main GUI class.
 */
class KCMGRUB2 : public KCModule, private Ui::KCMGRUB2
{
Q_OBJECT
public:
    KCMGRUB2(QWidget *parent = 0, const QVariantList &list = QVariantList());
    virtual ~KCMGRUB2();

    virtual void load();
    virtual void save();
private Q_SLOTS:
    void on_radioButton_default_clicked(bool checked);
    void on_comboBox_default_currentIndexChanged(int index);
    void on_radioButton_saved_clicked(bool checked);
    void on_checkBox_savedefault_clicked(bool checked);
    void on_checkBox_hiddenTimeout_clicked(bool checked);
    void on_kintspinbox_hiddenTimeout_valueChanged(int i);
    void on_checkBox_hiddenTimeoutShowTimer_clicked(bool checked);
    void on_checkBox_timeout_clicked(bool checked);
    void on_radioButton_timeout0_clicked(bool checked);
    void on_radioButton_timeout_clicked(bool checked);
    void on_kintspinbox_timeout_valueChanged(int i);

    void on_klineedit_gfxmode_textEdited(const QString &text);
    void on_radioButton_gfxpayloadText_clicked(bool checked);
    void on_radioButton_gfxpayloadKeep_clicked(bool checked);
    void on_radioButton_gfxpayloadOther_clicked(bool checked);
    void on_klineedit_gfxpayload_textEdited(const QString &text);
    void on_comboBox_normalForeground_currentIndexChanged(int index);
    void on_comboBox_normalBackground_currentIndexChanged(int index);
    void on_comboBox_highlightForeground_currentIndexChanged(int index);
    void on_comboBox_highlightBackground_currentIndexChanged(int index);
    void on_kurlrequester_background_textChanged(const QString &text);
    void on_kpushbutton_preview_clicked(bool checked);
    void on_kurlrequester_theme_textChanged(const QString &text);

    void on_klineedit_cmdlineDefault_textEdited(const QString &text);
    void on_klineedit_cmdline_textEdited(const QString &text);
    void on_klineedit_terminal_textEdited(const QString &text);
    void on_klineedit_terminalInput_textEdited(const QString &text);
    void on_klineedit_terminalOutput_textEdited(const QString &text);
    void on_klineedit_distributor_textEdited(const QString &text);
    void on_klineedit_serial_textEdited(const QString &text);
    void on_checkBox_uuid_clicked(bool checked);
    void on_checkBox_recovery_clicked(bool checked);
    void on_checkBox_osProber_clicked(bool checked);
private:
    void setupObjects();
    void setupConnections();

    QString convertToGRUBFileName(const QString &fileName);
    QString convertToLocalFileName(const QString &grubFileName);

    QString readFile(const QString &fileName);
    bool saveFile(const QString &fileName, const QString &fileContents);
    void updateGRUB(const QString &fileName);

    bool readDevices();
    bool readEntries();
    bool readSettings();

    void parseSettings(const QString &config);
    void parseEntries(const QString &config);
    QString unquoteWord(const QString &word);

    KSplashScreen *splash;
    QHash<QString, QString> m_settings;
    QStringList m_entries;
    QHash<QString, QString> m_devices;
};

#endif
