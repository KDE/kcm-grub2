/*******************************************************************************
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

//Krazy
//krazy:excludeall=cpp

//Own
#include "kcm_grub2.h"

//Qt
#include <QDebug>
#include <QDesktopWidget>
#include <QStandardItemModel>
#include <QTreeView>
#include <QPushButton>
#include <QIcon>
#include <QMenu>
#include <QProgressDialog>

//KDE
#include <KLocalizedString>
#include <KAboutData>
#include <KMessageBox>
#include <KPluginFactory>
#include <KAuthAction>
#include <KAuthExecuteJob>

//Project
#include "common.h"
#include "config.h"
#if HAVE_IMAGEMAGICK
#include "convertDlg.h"
#endif
#include "entry.h"
#include "installDlg.h"
#if HAVE_QAPT || HAVE_QPACKAGEKIT
#include "removeDlg.h"
#endif
#include "textinputdialog.h"

//Ui
#include "ui_kcm_grub2.h"

K_PLUGIN_FACTORY(GRUB2Factory, registerPlugin<KCMGRUB2>();)
K_EXPORT_PLUGIN(GRUB2Factory("kcmgrub2"))

KCMGRUB2::KCMGRUB2(QWidget *parent, const QVariantList &list) : KCModule(parent, list)
{
    KAboutData *about = new KAboutData(QStringLiteral("kcmgrub2"), i18nc("@title", "KDE GRUB2 Bootloader Control Module"),
                                       QStringLiteral(KCM_GRUB2_VERSION), i18nc("@title", "A KDE Control Module for configuring the GRUB2 bootloader."),
                                       KAboutLicense::GPL_V3, i18nc("@info:credit", "Copyright (C) 2008-2013 Konstantinos Smanis"), QString(),
                                       QStringLiteral("http://ksmanis.wordpress.com/projects/grub2-editor/"));
    about->addAuthor(i18nc("@info:credit", "Κonstantinos Smanis"), i18nc("@info:credit", "Main Developer"),
                     QStringLiteral("konstantinos.smanis@gmail.com"), QStringLiteral("http://ksmanis.wordpress.com/"));
    setAboutData(about);

    ui = new Ui::KCMGRUB2;
    ui->setupUi(this);
    setupObjects();
    setupConnections();
}
KCMGRUB2::~KCMGRUB2()
{
    delete ui;
}

void KCMGRUB2::defaults()
{
    Action defaultsAction(QLatin1String("org.kde.kcontrol.kcmgrub2.defaults"));
    defaultsAction.setHelperId(QLatin1String("org.kde.kcontrol.kcmgrub2"));
    defaultsAction.setParentWidget(this);

    KAuth::ExecuteJob *defaultsJob = defaultsAction.execute();
    if (defaultsJob->exec()) {
        load();
        save();
        KMessageBox::information(this, i18nc("@info", "Successfully restored the default values."));
    } else {
        KMessageBox::detailedError(this, i18nc("@info", "Failed to restore the default values."), defaultsJob->errorText());
    }
}
void KCMGRUB2::load()
{
    readAll();

    QString grubDefault = unquoteWord(m_settings.value(QLatin1String("GRUB_DEFAULT")));
    if (grubDefault == QLatin1String("saved")) {
        grubDefault = (m_env.value(QLatin1String("saved_entry")).isEmpty() ? QString(QLatin1Char('0')) : m_env.value(QLatin1String("saved_entry")));
    }

    ui->combobox_default->clear();
    if (!m_entries.isEmpty()) {
        int maxLen = 0, maxLvl = 0;
        QStandardItemModel *model = new QStandardItemModel(ui->combobox_default);
        QTreeView *view = qobject_cast<QTreeView *>(ui->combobox_default->view());
        QList<QStandardItem *> ancestors;
        ancestors.append(model->invisibleRootItem());
        for (int i = 0; i < m_entries.size(); i++) {
            const Entry &entry = m_entries.at(i);
            const QString &prettyTitle = entry.prettyTitle();
            if (prettyTitle.length() > maxLen) {
                maxLen = prettyTitle.length();
            }
            if (entry.level() > maxLvl) {
                maxLvl = entry.level();
            }
            QStandardItem *item = new QStandardItem(prettyTitle);
            item->setData(entry.fullTitle());
            item->setSelectable(entry.type() == Entry::Menuentry);
            ancestors.last()->appendRow(item);
            if (i + 1 < m_entries.size()) {
                int n = m_entries.at(i + 1).level() - entry.level();
                if (n == 1) {
                    ancestors.append(item);
                } else if (n < 0) {
                    for (int j = 0; j > n; j--) {
                        ancestors.removeLast();
                    }
                }
            }
        }
        view->setModel(model);
        view->expandAll();
        ui->combobox_default->setModel(model);
        ui->combobox_default->setMinimumContentsLength(maxLen + maxLvl * 3);

        bool numericDefault = QRegExp(QLatin1String("((\\d)+>)*(\\d)+")).exactMatch(grubDefault);
        int entryIndex = -1;
        for (int i = 0; i < m_entries.size(); i++) {
            if ((numericDefault && m_entries.at(i).fullNumTitle() == grubDefault) || (!numericDefault && m_entries.at(i).fullTitle() == grubDefault)) {
                entryIndex = i;
                break;
            }
        }
        if (entryIndex != -1) {
            const Entry &entry = m_entries.at(entryIndex);
            if (entry.level() == 0) {
                ui->combobox_default->setCurrentIndex(entry.title().num);
            } else {
                QStandardItem *item = model->item(entry.ancestors().at(0).num);
                for (int i = 1; i < entry.level(); i++) {
                    item = item->child(entry.ancestors().at(i).num);
                }
                ui->combobox_default->setRootModelIndex(model->indexFromItem(item));
                ui->combobox_default->setCurrentIndex(entry.title().num);
                ui->combobox_default->setRootModelIndex(model->indexFromItem(model->invisibleRootItem()));
            }
        } else {
            qWarning() << "Invalid GRUB_DEFAULT value";
        }
    }
    ui->pushbutton_remove->setEnabled(!m_entries.isEmpty());
    ui->checkBox_savedefault->setChecked(unquoteWord(m_settings.value(QLatin1String("GRUB_SAVEDEFAULT"))).compare(QLatin1String("true")) == 0);

    bool ok;
    QString grubHiddenTimeoutRaw = unquoteWord(m_settings.value(QLatin1String("GRUB_HIDDEN_TIMEOUT")));
    if (grubHiddenTimeoutRaw.isEmpty()) {
        ui->checkBox_hiddenTimeout->setChecked(false);
    } else {
        int grubHiddenTimeout = grubHiddenTimeoutRaw.toInt(&ok);
        if (ok && grubHiddenTimeout >= 0) {
            ui->checkBox_hiddenTimeout->setChecked(true);
            ui->spinBox_hiddenTimeout->setValue(grubHiddenTimeout);
            ui->checkBox_hiddenTimeoutShowTimer->setChecked(unquoteWord(m_settings.value(QLatin1String("GRUB_HIDDEN_TIMEOUT_QUIET"))).compare(QLatin1String("true")) != 0);
        } else {
            qWarning() << "Invalid GRUB_HIDDEN_TIMEOUT value";
        }
    }
    int grubTimeout = (m_settings.value(QLatin1String("GRUB_TIMEOUT")).isEmpty() ? 5 : unquoteWord(m_settings.value(QLatin1String("GRUB_TIMEOUT"))).toInt(&ok));
    if (ok && grubTimeout >= -1) {
        ui->checkBox_timeout->setChecked(grubTimeout > -1);
        ui->radioButton_timeout0->setChecked(grubTimeout == 0);
        ui->radioButton_timeout->setChecked(grubTimeout > 0);
        ui->spinBox_timeout->setValue(grubTimeout);
    } else {
        qWarning() << "Invalid GRUB_TIMEOUT value";
    }

    showLocales();
    int languageIndex = ui->combobox_language->findData(unquoteWord(m_settings.value(QLatin1String("LANGUAGE"))));
    if (languageIndex != -1) {
        ui->combobox_language->setCurrentIndex(languageIndex);
    } else {
        qWarning() << "Invalid LANGUAGE value";
    }
    ui->checkBox_recovery->setChecked(unquoteWord(m_settings.value(QLatin1String("GRUB_DISABLE_RECOVERY"))).compare(QLatin1String("true")) != 0);
    ui->checkBox_memtest->setVisible(m_memtest);
    ui->checkBox_memtest->setChecked(m_memtestOn);
    ui->checkBox_osProber->setChecked(unquoteWord(m_settings.value(QLatin1String("GRUB_DISABLE_OS_PROBER"))).compare(QLatin1String("true")) != 0);

    QString grubGfxmode = unquoteWord(m_settings.value(QLatin1String("GRUB_GFXMODE")));
    if (grubGfxmode.isEmpty()) {
        grubGfxmode = QLatin1String("auto");
    }
    if (grubGfxmode != QLatin1String("auto") && !m_resolutions.contains(grubGfxmode)) {
        m_resolutions.append(grubGfxmode);
    }
    QString grubGfxpayloadLinux = unquoteWord(m_settings.value(QLatin1String("GRUB_GFXPAYLOAD_LINUX")));
    if (!grubGfxpayloadLinux.isEmpty() && grubGfxpayloadLinux != QLatin1String("text") && grubGfxpayloadLinux != QLatin1String("keep") && !m_resolutions.contains(grubGfxpayloadLinux)) {
        m_resolutions.append(grubGfxpayloadLinux);
    }
    m_resolutions.removeDuplicates();
    sortResolutions();
    showResolutions();
    ui->combobox_gfxmode->setCurrentIndex(ui->combobox_gfxmode->findData(grubGfxmode));
    ui->toolButton_refreshGfxmode->setVisible(HAVE_HD && m_resolutionsEmpty);
    ui->combobox_gfxpayload->setCurrentIndex(ui->combobox_gfxpayload->findData(grubGfxpayloadLinux));
    ui->toolButton_refreshGfxpayload->setVisible(HAVE_HD && m_resolutionsEmpty);

    QString grubColorNormal = unquoteWord(m_settings.value(QLatin1String("GRUB_COLOR_NORMAL")));
    if (!grubColorNormal.isEmpty()) {
        int normalForegroundIndex = ui->combobox_normalForeground->findData(grubColorNormal.section(QLatin1Char('/'), 0, 0));
        int normalBackgroundIndex = ui->combobox_normalBackground->findData(grubColorNormal.section(QLatin1Char('/'), 1));
        if (normalForegroundIndex == -1 || normalBackgroundIndex == -1) {
            qWarning() << "Invalid GRUB_COLOR_NORMAL value";
        }
        if (normalForegroundIndex != -1) {
            ui->combobox_normalForeground->setCurrentIndex(normalForegroundIndex);
        }
        if (normalBackgroundIndex != -1) {
            ui->combobox_normalBackground->setCurrentIndex(normalBackgroundIndex);
        }
    }
    QString grubColorHighlight = unquoteWord(m_settings.value(QLatin1String("GRUB_COLOR_HIGHLIGHT")));
    if (!grubColorHighlight.isEmpty()) {
        int highlightForegroundIndex = ui->combobox_highlightForeground->findData(grubColorHighlight.section(QLatin1Char('/'), 0, 0));
        int highlightBackgroundIndex = ui->combobox_highlightBackground->findData(grubColorHighlight.section(QLatin1Char('/'), 1));
        if (highlightForegroundIndex == -1 || highlightBackgroundIndex == -1) {
            qWarning() << "Invalid GRUB_COLOR_HIGHLIGHT value";
        }
        if (highlightForegroundIndex != -1) {
            ui->combobox_highlightForeground->setCurrentIndex(highlightForegroundIndex);
        }
        if (highlightBackgroundIndex != -1) {
            ui->combobox_highlightBackground->setCurrentIndex(highlightBackgroundIndex);
        }
    }

    QString grubBackground = unquoteWord(m_settings.value(QLatin1String("GRUB_BACKGROUND")));
    ui->kurlrequester_background->setText(grubBackground);
    ui->pushbutton_preview->setEnabled(!grubBackground.isEmpty());
    ui->kurlrequester_theme->setText(unquoteWord(m_settings.value(QLatin1String("GRUB_THEME"))));

    ui->lineedit_cmdlineDefault->setText(unquoteWord(m_settings.value(QLatin1String("GRUB_CMDLINE_LINUX_DEFAULT"))));
    ui->lineedit_cmdline->setText(unquoteWord(m_settings.value(QLatin1String("GRUB_CMDLINE_LINUX"))));

    QString grubTerminal = unquoteWord(m_settings.value(QLatin1String("GRUB_TERMINAL")));
    ui->lineedit_terminal->setText(grubTerminal);
    ui->lineedit_terminalInput->setReadOnly(!grubTerminal.isEmpty());
    ui->lineedit_terminalOutput->setReadOnly(!grubTerminal.isEmpty());
    ui->lineedit_terminalInput->setText(!grubTerminal.isEmpty() ? grubTerminal : unquoteWord(m_settings.value(QLatin1String("GRUB_TERMINAL_INPUT"))));
    ui->lineedit_terminalOutput->setText(!grubTerminal.isEmpty() ? grubTerminal : unquoteWord(m_settings.value(QLatin1String("GRUB_TERMINAL_OUTPUT"))));

    ui->lineedit_distributor->setText(unquoteWord(m_settings.value(QLatin1String("GRUB_DISTRIBUTOR"))));
    ui->lineedit_serial->setText(unquoteWord(m_settings.value(QLatin1String("GRUB_SERIAL_COMMAND"))));
    ui->lineedit_initTune->setText(unquoteWord(m_settings.value(QLatin1String("GRUB_INIT_TUNE"))));
    ui->checkBox_uuid->setChecked(unquoteWord(m_settings.value(QLatin1String("GRUB_DISABLE_LINUX_UUID"))).compare(QLatin1String("true")) != 0);

    m_dirtyBits.fill(0);
    Q_EMIT changed(false);
}
void KCMGRUB2::save()
{
    QString grubDefault;
    if (!m_entries.isEmpty()) {
        m_settings[QLatin1String("GRUB_DEFAULT")] = QLatin1String("saved");
        QStandardItemModel *model = qobject_cast<QStandardItemModel *>(ui->combobox_default->model());
        QTreeView *view = qobject_cast<QTreeView *>(ui->combobox_default->view());
        //Ugly, ugly hack. The view's current QModelIndex is invalidated
        //while the view is hidden and there is no access to the internal
        //QPersistentModelIndex (it is hidden in QComboBox's pimpl).
        //While the popup is shown, the QComboBox selects the current entry.
        //TODO: Maybe move away from the QComboBox+QTreeView implementation?
        ui->combobox_default->showPopup();
        grubDefault = model->itemFromIndex(view->currentIndex())->data().toString();
        ui->combobox_default->hidePopup();
    }
    if (m_dirtyBits.testBit(grubSavedefaultDirty)) {
        if (ui->checkBox_savedefault->isChecked()) {
            m_settings[QLatin1String("GRUB_SAVEDEFAULT")] = QLatin1String("true");
        } else {
            m_settings.remove(QLatin1String("GRUB_SAVEDEFAULT"));
        }
    }
    if (m_dirtyBits.testBit(grubHiddenTimeoutDirty)) {
        if (ui->checkBox_hiddenTimeout->isChecked()) {
            m_settings[QLatin1String("GRUB_HIDDEN_TIMEOUT")] = QString::number(ui->spinBox_hiddenTimeout->value());
        } else {
            m_settings.remove(QLatin1String("GRUB_HIDDEN_TIMEOUT"));
        }
    }
    if (m_dirtyBits.testBit(grubHiddenTimeoutQuietDirty)) {
        if (ui->checkBox_hiddenTimeoutShowTimer->isChecked()) {
            m_settings.remove(QLatin1String("GRUB_HIDDEN_TIMEOUT_QUIET"));
        } else {
            m_settings[QLatin1String("GRUB_HIDDEN_TIMEOUT_QUIET")] = QLatin1String("true");
        }
    }
    if (m_dirtyBits.testBit(grubTimeoutDirty)) {
        if (ui->checkBox_timeout->isChecked()) {
            if (ui->radioButton_timeout0->isChecked()) {
                m_settings[QLatin1String("GRUB_TIMEOUT")] = QLatin1Char('0');
            } else {
                m_settings[QLatin1String("GRUB_TIMEOUT")] = QString::number(ui->spinBox_timeout->value());
            }
        } else {
            m_settings[QLatin1String("GRUB_TIMEOUT")] = QLatin1String("-1");
        }
    }
    if (m_dirtyBits.testBit(grubLocaleDirty)) {
        int langIndex = ui->combobox_language->currentIndex();
        if (langIndex > 0) {
            m_settings[QLatin1String("LANGUAGE")] = ui->combobox_language->itemData(langIndex).toString();
        } else {
            m_settings.remove(QLatin1String("LANGUAGE"));
        }
    }
    if (m_dirtyBits.testBit(grubDisableRecoveryDirty)) {
        if (ui->checkBox_recovery->isChecked()) {
            m_settings.remove(QLatin1String("GRUB_DISABLE_RECOVERY"));
        } else {
            m_settings[QLatin1String("GRUB_DISABLE_RECOVERY")] = QLatin1String("true");
        }
    }
    if (m_dirtyBits.testBit(grubDisableOsProberDirty)) {
        if (ui->checkBox_osProber->isChecked()) {
            m_settings.remove(QLatin1String("GRUB_DISABLE_OS_PROBER"));
        } else {
            m_settings[QLatin1String("GRUB_DISABLE_OS_PROBER")] = QLatin1String("true");
        }
    }
    if (m_dirtyBits.testBit(grubGfxmodeDirty)) {
        if (ui->combobox_gfxmode->currentIndex() <= 0) {
            qCritical() << "Something went terribly wrong!";
        } else {
            m_settings[QLatin1String("GRUB_GFXMODE")] = quoteWord(ui->combobox_gfxmode->itemData(ui->combobox_gfxmode->currentIndex()).toString());
        }
    }
    if (m_dirtyBits.testBit(grubGfxpayloadLinuxDirty)) {
        if (ui->combobox_gfxpayload->currentIndex() <= 0) {
            qCritical() << "Something went terribly wrong!";
        } else if (ui->combobox_gfxpayload->currentIndex() == 1) {
            m_settings.remove(QLatin1String("GRUB_GFXPAYLOAD_LINUX"));
        } else if (ui->combobox_gfxpayload->currentIndex() > 1) {
            m_settings[QLatin1String("GRUB_GFXPAYLOAD_LINUX")] = quoteWord(ui->combobox_gfxpayload->itemData(ui->combobox_gfxpayload->currentIndex()).toString());
        }
    }
    if (m_dirtyBits.testBit(grubColorNormalDirty)) {
        QString normalForeground = ui->combobox_normalForeground->itemData(ui->combobox_normalForeground->currentIndex()).toString();
        QString normalBackground = ui->combobox_normalBackground->itemData(ui->combobox_normalBackground->currentIndex()).toString();
        if (normalForeground.compare(QLatin1String("light-gray")) != 0 || normalBackground.compare(QLatin1String("black")) != 0) {
            m_settings[QLatin1String("GRUB_COLOR_NORMAL")] = normalForeground + QLatin1Char('/') + normalBackground;
        } else {
            m_settings.remove(QLatin1String("GRUB_COLOR_NORMAL"));
        }
    }
    if (m_dirtyBits.testBit(grubColorHighlightDirty)) {
        QString highlightForeground = ui->combobox_highlightForeground->itemData(ui->combobox_highlightForeground->currentIndex()).toString();
        QString highlightBackground = ui->combobox_highlightBackground->itemData(ui->combobox_highlightBackground->currentIndex()).toString();
        if (highlightForeground.compare(QLatin1String("black")) != 0 || highlightBackground.compare(QLatin1String("light-gray")) != 0) {
            m_settings[QLatin1String("GRUB_COLOR_HIGHLIGHT")] = highlightForeground + QLatin1Char('/') + highlightBackground;
        } else {
            m_settings.remove(QLatin1String("GRUB_COLOR_HIGHLIGHT"));
        }
    }
    if (m_dirtyBits.testBit(grubBackgroundDirty)) {
        QString background = ui->kurlrequester_background->url().toLocalFile();
        if (!background.isEmpty()) {
            m_settings[QLatin1String("GRUB_BACKGROUND")] = quoteWord(background);
        } else {
            m_settings.remove(QLatin1String("GRUB_BACKGROUND"));
        }
    }
    if (m_dirtyBits.testBit(grubThemeDirty)) {
        QString theme = ui->kurlrequester_theme->url().toLocalFile();
        if (!theme.isEmpty()) {
            m_settings[QLatin1String("GRUB_THEME")] = quoteWord(theme);
        } else {
            m_settings.remove(QLatin1String("GRUB_THEME"));
        }
    }
    if (m_dirtyBits.testBit(grubCmdlineLinuxDefaultDirty)) {
        QString cmdlineLinuxDefault = ui->lineedit_cmdlineDefault->text();
        if (!cmdlineLinuxDefault.isEmpty()) {
            m_settings[QLatin1String("GRUB_CMDLINE_LINUX_DEFAULT")] = quoteWord(cmdlineLinuxDefault);
        } else {
            m_settings.remove(QLatin1String("GRUB_CMDLINE_LINUX_DEFAULT"));
        }
    }
    if (m_dirtyBits.testBit(grubCmdlineLinuxDirty)) {
        QString cmdlineLinux = ui->lineedit_cmdline->text();
        if (!cmdlineLinux.isEmpty()) {
            m_settings[QLatin1String("GRUB_CMDLINE_LINUX")] = quoteWord(cmdlineLinux);
        } else {
            m_settings.remove(QLatin1String("GRUB_CMDLINE_LINUX"));
        }
    }
    if (m_dirtyBits.testBit(grubTerminalDirty)) {
        QString terminal = ui->lineedit_terminal->text();
        if (!terminal.isEmpty()) {
            m_settings[QLatin1String("GRUB_TERMINAL")] = quoteWord(terminal);
        } else {
            m_settings.remove(QLatin1String("GRUB_TERMINAL"));
        }
    }
    if (m_dirtyBits.testBit(grubTerminalInputDirty)) {
        QString terminalInput = ui->lineedit_terminalInput->text();
        if (!terminalInput.isEmpty()) {
            m_settings[QLatin1String("GRUB_TERMINAL_INPUT")] = quoteWord(terminalInput);
        } else {
            m_settings.remove(QLatin1String("GRUB_TERMINAL_INPUT"));
        }
    }
    if (m_dirtyBits.testBit(grubTerminalOutputDirty)) {
        QString terminalOutput = ui->lineedit_terminalOutput->text();
        if (!terminalOutput.isEmpty()) {
            m_settings[QLatin1String("GRUB_TERMINAL_OUTPUT")] = quoteWord(terminalOutput);
        } else {
            m_settings.remove(QLatin1String("GRUB_TERMINAL_OUTPUT"));
        }
    }
    if (m_dirtyBits.testBit(grubDistributorDirty)) {
        QString distributor = ui->lineedit_distributor->text();
        if (!distributor.isEmpty()) {
            m_settings[QLatin1String("GRUB_DISTRIBUTOR")] = quoteWord(distributor);
        } else {
            m_settings.remove(QLatin1String("GRUB_DISTRIBUTOR"));
        }
    }
    if (m_dirtyBits.testBit(grubSerialCommandDirty)) {
        QString serialCommand = ui->lineedit_serial->text();
        if (!serialCommand.isEmpty()) {
            m_settings[QLatin1String("GRUB_SERIAL_COMMAND")] = quoteWord(serialCommand);
        } else {
            m_settings.remove(QLatin1String("GRUB_SERIAL_COMMAND"));
        }
    }
    if (m_dirtyBits.testBit(grubInitTuneDirty)) {
        QString initTune = ui->lineedit_initTune->text();
        if (!initTune.isEmpty()) {
            m_settings[QLatin1String("GRUB_INIT_TUNE")] = quoteWord(initTune);
        } else {
            m_settings.remove(QLatin1String("GRUB_INIT_TUNE"));
        }
    }
    if (m_dirtyBits.testBit(grubDisableLinuxUuidDirty)) {
        if (ui->checkBox_uuid->isChecked()) {
            m_settings.remove(QLatin1String("GRUB_DISABLE_LINUX_UUID"));
        } else {
            m_settings[QLatin1String("GRUB_DISABLE_LINUX_UUID")] = QLatin1String("true");
        }
    }

    QString configFileContents;
    QTextStream stream(&configFileContents, QIODevice::WriteOnly | QIODevice::Text);
    QHash<QString, QString>::const_iterator it = m_settings.constBegin();
    QHash<QString, QString>::const_iterator end = m_settings.constEnd();
    for (; it != end; ++it) {
        stream << it.key() << '=' << it.value() << endl;
    }

    Action saveAction(QLatin1String("org.kde.kcontrol.kcmgrub2.save"));
    saveAction.setHelperId(QLatin1String("org.kde.kcontrol.kcmgrub2"));
    saveAction.addArgument(QLatin1String("rawConfigFileContents"), configFileContents.toUtf8());
    saveAction.addArgument(QLatin1String("rawDefaultEntry"), !m_entries.isEmpty() ? grubDefault.toUtf8() : m_settings.value(QLatin1String("GRUB_DEFAULT")).toUtf8());
    if (ui->combobox_language->currentIndex() > 0) {
        saveAction.addArgument(QLatin1String("LANG"), qgetenv("LANG"));
        saveAction.addArgument(QLatin1String("LANGUAGE"), m_settings.value(QLatin1String("LANGUAGE")));
    }
    if (m_dirtyBits.testBit(memtestDirty)) {
        saveAction.addArgument(QLatin1String("memtest"), ui->checkBox_memtest->isChecked());
    }
    saveAction.setParentWidget(this);
    saveAction.setTimeout(60000);

    KAuth::ExecuteJob *saveJob = saveAction.execute(KAuth::Action::AuthorizeOnlyMode);
    if (!saveJob->exec()) {
        return;
    }
    saveJob = saveAction.execute();

    QProgressDialog progressDlg(this);
    progressDlg.setWindowTitle(i18nc("@title:window Verb (gerund). Refers to current status.", "Saving"));
    progressDlg.setLabelText(i18nc("@info:progress", "Saving GRUB settings..."));
    progressDlg.setCancelButton(nullptr);
    progressDlg.setModal(true);
    progressDlg.setMinimum(0);
    progressDlg.setMaximum(0);
    progressDlg.show();
    connect(saveJob, SIGNAL(finished(KJob*)), &progressDlg, SLOT(hide()));

    if (saveJob->exec()) {
        QDialog *dialog = new QDialog(this);
        dialog->setWindowTitle(i18nc("@title:window", "Information"));
        dialog->setModal(true);
        dialog->setAttribute(Qt::WA_DeleteOnClose);

        QPushButton *detailsButton = new QPushButton;
        detailsButton->setObjectName(QStringLiteral("detailsButton"));
        detailsButton->setText(QApplication::translate("KMessageBox", "&Details") + QStringLiteral(" >>"));
        detailsButton->setIcon(QIcon::fromTheme(QStringLiteral("help-about")));

        QDialogButtonBox *buttonBox = new QDialogButtonBox(dialog);
        buttonBox->addButton(detailsButton, QDialogButtonBox::HelpRole);
        buttonBox->addButton(QDialogButtonBox::Ok);
        buttonBox->button(QDialogButtonBox::Ok)->setFocus();

        KMessageBox::createKMessageBox(dialog, buttonBox, QMessageBox::Information, i18nc("@info", "Successfully saved GRUB settings."),
                                       QStringList(), QString(), nullptr, KMessageBox::Notify,
                                       QString::fromUtf8(saveJob->data().value(QLatin1String("output")).toByteArray().constData())); // krazy:exclude=qclasses
        load();
    } else {
        KMessageBox::detailedError(this, i18nc("@info", "Failed to save GRUB settings."), saveJob->errorText());
    }
}

void KCMGRUB2::slotRemoveOldEntries()
{
#if HAVE_QAPT || HAVE_QPACKAGEKIT
    QPointer<RemoveDialog> removeDlg = new RemoveDialog(m_entries, this);
    if (removeDlg->exec()) {
        load();
    }
    delete removeDlg;
#endif
}
void KCMGRUB2::slotGrubSavedefaultChanged()
{
    m_dirtyBits.setBit(grubSavedefaultDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubHiddenTimeoutToggled(bool checked)
{
    ui->spinBox_hiddenTimeout->setEnabled(checked);
    ui->checkBox_hiddenTimeoutShowTimer->setEnabled(checked);
}
void KCMGRUB2::slotGrubHiddenTimeoutChanged()
{
    m_dirtyBits.setBit(grubHiddenTimeoutDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubHiddenTimeoutQuietChanged()
{
    m_dirtyBits.setBit(grubHiddenTimeoutQuietDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubTimeoutToggled(bool checked)
{
    ui->radioButton_timeout0->setEnabled(checked);
    ui->radioButton_timeout->setEnabled(checked);
    ui->spinBox_timeout->setEnabled(checked && ui->radioButton_timeout->isChecked());
}
void KCMGRUB2::slotGrubTimeoutChanged()
{
    m_dirtyBits.setBit(grubTimeoutDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubLanguageChanged()
{
    m_dirtyBits.setBit(grubLocaleDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubDisableRecoveryChanged()
{
    m_dirtyBits.setBit(grubDisableRecoveryDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotMemtestChanged()
{
    m_dirtyBits.setBit(memtestDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubDisableOsProberChanged()
{
    m_dirtyBits.setBit(grubDisableOsProberDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubGfxmodeChanged()
{
    if (ui->combobox_gfxmode->currentIndex() == 0) {
        bool ok;
        QRegExpValidator regExp(QRegExp(QLatin1String("\\d{3,4}x\\d{3,4}(x\\d{1,2})?")), this);
        QString resolution = TextInputDialog::getText(this, i18nc("@title:window", "Enter screen resolution"), i18nc("@label:textbox", "Please enter a GRUB resolution:"), QString(), &regExp, &ok);
        if (ok) {
            if (!m_resolutions.contains(resolution)) {
                QString gfxpayload = ui->combobox_gfxpayload->itemData(ui->combobox_gfxpayload->currentIndex()).toString();
                m_resolutions.append(resolution);
                sortResolutions();
                showResolutions();
                ui->combobox_gfxpayload->setCurrentIndex(ui->combobox_gfxpayload->findData(gfxpayload));
            }
            ui->combobox_gfxmode->setCurrentIndex(ui->combobox_gfxmode->findData(resolution));
        } else {
            ui->combobox_gfxmode->setCurrentIndex(ui->combobox_gfxmode->findData(QLatin1String("640x480")));
        }
    }
    m_dirtyBits.setBit(grubGfxmodeDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubGfxpayloadLinuxChanged()
{
    if (ui->combobox_gfxpayload->currentIndex() == 0) {
        bool ok;
        QRegExpValidator regExp(QRegExp(QLatin1String("\\d{3,4}x\\d{3,4}(x\\d{1,2})?")), this);
        QString resolution = TextInputDialog::getText(this, i18nc("@title:window", "Enter screen resolution"), i18nc("@label:textbox", "Please enter a Linux boot resolution:"), QString(), &regExp, &ok);
        if (ok) {
            if (!m_resolutions.contains(resolution)) {
                QString gfxmode = ui->combobox_gfxmode->itemData(ui->combobox_gfxmode->currentIndex()).toString();
                m_resolutions.append(resolution);
                sortResolutions();
                showResolutions();
                ui->combobox_gfxmode->setCurrentIndex(ui->combobox_gfxmode->findData(gfxmode));
            }
            ui->combobox_gfxpayload->setCurrentIndex(ui->combobox_gfxpayload->findData(resolution));
        } else {
            ui->combobox_gfxpayload->setCurrentIndex(ui->combobox_gfxpayload->findData(QString()));
        }
    }
    m_dirtyBits.setBit(grubGfxpayloadLinuxDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotResolutionsRefresh()
{
    m_resolutionsForceRead = true;
    load();
}
void KCMGRUB2::slotGrubColorNormalChanged()
{
    m_dirtyBits.setBit(grubColorNormalDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubColorHighlightChanged()
{
    m_dirtyBits.setBit(grubColorHighlightDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slowGrubBackgroundChanged()
{
    ui->pushbutton_preview->setEnabled(!ui->kurlrequester_background->text().isEmpty());
    m_dirtyBits.setBit(grubBackgroundDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotPreviewGrubBackground()
{
    QFile file(ui->kurlrequester_background->url().toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    //TODO: Need something more elegant.
    QDialog *dialog = new QDialog(this);
    QLabel *label = new QLabel(dialog);
    label->setPixmap(QPixmap::fromImage(QImage::fromData(file.readAll())).scaled(QDesktopWidget().screenGeometry(this).size()));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->showFullScreen();
    KMessageBox::information(dialog, i18nc("@info", "Press <shortcut>Escape</shortcut> to exit fullscreen mode."), QString(), QLatin1String("GRUBFullscreenPreview"));
}
void KCMGRUB2::slotCreateGrubBackground()
{
#if HAVE_IMAGEMAGICK
    QPointer<ConvertDialog> convertDlg = new ConvertDialog(this);
    QString resolution = ui->combobox_gfxmode->itemData(ui->combobox_gfxmode->currentIndex()).toString();
    convertDlg->setResolution(resolution.section(QLatin1Char('x'), 0, 0).toInt(), resolution.section(QLatin1Char('x'), 1, 1).toInt());
    connect(convertDlg, SIGNAL(splashImageCreated(QString)), ui->kurlrequester_background, SLOT(setText(QString)));
    convertDlg->exec();
    delete convertDlg;
#endif
}
void KCMGRUB2::slotGrubThemeChanged()
{
    m_dirtyBits.setBit(grubThemeDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubCmdlineLinuxDefaultChanged()
{
    m_dirtyBits.setBit(grubCmdlineLinuxDefaultDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubCmdlineLinuxChanged()
{
    m_dirtyBits.setBit(grubCmdlineLinuxDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubTerminalChanged()
{
    QString grubTerminal = ui->lineedit_terminal->text();
    ui->lineedit_terminalInput->setReadOnly(!grubTerminal.isEmpty());
    ui->lineedit_terminalOutput->setReadOnly(!grubTerminal.isEmpty());
    ui->lineedit_terminalInput->setText(!grubTerminal.isEmpty() ? grubTerminal : unquoteWord(m_settings.value(QLatin1String("GRUB_TERMINAL_INPUT"))));
    ui->lineedit_terminalOutput->setText(!grubTerminal.isEmpty() ? grubTerminal : unquoteWord(m_settings.value(QLatin1String("GRUB_TERMINAL_OUTPUT"))));
    m_dirtyBits.setBit(grubTerminalDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubTerminalInputChanged()
{
    m_dirtyBits.setBit(grubTerminalInputDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubTerminalOutputChanged()
{
    m_dirtyBits.setBit(grubTerminalOutputDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubDistributorChanged()
{
    m_dirtyBits.setBit(grubDistributorDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubSerialCommandChanged()
{
    m_dirtyBits.setBit(grubSerialCommandDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubInitTuneChanged()
{
    m_dirtyBits.setBit(grubInitTuneDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotGrubDisableLinuxUuidChanged()
{
    m_dirtyBits.setBit(grubDisableLinuxUuidDirty);
    Q_EMIT changed(true);
}
void KCMGRUB2::slotInstallBootloader()
{
    QPointer<InstallDialog> installDlg = new InstallDialog(this);
    installDlg->exec();
    delete installDlg;
}

void KCMGRUB2::slotUpdateSuggestions()
{
    if (!sender()->isWidgetType()) {
        return;
    }

    QLineEdit *lineEdit = nullptr;
    if (ui->pushbutton_cmdlineDefaultSuggestions->isDown()) {
        lineEdit = ui->lineedit_cmdlineDefault;
    } else if (ui->pushbutton_cmdlineSuggestions->isDown()) {
        lineEdit = ui->lineedit_cmdline;
    } else if (ui->pushbutton_terminalSuggestions->isDown()) {
        lineEdit = ui->lineedit_terminal;
    } else if (ui->pushbutton_terminalInputSuggestions->isDown()) {
        lineEdit = ui->lineedit_terminalInput;
    } else if (ui->pushbutton_terminalOutputSuggestions->isDown()) {
        lineEdit = ui->lineedit_terminalOutput;
    } else {
        return;
    }

    Q_FOREACH(QAction *action, qobject_cast<const QWidget*>(sender())->actions()) {
        if (!action->isCheckable()) {
            action->setCheckable(true);
        }
        action->setChecked(lineEdit->text().contains(QRegExp(QString(QLatin1String("\\b%1\\b")).arg(action->data().toString()))));
    }
}
void KCMGRUB2::slotTriggeredSuggestion(QAction *action)
{
    QLineEdit *lineEdit = nullptr;
    void (KCMGRUB2::*updateFunction)() = 0;
    if (ui->pushbutton_cmdlineDefaultSuggestions->isDown()) {
        lineEdit = ui->lineedit_cmdlineDefault;
        updateFunction = &KCMGRUB2::slotGrubCmdlineLinuxDefaultChanged;
    } else if (ui->pushbutton_cmdlineSuggestions->isDown()) {
        lineEdit = ui->lineedit_cmdline;
        updateFunction = &KCMGRUB2::slotGrubCmdlineLinuxChanged;
    } else if (ui->pushbutton_terminalSuggestions->isDown()) {
        lineEdit = ui->lineedit_terminal;
        updateFunction = &KCMGRUB2::slotGrubTerminalChanged;
    } else if (ui->pushbutton_terminalInputSuggestions->isDown()) {
        lineEdit = ui->lineedit_terminalInput;
        updateFunction = &KCMGRUB2::slotGrubTerminalInputChanged;
    } else if (ui->pushbutton_terminalOutputSuggestions->isDown()) {
        lineEdit = ui->lineedit_terminalOutput;
        updateFunction = &KCMGRUB2::slotGrubTerminalOutputChanged;
    } else {
        return;
    }

    QString lineEditText = lineEdit->text();
    if (!action->isChecked()) {
        lineEdit->setText(lineEditText.remove(QRegExp(QString(QLatin1String("\\b%1\\b")).arg(action->data().toString()))).simplified());
    } else {
        lineEdit->setText(lineEditText.isEmpty() ? action->data().toString() : lineEditText + QLatin1Char(' ') + action->data().toString());
    }
    (this->*updateFunction)();
}

void KCMGRUB2::setupObjects()
{
    setButtons(Default | Apply);
    setNeedsAuthorization(true);

    m_dirtyBits.resize(lastDirtyBit);
    m_resolutionsEmpty = true;
    m_resolutionsForceRead = false;

    QTreeView *view = new QTreeView(ui->combobox_default);
    view->setHeaderHidden(true);
    view->setItemsExpandable(false);
    view->setRootIsDecorated(false);
    ui->combobox_default->setView(view);

    ui->pushbutton_remove->setIcon(QIcon::fromTheme(QLatin1String("list-remove")));
    ui->pushbutton_remove->setVisible(HAVE_QAPT || HAVE_QPACKAGEKIT);

    ui->toolButton_refreshGfxmode->setIcon(QIcon::fromTheme(QLatin1String("view-refresh")));
    ui->toolButton_refreshGfxpayload->setIcon(QIcon::fromTheme(QLatin1String("view-refresh")));

    QPixmap black(16, 16), transparent(16, 16);
    black.fill(Qt::black);
    transparent.fill(Qt::transparent);
    ui->combobox_normalForeground->addItem(QIcon(black), i18nc("@item:inlistbox Refers to color.", "Black"), QLatin1String("black"));
    ui->combobox_highlightForeground->addItem(QIcon(black), i18nc("@item:inlistbox Refers to color.", "Black"), QLatin1String("black"));
    ui->combobox_normalBackground->addItem(QIcon(transparent), i18nc("@item:inlistbox Refers to color.", "Transparent"), QLatin1String("black"));
    ui->combobox_highlightBackground->addItem(QIcon(transparent), i18nc("@item:inlistbox Refers to color.", "Transparent"), QLatin1String("black"));
    QHash<QString, QString> colors;
    colors.insertMulti(QLatin1String("blue"), i18nc("@item:inlistbox Refers to color.", "Blue"));
    colors.insertMulti(QLatin1String("blue"), QLatin1String("blue"));
    colors.insertMulti(QLatin1String("cyan"), i18nc("@item:inlistbox Refers to color.", "Cyan"));
    colors.insertMulti(QLatin1String("cyan"), QLatin1String("cyan"));
    colors.insertMulti(QLatin1String("dark-gray"), i18nc("@item:inlistbox Refers to color.", "Dark Gray"));
    colors.insertMulti(QLatin1String("dark-gray"), QLatin1String("darkgray"));
    colors.insertMulti(QLatin1String("green"), i18nc("@item:inlistbox Refers to color.", "Green"));
    colors.insertMulti(QLatin1String("green"), QLatin1String("green"));
    colors.insertMulti(QLatin1String("light-cyan"), i18nc("@item:inlistbox Refers to color.", "Light Cyan"));
    colors.insertMulti(QLatin1String("light-cyan"), QLatin1String("lightcyan"));
    colors.insertMulti(QLatin1String("light-blue"), i18nc("@item:inlistbox Refers to color.", "Light Blue"));
    colors.insertMulti(QLatin1String("light-blue"), QLatin1String("lightblue"));
    colors.insertMulti(QLatin1String("light-green"), i18nc("@item:inlistbox Refers to color.", "Light Green"));
    colors.insertMulti(QLatin1String("light-green"), QLatin1String("lightgreen"));
    colors.insertMulti(QLatin1String("light-gray"), i18nc("@item:inlistbox Refers to color.", "Light Gray"));
    colors.insertMulti(QLatin1String("light-gray"), QLatin1String("lightgray"));
    colors.insertMulti(QLatin1String("light-magenta"), i18nc("@item:inlistbox Refers to color.", "Light Magenta"));
    colors.insertMulti(QLatin1String("light-magenta"), QLatin1String("magenta"));
    colors.insertMulti(QLatin1String("light-red"), i18nc("@item:inlistbox Refers to color.", "Light Red"));
    colors.insertMulti(QLatin1String("light-red"), QLatin1String("orangered"));
    colors.insertMulti(QLatin1String("magenta"), i18nc("@item:inlistbox Refers to color.", "Magenta"));
    colors.insertMulti(QLatin1String("magenta"), QLatin1String("darkmagenta"));
    colors.insertMulti(QLatin1String("red"), i18nc("@item:inlistbox Refers to color.", "Red"));
    colors.insertMulti(QLatin1String("red"), QLatin1String("red"));
    colors.insertMulti(QLatin1String("white"), i18nc("@item:inlistbox Refers to color.", "White"));
    colors.insertMulti(QLatin1String("white"), QLatin1String("white"));
    colors.insertMulti(QLatin1String("yellow"), i18nc("@item:inlistbox Refers to color.", "Yellow"));
    colors.insertMulti(QLatin1String("yellow"), QLatin1String("yellow"));
    QHash<QString, QString>::const_iterator it = colors.constBegin();
    QHash<QString, QString>::const_iterator end = colors.constEnd();
    for (; it != end; it += 2) {
        QPixmap color(16, 16);
        color.fill(QColor(colors.values(it.key()).at(0)));
        ui->combobox_normalForeground->addItem(QIcon(color), colors.values(it.key()).at(1), it.key());
        ui->combobox_highlightForeground->addItem(QIcon(color), colors.values(it.key()).at(1), it.key());
        ui->combobox_normalBackground->addItem(QIcon(color), colors.values(it.key()).at(1), it.key());
        ui->combobox_highlightBackground->addItem(QIcon(color), colors.values(it.key()).at(1), it.key());
    }
    ui->combobox_normalForeground->setCurrentIndex(ui->combobox_normalForeground->findData(QLatin1String("light-gray")));
    ui->combobox_normalBackground->setCurrentIndex(ui->combobox_normalBackground->findData(QLatin1String("black")));
    ui->combobox_highlightForeground->setCurrentIndex(ui->combobox_highlightForeground->findData(QLatin1String("black")));
    ui->combobox_highlightBackground->setCurrentIndex(ui->combobox_highlightBackground->findData(QLatin1String("light-gray")));

    ui->pushbutton_preview->setIcon(QIcon::fromTheme(QLatin1String("image-png")));
    ui->pushbutton_create->setIcon(QIcon::fromTheme(QLatin1String("insert-image")));
    ui->pushbutton_create->setVisible(HAVE_IMAGEMAGICK);

    ui->pushbutton_cmdlineDefaultSuggestions->setIcon(QIcon::fromTheme(QLatin1String("tools-wizard")));
    ui->pushbutton_cmdlineDefaultSuggestions->setMenu(new QMenu(ui->pushbutton_cmdlineDefaultSuggestions));
    ui->pushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Quiet Boot"))->setData(QLatin1String("quiet"));
    ui->pushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Show Splash Screen"))->setData(QLatin1String("splash"));
    ui->pushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Disable Plymouth"))->setData(QLatin1String("noplymouth"));
    ui->pushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off ACPI"))->setData(QLatin1String("acpi=off"));
    ui->pushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off APIC"))->setData(QLatin1String("noapic"));
    ui->pushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off Local APIC"))->setData(QLatin1String("nolapic"));
    ui->pushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Single User Mode"))->setData(QLatin1String("single"));
    ui->pushbutton_cmdlineSuggestions->setIcon(QIcon::fromTheme(QLatin1String("tools-wizard")));
    ui->pushbutton_cmdlineSuggestions->setMenu(new QMenu(ui->pushbutton_cmdlineSuggestions));
    ui->pushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Quiet Boot"))->setData(QLatin1String("quiet"));
    ui->pushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Show Splash Screen"))->setData(QLatin1String("splash"));
    ui->pushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Disable Plymouth"))->setData(QLatin1String("noplymouth"));
    ui->pushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off ACPI"))->setData(QLatin1String("acpi=off"));
    ui->pushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off APIC"))->setData(QLatin1String("noapic"));
    ui->pushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off Local APIC"))->setData(QLatin1String("nolapic"));
    ui->pushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Single User Mode"))->setData(QLatin1String("single"));
    ui->pushbutton_terminalSuggestions->setIcon(QIcon::fromTheme(QLatin1String("tools-wizard")));
    ui->pushbutton_terminalSuggestions->setMenu(new QMenu(ui->pushbutton_terminalSuggestions));
    ui->pushbutton_terminalSuggestions->menu()->addAction(i18nc("@action:inmenu", "PC BIOS && EFI Console"))->setData(QLatin1String("console"));
    ui->pushbutton_terminalSuggestions->menu()->addAction(i18nc("@action:inmenu", "Serial Terminal"))->setData(QLatin1String("serial"));
    ui->pushbutton_terminalSuggestions->menu()->addAction(i18nc("@action:inmenu 'Open' is an adjective here, not a verb. 'Open Firmware' is a former IEEE standard.", "Open Firmware Console"))->setData(QLatin1String("ofconsole"));
    ui->pushbutton_terminalInputSuggestions->setIcon(QIcon::fromTheme(QLatin1String("tools-wizard")));
    ui->pushbutton_terminalInputSuggestions->setMenu(new QMenu(ui->pushbutton_terminalInputSuggestions));
    ui->pushbutton_terminalInputSuggestions->menu()->addAction(i18nc("@action:inmenu", "PC BIOS && EFI Console"))->setData(QLatin1String("console"));
    ui->pushbutton_terminalInputSuggestions->menu()->addAction(i18nc("@action:inmenu", "Serial Terminal"))->setData(QLatin1String("serial"));
    ui->pushbutton_terminalInputSuggestions->menu()->addAction(i18nc("@action:inmenu 'Open' is an adjective here, not a verb. 'Open Firmware' is a former IEEE standard.", "Open Firmware Console"))->setData(QLatin1String("ofconsole"));
    ui->pushbutton_terminalInputSuggestions->menu()->addAction(i18nc("@action:inmenu", "PC AT Keyboard (Coreboot)"))->setData(QLatin1String("at_keyboard"));
    ui->pushbutton_terminalInputSuggestions->menu()->addAction(i18nc("@action:inmenu", "USB Keyboard (HID Boot Protocol)"))->setData(QLatin1String("usb_keyboard"));
    ui->pushbutton_terminalOutputSuggestions->setIcon(QIcon::fromTheme(QLatin1String("tools-wizard")));
    ui->pushbutton_terminalOutputSuggestions->setMenu(new QMenu(ui->pushbutton_terminalOutputSuggestions));
    ui->pushbutton_terminalOutputSuggestions->menu()->addAction(i18nc("@action:inmenu", "PC BIOS && EFI Console"))->setData(QLatin1String("console"));
    ui->pushbutton_terminalOutputSuggestions->menu()->addAction(i18nc("@action:inmenu", "Serial Terminal"))->setData(QLatin1String("serial"));
    ui->pushbutton_terminalOutputSuggestions->menu()->addAction(i18nc("@action:inmenu 'Open' is an adjective here, not a verb. 'Open Firmware' is a former IEEE standard.", "Open Firmware Console"))->setData(QLatin1String("ofconsole"));
    ui->pushbutton_terminalOutputSuggestions->menu()->addAction(i18nc("@action:inmenu", "Graphics Mode Output"))->setData(QLatin1String("gfxterm"));
    ui->pushbutton_terminalOutputSuggestions->menu()->addAction(i18nc("@action:inmenu", "VGA Text Output (Coreboot)"))->setData(QLatin1String("vga_text"));

    ui->pushbutton_install->setIcon(QIcon::fromTheme(QLatin1String("system-software-update")));
}
void KCMGRUB2::setupConnections()
{
    connect(ui->combobox_default, SIGNAL(activated(int)), this, SLOT(changed()));
    connect(ui->pushbutton_remove, SIGNAL(clicked(bool)), this, SLOT(slotRemoveOldEntries()));
    connect(ui->checkBox_savedefault, SIGNAL(clicked(bool)), this, SLOT(slotGrubSavedefaultChanged()));

    connect(ui->checkBox_hiddenTimeout, SIGNAL(toggled(bool)), this, SLOT(slotGrubHiddenTimeoutToggled(bool)));
    connect(ui->checkBox_hiddenTimeout, SIGNAL(clicked(bool)), this, SLOT(slotGrubHiddenTimeoutChanged()));
    connect(ui->spinBox_hiddenTimeout, SIGNAL(valueChanged(int)), this, SLOT(slotGrubHiddenTimeoutChanged()));
    connect(ui->checkBox_hiddenTimeoutShowTimer, SIGNAL(clicked(bool)), this, SLOT(slotGrubHiddenTimeoutQuietChanged()));

    connect(ui->checkBox_timeout, SIGNAL(toggled(bool)), this, SLOT(slotGrubTimeoutToggled(bool)));
    connect(ui->checkBox_timeout, SIGNAL(clicked(bool)), this, SLOT(slotGrubTimeoutChanged()));
    connect(ui->radioButton_timeout0, SIGNAL(clicked(bool)), this, SLOT(slotGrubTimeoutChanged()));
    connect(ui->radioButton_timeout, SIGNAL(toggled(bool)), ui->spinBox_timeout, SLOT(setEnabled(bool)));
    connect(ui->radioButton_timeout, SIGNAL(clicked(bool)), this, SLOT(slotGrubTimeoutChanged()));
    connect(ui->spinBox_timeout, SIGNAL(valueChanged(int)), this, SLOT(slotGrubTimeoutChanged()));

    connect(ui->combobox_language, SIGNAL(activated(int)), this, SLOT(slotGrubLanguageChanged()));
    connect(ui->checkBox_recovery, SIGNAL(clicked(bool)), this, SLOT(slotGrubDisableRecoveryChanged()));
    connect(ui->checkBox_memtest, SIGNAL(clicked(bool)), this, SLOT(slotMemtestChanged()));
    connect(ui->checkBox_osProber, SIGNAL(clicked(bool)), this, SLOT(slotGrubDisableOsProberChanged()));

    connect(ui->combobox_gfxmode, SIGNAL(activated(int)), this, SLOT(slotGrubGfxmodeChanged()));
    connect(ui->toolButton_refreshGfxmode, SIGNAL(clicked(bool)), this, SLOT(slotResolutionsRefresh()));
    connect(ui->combobox_gfxpayload, SIGNAL(activated(int)), this, SLOT(slotGrubGfxpayloadLinuxChanged()));
    connect(ui->toolButton_refreshGfxpayload, SIGNAL(clicked(bool)), this, SLOT(slotResolutionsRefresh()));

    connect(ui->combobox_normalForeground, SIGNAL(activated(int)), this, SLOT(slotGrubColorNormalChanged()));
    connect(ui->combobox_normalBackground, SIGNAL(activated(int)), this, SLOT(slotGrubColorNormalChanged()));
    connect(ui->combobox_highlightForeground, SIGNAL(activated(int)), this, SLOT(slotGrubColorHighlightChanged()));
    connect(ui->combobox_highlightBackground, SIGNAL(activated(int)), this, SLOT(slotGrubColorHighlightChanged()));

    connect(ui->kurlrequester_background, SIGNAL(textChanged(QString)), this, SLOT(slowGrubBackgroundChanged()));
    connect(ui->pushbutton_preview, SIGNAL(clicked(bool)), this, SLOT(slotPreviewGrubBackground()));
    connect(ui->pushbutton_create, SIGNAL(clicked(bool)), this, SLOT(slotCreateGrubBackground()));
    connect(ui->kurlrequester_theme, SIGNAL(textChanged(QString)), this, SLOT(slotGrubThemeChanged()));

    connect(ui->lineedit_cmdlineDefault, SIGNAL(textEdited(QString)), this, SLOT(slotGrubCmdlineLinuxDefaultChanged()));
    connect(ui->pushbutton_cmdlineDefaultSuggestions->menu(), SIGNAL(aboutToShow()), this, SLOT(slotUpdateSuggestions()));
    connect(ui->pushbutton_cmdlineDefaultSuggestions->menu(), SIGNAL(triggered(QAction*)), this, SLOT(slotTriggeredSuggestion(QAction*)));
    connect(ui->lineedit_cmdline, SIGNAL(textEdited(QString)), this, SLOT(slotGrubCmdlineLinuxChanged()));
    connect(ui->pushbutton_cmdlineSuggestions->menu(), SIGNAL(aboutToShow()), this, SLOT(slotUpdateSuggestions()));
    connect(ui->pushbutton_cmdlineSuggestions->menu(), SIGNAL(triggered(QAction*)), this, SLOT(slotTriggeredSuggestion(QAction*)));

    connect(ui->lineedit_terminal, SIGNAL(textEdited(QString)), this, SLOT(slotGrubTerminalChanged()));
    connect(ui->pushbutton_terminalSuggestions->menu(), SIGNAL(aboutToShow()), this, SLOT(slotUpdateSuggestions()));
    connect(ui->pushbutton_terminalSuggestions->menu(), SIGNAL(triggered(QAction*)), this, SLOT(slotTriggeredSuggestion(QAction*)));
    connect(ui->lineedit_terminalInput, SIGNAL(textEdited(QString)), this, SLOT(slotGrubTerminalInputChanged()));
    connect(ui->pushbutton_terminalInputSuggestions->menu(), SIGNAL(aboutToShow()), this, SLOT(slotUpdateSuggestions()));
    connect(ui->pushbutton_terminalInputSuggestions->menu(), SIGNAL(triggered(QAction*)), this, SLOT(slotTriggeredSuggestion(QAction*)));
    connect(ui->lineedit_terminalOutput, SIGNAL(textEdited(QString)), this, SLOT(slotGrubTerminalOutputChanged()));
    connect(ui->pushbutton_terminalOutputSuggestions->menu(), SIGNAL(aboutToShow()), this, SLOT(slotUpdateSuggestions()));
    connect(ui->pushbutton_terminalOutputSuggestions->menu(), SIGNAL(triggered(QAction*)), this, SLOT(slotTriggeredSuggestion(QAction*)));

    connect(ui->lineedit_distributor, SIGNAL(textEdited(QString)), this, SLOT(slotGrubDistributorChanged()));
    connect(ui->lineedit_serial, SIGNAL(textEdited(QString)), this, SLOT(slotGrubSerialCommandChanged()));
    connect(ui->lineedit_initTune, SIGNAL(textEdited(QString)), this, SLOT(slotGrubInitTuneChanged()));
    connect(ui->checkBox_uuid, SIGNAL(clicked(bool)), this, SLOT(slotGrubDisableLinuxUuidChanged()));

    connect(ui->pushbutton_install, SIGNAL(clicked(bool)), this, SLOT(slotInstallBootloader()));
}

bool KCMGRUB2::readFile(const QString &fileName, QByteArray &fileContents)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file for reading:" << fileName;
        qDebug() << "Error code:" << file.error();
        qDebug() << "Error description:" << file.errorString();
        qDebug() << "The helper will now attempt to read this file.";
        return false;
    }
    fileContents = file.readAll();
    return true;
}
void KCMGRUB2::readAll()
{
    QByteArray fileContents;
    LoadOperations operations = NoOperation;

    if (readFile(grubMenuPath(), fileContents)) {
        parseEntries(QString::fromUtf8(fileContents.constData()));
    } else {
        operations |= MenuFile;
    }
    if (readFile(grubConfigPath(), fileContents)) {
        parseSettings(QString::fromUtf8(fileContents.constData()));
    } else {
        operations |= ConfigurationFile;
    }
    if (readFile(grubEnvPath(), fileContents)) {
        parseEnv(QString::fromUtf8(fileContents.constData()));
    } else {
        operations |= EnvironmentFile;
    }
    if (QFile::exists(grubMemtestPath())) {
        m_memtest = true;
        m_memtestOn = (bool)(QFile::permissions(grubMemtestPath()) & (QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther));
    } else {
        operations |= MemtestFile;
    }
#if HAVE_HD
    if (m_resolutionsEmpty) {
        operations |= Vbe;
    }
#endif
    if (QFileInfo(grubLocalePath()).isReadable()) {
        m_locales = QDir(grubLocalePath()).entryList(QStringList() << QLatin1String("*.mo"), QDir::Files).replaceInStrings(QRegExp(QLatin1String("\\.mo$")), QString());
    } else {
        operations |= Locales;
    }

    //Do not prompt for password if only the VBE operation is required, unless forced.
    if (operations && ((operations & (~Vbe)) || m_resolutionsForceRead)) {
        Action loadAction(QLatin1String("org.kde.kcontrol.kcmgrub2.load"));
        loadAction.setHelperId(QLatin1String("org.kde.kcontrol.kcmgrub2"));
        loadAction.addArgument(QLatin1String("operations"), (int)(operations));
        loadAction.setParentWidget(this);

        KAuth::ExecuteJob *loadJob = loadAction.execute();
        if (!loadJob->exec()) {
            qCritical() << "KAuth error!";
            qCritical() << "Error code:" << loadJob->error();
            qCritical() << "Error description:" << loadJob->errorText();
            return;
        }

        if (operations.testFlag(MenuFile)) {
            if (loadJob->data().value(QLatin1String("menuSuccess")).toBool()) {
                parseEntries(QString::fromUtf8(loadJob->data().value(QLatin1String("menuContents")).toByteArray().constData()));
            } else {
                qCritical() << "Helper failed to read file:" << grubMenuPath();
                qCritical() << "Error code:" << loadJob->data().value(QLatin1String("menuError")).toInt();
                qCritical() << "Error description:" << loadJob->data().value(QLatin1String("menuErrorString")).toString();
            }
        }
        if (operations.testFlag(ConfigurationFile)) {
            if (loadJob->data().value(QLatin1String("configSuccess")).toBool()) {
                parseSettings(QString::fromUtf8(loadJob->data().value(QLatin1String("configContents")).toByteArray().constData()));
            } else {
                qCritical() << "Helper failed to read file:" << grubConfigPath();
                qCritical() << "Error code:" << loadJob->data().value(QLatin1String("configError")).toInt();
                qCritical() << "Error description:" << loadJob->data().value(QLatin1String("configErrorString")).toString();
            }
        }
        if (operations.testFlag(EnvironmentFile)) {
            if (loadJob->data().value(QLatin1String("envSuccess")).toBool()) {
                parseEnv(QString::fromUtf8(loadJob->data().value(QLatin1String("envContents")).toByteArray().constData()));
            } else {
                qCritical() << "Helper failed to read file:" << grubEnvPath();
                qCritical() << "Error code:" << loadJob->data().value(QLatin1String("envError")).toInt();
                qCritical() << "Error description:" << loadJob->data().value(QLatin1String("envErrorString")).toString();
            }
        }
        if (operations.testFlag(MemtestFile)) {
            m_memtest = loadJob->data().value(QLatin1String("memtest")).toBool();
            if (m_memtest) {
                m_memtestOn = loadJob->data().value(QLatin1String("memtestOn")).toBool();
            }
        }
        if (operations.testFlag(Vbe)) {
            m_resolutions = loadJob->data().value(QLatin1String("gfxmodes")).toStringList();
            m_resolutionsEmpty = false;
            m_resolutionsForceRead = false;
        }
        if (operations.testFlag(Locales)) {
            m_locales = loadJob->data().value(QLatin1String("locales")).toStringList();
        }
    }
}

void KCMGRUB2::showLocales()
{
    ui->combobox_language->clear();
    ui->combobox_language->addItem(i18nc("@item:inlistbox", "No translation"), QString());

    Q_FOREACH(const QString &locale, m_locales) {
        QString language = QLocale(locale).nativeLanguageName();
        if (language.isEmpty()) {
            language = QLocale(locale.split(QLatin1Char('@')).first()).nativeLanguageName();
            if (language.isEmpty()) {
                language = QLocale(locale.split(QLatin1Char('@')).first().split(QLatin1Char('_')).first()).nativeLanguageName();
            }
        }
        ui->combobox_language->addItem(QString(QLatin1String("%1 (%2)")).arg(language, locale), locale);
    }
}
void KCMGRUB2::sortResolutions()
{
    for (int i = 0; i < m_resolutions.size(); i++) {
        if (m_resolutions.at(i).contains(QRegExp(QLatin1String("^\\d{3,4}x\\d{3,4}$")))) {
            m_resolutions[i] = QString(QLatin1String("%1x%2x0")).arg(m_resolutions.at(i).section(QLatin1Char('x'), 0, 0).rightJustified(4, QLatin1Char('0')), m_resolutions.at(i).section(QLatin1Char('x'), 1).rightJustified(4, QLatin1Char('0')));
        } else if (m_resolutions.at(i).contains(QRegExp(QLatin1String("^\\d{3,4}x\\d{3,4}x\\d{1,2}$")))) {
            m_resolutions[i] = QString(QLatin1String("%1x%2x%3")).arg(m_resolutions.at(i).section(QLatin1Char('x'), 0, 0).rightJustified(4, QLatin1Char('0')), m_resolutions.at(i).section(QLatin1Char('x'), 1, 1).rightJustified(4, QLatin1Char('0')), m_resolutions.at(i).section(QLatin1Char('x'), 2).rightJustified(2, QLatin1Char('0')));
        }
    }
    m_resolutions.sort();
    for (int i = 0; i < m_resolutions.size(); i++) {
        if (!m_resolutions.at(i).contains(QRegExp(QLatin1String("^\\d{3,4}x\\d{3,4}x\\d{1,2}$")))) {
            continue;
        }
        if (m_resolutions.at(i).startsWith(QLatin1Char('0'))) {
            m_resolutions[i].remove(0, 1);
        }
        m_resolutions[i].replace(QLatin1String("x0"), QLatin1String("x"));
        if (m_resolutions.at(i).endsWith(QLatin1Char('x'))) {
            m_resolutions[i].remove(m_resolutions.at(i).length() - 1, 1);
        }
    }
}
void KCMGRUB2::showResolutions()
{
    ui->combobox_gfxmode->clear();
    ui->combobox_gfxmode->addItem(i18nc("@item:inlistbox Refers to screen resolution.", "Custom..."), QLatin1String("custom"));
    ui->combobox_gfxmode->addItem(i18nc("@item:inlistbox Refers to screen resolution.", "Auto"), QLatin1String("auto"));

    ui->combobox_gfxpayload->clear();
    ui->combobox_gfxpayload->addItem(i18nc("@item:inlistbox Refers to screen resolution.", "Custom..."), QLatin1String("custom"));
    ui->combobox_gfxpayload->addItem(i18nc("@item:inlistbox Refers to screen resolution.", "Auto"), QLatin1String("auto"));
    ui->combobox_gfxpayload->addItem(i18nc("@item:inlistbox Refers to screen resolution.", "Unspecified"), QString());
    ui->combobox_gfxpayload->addItem(i18nc("@item:inlistbox", "Boot in Text Mode"), QLatin1String("text"));
    ui->combobox_gfxpayload->addItem(i18nc("@item:inlistbox", "Keep GRUB's Resolution"), QLatin1String("keep"));

    Q_FOREACH(const QString &resolution, m_resolutions) {
        ui->combobox_gfxmode->addItem(resolution, resolution);
        ui->combobox_gfxpayload->addItem(resolution, resolution);
    }
}

QString KCMGRUB2::parseTitle(const QString &line)
{
    QChar ch;
    QString entry, lineStr = line;
    QTextStream stream(&lineStr, QIODevice::ReadOnly | QIODevice::Text);

    stream.skipWhiteSpace();
    if (stream.atEnd()) {
        return QString();
    }

    stream >> ch;
    entry += ch;
    if (ch == QLatin1Char('\'')) {
        do {
            if (stream.atEnd()) {
                return QString();
            }
            stream >> ch;
            entry += ch;
        } while (ch != QLatin1Char('\''));
    } else if (ch == QLatin1Char('"')) {
        do {
            if (stream.atEnd()) {
                return QString();
            }
            stream >> ch;
            entry += ch;
        } while (ch != QLatin1Char('"') || entry.at(entry.size() - 2) == QLatin1Char('\\'));
    } else {
        do {
            if (stream.atEnd()) {
                return QString();
            }
            stream >> ch;
            entry += ch;
        } while (!ch.isSpace() || entry.at(entry.size() - 2) == QLatin1Char('\\'));
        entry.chop(1); //remove trailing space
    }
    return entry;
}
void KCMGRUB2::parseEntries(const QString &config)
{
    bool inEntry = false;
    int menuLvl = 0;
    QList<int> levelCount;
    levelCount.append(0);
    QList<Entry::Title> submenus;
    QString word, configStr = config;
    QTextStream stream(&configStr, QIODevice::ReadOnly | QIODevice::Text);

    m_entries.clear();
    while (!stream.atEnd()) {
        //Read the first word of the line
        stream >> word;
        if (stream.atEnd()) {
            return;
        }
        //If the first word is known, process the rest of the line
        if (word == QLatin1String("menuentry")) {
            if (inEntry) {
                qCritical() << "Malformed configuration file! Aborting entries' parsing.";
                qDebug() << "A 'menuentry' directive was detected inside the scope of a menuentry.";
                m_entries.clear();
                return;
            }
            Entry entry(parseTitle(stream.readLine()), levelCount.at(menuLvl), Entry::Menuentry, menuLvl);
            if (menuLvl > 0) {
                entry.setAncestors(submenus);
            }
            m_entries.append(entry);
            levelCount[menuLvl]++;
            inEntry = true;
            continue;
        } else if (word == QLatin1String("submenu")) {
            if (inEntry) {
                qCritical() << "Malformed configuration file! Aborting entries' parsing.";
                qDebug() << "A 'submenu' directive was detected inside the scope of a menuentry.";
                m_entries.clear();
                return;
            }
            Entry entry(parseTitle(stream.readLine()), levelCount.at(menuLvl), Entry::Submenu, menuLvl);
            if (menuLvl > 0) {
                entry.setAncestors(submenus);
            }
            m_entries.append(entry);
            submenus.append(entry.title());
            levelCount[menuLvl]++;
            levelCount.append(0);
            menuLvl++;
            continue;
        } else if (word == QLatin1String("linux")) {
            if (!inEntry) {
                qCritical() << "Malformed configuration file! Aborting entries' parsing.";
                qDebug() << "A 'linux' directive was detected outside the scope of a menuentry.";
                m_entries.clear();
                return;
            }
            stream >> word;
            m_entries.last().setKernel(word);
        } else if (word == QLatin1String("}")) {
            if (inEntry) {
                inEntry = false;
            } else if (menuLvl > 0) {
                submenus.removeLast();
                levelCount[menuLvl] = 0;
                menuLvl--;
            }
        }
        //Drop the rest of the line
        stream.readLine();
    }
}
void KCMGRUB2::parseSettings(const QString &config)
{
    QString line, configStr = config;
    QTextStream stream(&configStr, QIODevice::ReadOnly | QIODevice::Text);

    m_settings.clear();
    while (!stream.atEnd()) {
        line = stream.readLine().trimmed();
        if (line.contains(QRegExp(QLatin1String("^(GRUB_|LANGUAGE=)")))) {
            m_settings[line.section(QLatin1Char('='), 0, 0)] = line.section(QLatin1Char('='), 1);
        }
    }
}
void KCMGRUB2::parseEnv(const QString &config)
{
    QString line, configStr = config;
    QTextStream stream(&configStr, QIODevice::ReadOnly | QIODevice::Text);

    m_env.clear();
    while (!stream.atEnd()) {
        line = stream.readLine().trimmed();
        if (line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        m_env[line.section(QLatin1Char('='), 0, 0)] = line.section(QLatin1Char('='), 1);
    }
}

#include "kcm_grub2.moc"
