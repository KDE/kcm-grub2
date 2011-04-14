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
#include "kcm_grub2.h"

//Qt
#include <QDesktopWidget>

//KDE
#include <KAboutData>
#include <KFileDialog>
#include <KMenu>
#include <KMessageBox>
#include <kmountpoint.h>
#include <KPluginFactory>
#include <KProcess>
#include <KProgressDialog>
#include <KShell>
#include <KAuth/ActionWatcher>
using namespace KAuth;

//Project
#include "config.h"
#ifdef HAVE_IMAGEMAGICK
#include "convertDlg.h"
#endif
#include "settings.h"

K_PLUGIN_FACTORY(GRUB2Factory, registerPlugin<KCMGRUB2>();)
K_EXPORT_PLUGIN(GRUB2Factory("kcmgrub2"))

KCMGRUB2::KCMGRUB2(QWidget *parent, const QVariantList &list) : KCModule(GRUB2Factory::componentData(), parent, list)
{
    KAboutData *about = new KAboutData("kcmgrub2", 0, ki18n("KDE GRUB2 Bootloader Control Module"), KCM_GRUB2_VERSION, KLocalizedString(), KAboutData::License_GPL_V3, ki18n("Copyright (C) 2008-2011 Konstantinos Smanis"), KLocalizedString(), QByteArray(), "konstantinos.smanis@gmail.com");
    about->addAuthor(ki18n("Κonstantinos Smanis"), ki18n("Main Developer"), "konstantinos.smanis@gmail.com");
    setAboutData(about);

    ui.setupUi(this);
    setupObjects();
    setupConnections();
}

void KCMGRUB2::load()
{
    readGfxmodes();
    readEntries();
    readSettings();

    bool ok;
    ui.comboBox_default->clear();
    ui.comboBox_default->addItems(m_entries);
    if (m_settings.value("GRUB_DEFAULT").compare("saved") == 0) {
        ui.radioButton_saved->setChecked(true);
    } else {
        int entryIndex = m_entries.indexOf(m_settings.value("GRUB_DEFAULT"));
        if (entryIndex != -1) {
            ui.radioButton_default->setChecked(true);
            ui.comboBox_default->setCurrentIndex(entryIndex);
        } else {
            entryIndex = m_settings.value("GRUB_DEFAULT").toInt(&ok);
            if (ok && entryIndex >= 0 && entryIndex < m_entries.size()) {
                ui.radioButton_default->setChecked(true);
                ui.comboBox_default->setCurrentIndex(entryIndex);
            } else {
                kWarning() << "Invalid GRUB_DEFAULT value";
            }
        }
    }
    ui.checkBox_savedefault->setChecked(m_settings.value("GRUB_SAVEDEFAULT").compare("true") == 0);

    ui.spinBox_hiddenTimeout->blockSignals(true);
    ui.spinBox_timeout->blockSignals(true);
    int hiddenTimeout = m_settings.value("GRUB_HIDDEN_TIMEOUT").toInt(&ok);
    if (ok && hiddenTimeout >= 0) {
        ui.checkBox_hiddenTimeout->setChecked(hiddenTimeout > 0);
        ui.spinBox_hiddenTimeout->setValue(hiddenTimeout);
    } else {
        kWarning() << "Invalid GRUB_HIDDEN_TIMEOUT value";
    }
    ui.checkBox_hiddenTimeoutShowTimer->setChecked(m_settings.value("GRUB_HIDDEN_TIMEOUT_QUIET").compare("true") != 0);
    int timeout = m_settings.value("GRUB_TIMEOUT").toInt(&ok);
    if (ok && timeout >= -1) {
        ui.checkBox_timeout->setChecked(timeout > -1);
        ui.radioButton_timeout0->setChecked(timeout == 0);
        ui.radioButton_timeout->setChecked(timeout > 0);
        ui.spinBox_timeout->setValue(timeout);
    } else {
        kWarning() << "Invalid GRUB_TIMEOUT value";
    }
    ui.spinBox_hiddenTimeout->blockSignals(false);
    ui.spinBox_timeout->blockSignals(false);

    ui.comboBox_gfxmode->blockSignals(true);
    ui.comboBox_gfxpayload->blockSignals(true);
    ui.comboBox_gfxmode->clear();
    ui.comboBox_gfxmode->addItems(m_gfxmodes);
    ui.comboBox_gfxmode->setEditText(m_settings.value("GRUB_GFXMODE"));
    ui.comboBox_gfxpayload->clear();
    ui.comboBox_gfxpayload->addItems(m_gfxmodes);
    if (m_settings.value("GRUB_GFXPAYLOAD_LINUX").compare("text") == 0) {
        ui.radioButton_gfxpayloadText->setChecked(true);
    } else if (m_settings.value("GRUB_GFXPAYLOAD_LINUX").compare("keep") == 0) {
        ui.radioButton_gfxpayloadKeep->setChecked(true);
    } else {
        ui.radioButton_gfxpayloadOther->setChecked(true);
        ui.comboBox_gfxpayload->setEditText(m_settings.value("GRUB_GFXPAYLOAD_LINUX"));
    }
    ui.comboBox_gfxpayload->blockSignals(false);
    ui.comboBox_gfxmode->blockSignals(false);

    if (!m_settings.value("GRUB_COLOR_NORMAL").isEmpty()) {
        int normalForegroundIndex = ui.comboBox_normalForeground->findData(m_settings.value("GRUB_COLOR_NORMAL").section('/', 0, 0));
        int normalBackgroundIndex = ui.comboBox_normalBackground->findData(m_settings.value("GRUB_COLOR_NORMAL").section('/', 1));
        if (normalForegroundIndex == -1 || normalBackgroundIndex == -1) {
            kWarning() << "Invalid GRUB_COLOR_NORMAL value";
        }
        if (normalForegroundIndex != -1) {
            ui.comboBox_normalForeground->setCurrentIndex(normalForegroundIndex);
        }
        if (normalBackgroundIndex != -1) {
            ui.comboBox_normalBackground->setCurrentIndex(normalBackgroundIndex);
        }
    }
    if (!m_settings.value("GRUB_COLOR_HIGHLIGHT").isEmpty()) {
        int highlightForegroundIndex = ui.comboBox_highlightForeground->findData(m_settings.value("GRUB_COLOR_HIGHLIGHT").section('/', 0, 0));
        int highlightBackgroundIndex = ui.comboBox_highlightBackground->findData(m_settings.value("GRUB_COLOR_HIGHLIGHT").section('/', 1));
        if (highlightForegroundIndex == -1 || highlightBackgroundIndex == -1) {
            kWarning() << "Invalid GRUB_COLOR_HIGHLIGHT value";
        }
        if (highlightForegroundIndex != -1) {
            ui.comboBox_highlightForeground->setCurrentIndex(highlightForegroundIndex);
        }
        if (highlightBackgroundIndex != -1) {
            ui.comboBox_highlightBackground->setCurrentIndex(highlightBackgroundIndex);
        }
    }

    ui.kurlrequester_background->blockSignals(true);
    ui.kurlrequester_theme->blockSignals(true);
    ui.kurlrequester_background->setText(m_settings.value("GRUB_BACKGROUND"));
    ui.kpushbutton_preview->setEnabled(!m_settings.value("GRUB_BACKGROUND").isEmpty());
    ui.kurlrequester_theme->setText(m_settings.value("GRUB_THEME"));
    ui.kurlrequester_background->blockSignals(false);
    ui.kurlrequester_theme->blockSignals(false);

    ui.lineEdit_cmdlineDefault->setText(m_settings.value("GRUB_CMDLINE_LINUX_DEFAULT"));
    ui.lineEdit_cmdline->setText(m_settings.value("GRUB_CMDLINE_LINUX"));

    ui.lineEdit_terminal->setText(m_settings.value("GRUB_TERMINAL"));
    if (m_settings.value("GRUB_TERMINAL").isEmpty()) {
        ui.lineEdit_terminalInput->setText(m_settings.value("GRUB_TERMINAL_INPUT"));
        ui.lineEdit_terminalOutput->setText(m_settings.value("GRUB_TERMINAL_OUTPUT"));
    } else {
        ui.lineEdit_terminalInput->setReadOnly(true);
        ui.lineEdit_terminalOutput->setReadOnly(true);
        ui.lineEdit_terminalInput->setText(m_settings.value("GRUB_TERMINAL"));
        ui.lineEdit_terminalOutput->setText(m_settings.value("GRUB_TERMINAL"));
    }

    ui.lineEdit_distributor->setText(m_settings.value("GRUB_DISTRIBUTOR"));
    ui.lineEdit_serial->setText(m_settings.value("GRUB_SERIAL_COMMAND"));
    ui.lineEdit_initTune->setText(m_settings.value("GRUB_INIT_TUNE"));

    ui.checkBox_uuid->setChecked(m_settings.value("GRUB_DISABLE_LINUX_UUID").compare("true") != 0);
    ui.checkBox_recovery->setChecked(m_settings.value("GRUB_DISABLE_RECOVERY").compare("true") != 0);
    ui.checkBox_osProber->setChecked(m_settings.value("GRUB_DISABLE_OS_PROBER").compare("true") != 0);

    m_dirtyBits.fill(0);
    emit changed(false);
}
void KCMGRUB2::save()
{
    kDebug() << m_dirtyBits;
    if (m_dirtyBits.testBit(grubDefaultDirty)) {
        if (ui.radioButton_default->isChecked()) {
            //TODO: Use m_entries instead?
            m_settings["GRUB_DEFAULT"] = ui.comboBox_default->currentText();
        } else if (ui.radioButton_saved->isChecked()) {
            m_settings["GRUB_DEFAULT"] = "saved";
        }
    }
    if (m_dirtyBits.testBit(grubSavedefaultDirty)) {
        if (ui.checkBox_savedefault->isChecked()) {
            m_settings["GRUB_SAVEDEFAULT"] = "true";
        } else {
            m_settings.remove("GRUB_SAVEDEFAULT");
        }
    }
    if (m_dirtyBits.testBit(grubHiddenTimeoutDirty)) {
        if (ui.checkBox_hiddenTimeout->isChecked()) {
            m_settings["GRUB_HIDDEN_TIMEOUT"] = QString::number(ui.spinBox_hiddenTimeout->value());
        } else {
            m_settings.remove("GRUB_HIDDEN_TIMEOUT");
        }
    }
    if (m_dirtyBits.testBit(grubHiddenTimeoutQuietDirty)) {
        if (ui.checkBox_hiddenTimeoutShowTimer->isChecked()) {
            m_settings.remove("GRUB_HIDDEN_TIMEOUT_QUIET");
        } else {
            m_settings["GRUB_HIDDEN_TIMEOUT_QUIET"] = "true";
        }
    }
    if (m_dirtyBits.testBit(grubTimeoutDirty)) {
        if (ui.checkBox_timeout->isChecked()) {
            if (ui.radioButton_timeout0->isChecked()) {
                m_settings["GRUB_TIMEOUT"] = "0";
            } else {
                m_settings["GRUB_TIMEOUT"] = QString::number(ui.spinBox_timeout->value());
            }
        } else {
            m_settings["GRUB_TIMEOUT"] = "-1";
        }
    }
    if (m_dirtyBits.testBit(grubGfxmodeDirty)) {
        QString gfxmode = ui.comboBox_gfxmode->currentText();
        if (!gfxmode.isEmpty()) {
            m_settings["GRUB_GFXMODE"] = gfxmode;
        } else {
            m_settings.remove("GRUB_GFXMODE");
        }
    }
    if (m_dirtyBits.testBit(grubGfxpayloadLinuxDirty)) {
        if (ui.radioButton_gfxpayloadText->isChecked()) {
            m_settings["GRUB_GFXPAYLOAD_LINUX"] = "text";
        } else if (ui.radioButton_gfxpayloadKeep->isChecked()) {
            m_settings["GRUB_GFXPAYLOAD_LINUX"] = "keep";
        } else {
            QString gfxPayload = ui.comboBox_gfxpayload->currentText();
            if (!gfxPayload.isEmpty()) {
                m_settings["GRUB_GFXPAYLOAD_LINUX"] = gfxPayload;
            } else {
                m_settings.remove("GRUB_GFXPAYLOAD_LINUX");
            }
        }
    }
    if (m_dirtyBits.testBit(grubColorNormalDirty)) {
        QString normalForeground = ui.comboBox_normalForeground->itemData(ui.comboBox_normalForeground->currentIndex()).toString();
        QString normalBackground = ui.comboBox_normalBackground->itemData(ui.comboBox_normalBackground->currentIndex()).toString();
        if (normalForeground.compare("light-gray") != 0 || normalBackground.compare("black") != 0) {
            m_settings["GRUB_COLOR_NORMAL"] = normalForeground + '/' + normalBackground;
        } else {
            m_settings.remove("GRUB_COLOR_NORMAL");
        }
    }
    if (m_dirtyBits.testBit(grubColorHighlightDirty)) {
        QString highlightForeground = ui.comboBox_highlightForeground->itemData(ui.comboBox_highlightForeground->currentIndex()).toString();
        QString highlightBackground = ui.comboBox_highlightBackground->itemData(ui.comboBox_highlightBackground->currentIndex()).toString();
        if (highlightForeground.compare("black") != 0 || highlightBackground.compare("light-gray") != 0) {
            m_settings["GRUB_COLOR_HIGHLIGHT"] = highlightForeground + '/' + highlightBackground;
        } else {
            m_settings.remove("GRUB_COLOR_HIGHLIGHT");
        }
    }
    if (m_dirtyBits.testBit(grubBackgroundDirty)) {
        QString background = ui.kurlrequester_background->url().toLocalFile();
        if (!background.isEmpty()) {
            m_settings["GRUB_BACKGROUND"] = background;
        } else {
            m_settings.remove("GRUB_BACKGROUND");
        }
    }
    if (m_dirtyBits.testBit(grubThemeDirty)) {
        QString theme = ui.kurlrequester_theme->url().toLocalFile();
        if (!theme.isEmpty()) {
            m_settings["GRUB_THEME"] = theme;
        } else {
            m_settings.remove("GRUB_THEME");
        }
    }
    if (m_dirtyBits.testBit(grubCmdlineLinuxDefaultDirty)) {
        QString cmdlineLinuxDefault = ui.lineEdit_cmdlineDefault->text();
        if (!cmdlineLinuxDefault.isEmpty()) {
            m_settings["GRUB_CMDLINE_LINUX_DEFAULT"] = cmdlineLinuxDefault;
        } else {
            m_settings.remove("GRUB_CMDLINE_LINUX_DEFAULT");
        }
    }
    if (m_dirtyBits.testBit(grubCmdlineLinuxDirty)) {
        QString cmdlineLinux = ui.lineEdit_cmdline->text();
        if (!cmdlineLinux.isEmpty()) {
            m_settings["GRUB_CMDLINE_LINUX"] = cmdlineLinux;
        } else {
            m_settings.remove("GRUB_CMDLINE_LINUX");
        }
    }
    if (m_dirtyBits.testBit(grubTerminalDirty)) {
        QString terminal = ui.lineEdit_terminal->text();
        if (!terminal.isEmpty()) {
            m_settings["GRUB_TERMINAL"] = terminal;
        } else {
            m_settings.remove("GRUB_TERMINAL");
        }
    }
    if (m_dirtyBits.testBit(grubTerminalInputDirty)) {
        QString terminalInput = ui.lineEdit_terminalInput->text();
        if (!terminalInput.isEmpty()) {
            m_settings["GRUB_TERMINAL_INPUT"] = terminalInput;
        } else {
            m_settings.remove("GRUB_TERMINAL_INPUT");
        }
    }
    if (m_dirtyBits.testBit(grubTerminalOutputDirty)) {
        QString terminalOutput = ui.lineEdit_terminalOutput->text();
        if (!terminalOutput.isEmpty()) {
            m_settings["GRUB_TERMINAL_OUTPUT"] = terminalOutput;
        } else {
            m_settings.remove("GRUB_TERMINAL_OUTPUT");
        }
    }
    if (m_dirtyBits.testBit(grubDistributorDirty)) {
        QString distributor = ui.lineEdit_distributor->text();
        if (!distributor.isEmpty()) {
            m_settings["GRUB_DISTRIBUTOR"] = distributor;
        } else {
            m_settings.remove("GRUB_DISTRIBUTOR");
        }
    }
    if (m_dirtyBits.testBit(grubSerialCommandDirty)) {
        QString serialCommand = ui.lineEdit_serial->text();
        if (!serialCommand.isEmpty()) {
            m_settings["GRUB_SERIAL_COMMAND"] = serialCommand;
        } else {
            m_settings.remove("GRUB_SERIAL_COMMAND");
        }
    }
    if (m_dirtyBits.testBit(grubInitTuneDirty)) {
        QString initTune = ui.lineEdit_initTune->text();
        if (!initTune.isEmpty()) {
            m_settings["GRUB_INIT_TUNE"] = initTune;
        } else {
            m_settings.remove("GRUB_INIT_TUNE");
        }
    }
    if (m_dirtyBits.testBit(grubDisableLinuxUuidDirty)) {
        if (ui.checkBox_uuid->isChecked()) {
            m_settings.remove("GRUB_DISABLE_LINUX_UUID");
        } else {
            m_settings["GRUB_DISABLE_LINUX_UUID"] = "true";
        }
    }
    if (m_dirtyBits.testBit(grubDisableRecoveryDirty)) {
        if (ui.checkBox_recovery->isChecked()) {
            m_settings.remove("GRUB_DISABLE_RECOVERY");
        } else {
            m_settings["GRUB_DISABLE_RECOVERY"] = "true";
        }
    }
    if (m_dirtyBits.testBit(grubDisableOsProberDirty)) {
        if (ui.checkBox_osProber->isChecked()) {
            m_settings.remove("GRUB_DISABLE_OS_PROBER");
        } else {
            m_settings["GRUB_DISABLE_OS_PROBER"] = "true";
        }
    }

    QString config;
    QTextStream stream(&config, QIODevice::WriteOnly | QIODevice::Text);
    for (QHash<QString, QString>::const_iterator it = m_settings.constBegin(); it != m_settings.constEnd(); it++) {
        QString key = it.key(), value = it.value();
        if (!value.startsWith('`') || !value.endsWith('`')) {
            value = KShell::quoteArg(value);
        }
        stream << key << '=' << value << endl;
    }
    if (!saveFile(Settings::configPaths().at(0), config)) {
        return;
    }
    if (KMessageBox::questionYesNo(this, i18nc("@info", "<para>Your configuration was successfully saved.<nl/>For the changes to take effect your GRUB menu has to be updated.</para><para>Update your GRUB menu?</para>")) == KMessageBox::Yes) {
        if (updateGRUB(Settings::menuPaths().at(0))) {
            load();
        }
    }
}

void KCMGRUB2::updateGrubDefault()
{
    m_dirtyBits.setBit(grubDefaultDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubSavedefault(bool checked)
{
    Q_UNUSED(checked)
    m_dirtyBits.setBit(grubSavedefaultDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubHiddenTimeout()
{
    m_dirtyBits.setBit(grubHiddenTimeoutDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubHiddenTimeoutQuiet(bool checked)
{
    Q_UNUSED(checked)
    m_dirtyBits.setBit(grubHiddenTimeoutQuietDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubTimeout()
{
    m_dirtyBits.setBit(grubTimeoutDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubGfxmode(const QString &text)
{
    Q_UNUSED(text)
    m_dirtyBits.setBit(grubGfxmodeDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubGfxpayloadLinux()
{
    m_dirtyBits.setBit(grubGfxpayloadLinuxDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubColorNormal()
{
    m_dirtyBits.setBit(grubColorNormalDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubColorHighlight()
{
    m_dirtyBits.setBit(grubColorHighlightDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubBackground(const QString &text)
{
    Q_UNUSED(text)
    ui.kpushbutton_preview->setEnabled(!text.isEmpty());
    m_dirtyBits.setBit(grubBackgroundDirty);
    emit changed(true);
}
void KCMGRUB2::previewGrubBackground()
{
    QFile file(ui.kurlrequester_background->url().toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QDialog *dialog  = new QDialog(this);
    QLabel *label = new QLabel(dialog);
    label->setPixmap(QPixmap::fromImage(QImage::fromData(file.readAll())).scaled(QDesktopWidget().screenGeometry(this).size()));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->showFullScreen();
    KMessageBox::information(dialog, i18nc("@info", "Press <shortcut>Escape</shortcut> to exit fullscreen mode."), QString(), "GRUBFullscreenPreview");
}
void KCMGRUB2::createGrubBackground()
{
#ifdef HAVE_IMAGEMAGICK
    ConvertDialog convertDlg(this);
    connect(&convertDlg, SIGNAL(splashImageCreated(QString)), ui.kurlrequester_background, SLOT(setText(QString)));
    convertDlg.exec();
#endif
}
void KCMGRUB2::updateGrubTheme(const QString &text)
{
    Q_UNUSED(text)
    m_dirtyBits.setBit(grubThemeDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubCmdlineLinuxDefault(const QString &text)
{
    Q_UNUSED(text)
    m_dirtyBits.setBit(grubCmdlineLinuxDefaultDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubCmdlineLinux(const QString &text)
{
    Q_UNUSED(text)
    m_dirtyBits.setBit(grubCmdlineLinuxDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubTerminal(const QString &text)
{
    Q_UNUSED(text)
    ui.lineEdit_terminalInput->setReadOnly(!text.isEmpty());
    ui.lineEdit_terminalOutput->setReadOnly(!text.isEmpty());
    ui.lineEdit_terminalInput->setText(!text.isEmpty() ? text : m_settings.value("GRUB_TERMINAL_INPUT"));
    ui.lineEdit_terminalOutput->setText(!text.isEmpty() ? text : m_settings.value("GRUB_TERMINAL_OUTPUT"));
    m_dirtyBits.setBit(grubTerminalDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubTerminalInput(const QString &text)
{
    Q_UNUSED(text)
    m_dirtyBits.setBit(grubTerminalInputDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubTerminalOutput(const QString &text)
{
    Q_UNUSED(text)
    m_dirtyBits.setBit(grubTerminalOutputDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubDistributor(const QString &text)
{
    Q_UNUSED(text)
    m_dirtyBits.setBit(grubDistributorDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubSerialCommand(const QString &text)
{
    Q_UNUSED(text)
    m_dirtyBits.setBit(grubSerialCommandDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubInitTune(const QString& text)
{
    Q_UNUSED(text)
    m_dirtyBits.setBit(grubInitTuneDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubDisableLinuxUUID(bool checked)
{
    Q_UNUSED(checked)
    m_dirtyBits.setBit(grubDisableLinuxUuidDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubDisableRecovery(bool checked)
{
    Q_UNUSED(checked)
    m_dirtyBits.setBit(grubDisableRecoveryDirty);
    emit changed(true);
}
void KCMGRUB2::updateGrubDisableOsProber(bool checked)
{
    Q_UNUSED(checked)
    m_dirtyBits.setBit(grubDisableOsProberDirty);
    emit changed(true);
}

void KCMGRUB2::updateSuggestions()
{
    if (!sender()->isWidgetType()) {
        return;
    }

    QLineEdit *lineEdit = 0;
    if (ui.kpushbutton_cmdlineDefaultSuggestions->isDown()) {
        lineEdit = ui.lineEdit_cmdlineDefault;
    } else if (ui.kpushbutton_cmdlineSuggestions->isDown()) {
        lineEdit = ui.lineEdit_cmdline;
    } else if (ui.kpushbutton_terminalSuggestions->isDown()) {
        lineEdit = ui.lineEdit_terminal;
    } else if (ui.kpushbutton_terminalInputSuggestions->isDown()) {
        lineEdit = ui.lineEdit_terminalInput;
    } else if (ui.kpushbutton_terminalOutputSuggestions->isDown()) {
        lineEdit = ui.lineEdit_terminalOutput;
    } else {
        return;
    }

    foreach(QAction *action, qobject_cast<const QWidget*>(sender())->actions()) {
        if (!action->isCheckable()) {
            action->setCheckable(true);
        }
        action->setChecked(lineEdit->text().contains(QRegExp(QString("\\b%1\\b").arg(action->data().toString()))));
    }
}
void KCMGRUB2::triggeredSuggestion(QAction *action)
{
    QLineEdit *lineEdit = 0;
    void (KCMGRUB2::*updateFunction)(const QString &) = 0;
    if (ui.kpushbutton_cmdlineDefaultSuggestions->isDown()) {
        lineEdit = ui.lineEdit_cmdlineDefault;
        updateFunction = &KCMGRUB2::updateGrubCmdlineLinuxDefault;
    } else if (ui.kpushbutton_cmdlineSuggestions->isDown()) {
        lineEdit = ui.lineEdit_cmdline;
        updateFunction = &KCMGRUB2::updateGrubCmdlineLinux;
    } else if (ui.kpushbutton_terminalSuggestions->isDown()) {
        lineEdit = ui.lineEdit_terminal;
        updateFunction = &KCMGRUB2::updateGrubTerminal;
    } else if (ui.kpushbutton_terminalInputSuggestions->isDown()) {
        lineEdit = ui.lineEdit_terminalInput;
        updateFunction = &KCMGRUB2::updateGrubTerminalInput;
    } else if (ui.kpushbutton_terminalOutputSuggestions->isDown()) {
        lineEdit = ui.lineEdit_terminalOutput;
        updateFunction = &KCMGRUB2::updateGrubTerminalOutput;
    } else {
        return;
    }

    QString lineEditText = lineEdit->text();
    if (!action->isChecked()) {
        lineEdit->setText(lineEditText.remove(QRegExp(QString("\\b%1\\b").arg(action->data().toString()))).simplified());
    } else {
        lineEdit->setText(lineEditText.isEmpty() ? action->data().toString() : lineEditText + ' ' + action->data().toString());
    }
    (this->*updateFunction)(lineEdit->text());
}

void KCMGRUB2::setupObjects()
{
    setButtons(Apply);
    setNeedsAuthorization(true);

    m_dirtyBits.resize(grubDisableOsProberDirty + 1);

    QPixmap black(16, 16), transparent(16, 16);
    black.fill(Qt::black);
    transparent.fill(Qt::transparent);
    ui.comboBox_normalForeground->addItem(QIcon(black), i18nc("@item:inlistbox", "Black"), "black");
    ui.comboBox_highlightForeground->addItem(QIcon(black), i18nc("@item:inlistbox", "Black"), "black");
    ui.comboBox_normalBackground->addItem(QIcon(transparent), i18nc("@item:inlistbox", "Transparent"), "black");
    ui.comboBox_highlightBackground->addItem(QIcon(transparent), i18nc("@item:inlistbox", "Transparent"), "black");
    QHash<QString, QString> colors;
    colors.insertMulti("blue", i18nc("@item:inlistbox", "Blue"));
    colors.insertMulti("blue", "blue");
    colors.insertMulti("cyan", i18nc("@item:inlistbox", "Cyan"));
    colors.insertMulti("cyan", "cyan");
    colors.insertMulti("dark-gray", i18nc("@item:inlistbox", "Dark Gray"));
    colors.insertMulti("dark-gray", "darkgray");
    colors.insertMulti("green", i18nc("@item:inlistbox", "Green"));
    colors.insertMulti("green", "green");
    colors.insertMulti("light-cyan", i18nc("@item:inlistbox", "Light Cyan"));
    colors.insertMulti("light-cyan", "lightcyan");
    colors.insertMulti("light-blue", i18nc("@item:inlistbox", "Light Blue"));
    colors.insertMulti("light-blue", "lightblue");
    colors.insertMulti("light-green", i18nc("@item:inlistbox", "Light Green"));
    colors.insertMulti("light-green", "lightgreen");
    colors.insertMulti("light-gray", i18nc("@item:inlistbox", "Light Gray"));
    colors.insertMulti("light-gray", "lightgray");
    colors.insertMulti("light-magenta", i18nc("@item:inlistbox", "Light Magenta"));
    colors.insertMulti("light-magenta", "magenta");
    colors.insertMulti("light-red", i18nc("@item:inlistbox", "Light Red"));
    colors.insertMulti("light-red", "orangered");
    colors.insertMulti("magenta", i18nc("@item:inlistbox", "Magenta"));
    colors.insertMulti("magenta", "darkmagenta");
    colors.insertMulti("red", i18nc("@item:inlistbox", "Red"));
    colors.insertMulti("red", "red");
    colors.insertMulti("white", i18nc("@item:inlistbox", "White"));
    colors.insertMulti("white", "white");
    colors.insertMulti("yellow", i18nc("@item:inlistbox", "Yellow"));
    colors.insertMulti("yellow", "yellow");
    for (QHash<QString, QString>::const_iterator it = colors.constBegin(); it != colors.constEnd(); it += 2) {
        QPixmap color(16, 16);
        color.fill(QColor(colors.values(it.key()).at(0)));
        ui.comboBox_normalForeground->addItem(QIcon(color), colors.values(it.key()).at(1), it.key());
        ui.comboBox_highlightForeground->addItem(QIcon(color), colors.values(it.key()).at(1), it.key());
        ui.comboBox_normalBackground->addItem(QIcon(color), colors.values(it.key()).at(1), it.key());
        ui.comboBox_highlightBackground->addItem(QIcon(color), colors.values(it.key()).at(1), it.key());
    }
    ui.comboBox_normalForeground->setCurrentIndex(ui.comboBox_normalForeground->findData("light-gray"));
    ui.comboBox_normalBackground->setCurrentIndex(ui.comboBox_normalBackground->findData("black"));
    ui.comboBox_highlightForeground->setCurrentIndex(ui.comboBox_highlightForeground->findData("black"));
    ui.comboBox_highlightBackground->setCurrentIndex(ui.comboBox_highlightBackground->findData("light-gray"));

    ui.kpushbutton_preview->setIcon(KIcon("image-png"));
    ui.kpushbutton_create->setIcon(KIcon("insert-image"));
#ifndef HAVE_IMAGEMAGICK
    ui.kpushbutton_create->setEnabled(false);
    ui.kpushbutton_create->setToolTip(i18nc("@info:tooltip", "ImageMagick was not found."));
#endif

    ui.kpushbutton_cmdlineDefaultSuggestions->setIcon(KIcon("tools-wizard"));
    ui.kpushbutton_cmdlineDefaultSuggestions->setMenu(new KMenu(ui.kpushbutton_cmdlineDefaultSuggestions));
    ui.kpushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Quiet Boot"))->setData("quiet");
    ui.kpushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Show Splash Screen"))->setData("splash");
    ui.kpushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off ACPI"))->setData("acpi=off");
    ui.kpushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off APIC"))->setData("noapic");
    ui.kpushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off Local APIC"))->setData("nolapic");
    ui.kpushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Single User Mode"))->setData("single");
    ui.kpushbutton_cmdlineSuggestions->setIcon(KIcon("tools-wizard"));
    ui.kpushbutton_cmdlineSuggestions->setMenu(new KMenu(ui.kpushbutton_cmdlineSuggestions));
    ui.kpushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Quiet Boot"))->setData("quiet");
    ui.kpushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Show Splash Screen"))->setData("splash");
    ui.kpushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off ACPI"))->setData("acpi=off");
    ui.kpushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off APIC"))->setData("noapic");
    ui.kpushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off Local APIC"))->setData("nolapic");
    ui.kpushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Single User Mode"))->setData("single");
    ui.kpushbutton_terminalSuggestions->setIcon(KIcon("tools-wizard"));
    ui.kpushbutton_terminalSuggestions->setMenu(new KMenu(ui.kpushbutton_terminalSuggestions));
    ui.kpushbutton_terminalSuggestions->menu()->addAction(i18nc("@action:inmenu", "PC BIOS && EFI Console"))->setData("console");
    ui.kpushbutton_terminalSuggestions->menu()->addAction(i18nc("@action:inmenu", "Serial Terminal"))->setData("serial");
    ui.kpushbutton_terminalSuggestions->menu()->addAction(i18nc("@action:inmenu", "Open Firmware Console"))->setData("ofconsole");
    ui.kpushbutton_terminalInputSuggestions->setIcon(KIcon("tools-wizard"));
    ui.kpushbutton_terminalInputSuggestions->setMenu(new KMenu(ui.kpushbutton_terminalInputSuggestions));
    ui.kpushbutton_terminalInputSuggestions->menu()->addAction(i18nc("@action:inmenu", "PC BIOS && EFI Console"))->setData("console");
    ui.kpushbutton_terminalInputSuggestions->menu()->addAction(i18nc("@action:inmenu", "Serial Terminal"))->setData("serial");
    ui.kpushbutton_terminalInputSuggestions->menu()->addAction(i18nc("@action:inmenu", "Open Firmware Console"))->setData("ofconsole");
    ui.kpushbutton_terminalInputSuggestions->menu()->addAction(i18nc("@action:inmenu", "PC AT Keyboard (Coreboot)"))->setData("at_keyboard");
    ui.kpushbutton_terminalInputSuggestions->menu()->addAction(i18nc("@action:inmenu", "USB Keyboard (HID Boot Protocol)"))->setData("usb_keyboard");
    ui.kpushbutton_terminalOutputSuggestions->setIcon(KIcon("tools-wizard"));
    ui.kpushbutton_terminalOutputSuggestions->setMenu(new KMenu(ui.kpushbutton_terminalOutputSuggestions));
    ui.kpushbutton_terminalOutputSuggestions->menu()->addAction(i18nc("@action:inmenu", "PC BIOS && EFI Console"))->setData("console");
    ui.kpushbutton_terminalOutputSuggestions->menu()->addAction(i18nc("@action:inmenu", "Serial Terminal"))->setData("serial");
    ui.kpushbutton_terminalOutputSuggestions->menu()->addAction(i18nc("@action:inmenu", "Open Firmware Console"))->setData("ofconsole");
    ui.kpushbutton_terminalOutputSuggestions->menu()->addAction(i18nc("@action:inmenu", "Graphics Mode Output"))->setData("gfxterm");
    ui.kpushbutton_terminalOutputSuggestions->menu()->addAction(i18nc("@action:inmenu", "VGA Text Output (Coreboot)"))->setData("vga_text");
}
void KCMGRUB2::setupConnections()
{
    connect(ui.radioButton_default, SIGNAL(clicked(bool)), this, SLOT(updateGrubDefault()));
    connect(ui.comboBox_default, SIGNAL(activated(int)), this, SLOT(updateGrubDefault()));
    connect(ui.radioButton_saved, SIGNAL(clicked(bool)), this, SLOT(updateGrubDefault()));
    connect(ui.checkBox_savedefault, SIGNAL(clicked(bool)), this, SLOT(updateGrubSavedefault(bool)));

    connect(ui.checkBox_hiddenTimeout, SIGNAL(clicked(bool)), this, SLOT(updateGrubHiddenTimeout()));
    connect(ui.spinBox_hiddenTimeout, SIGNAL(valueChanged(int)), this, SLOT(updateGrubHiddenTimeout()));
    connect(ui.checkBox_hiddenTimeoutShowTimer, SIGNAL(clicked(bool)), this, SLOT(updateGrubHiddenTimeoutQuiet(bool)));
    connect(ui.checkBox_timeout, SIGNAL(clicked(bool)), this, SLOT(updateGrubTimeout()));
    connect(ui.radioButton_timeout0, SIGNAL(clicked(bool)), this, SLOT(updateGrubTimeout()));
    connect(ui.radioButton_timeout, SIGNAL(clicked(bool)), this, SLOT(updateGrubTimeout()));
    connect(ui.spinBox_timeout, SIGNAL(valueChanged(int)), this, SLOT(updateGrubTimeout()));

    connect(ui.comboBox_gfxmode, SIGNAL(editTextChanged(QString)), this, SLOT(updateGrubGfxmode(QString)));
    connect(ui.radioButton_gfxpayloadText, SIGNAL(clicked(bool)), this, SLOT(updateGrubGfxpayloadLinux()));
    connect(ui.radioButton_gfxpayloadKeep, SIGNAL(clicked(bool)), this, SLOT(updateGrubGfxpayloadLinux()));
    connect(ui.radioButton_gfxpayloadOther, SIGNAL(clicked(bool)), this, SLOT(updateGrubGfxpayloadLinux()));
    connect(ui.comboBox_gfxpayload, SIGNAL(editTextChanged(QString)), this, SLOT(updateGrubGfxpayloadLinux()));

    connect(ui.comboBox_normalForeground, SIGNAL(activated(int)), this, SLOT(updateGrubColorNormal()));
    connect(ui.comboBox_normalBackground, SIGNAL(activated(int)), this, SLOT(updateGrubColorNormal()));
    connect(ui.comboBox_highlightForeground, SIGNAL(activated(int)), this, SLOT(updateGrubColorHighlight()));
    connect(ui.comboBox_highlightBackground, SIGNAL(activated(int)), this, SLOT(updateGrubColorHighlight()));

    connect(ui.kurlrequester_background, SIGNAL(textChanged(QString)), this, SLOT(updateGrubBackground(QString)));
    connect(ui.kpushbutton_preview, SIGNAL(clicked(bool)), this, SLOT(previewGrubBackground()));
    connect(ui.kpushbutton_create, SIGNAL(clicked(bool)), this, SLOT(createGrubBackground()));
    connect(ui.kurlrequester_theme, SIGNAL(textChanged(QString)), this, SLOT(updateGrubTheme(QString)));

    connect(ui.lineEdit_cmdlineDefault, SIGNAL(textEdited(QString)), this, SLOT(updateGrubCmdlineLinuxDefault(QString)));
    connect(ui.kpushbutton_cmdlineDefaultSuggestions->menu(), SIGNAL(aboutToShow()), this, SLOT(updateSuggestions()));
    connect(ui.kpushbutton_cmdlineDefaultSuggestions->menu(), SIGNAL(triggered(QAction*)), this, SLOT(triggeredSuggestion(QAction*)));
    connect(ui.lineEdit_cmdline, SIGNAL(textEdited(QString)), this, SLOT(updateGrubCmdlineLinux(QString)));
    connect(ui.kpushbutton_cmdlineSuggestions->menu(), SIGNAL(aboutToShow()), this, SLOT(updateSuggestions()));
    connect(ui.kpushbutton_cmdlineSuggestions->menu(), SIGNAL(triggered(QAction*)), this, SLOT(triggeredSuggestion(QAction*)));

    connect(ui.lineEdit_terminal, SIGNAL(textEdited(QString)), this, SLOT(updateGrubTerminal(QString)));
    connect(ui.kpushbutton_terminalSuggestions->menu(), SIGNAL(aboutToShow()), this, SLOT(updateSuggestions()));
    connect(ui.kpushbutton_terminalSuggestions->menu(), SIGNAL(triggered(QAction*)), this, SLOT(triggeredSuggestion(QAction*)));
    connect(ui.lineEdit_terminalInput, SIGNAL(textEdited(QString)), this, SLOT(updateGrubTerminalInput(QString)));
    connect(ui.kpushbutton_terminalInputSuggestions->menu(), SIGNAL(aboutToShow()), this, SLOT(updateSuggestions()));
    connect(ui.kpushbutton_terminalInputSuggestions->menu(), SIGNAL(triggered(QAction*)), this, SLOT(triggeredSuggestion(QAction*)));
    connect(ui.lineEdit_terminalOutput, SIGNAL(textEdited(QString)), this, SLOT(updateGrubTerminalOutput(QString)));
    connect(ui.kpushbutton_terminalOutputSuggestions->menu(), SIGNAL(aboutToShow()), this, SLOT(updateSuggestions()));
    connect(ui.kpushbutton_terminalOutputSuggestions->menu(), SIGNAL(triggered(QAction*)), this, SLOT(triggeredSuggestion(QAction*)));

    connect(ui.lineEdit_distributor, SIGNAL(textEdited(QString)), this, SLOT(updateGrubDistributor(QString)));
    connect(ui.lineEdit_serial, SIGNAL(textEdited(QString)), this, SLOT(updateGrubSerialCommand(QString)));
    connect(ui.lineEdit_initTune, SIGNAL(textEdited(QString)), this, SLOT(updateGrubInitTune(QString)));

    connect(ui.checkBox_uuid, SIGNAL(clicked(bool)), this, SLOT(updateGrubDisableLinuxUUID(bool)));
    connect(ui.checkBox_recovery, SIGNAL(clicked(bool)), this, SLOT(updateGrubDisableRecovery(bool)));
    connect(ui.checkBox_osProber, SIGNAL(clicked(bool)), this, SLOT(updateGrubDisableOsProber(bool)));
}

QString KCMGRUB2::convertToGRUBFileName(const QString &fileName)
{
    QString grubFileName = fileName;
    QString mountpoint = KMountPoint::currentMountPoints().findByPath(grubFileName)->mountPoint();
    if (m_devices.contains(mountpoint)) {
        if (mountpoint.compare("/") != 0) {
            grubFileName.remove(0, mountpoint.length());
        }
        grubFileName.prepend(m_devices.value(mountpoint));
    }
    return grubFileName;
}
QString KCMGRUB2::convertToLocalFileName(const QString &grubFileName)
{
    QString fileName = grubFileName;
    for (QHash<QString, QString>::const_iterator it = m_devices.constBegin(); it != m_devices.constEnd(); it++) {
        if (fileName.startsWith(it.value())) {
            fileName.remove(0, it.value().length());
            if (it.key().compare("/") != 0) {
                fileName.prepend(it.key());
            }
            break;
        }
    }
    return fileName;
}

QString KCMGRUB2::readFile(const QString &fileName)
{
    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        return stream.readAll();
    }

    Action loadAction("org.kde.kcontrol.kcmgrub2.load");
    loadAction.setHelperID("org.kde.kcontrol.kcmgrub2");
    loadAction.addArgument("fileName", fileName);

    ActionReply reply = loadAction.execute();
    if (reply.failed()) {
        kDebug() << "Error reading:" << fileName;
        kDebug() << "Error code:" << reply.errorCode();
        kDebug() << "Error description:" << reply.errorDescription();
        return QString();
    }
    return reply.data().value("fileContents").toString();
}
bool KCMGRUB2::saveFile(const QString &fileName, const QString &fileContents)
{
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << fileContents;
        return true;
    }

    Action saveAction("org.kde.kcontrol.kcmgrub2.save");
    saveAction.setHelperID("org.kde.kcontrol.kcmgrub2");
    saveAction.addArgument("fileName", fileName);
    saveAction.addArgument("fileContents", fileContents);

    ActionReply reply = saveAction.execute();
    if (reply.failed()) {
        KMessageBox::detailedError(this, i18nc("@info", "Error writing <filename>%1</filename>", fileName), i18nc("@info", "Error code: %1<nl/>Error description: %2", reply.errorCode(), reply.errorDescription()));
        return false;
    }
    return true;
}
bool KCMGRUB2::updateGRUB(const QString &fileName)
{
    Action updateAction("org.kde.kcontrol.kcmgrub2.update");
    updateAction.setHelperID("org.kde.kcontrol.kcmgrub2");
    updateAction.addArgument("fileName", fileName);

    if (updateAction.authorize() == Action::Authorized) {
        KProgressDialog progressDlg(this, i18nc("@title:window", "Updating GRUB"), i18nc("@info:progress", "Updating the GRUB menu.."));
        progressDlg.setAllowCancel(false);
        progressDlg.progressBar()->setMinimum(0);
        progressDlg.progressBar()->setMaximum(0);
        progressDlg.show();
        connect(updateAction.watcher(), SIGNAL(actionPerformed(ActionReply)), &progressDlg, SLOT(hide()));
        ActionReply reply = updateAction.execute();
        if (reply.succeeded()) {
            KDialog *dialog = new KDialog(this, Qt::Dialog);
            dialog->setCaption(i18nc("@title:window", "Information"));
            dialog->setButtons(KDialog::Ok | KDialog::Details);
            dialog->setModal(true);
            dialog->setDefaultButton(KDialog::Ok);
            dialog->setEscapeButton(KDialog::Ok);
            KMessageBox::createKMessageBox(dialog, QMessageBox::Information, i18nc("@info", "Successfully updated the GRUB menu."), QStringList(), QString(), 0, KMessageBox::Notify, reply.data().value("output").toString());
            return true;
        } else {
            KMessageBox::detailedError(this, i18nc("@info", "Failed to update the GRUB menu."), reply.data().value("output").toString());
        }
    }
    return false;
}

bool KCMGRUB2::readGfxmodes()
{
    Action probeVbeAction("org.kde.kcontrol.kcmgrub2.probevbe");
    probeVbeAction.setHelperID("org.kde.kcontrol.kcmgrub2");
    ActionReply reply = probeVbeAction.execute();
    if (reply.succeeded()) {
        m_gfxmodes = reply.data().value("gfxmodes").toStringList();
        return true;
    }
    return false;
}
bool KCMGRUB2::readDevices()
{
    QStringList mountPoints;
    foreach(const KMountPoint::Ptr mp, KMountPoint::currentMountPoints()) {
        if (mp->mountedFrom().startsWith("/dev")) {
            mountPoints.append(mp->mountPoint());
        }
    }

    Action probeAction("org.kde.kcontrol.kcmgrub2.probe");
    probeAction.setHelperID("org.kde.kcontrol.kcmgrub2");
    probeAction.addArgument("mountPoints", mountPoints);

    if (probeAction.authorize() == Action::Authorized) {
        KProgressDialog progressDlg(this, i18nc("@title:window", "Probing devices"), i18nc("@info:progress", "Probing devices for their GRUB names.."));
        progressDlg.setAllowCancel(false);
        progressDlg.show();
        connect(probeAction.watcher(), SIGNAL(progressStep(int)), progressDlg.progressBar(), SLOT(setValue(int)));
        ActionReply reply = probeAction.execute();
        if (reply.succeeded()) {
            QStringList grubPartitions = reply.data().value("grubPartitions").toStringList();
            if (mountPoints.size() != grubPartitions.size()) {
                KMessageBox::error(this, i18nc("@info", "Helper returned malformed device list."));
                return false;
            }
            for (int i = 0; i < mountPoints.size(); i++) {
                m_devices[mountPoints.at(i)] = grubPartitions.at(i);
            }
        } else {
            KMessageBox::error(this, i18nc("@info", "Failed to get GRUB device names."));
            return false;
        }
    }
    return true;
}
bool KCMGRUB2::readEntries()
{
    QStringList fileNames = Settings::menuPaths();
    for (int i = 0; i < fileNames.size(); i++) {
        QString fileContents = readFile(fileNames.at(i));
        if (!fileContents.isEmpty()) {
            if (i != 0) {
                fileNames.prepend(fileNames.at(i));
                fileNames.removeDuplicates();
                Settings::setMenuPaths(fileNames);
                Settings::self()->writeConfig();
            }
            parseEntries(fileContents);
            return true;
        }
    }
    while (KMessageBox::questionYesNoList(this, i18nc("@info", "<para>None of the following files were readable.</para><para>Select another file?</para>"), fileNames) == KMessageBox::Yes) {
        QString newPath = KFileDialog::getOpenFileName();
        if (newPath.isEmpty()) {
            break;
        }
        fileNames.prepend(newPath);
        fileNames.removeDuplicates();
        Settings::setMenuPaths(fileNames);
        Settings::self()->writeConfig();
        QString fileContents = readFile(newPath);
        if (!fileContents.isEmpty()) {
            parseEntries(fileContents);
            return true;
        }
    }
    return false;
}
bool KCMGRUB2::readSettings()
{
    QStringList fileNames = Settings::configPaths();
    for (int i = 0; i < fileNames.size(); i++) {
        QString fileContents = readFile(fileNames.at(i));
        if (!fileContents.isEmpty()) {
            if (i != 0) {
                fileNames.prepend(fileNames.at(i));
                fileNames.removeDuplicates();
                Settings::setConfigPaths(fileNames);
                Settings::self()->writeConfig();
            }
            parseSettings(fileContents);
            return true;
        }
    }
    while (KMessageBox::questionYesNoList(this, i18nc("@info", "<para>None of the following files were readable.</para><para>Select another file?</para>"), fileNames) == KMessageBox::Yes) {
        QString newPath = KFileDialog::getOpenFileName();
        if (newPath.isEmpty()) {
            break;
        }
        fileNames.prepend(newPath);
        fileNames.removeDuplicates();
        Settings::setConfigPaths(fileNames);
        Settings::self()->writeConfig();
        QString fileContents = readFile(newPath);
        if (!fileContents.isEmpty()) {
            parseSettings(fileContents);
            return true;
        }
    }
    return false;
}

QString KCMGRUB2::unquoteWord(const QString &word)
{
    KProcess echo(this);
    echo.setShellCommand(QString("echo -n %1").arg(word));
    echo.setOutputChannelMode(KProcess::OnlyStdoutChannel);
    if (echo.execute() == 0) {
        return QString(echo.readAllStandardOutput());
    }

    QChar ch;
    QString quotedWord = word, unquotedWord;
    QTextStream stream(&quotedWord, QIODevice::ReadOnly | QIODevice::Text);
    while (!stream.atEnd()) {
        stream >> ch;
        if (ch == '\'') {
            while (true) {
                if (stream.atEnd()) {
                    return QString();
                }
                stream >> ch;
                if (ch == '\'') {
                    return unquotedWord;
                } else {
                    unquotedWord.append(ch);
                }
            }
        } else if (ch == '"') {
            while (true) {
                if (stream.atEnd()) {
                    return QString();
                }
                stream >> ch;
                if (ch == '\\') {
                    if (stream.atEnd()) {
                        return QString();
                    }
                    stream >> ch;
                    switch (ch.toAscii()) {
                    case '$':
                    case '"':
                    case '\\':
                        unquotedWord.append(ch);
                        break;
                    case '\n':
                        unquotedWord.append(' ');
                        break;
                    default:
                        unquotedWord.append('\\').append(ch);
                        break;
                    }
                } else if (ch == '"') {
                    return unquotedWord;
                } else {
                    unquotedWord.append(ch);
                }
            }
        } else {
            while (true) {
                if (ch == '\\') {
                    if (stream.atEnd()) {
                        return unquotedWord;
                    }
                    stream >> ch;
                    switch (ch.toAscii()) {
                    case '\n':
                        break;
                    default:
                        unquotedWord.append(ch);
                        break;
                    }
                } else if (ch.isSpace()) {
                    return unquotedWord;
                } else {
                    unquotedWord.append(ch);
                }
                if (stream.atEnd()) {
                    return unquotedWord;
                }
                stream >> ch;
            }
        }
    }
    return QString();
}
void KCMGRUB2::parseSettings(const QString &config)
{
    m_settings.clear();
    m_settings["GRUB_DEFAULT"] = "0";
    m_settings["GRUB_TIMEOUT"] = "5";
    m_settings["GRUB_GFXMODE"] = "640x480";

    QString line, settingsConfig = config;
    QTextStream stream(&settingsConfig, QIODevice::ReadOnly);
    while (!stream.atEnd()) {
        line = stream.readLine().trimmed();
        if (line.startsWith("GRUB_")) {
            m_settings[line.section('=', 0, 0)] = unquoteWord(line.section('=', 1));
        }
    }
}
void KCMGRUB2::parseEntries(const QString &config)
{
    m_entries.clear();

    QChar ch;
    QString entry, entriesConfig = config;
    QTextStream stream(&entriesConfig, QIODevice::ReadOnly | QIODevice::Text);
    while (!stream.atEnd()) {
        stream >> ch;
        if (ch != '\n') {
            continue;
        }
        stream.skipWhiteSpace();
        if (stream.atEnd()) {
            return;
        }
        QString word;
        stream >> word;
        if (word.compare("menuentry") != 0) {
            continue;
        }
        stream.skipWhiteSpace();
        if (stream.atEnd()) {
            return;
        }
        stream >> ch;
        entry += ch;
        if (ch == '\'') {
            do {
                if (stream.atEnd()) {
                    return;
                }
                stream >> ch;
                entry += ch;
            } while (ch != '\'');
        } else if (ch == '"') {
            while (true) {
                if (stream.atEnd()) {
                    return;
                }
                stream >> ch;
                entry += ch;
                if (ch == '\\') {
                    stream >> ch;
                    entry += ch;
                } else if (ch == '"') {
                    break;
                }
            }
        } else {
            while (true) {
                if (stream.atEnd()) {
                    return;
                }
                stream >> ch;
                if (ch.isSpace()) {
                    break;
                }
                entry += ch;
                if (ch == '\\') {
                    stream >> ch;
                    entry += ch;
                }
            }
        }
        m_entries.append(unquoteWord(entry));
        entry.clear();
    }
}
