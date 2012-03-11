/*******************************************************************************
 * Copyright (C) 2008-2012 Konstantinos Smanis <konstantinos.smanis@gmail.com> *
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
#include <QtGui/QDesktopWidget>

//KDE
#include <KAboutData>
#include <KDebug>
#include <KInputDialog>
#include <KMenu>
#include <KMessageBox>
#include <kmountpoint.h>
#include <KPluginFactory>
#include <KProcess>
#include <KProgressDialog>
#include <KShell>
#include <KStandardDirs>
#include <KAuth/ActionWatcher>
using namespace KAuth;

//Project
#include <config.h>
#if HAVE_IMAGEMAGICK
#include "convertDlg.h"
#endif
#include "installDlg.h"
#if HAVE_QAPT || HAVE_QPACKAGEKIT
#include "removeDlg.h"
#endif

K_PLUGIN_FACTORY(GRUB2Factory, registerPlugin<KCMGRUB2>();)
K_EXPORT_PLUGIN(GRUB2Factory("kcmgrub2"))

KCMGRUB2::KCMGRUB2(QWidget *parent, const QVariantList &list) : KCModule(GRUB2Factory::componentData(), parent, list)
{
    //Isn't KAboutData's second argument supposed to do this?
    KGlobal::locale()->insertCatalog("kcm-grub2");

    KAboutData *about = new KAboutData("kcmgrub2", "kcm-grub2", ki18nc("@title", "KDE GRUB2 Bootloader Control Module"), KCM_GRUB2_VERSION, ki18nc("@title", "A KDE Control Module for configuring the GRUB2 bootloader."), KAboutData::License_GPL_V3, ki18nc("@info:credit", "Copyright (C) 2008-2012 Konstantinos Smanis"), KLocalizedString(), "http://ksmanis.wordpress.com/projects/grub2-editor/");
    about->addAuthor(ki18nc("@info:credit", "Κonstantinos Smanis"), ki18nc("@info:credit", "Main Developer"), "konstantinos.smanis@gmail.com", "http://ksmanis.wordpress.com/");
    setAboutData(about);

    ui.setupUi(this);
    setupObjects();
    setupConnections();
    if (check()) {
        ui.stackedWidget->setCurrentIndex(1);
    }
}

void KCMGRUB2::defaults()
{
    Action defaultsAction("org.kde.kcontrol.kcmgrub2.defaults");
    defaultsAction.setHelperID("org.kde.kcontrol.kcmgrub2");
    defaultsAction.addArgument("configFileName", configPath);
#if KDE_IS_VERSION(4,6,0)
    defaultsAction.setParentWidget(this);
#endif

    ActionReply reply = defaultsAction.execute();
    if (reply.succeeded()) {
        load();
        save();
        KMessageBox::information(this, i18nc("@info", "Successfully restored the default values."));
    } else {
        KMessageBox::detailedError(this, i18nc("@info", "Failed to restore the default values."), reply.data().value("output").toString());
    }
}
void KCMGRUB2::load()
{
    if (ui.stackedWidget->currentIndex() == 0) {
        return;
    }

    readEntries();
    readSettings();
    readEnv();
#if HAVE_HD
    readResolutions();
#endif
    if (unquoteWord(m_settings.value("GRUB_DEFAULT")).compare("saved") == 0) {
        m_settings["GRUB_DEFAULT"] = (!m_env.value("saved_entry").isEmpty() ? m_env.value("saved_entry") : "0");
    }

    bool ok;
    ui.kcombobox_default->clear();
    if (m_entries.size() > 0) {
        Q_FOREACH(const QString &entry, m_entries) {
            ui.kcombobox_default->addItem(unquoteWord(entry), entry);
        }
        int entryIndex = ui.kcombobox_default->findText(m_settings.value("GRUB_DEFAULT"));
        if (entryIndex != -1) {
            ui.kcombobox_default->setCurrentIndex(entryIndex);
        } else {
            entryIndex = unquoteWord(m_settings.value("GRUB_DEFAULT")).toInt(&ok);
            if (ok && entryIndex >= 0 && entryIndex < m_entries.size()) {
                ui.kcombobox_default->setCurrentIndex(entryIndex);
            } else {
                kWarning() << "Invalid GRUB_DEFAULT value";
            }
        }
    }
    ui.kpushbutton_remove->setEnabled(m_entries.size() > 0);
    ui.checkBox_savedefault->setChecked(unquoteWord(m_settings.value("GRUB_SAVEDEFAULT")).compare("true") == 0);

    if (!m_settings.value("GRUB_HIDDEN_TIMEOUT").isEmpty()) {
        int grubHiddenTimeout = unquoteWord(m_settings.value("GRUB_HIDDEN_TIMEOUT")).toInt(&ok);
        if (ok && grubHiddenTimeout >= 0) {
            ui.checkBox_hiddenTimeout->setChecked(grubHiddenTimeout > 0);
            ui.spinBox_hiddenTimeout->setValue(grubHiddenTimeout);
        } else {
            kWarning() << "Invalid GRUB_HIDDEN_TIMEOUT value";
        }
    }
    ui.checkBox_hiddenTimeoutShowTimer->setChecked(unquoteWord(m_settings.value("GRUB_HIDDEN_TIMEOUT_QUIET")).compare("true") != 0);
    int grubTimeout = (m_settings.value("GRUB_TIMEOUT").isEmpty() ? 5 : unquoteWord(m_settings.value("GRUB_TIMEOUT")).toInt(&ok));
    if (ok && grubTimeout >= -1) {
        ui.checkBox_timeout->setChecked(grubTimeout > -1);
        ui.radioButton_timeout0->setChecked(grubTimeout == 0);
        ui.radioButton_timeout->setChecked(grubTimeout > 0);
        ui.spinBox_timeout->setValue(grubTimeout);
    } else {
        kWarning() << "Invalid GRUB_TIMEOUT value";
    }

    ui.checkBox_recovery->setChecked(unquoteWord(m_settings.value("GRUB_DISABLE_RECOVERY")).compare("true") != 0);
    if (QFile::exists(memtestPath)) {
        ui.checkBox_memtest->setVisible(true);
        ui.checkBox_memtest->setChecked(QFile::permissions(memtestPath) & (QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther));
    } else {
        ui.checkBox_memtest->setVisible(false);
    }
    ui.checkBox_osProber->setChecked(unquoteWord(m_settings.value("GRUB_DISABLE_OS_PROBER")).compare("true") != 0);

    m_resolutions.append("640x480");
    QString grubGfxmode = (m_settings.value("GRUB_GFXMODE").isEmpty() ? "640x480" : unquoteWord(m_settings.value("GRUB_GFXMODE")));
    if (!grubGfxmode.isEmpty() && !m_resolutions.contains(grubGfxmode)) {
        m_resolutions.append(grubGfxmode);
    }
    QString grubGfxpayloadLinux = unquoteWord(m_settings.value("GRUB_GFXPAYLOAD_LINUX"));
    if (!grubGfxpayloadLinux.isEmpty() && grubGfxpayloadLinux.compare("text") != 0 && grubGfxpayloadLinux.compare("keep") != 0 && !m_resolutions.contains(grubGfxpayloadLinux)) {
        m_resolutions.append(grubGfxpayloadLinux);
    }
    m_resolutions.removeDuplicates();
    sortResolutions();
    showResolutions();
    ui.kcombobox_gfxmode->setCurrentIndex(ui.kcombobox_gfxmode->findData(grubGfxmode));
    ui.kcombobox_gfxpayload->setCurrentIndex(ui.kcombobox_gfxpayload->findData(grubGfxpayloadLinux));

    QString grubColorNormal = unquoteWord(m_settings.value("GRUB_COLOR_NORMAL"));
    if (!grubColorNormal.isEmpty()) {
        int normalForegroundIndex = ui.kcombobox_normalForeground->findData(grubColorNormal.section('/', 0, 0));
        int normalBackgroundIndex = ui.kcombobox_normalBackground->findData(grubColorNormal.section('/', 1));
        if (normalForegroundIndex == -1 || normalBackgroundIndex == -1) {
            kWarning() << "Invalid GRUB_COLOR_NORMAL value";
        }
        if (normalForegroundIndex != -1) {
            ui.kcombobox_normalForeground->setCurrentIndex(normalForegroundIndex);
        }
        if (normalBackgroundIndex != -1) {
            ui.kcombobox_normalBackground->setCurrentIndex(normalBackgroundIndex);
        }
    }
    QString grubColorHighlight = unquoteWord(m_settings.value("GRUB_COLOR_HIGHLIGHT"));
    if (!grubColorHighlight.isEmpty()) {
        int highlightForegroundIndex = ui.kcombobox_highlightForeground->findData(grubColorHighlight.section('/', 0, 0));
        int highlightBackgroundIndex = ui.kcombobox_highlightBackground->findData(grubColorHighlight.section('/', 1));
        if (highlightForegroundIndex == -1 || highlightBackgroundIndex == -1) {
            kWarning() << "Invalid GRUB_COLOR_HIGHLIGHT value";
        }
        if (highlightForegroundIndex != -1) {
            ui.kcombobox_highlightForeground->setCurrentIndex(highlightForegroundIndex);
        }
        if (highlightBackgroundIndex != -1) {
            ui.kcombobox_highlightBackground->setCurrentIndex(highlightBackgroundIndex);
        }
    }

    QString grubBackground = unquoteWord(m_settings.value("GRUB_BACKGROUND"));
    ui.kurlrequester_background->setText(grubBackground);
    ui.kpushbutton_preview->setEnabled(!grubBackground.isEmpty());
    ui.kurlrequester_theme->setText(unquoteWord(m_settings.value("GRUB_THEME")));

    ui.klineedit_cmdlineDefault->setText(unquoteWord(m_settings.value("GRUB_CMDLINE_LINUX_DEFAULT")));
    ui.klineedit_cmdline->setText(unquoteWord(m_settings.value("GRUB_CMDLINE_LINUX")));

    QString grubTerminal = unquoteWord(m_settings.value("GRUB_TERMINAL"));
    ui.klineedit_terminal->setText(grubTerminal);
    ui.klineedit_terminalInput->setReadOnly(!grubTerminal.isEmpty());
    ui.klineedit_terminalOutput->setReadOnly(!grubTerminal.isEmpty());
    ui.klineedit_terminalInput->setText(!grubTerminal.isEmpty() ? grubTerminal : unquoteWord(m_settings.value("GRUB_TERMINAL_INPUT")));
    ui.klineedit_terminalOutput->setText(!grubTerminal.isEmpty() ? grubTerminal : unquoteWord(m_settings.value("GRUB_TERMINAL_OUTPUT")));

    ui.klineedit_distributor->setText(unquoteWord(m_settings.value("GRUB_DISTRIBUTOR")));
    ui.klineedit_serial->setText(unquoteWord(m_settings.value("GRUB_SERIAL_COMMAND")));
    ui.klineedit_initTune->setText(unquoteWord(m_settings.value("GRUB_INIT_TUNE")));
    ui.checkBox_uuid->setChecked(unquoteWord(m_settings.value("GRUB_DISABLE_LINUX_UUID")).compare("true") != 0);

    m_dirtyBits.fill(0);
    emit changed(false);
}
void KCMGRUB2::save()
{
    if (m_entries.size() > 0) {
        m_settings["GRUB_DEFAULT"] = "saved";
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
                m_settings["GRUB_TIMEOUT"] = '0';
            } else {
                m_settings["GRUB_TIMEOUT"] = QString::number(ui.spinBox_timeout->value());
            }
        } else {
            m_settings["GRUB_TIMEOUT"] = "-1";
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
    if (m_dirtyBits.testBit(grubGfxmodeDirty)) {
        if (ui.kcombobox_gfxmode->currentIndex() <= 0) {
            kError() << "Something went terribly wrong!";
        } else {
            m_settings["GRUB_GFXMODE"] = quoteWord(ui.kcombobox_gfxmode->itemData(ui.kcombobox_gfxmode->currentIndex()).toString());
        }
    }
    if (m_dirtyBits.testBit(grubGfxpayloadLinuxDirty)) {
        if (ui.kcombobox_gfxpayload->currentIndex() <= 0) {
            kError() << "Something went terribly wrong!";
        } else if (ui.kcombobox_gfxpayload->currentIndex() == 1) {
            m_settings.remove("GRUB_GFXPAYLOAD_LINUX");
        } else if (ui.kcombobox_gfxpayload->currentIndex() > 1) {
            m_settings["GRUB_GFXPAYLOAD_LINUX"] = quoteWord(ui.kcombobox_gfxpayload->itemData(ui.kcombobox_gfxpayload->currentIndex()).toString());
        }
    }
    if (m_dirtyBits.testBit(grubColorNormalDirty)) {
        QString normalForeground = ui.kcombobox_normalForeground->itemData(ui.kcombobox_normalForeground->currentIndex()).toString();
        QString normalBackground = ui.kcombobox_normalBackground->itemData(ui.kcombobox_normalBackground->currentIndex()).toString();
        if (normalForeground.compare("light-gray") != 0 || normalBackground.compare("black") != 0) {
            m_settings["GRUB_COLOR_NORMAL"] = normalForeground + '/' + normalBackground;
        } else {
            m_settings.remove("GRUB_COLOR_NORMAL");
        }
    }
    if (m_dirtyBits.testBit(grubColorHighlightDirty)) {
        QString highlightForeground = ui.kcombobox_highlightForeground->itemData(ui.kcombobox_highlightForeground->currentIndex()).toString();
        QString highlightBackground = ui.kcombobox_highlightBackground->itemData(ui.kcombobox_highlightBackground->currentIndex()).toString();
        if (highlightForeground.compare("black") != 0 || highlightBackground.compare("light-gray") != 0) {
            m_settings["GRUB_COLOR_HIGHLIGHT"] = highlightForeground + '/' + highlightBackground;
        } else {
            m_settings.remove("GRUB_COLOR_HIGHLIGHT");
        }
    }
    if (m_dirtyBits.testBit(grubBackgroundDirty)) {
        QString background = ui.kurlrequester_background->url().toLocalFile();
        if (!background.isEmpty()) {
            m_settings["GRUB_BACKGROUND"] = quoteWord(background);
        } else {
            m_settings.remove("GRUB_BACKGROUND");
        }
    }
    if (m_dirtyBits.testBit(grubThemeDirty)) {
        QString theme = ui.kurlrequester_theme->url().toLocalFile();
        if (!theme.isEmpty()) {
            m_settings["GRUB_THEME"] = quoteWord(theme);
        } else {
            m_settings.remove("GRUB_THEME");
        }
    }
    if (m_dirtyBits.testBit(grubCmdlineLinuxDefaultDirty)) {
        QString cmdlineLinuxDefault = ui.klineedit_cmdlineDefault->text();
        if (!cmdlineLinuxDefault.isEmpty()) {
            m_settings["GRUB_CMDLINE_LINUX_DEFAULT"] = quoteWord(cmdlineLinuxDefault);
        } else {
            m_settings.remove("GRUB_CMDLINE_LINUX_DEFAULT");
        }
    }
    if (m_dirtyBits.testBit(grubCmdlineLinuxDirty)) {
        QString cmdlineLinux = ui.klineedit_cmdline->text();
        if (!cmdlineLinux.isEmpty()) {
            m_settings["GRUB_CMDLINE_LINUX"] = quoteWord(cmdlineLinux);
        } else {
            m_settings.remove("GRUB_CMDLINE_LINUX");
        }
    }
    if (m_dirtyBits.testBit(grubTerminalDirty)) {
        QString terminal = ui.klineedit_terminal->text();
        if (!terminal.isEmpty()) {
            m_settings["GRUB_TERMINAL"] = quoteWord(terminal);
        } else {
            m_settings.remove("GRUB_TERMINAL");
        }
    }
    if (m_dirtyBits.testBit(grubTerminalInputDirty)) {
        QString terminalInput = ui.klineedit_terminalInput->text();
        if (!terminalInput.isEmpty()) {
            m_settings["GRUB_TERMINAL_INPUT"] = quoteWord(terminalInput);
        } else {
            m_settings.remove("GRUB_TERMINAL_INPUT");
        }
    }
    if (m_dirtyBits.testBit(grubTerminalOutputDirty)) {
        QString terminalOutput = ui.klineedit_terminalOutput->text();
        if (!terminalOutput.isEmpty()) {
            m_settings["GRUB_TERMINAL_OUTPUT"] = quoteWord(terminalOutput);
        } else {
            m_settings.remove("GRUB_TERMINAL_OUTPUT");
        }
    }
    if (m_dirtyBits.testBit(grubDistributorDirty)) {
        QString distributor = ui.klineedit_distributor->text();
        if (!distributor.isEmpty()) {
            m_settings["GRUB_DISTRIBUTOR"] = quoteWord(distributor);
        } else {
            m_settings.remove("GRUB_DISTRIBUTOR");
        }
    }
    if (m_dirtyBits.testBit(grubSerialCommandDirty)) {
        QString serialCommand = ui.klineedit_serial->text();
        if (!serialCommand.isEmpty()) {
            m_settings["GRUB_SERIAL_COMMAND"] = quoteWord(serialCommand);
        } else {
            m_settings.remove("GRUB_SERIAL_COMMAND");
        }
    }
    if (m_dirtyBits.testBit(grubInitTuneDirty)) {
        QString initTune = ui.klineedit_initTune->text();
        if (!initTune.isEmpty()) {
            m_settings["GRUB_INIT_TUNE"] = quoteWord(initTune);
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

    QString configFileContents;
    QTextStream stream(&configFileContents, QIODevice::WriteOnly | QIODevice::Text);
    QHash<QString, QString>::const_iterator it = m_settings.constBegin();
    QHash<QString, QString>::const_iterator end = m_settings.constEnd();
    for (; it != end; ++it) {
        stream << it.key() << '=' << it.value() << endl;
    }

    Action saveAction("org.kde.kcontrol.kcmgrub2.save");
    saveAction.setHelperID("org.kde.kcontrol.kcmgrub2");
    saveAction.addArgument("PATH", qgetenv("PATH"));
    saveAction.addArgument("mkconfigExePath", mkconfigExePath);
    saveAction.addArgument("set_defaultExePath", set_defaultExePath);
    saveAction.addArgument("configFileName", configPath);
    saveAction.addArgument("configFileContents", configFileContents);
    saveAction.addArgument("menuFileName", menuPath);
    saveAction.addArgument("defaultEntry", m_entries.size() > 0 ? m_entries.at(ui.kcombobox_default->currentIndex()) : m_settings.value("GRUB_DEFAULT"));
    if (m_dirtyBits.testBit(memtestDirty)) {
        saveAction.addArgument("memtestFileName", memtestPath);
        saveAction.addArgument("memtest", ui.checkBox_memtest->isChecked());
    }
#if KDE_IS_VERSION(4,6,0)
    saveAction.setParentWidget(this);
#endif

    if (saveAction.authorize() != Action::Authorized) {
        return;
    }

    KProgressDialog progressDlg(this, i18nc("@title:window Verb (gerund). Refers to current status.", "Saving"), i18nc("@info:progress", "Saving GRUB settings..."));
    progressDlg.setAllowCancel(false);
    progressDlg.setModal(true);
    progressDlg.progressBar()->setMinimum(0);
    progressDlg.progressBar()->setMaximum(0);
    progressDlg.show();
    connect(saveAction.watcher(), SIGNAL(actionPerformed(ActionReply)), &progressDlg, SLOT(hide()));

    ActionReply reply = saveAction.execute();
    if (reply.succeeded()) {
        KDialog *dialog = new KDialog(this, Qt::Dialog);
        dialog->setCaption(i18nc("@title:window", "Information"));
        dialog->setButtons(KDialog::Ok | KDialog::Details);
        dialog->setModal(true);
        dialog->setDefaultButton(KDialog::Ok);
        dialog->setEscapeButton(KDialog::Ok);
        KMessageBox::createKMessageBox(dialog, QMessageBox::Information, i18nc("@info", "Successfully saved GRUB settings."), QStringList(), QString(), 0, KMessageBox::Notify, reply.data().value("output").toString()); // krazy:exclude=qclasses
        load();
    } else {
        KMessageBox::detailedError(this, i18nc("@info", "Failed to save GRUB settings."), reply.data().value("output").toString());
    }
}

void KCMGRUB2::slotRemoveOldEntries()
{
#if HAVE_QAPT || HAVE_QPACKAGEKIT
    QStringList entries;
    Q_FOREACH(const QString &entry, m_entries) {
        entries.append(unquoteWord(entry));
    }
    QPointer<RemoveDialog> removeDlg = new RemoveDialog(entries, m_kernels, this);
    if (removeDlg->exec()) {
        load();
    }
    delete removeDlg;
#endif
}
void KCMGRUB2::slotGrubSavedefaultChanged()
{
    m_dirtyBits.setBit(grubSavedefaultDirty);
    emit changed(true);
}
void KCMGRUB2::slotGrubHiddenTimeoutChanged()
{
    m_dirtyBits.setBit(grubHiddenTimeoutDirty);
    emit changed(true);
}
void KCMGRUB2::slotGrubHiddenTimeoutQuietChanged()
{
    m_dirtyBits.setBit(grubHiddenTimeoutQuietDirty);
    emit changed(true);
}
void KCMGRUB2::slotGrubTimeoutChanged()
{
    m_dirtyBits.setBit(grubTimeoutDirty);
    emit changed(true);
}
void KCMGRUB2::slotGrubDisableRecoveryChanged()
{
    m_dirtyBits.setBit(grubDisableRecoveryDirty);
    emit changed(true);
}
void KCMGRUB2::slotMemtestChanged()
{
    m_dirtyBits.setBit(memtestDirty);
    emit changed(true);
}
void KCMGRUB2::slotGrubDisableOsProberChanged()
{
    m_dirtyBits.setBit(grubDisableOsProberDirty);
    emit changed(true);
}
void KCMGRUB2::slotInstallBootloader()
{
    QPointer<InstallDialog> installDlg = new InstallDialog(installExePath, this);
    installDlg->exec();
    delete installDlg;
}
void KCMGRUB2::slotGrubGfxmodeChanged()
{
    if (ui.kcombobox_gfxmode->currentIndex() == 0) {
        bool ok;
        QRegExpValidator regExp(QRegExp("\\d{3,4}x\\d{3,4}(x\\d{1,2})?"), this);
        QString resolution = KInputDialog::getText(i18nc("@title:window", "Enter screen resolution"), i18nc("@label:textbox", "Please enter a GRUB resolution:"), QString(), &ok, this, &regExp);
        if (ok) {
            if (!m_resolutions.contains(resolution)) {
                QString gfxpayload = ui.kcombobox_gfxpayload->itemData(ui.kcombobox_gfxpayload->currentIndex()).toString();
                m_resolutions.append(resolution);
                sortResolutions();
                showResolutions();
                ui.kcombobox_gfxpayload->setCurrentIndex(ui.kcombobox_gfxpayload->findData(gfxpayload));
            }
            ui.kcombobox_gfxmode->setCurrentIndex(ui.kcombobox_gfxmode->findData(resolution));
        } else {
            ui.kcombobox_gfxmode->setCurrentIndex(ui.kcombobox_gfxmode->findData("640x480"));
        }
    }
    m_dirtyBits.setBit(grubGfxmodeDirty);
    emit changed(true);
}
void KCMGRUB2::slotGrubGfxpayloadLinuxChanged()
{
    if (ui.kcombobox_gfxpayload->currentIndex() == 0) {
        bool ok;
        QRegExpValidator regExp(QRegExp("\\d{3,4}x\\d{3,4}(x\\d{1,2})?"), this);
        QString resolution = KInputDialog::getText(i18nc("@title:window", "Enter screen resolution"), i18nc("@label:textbox", "Please enter a Linux boot resolution:"), QString(), &ok, this, &regExp);
        if (ok) {
            if (!m_resolutions.contains(resolution)) {
                QString gfxmode = ui.kcombobox_gfxmode->itemData(ui.kcombobox_gfxmode->currentIndex()).toString();
                m_resolutions.append(resolution);
                sortResolutions();
                showResolutions();
                ui.kcombobox_gfxmode->setCurrentIndex(ui.kcombobox_gfxmode->findData(gfxmode));
            }
            ui.kcombobox_gfxpayload->setCurrentIndex(ui.kcombobox_gfxpayload->findData(resolution));
        } else {
            ui.kcombobox_gfxpayload->setCurrentIndex(ui.kcombobox_gfxpayload->findData(QString()));
        }
    }
    m_dirtyBits.setBit(grubGfxpayloadLinuxDirty);
    emit changed(true);
}
void KCMGRUB2::slotGrubColorNormalChanged()
{
    m_dirtyBits.setBit(grubColorNormalDirty);
    emit changed(true);
}
void KCMGRUB2::slotGrubColorHighlightChanged()
{
    m_dirtyBits.setBit(grubColorHighlightDirty);
    emit changed(true);
}
void KCMGRUB2::slowGrubBackgroundChanged()
{
    ui.kpushbutton_preview->setEnabled(!ui.kurlrequester_background->text().isEmpty());
    m_dirtyBits.setBit(grubBackgroundDirty);
    emit changed(true);
}
void KCMGRUB2::slotPreviewGrubBackground()
{
    QFile file(ui.kurlrequester_background->url().toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    //TODO: Need something more elegant.
    QDialog *dialog = new QDialog(this);
    QLabel *label = new QLabel(dialog);
    label->setPixmap(QPixmap::fromImage(QImage::fromData(file.readAll())).scaled(QDesktopWidget().screenGeometry(this).size()));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->showFullScreen();
    KMessageBox::information(dialog, i18nc("@info", "Press <shortcut>Escape</shortcut> to exit fullscreen mode."), QString(), "GRUBFullscreenPreview");
}
void KCMGRUB2::slotCreateGrubBackground()
{
#if HAVE_IMAGEMAGICK
    QPointer<ConvertDialog> convertDlg = new ConvertDialog(this);
    QString resolution = ui.kcombobox_gfxmode->itemData(ui.kcombobox_gfxmode->currentIndex()).toString();
    convertDlg->setResolution(resolution.section('x', 0, 0).toInt(), resolution.section('x', 1, 1).toInt());
    connect(convertDlg, SIGNAL(splashImageCreated(QString)), ui.kurlrequester_background, SLOT(setText(QString)));
    convertDlg->exec();
    delete convertDlg;
#endif
}
void KCMGRUB2::slotGrubThemeChanged()
{
    m_dirtyBits.setBit(grubThemeDirty);
    emit changed(true);
}
void KCMGRUB2::slotGrubCmdlineLinuxDefaultChanged()
{
    m_dirtyBits.setBit(grubCmdlineLinuxDefaultDirty);
    emit changed(true);
}
void KCMGRUB2::slotGrubCmdlineLinuxChanged()
{
    m_dirtyBits.setBit(grubCmdlineLinuxDirty);
    emit changed(true);
}
void KCMGRUB2::slotGrubTerminalChanged()
{
    QString grubTerminal = ui.klineedit_terminal->text();
    ui.klineedit_terminalInput->setReadOnly(!grubTerminal.isEmpty());
    ui.klineedit_terminalOutput->setReadOnly(!grubTerminal.isEmpty());
    ui.klineedit_terminalInput->setText(!grubTerminal.isEmpty() ? grubTerminal : unquoteWord(m_settings.value("GRUB_TERMINAL_INPUT")));
    ui.klineedit_terminalOutput->setText(!grubTerminal.isEmpty() ? grubTerminal : unquoteWord(m_settings.value("GRUB_TERMINAL_OUTPUT")));
    m_dirtyBits.setBit(grubTerminalDirty);
    emit changed(true);
}
void KCMGRUB2::slotGrubTerminalInputChanged()
{
    m_dirtyBits.setBit(grubTerminalInputDirty);
    emit changed(true);
}
void KCMGRUB2::slotGrubTerminalOutputChanged()
{
    m_dirtyBits.setBit(grubTerminalOutputDirty);
    emit changed(true);
}
void KCMGRUB2::slotGrubDistributorChanged()
{
    m_dirtyBits.setBit(grubDistributorDirty);
    emit changed(true);
}
void KCMGRUB2::slotGrubSerialCommandChanged()
{
    m_dirtyBits.setBit(grubSerialCommandDirty);
    emit changed(true);
}
void KCMGRUB2::slotGrubInitTuneChanged()
{
    m_dirtyBits.setBit(grubInitTuneDirty);
    emit changed(true);
}
void KCMGRUB2::slotGrubDisableLinuxUuidChanged()
{
    m_dirtyBits.setBit(grubDisableLinuxUuidDirty);
    emit changed(true);
}

void KCMGRUB2::slotUpdateSuggestions()
{
    if (!sender()->isWidgetType()) {
        return;
    }

    KLineEdit *lineEdit = 0;
    if (ui.kpushbutton_cmdlineDefaultSuggestions->isDown()) {
        lineEdit = ui.klineedit_cmdlineDefault;
    } else if (ui.kpushbutton_cmdlineSuggestions->isDown()) {
        lineEdit = ui.klineedit_cmdline;
    } else if (ui.kpushbutton_terminalSuggestions->isDown()) {
        lineEdit = ui.klineedit_terminal;
    } else if (ui.kpushbutton_terminalInputSuggestions->isDown()) {
        lineEdit = ui.klineedit_terminalInput;
    } else if (ui.kpushbutton_terminalOutputSuggestions->isDown()) {
        lineEdit = ui.klineedit_terminalOutput;
    } else {
        return;
    }

    Q_FOREACH(QAction *action, qobject_cast<const QWidget*>(sender())->actions()) {
        if (!action->isCheckable()) {
            action->setCheckable(true);
        }
        action->setChecked(lineEdit->text().contains(QRegExp(QString("\\b%1\\b").arg(action->data().toString()))));
    }
}
void KCMGRUB2::slotTriggeredSuggestion(QAction *action)
{
    KLineEdit *lineEdit = 0;
    void (KCMGRUB2::*updateFunction)() = 0;
    if (ui.kpushbutton_cmdlineDefaultSuggestions->isDown()) {
        lineEdit = ui.klineedit_cmdlineDefault;
        updateFunction = &KCMGRUB2::slotGrubCmdlineLinuxDefaultChanged;
    } else if (ui.kpushbutton_cmdlineSuggestions->isDown()) {
        lineEdit = ui.klineedit_cmdline;
        updateFunction = &KCMGRUB2::slotGrubCmdlineLinuxChanged;
    } else if (ui.kpushbutton_terminalSuggestions->isDown()) {
        lineEdit = ui.klineedit_terminal;
        updateFunction = &KCMGRUB2::slotGrubTerminalChanged;
    } else if (ui.kpushbutton_terminalInputSuggestions->isDown()) {
        lineEdit = ui.klineedit_terminalInput;
        updateFunction = &KCMGRUB2::slotGrubTerminalInputChanged;
    } else if (ui.kpushbutton_terminalOutputSuggestions->isDown()) {
        lineEdit = ui.klineedit_terminalOutput;
        updateFunction = &KCMGRUB2::slotGrubTerminalOutputChanged;
    } else {
        return;
    }

    QString lineEditText = lineEdit->text();
    if (!action->isChecked()) {
        lineEdit->setText(lineEditText.remove(QRegExp(QString("\\b%1\\b").arg(action->data().toString()))).simplified());
    } else {
        lineEdit->setText(lineEditText.isEmpty() ? action->data().toString() : lineEditText + ' ' + action->data().toString());
    }
    (this->*updateFunction)();
}

void KCMGRUB2::setupObjects()
{
    setButtons(Default | Apply);
    setNeedsAuthorization(true);

    m_dirtyBits.resize(lastDirtyBit);

    ui.kpushbutton_install->setIcon(KIcon("system-software-update"));
    ui.kpushbutton_remove->setIcon(KIcon("list-remove"));
    ui.kpushbutton_remove->setVisible(HAVE_QAPT || HAVE_QPACKAGEKIT);

    QPixmap black(16, 16), transparent(16, 16);
    black.fill(Qt::black);
    transparent.fill(Qt::transparent);
    ui.kcombobox_normalForeground->addItem(QIcon(black), i18nc("@item:inlistbox Refers to color.", "Black"), "black");
    ui.kcombobox_highlightForeground->addItem(QIcon(black), i18nc("@item:inlistbox Refers to color.", "Black"), "black");
    ui.kcombobox_normalBackground->addItem(QIcon(transparent), i18nc("@item:inlistbox Refers to color.", "Transparent"), "black");
    ui.kcombobox_highlightBackground->addItem(QIcon(transparent), i18nc("@item:inlistbox Refers to color.", "Transparent"), "black");
    QHash<QString, QString> colors;
    colors.insertMulti("blue", i18nc("@item:inlistbox Refers to color.", "Blue"));
    colors.insertMulti("blue", "blue");
    colors.insertMulti("cyan", i18nc("@item:inlistbox Refers to color.", "Cyan"));
    colors.insertMulti("cyan", "cyan");
    colors.insertMulti("dark-gray", i18nc("@item:inlistbox Refers to color.", "Dark Gray"));
    colors.insertMulti("dark-gray", "darkgray");
    colors.insertMulti("green", i18nc("@item:inlistbox Refers to color.", "Green"));
    colors.insertMulti("green", "green");
    colors.insertMulti("light-cyan", i18nc("@item:inlistbox Refers to color.", "Light Cyan"));
    colors.insertMulti("light-cyan", "lightcyan");
    colors.insertMulti("light-blue", i18nc("@item:inlistbox Refers to color.", "Light Blue"));
    colors.insertMulti("light-blue", "lightblue");
    colors.insertMulti("light-green", i18nc("@item:inlistbox Refers to color.", "Light Green"));
    colors.insertMulti("light-green", "lightgreen");
    colors.insertMulti("light-gray", i18nc("@item:inlistbox Refers to color.", "Light Gray"));
    colors.insertMulti("light-gray", "lightgray");
    colors.insertMulti("light-magenta", i18nc("@item:inlistbox Refers to color.", "Light Magenta"));
    colors.insertMulti("light-magenta", "magenta");
    colors.insertMulti("light-red", i18nc("@item:inlistbox Refers to color.", "Light Red"));
    colors.insertMulti("light-red", "orangered");
    colors.insertMulti("magenta", i18nc("@item:inlistbox Refers to color.", "Magenta"));
    colors.insertMulti("magenta", "darkmagenta");
    colors.insertMulti("red", i18nc("@item:inlistbox Refers to color.", "Red"));
    colors.insertMulti("red", "red");
    colors.insertMulti("white", i18nc("@item:inlistbox Refers to color.", "White"));
    colors.insertMulti("white", "white");
    colors.insertMulti("yellow", i18nc("@item:inlistbox Refers to color.", "Yellow"));
    colors.insertMulti("yellow", "yellow");
    QHash<QString, QString>::const_iterator it = colors.constBegin();
    QHash<QString, QString>::const_iterator end = colors.constEnd();
    for (; it != end; it += 2) {
        QPixmap color(16, 16);
        color.fill(QColor(colors.values(it.key()).at(0)));
        ui.kcombobox_normalForeground->addItem(QIcon(color), colors.values(it.key()).at(1), it.key());
        ui.kcombobox_highlightForeground->addItem(QIcon(color), colors.values(it.key()).at(1), it.key());
        ui.kcombobox_normalBackground->addItem(QIcon(color), colors.values(it.key()).at(1), it.key());
        ui.kcombobox_highlightBackground->addItem(QIcon(color), colors.values(it.key()).at(1), it.key());
    }
    ui.kcombobox_normalForeground->setCurrentIndex(ui.kcombobox_normalForeground->findData("light-gray"));
    ui.kcombobox_normalBackground->setCurrentIndex(ui.kcombobox_normalBackground->findData("black"));
    ui.kcombobox_highlightForeground->setCurrentIndex(ui.kcombobox_highlightForeground->findData("black"));
    ui.kcombobox_highlightBackground->setCurrentIndex(ui.kcombobox_highlightBackground->findData("light-gray"));

    ui.kpushbutton_preview->setIcon(KIcon("image-png"));
    ui.kpushbutton_create->setIcon(KIcon("insert-image"));
    ui.kpushbutton_create->setVisible(HAVE_IMAGEMAGICK);

    ui.kpushbutton_cmdlineDefaultSuggestions->setIcon(KIcon("tools-wizard"));
    ui.kpushbutton_cmdlineDefaultSuggestions->setMenu(new KMenu(ui.kpushbutton_cmdlineDefaultSuggestions));
    ui.kpushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Quiet Boot"))->setData("quiet");
    ui.kpushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Show Splash Screen"))->setData("splash");
    ui.kpushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Disable Plymouth"))->setData("noplymouth");
    ui.kpushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off ACPI"))->setData("acpi=off");
    ui.kpushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off APIC"))->setData("noapic");
    ui.kpushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off Local APIC"))->setData("nolapic");
    ui.kpushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Single User Mode"))->setData("single");
    ui.kpushbutton_cmdlineSuggestions->setIcon(KIcon("tools-wizard"));
    ui.kpushbutton_cmdlineSuggestions->setMenu(new KMenu(ui.kpushbutton_cmdlineSuggestions));
    ui.kpushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Quiet Boot"))->setData("quiet");
    ui.kpushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Show Splash Screen"))->setData("splash");
    ui.kpushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Disable Plymouth"))->setData("noplymouth");
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
    connect(ui.kcombobox_default, SIGNAL(activated(int)), this, SLOT(changed()));
    connect(ui.kpushbutton_remove, SIGNAL(clicked(bool)), this, SLOT(slotRemoveOldEntries()));
    connect(ui.checkBox_savedefault, SIGNAL(clicked(bool)), this, SLOT(slotGrubSavedefaultChanged()));

    connect(ui.checkBox_hiddenTimeout, SIGNAL(clicked(bool)), this, SLOT(slotGrubHiddenTimeoutChanged()));
    connect(ui.spinBox_hiddenTimeout, SIGNAL(valueChanged(int)), this, SLOT(slotGrubHiddenTimeoutChanged()));
    connect(ui.checkBox_hiddenTimeoutShowTimer, SIGNAL(clicked(bool)), this, SLOT(slotGrubHiddenTimeoutQuietChanged()));
    connect(ui.checkBox_timeout, SIGNAL(clicked(bool)), this, SLOT(slotGrubTimeoutChanged()));
    connect(ui.radioButton_timeout0, SIGNAL(clicked(bool)), this, SLOT(slotGrubTimeoutChanged()));
    connect(ui.radioButton_timeout, SIGNAL(clicked(bool)), this, SLOT(slotGrubTimeoutChanged()));
    connect(ui.spinBox_timeout, SIGNAL(valueChanged(int)), this, SLOT(slotGrubTimeoutChanged()));

    connect(ui.checkBox_recovery, SIGNAL(clicked(bool)), this, SLOT(slotGrubDisableRecoveryChanged()));
    connect(ui.checkBox_memtest, SIGNAL(clicked(bool)), this, SLOT(slotMemtestChanged()));
    connect(ui.checkBox_osProber, SIGNAL(clicked(bool)), this, SLOT(slotGrubDisableOsProberChanged()));

    connect(ui.kcombobox_gfxmode, SIGNAL(activated(int)), this, SLOT(slotGrubGfxmodeChanged()));
    connect(ui.kcombobox_gfxpayload, SIGNAL(activated(int)), this, SLOT(slotGrubGfxpayloadLinuxChanged()));

    connect(ui.kcombobox_normalForeground, SIGNAL(activated(int)), this, SLOT(slotGrubColorNormalChanged()));
    connect(ui.kcombobox_normalBackground, SIGNAL(activated(int)), this, SLOT(slotGrubColorNormalChanged()));
    connect(ui.kcombobox_highlightForeground, SIGNAL(activated(int)), this, SLOT(slotGrubColorHighlightChanged()));
    connect(ui.kcombobox_highlightBackground, SIGNAL(activated(int)), this, SLOT(slotGrubColorHighlightChanged()));

    connect(ui.kurlrequester_background, SIGNAL(textChanged(QString)), this, SLOT(slowGrubBackgroundChanged()));
    connect(ui.kpushbutton_preview, SIGNAL(clicked(bool)), this, SLOT(slotPreviewGrubBackground()));
    connect(ui.kpushbutton_create, SIGNAL(clicked(bool)), this, SLOT(slotCreateGrubBackground()));
    connect(ui.kurlrequester_theme, SIGNAL(textChanged(QString)), this, SLOT(slotGrubThemeChanged()));

    connect(ui.klineedit_cmdlineDefault, SIGNAL(textEdited(QString)), this, SLOT(slotGrubCmdlineLinuxDefaultChanged()));
    connect(ui.kpushbutton_cmdlineDefaultSuggestions->menu(), SIGNAL(aboutToShow()), this, SLOT(slotUpdateSuggestions()));
    connect(ui.kpushbutton_cmdlineDefaultSuggestions->menu(), SIGNAL(triggered(QAction*)), this, SLOT(slotTriggeredSuggestion(QAction*)));
    connect(ui.klineedit_cmdline, SIGNAL(textEdited(QString)), this, SLOT(slotGrubCmdlineLinuxChanged()));
    connect(ui.kpushbutton_cmdlineSuggestions->menu(), SIGNAL(aboutToShow()), this, SLOT(slotUpdateSuggestions()));
    connect(ui.kpushbutton_cmdlineSuggestions->menu(), SIGNAL(triggered(QAction*)), this, SLOT(slotTriggeredSuggestion(QAction*)));

    connect(ui.klineedit_terminal, SIGNAL(textEdited(QString)), this, SLOT(slotGrubTerminalChanged()));
    connect(ui.kpushbutton_terminalSuggestions->menu(), SIGNAL(aboutToShow()), this, SLOT(slotUpdateSuggestions()));
    connect(ui.kpushbutton_terminalSuggestions->menu(), SIGNAL(triggered(QAction*)), this, SLOT(slotTriggeredSuggestion(QAction*)));
    connect(ui.klineedit_terminalInput, SIGNAL(textEdited(QString)), this, SLOT(slotGrubTerminalInputChanged()));
    connect(ui.kpushbutton_terminalInputSuggestions->menu(), SIGNAL(aboutToShow()), this, SLOT(slotUpdateSuggestions()));
    connect(ui.kpushbutton_terminalInputSuggestions->menu(), SIGNAL(triggered(QAction*)), this, SLOT(slotTriggeredSuggestion(QAction*)));
    connect(ui.klineedit_terminalOutput, SIGNAL(textEdited(QString)), this, SLOT(slotGrubTerminalOutputChanged()));
    connect(ui.kpushbutton_terminalOutputSuggestions->menu(), SIGNAL(aboutToShow()), this, SLOT(slotUpdateSuggestions()));
    connect(ui.kpushbutton_terminalOutputSuggestions->menu(), SIGNAL(triggered(QAction*)), this, SLOT(slotTriggeredSuggestion(QAction*)));

    connect(ui.klineedit_distributor, SIGNAL(textEdited(QString)), this, SLOT(slotGrubDistributorChanged()));
    connect(ui.klineedit_serial, SIGNAL(textEdited(QString)), this, SLOT(slotGrubSerialCommandChanged()));
    connect(ui.klineedit_initTune, SIGNAL(textEdited(QString)), this, SLOT(slotGrubInitTuneChanged()));
    connect(ui.checkBox_uuid, SIGNAL(clicked(bool)), this, SLOT(slotGrubDisableLinuxUuidChanged()));

    connect(ui.kpushbutton_install, SIGNAL(clicked(bool)), this, SLOT(slotInstallBootloader()));
}

QString KCMGRUB2::findExe(const QString &exeName)
{
    QString exePath = KStandardDirs::findExe(exeName);
    if (!exePath.isEmpty()) {
        return exePath;
    }
    KProcess whereis;
    whereis << "whereis" << "-b" << exeName;
    whereis.setOutputChannelMode(KProcess::OnlyStdoutChannel);
    if (whereis.execute() == 0) {
        QString output = whereis.readAllStandardOutput();
        if (output.startsWith(exeName + ':')) {
            return output.mid(exeName.length() + 1).trimmed();
        }
    }
    return QString();
}
bool KCMGRUB2::check()
{
    installExePath = findExe("grub-install");
    mkconfigExePath = findExe("grub-mkconfig");
    probeExePath = findExe("grub-probe");
    set_defaultExePath = findExe("grub-set-default");
    if (!installExePath.isEmpty() && !mkconfigExePath.isEmpty() && !probeExePath.isEmpty() && !set_defaultExePath.isEmpty()) {
        kDebug() << "GRUB2 installation detected!";
        menuPath = "/boot/grub/grub.cfg";
        configPath = "/etc/default/grub";
        envPath = "/boot/grub/grubenv";
        memtestPath = "/etc/grub.d/20_memtest86+";
        return true;
    }
    installExePath = findExe("grub2-install");
    mkconfigExePath = findExe("grub2-mkconfig");
    probeExePath = findExe("grub2-probe");
    set_defaultExePath = findExe("grub2-set-default");
    if (!installExePath.isEmpty() && !mkconfigExePath.isEmpty() && !probeExePath.isEmpty() && !set_defaultExePath.isEmpty()) {
        kDebug() << "GRUB2 (RPM) installation detected!";
        menuPath = "/boot/grub2/grub.cfg";
        configPath = "/etc/default/grub";
        envPath = "/boot/grub2/grubenv";
        memtestPath = "/etc/grub.d/20_memtest86+";
        return true;
    }
    installExePath = findExe("burg-install");
    mkconfigExePath = findExe("burg-mkconfig");
    probeExePath = findExe("burg-probe");
    set_defaultExePath = findExe("burg-set-default");
    if (!installExePath.isEmpty() && !mkconfigExePath.isEmpty() && !probeExePath.isEmpty() && !set_defaultExePath.isEmpty()) {
        kDebug() << "BURG installation detected!";
        menuPath = "/boot/burg/burg.cfg";
        configPath = "/etc/default/burg";
        envPath = "/boot/burg/burgenv";
        memtestPath = "/etc/burg.d/20_memtest86+";
        return true;
    }
    return false;
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
    QHash<QString, QString>::const_iterator it = m_devices.constBegin();
    QHash<QString, QString>::const_iterator end = m_devices.constEnd();
    for (; it != end; ++it) {
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
#if KDE_IS_VERSION(4,6,0)
    loadAction.setParentWidget(this);
#endif

    ActionReply reply = loadAction.execute();
    if (reply.failed()) {
        kError() << "Error reading:" << fileName;
        kError() << "Error description:" << reply.data().value("output").toString();
        return QString();
    }
    return reply.data().value("fileContents").toString();
}
void KCMGRUB2::readEntries()
{
    QString fileContents = readFile(menuPath);

    m_entries.clear();
    m_kernels.clear();
    parseEntries(fileContents);
}
void KCMGRUB2::readSettings()
{
    QString fileContents = readFile(configPath);

    m_settings.clear();
    parseSettings(fileContents);
}
void KCMGRUB2::readEnv()
{
    QString fileContents = readFile(envPath);

    m_env.clear();
    parseEnv(fileContents);
}
void KCMGRUB2::readDevices()
{
    QStringList mountPoints;
    Q_FOREACH(const KMountPoint::Ptr mp, KMountPoint::currentMountPoints()) {
        if (mp->mountedFrom().startsWith(QLatin1String("/dev"))) {
            mountPoints.append(mp->mountPoint());
        }
    }

    Action probeAction("org.kde.kcontrol.kcmgrub2.probe");
    probeAction.setHelperID("org.kde.kcontrol.kcmgrub2");
    probeAction.addArgument("probeExePath", probeExePath);
    probeAction.addArgument("mountPoints", mountPoints);
#if KDE_IS_VERSION(4,6,0)
    probeAction.setParentWidget(this);
#endif
    if (probeAction.authorize() != Action::Authorized) {
        return;
    }

    KProgressDialog progressDlg(this, i18nc("@title:window", "Probing devices"), i18nc("@info:progress", "Probing devices for their GRUB names..."));
    progressDlg.setAllowCancel(false);
    progressDlg.setModal(true);
    progressDlg.show();
    connect(probeAction.watcher(), SIGNAL(progressStep(int)), progressDlg.progressBar(), SLOT(setValue(int)));

    ActionReply reply = probeAction.execute();
    if (reply.failed()) {
        KMessageBox::error(this, i18nc("@info", "Failed to get GRUB device names."));
        return;
    }
    QStringList grubPartitions = reply.data().value("grubPartitions").toStringList();
    if (mountPoints.size() != grubPartitions.size()) {
        KMessageBox::error(this, i18nc("@info", "Helper returned malformed device list."));
        return;
    }

    m_devices.clear();
    for (int i = 0; i < mountPoints.size(); i++) {
        m_devices[mountPoints.at(i)] = grubPartitions.at(i);
    }
}
void KCMGRUB2::readResolutions()
{
    Action probeVbeAction("org.kde.kcontrol.kcmgrub2.probevbe");
    probeVbeAction.setHelperID("org.kde.kcontrol.kcmgrub2");
#if KDE_IS_VERSION(4,6,0)
    probeVbeAction.setParentWidget(this);
#endif

    ActionReply reply = probeVbeAction.execute();
    if (reply.failed()) {
        return;
    }

    m_resolutions.clear();
    m_resolutions = reply.data().value("gfxmodes").toStringList();
}

void KCMGRUB2::sortResolutions()
{
    for (int i = 0; i < m_resolutions.size(); i++) {
        if (m_resolutions.at(i).contains(QRegExp("^\\d{3,4}x\\d{3,4}$"))) {
            m_resolutions[i] = QString("%1x%2x0").arg(m_resolutions.at(i).section('x', 0, 0).rightJustified(4, '0'), m_resolutions.at(i).section('x', 1).rightJustified(4, '0'));
        } else if (m_resolutions.at(i).contains(QRegExp("^\\d{3,4}x\\d{3,4}x\\d{1,2}$"))) {
            m_resolutions[i] = QString("%1x%2x%3").arg(m_resolutions.at(i).section('x', 0, 0).rightJustified(4, '0'), m_resolutions.at(i).section('x', 1, 1).rightJustified(4, '0'), m_resolutions.at(i).section('x', 2).rightJustified(2, '0'));
        }
    }
    m_resolutions.sort();
    for (int i = 0; i < m_resolutions.size(); i++) {
        if (!m_resolutions.at(i).contains(QRegExp("^\\d{3,4}x\\d{3,4}x\\d{1,2}$"))) {
            continue;
        }
        if (m_resolutions.at(i).startsWith('0')) {
            m_resolutions[i].remove(0, 1);
        }
        m_resolutions[i].replace("x0", "x");
        if (m_resolutions.at(i).endsWith('x')) {
            m_resolutions[i].remove(m_resolutions.at(i).length() - 1, 1);
        }
    }
}
void KCMGRUB2::showResolutions()
{
    ui.kcombobox_gfxmode->clear();
    ui.kcombobox_gfxmode->addItem(i18nc("@item:inlistbox Refers to screen resolution.", "Custom..."), "custom");

    ui.kcombobox_gfxpayload->clear();
    ui.kcombobox_gfxpayload->addItem(i18nc("@item:inlistbox Refers to screen resolution.", "Custom..."), "custom");
    ui.kcombobox_gfxpayload->addItem(i18nc("@item:inlistbox", "Unspecified"), QString());
    ui.kcombobox_gfxpayload->addItem(i18nc("@item:inlistbox", "Boot in Text Mode"), "text");
    ui.kcombobox_gfxpayload->addItem(i18nc("@item:inlistbox", "Keep GRUB's Resolution"), "keep");

    Q_FOREACH(const QString &resolution, m_resolutions) {
        ui.kcombobox_gfxmode->addItem(resolution, resolution);
        ui.kcombobox_gfxpayload->addItem(resolution, resolution);
    }
}

QString KCMGRUB2::quoteWord(const QString &word)
{
    return !word.startsWith('`') || !word.endsWith('`') ? KShell::quoteArg(word) : word;
}
QString KCMGRUB2::unquoteWord(const QString &word)
{
    KProcess echo(this);
    echo.setShellCommand(QString("echo -n %1").arg(word));
    echo.setOutputChannelMode(KProcess::OnlyStdoutChannel);
    if (echo.execute() == 0) {
        return QString::fromUtf8(echo.readAllStandardOutput());
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

void KCMGRUB2::parseEntries(const QString &config)
{
    QChar ch;
    QString word, entry, entriesConfig = config;
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
        stream >> word;
        if (word.compare("menuentry") == 0) {
            stream.skipWhiteSpace();
            if (stream.atEnd()) {
                return;
            }
            entry.clear();
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
            m_entries.append(entry);
        } else if (word.compare("linux") == 0 && !entry.isEmpty()) {
            stream.skipWhiteSpace();
            if (stream.atEnd()) {
                return;
            }
            stream >> word;
            m_kernels[unquoteWord(entry)] = word;
            entry.clear();
        }
    }
}
void KCMGRUB2::parseSettings(const QString &config)
{
    QString line, settingsConfig = config;
    QTextStream stream(&settingsConfig, QIODevice::ReadOnly);
    while (!stream.atEnd()) {
        line = stream.readLine().trimmed();
        if (line.startsWith(QLatin1String("GRUB_"))) {
            m_settings[line.section('=', 0, 0)] = line.section('=', 1);
        }
    }
}
void KCMGRUB2::parseEnv(const QString &config)
{
    QString line, settingsConfig = config;
    QTextStream stream(&settingsConfig, QIODevice::ReadOnly);
    while (!stream.atEnd()) {
        line = stream.readLine().trimmed();
        if (line.startsWith('#')) {
            continue;
        }
        m_env[line.section('=', 0, 0)] = line.section('=', 1);
    }
}
