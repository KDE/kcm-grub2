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

// Krazy
// krazy:excludeall=cpp

// Own
#include "kcm_grub2.h"

// Qt
#include <QDebug>
#include <QDesktopWidget>
#include <QIcon>
#include <QMenu>
#include <QProgressDialog>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTreeView>

// KDE
#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <KLocalizedString>
#include <KMessageBox>
#include <KPluginFactory>

using namespace KAuth;

// Project
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

// Ui
#include "ui_kcm_grub2.h"

K_PLUGIN_CLASS_WITH_JSON(KCMGRUB2, "kcm_grub2.json")

KCMGRUB2::KCMGRUB2(QObject *parent, const KPluginMetaData &data, const QVariantList &list)
    : KCModule(parent, data, list)
{
    // Make this setAuthActionName in KF6
    setAuthAction(Action(QStringLiteral("org.kde.kcontrol.kcmgrub2.save")));

    ui = new Ui::KCMGRUB2;
    ui->setupUi(widget());
    setupObjects();
    setupConnections();
}
KCMGRUB2::~KCMGRUB2()
{
    delete ui;
}

void KCMGRUB2::defaults()
{
    Action defaultsAction(QStringLiteral("org.kde.kcontrol.kcmgrub2.defaults"));
    defaultsAction.setHelperId(QStringLiteral("org.kde.kcontrol.kcmgrub2"));
    defaultsAction.setParentWidget(widget());

    KAuth::ExecuteJob *defaultsJob = defaultsAction.execute();
    if (defaultsJob->exec()) {
        load();
        save();
        KMessageBox::information(widget(), i18nc("@info", "Successfully restored the default values."));
    } else {
        KMessageBox::detailedError(widget(), i18nc("@info", "Failed to restore the default values."), defaultsJob->errorText());
    }
}
void KCMGRUB2::load()
{
    readAll();

    QString grubDefault = unquoteWord(m_settings.value(QStringLiteral("GRUB_DEFAULT")));
    if (grubDefault == QLatin1String("saved")) {
        grubDefault = (m_env.value(QStringLiteral("saved_entry")).isEmpty() ? QString(QLatin1Char('0')) : m_env.value(QStringLiteral("saved_entry")));
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
    ui->checkBox_savedefault->setChecked(unquoteWord(m_settings.value(QStringLiteral("GRUB_SAVEDEFAULT"))).compare(QLatin1String("true")) == 0);

    QString grubHiddenTimeoutRaw = unquoteWord(m_settings.value(QStringLiteral("GRUB_HIDDEN_TIMEOUT")));
    if (grubHiddenTimeoutRaw.isEmpty()) {
        ui->checkBox_hiddenTimeout->setChecked(false);
    } else {
        bool ok;
        int grubHiddenTimeout = grubHiddenTimeoutRaw.toInt(&ok);
        if (ok && grubHiddenTimeout >= 0) {
            ui->checkBox_hiddenTimeout->setChecked(true);
            ui->spinBox_hiddenTimeout->setValue(grubHiddenTimeout);
            ui->checkBox_hiddenTimeoutShowTimer->setChecked(
                unquoteWord(m_settings.value(QStringLiteral("GRUB_HIDDEN_TIMEOUT_QUIET"))).compare(QLatin1String("true")) != 0);
        } else {
            qWarning() << "Invalid GRUB_HIDDEN_TIMEOUT value";
        }
    }
    bool ok = true;
    int grubTimeout =
        (m_settings.value(QStringLiteral("GRUB_TIMEOUT")).isEmpty() ? 5 : unquoteWord(m_settings.value(QStringLiteral("GRUB_TIMEOUT"))).toInt(&ok));
    if (ok && grubTimeout >= -1) {
        ui->checkBox_timeout->setChecked(grubTimeout > -1);
        ui->radioButton_timeout0->setChecked(grubTimeout == 0);
        ui->radioButton_timeout->setChecked(grubTimeout > 0);
        ui->spinBox_timeout->setValue(grubTimeout);
    } else {
        qWarning() << "Invalid GRUB_TIMEOUT value";
    }

    showLocales();
    int languageIndex = ui->combobox_language->findData(unquoteWord(m_settings.value(QStringLiteral("LANGUAGE"))));
    if (languageIndex != -1) {
        ui->combobox_language->setCurrentIndex(languageIndex);
    } else {
        qWarning() << "Invalid LANGUAGE value";
    }
    ui->checkBox_recovery->setChecked(unquoteWord(m_settings.value(QStringLiteral("GRUB_DISABLE_RECOVERY"))).compare(QLatin1String("true")) != 0);
    ui->checkBox_memtest->setVisible(m_memtest);
    ui->checkBox_memtest->setChecked(m_memtestOn);
    ui->checkBox_osProber->setChecked(unquoteWord(m_settings.value(QStringLiteral("GRUB_DISABLE_OS_PROBER"))).compare(QLatin1String("true")) != 0);

    QString grubGfxmode = unquoteWord(m_settings.value(QStringLiteral("GRUB_GFXMODE")));
    if (grubGfxmode.isEmpty()) {
        grubGfxmode = QStringLiteral("auto");
    }
    if (grubGfxmode != QLatin1String("auto") && !m_resolutions.contains(grubGfxmode)) {
        m_resolutions.append(grubGfxmode);
    }
    QString grubGfxpayloadLinux = unquoteWord(m_settings.value(QStringLiteral("GRUB_GFXPAYLOAD_LINUX")));
    if (!grubGfxpayloadLinux.isEmpty() && grubGfxpayloadLinux != QLatin1String("text") && grubGfxpayloadLinux != QLatin1String("keep")
        && !m_resolutions.contains(grubGfxpayloadLinux)) {
        m_resolutions.append(grubGfxpayloadLinux);
    }
    m_resolutions.removeDuplicates();
    sortResolutions();
    showResolutions();
    ui->combobox_gfxmode->setCurrentIndex(ui->combobox_gfxmode->findData(grubGfxmode));
    ui->toolButton_refreshGfxmode->setVisible(HAVE_HD && m_resolutionsEmpty);
    ui->combobox_gfxpayload->setCurrentIndex(ui->combobox_gfxpayload->findData(grubGfxpayloadLinux));
    ui->toolButton_refreshGfxpayload->setVisible(HAVE_HD && m_resolutionsEmpty);

    QString grubColorNormal = unquoteWord(m_settings.value(QStringLiteral("GRUB_COLOR_NORMAL")));
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
    QString grubColorHighlight = unquoteWord(m_settings.value(QStringLiteral("GRUB_COLOR_HIGHLIGHT")));
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

    QString grubBackground = unquoteWord(m_settings.value(QStringLiteral("GRUB_BACKGROUND")));
    ui->kurlrequester_background->setText(grubBackground);
    ui->pushbutton_preview->setEnabled(!grubBackground.isEmpty());
    ui->kurlrequester_theme->setText(unquoteWord(m_settings.value(QStringLiteral("GRUB_THEME"))));

    ui->lineedit_cmdlineDefault->setText(unquoteWord(m_settings.value(QStringLiteral("GRUB_CMDLINE_LINUX_DEFAULT"))));
    ui->lineedit_cmdline->setText(unquoteWord(m_settings.value(QStringLiteral("GRUB_CMDLINE_LINUX"))));

    QString grubTerminal = unquoteWord(m_settings.value(QStringLiteral("GRUB_TERMINAL")));
    ui->lineedit_terminal->setText(grubTerminal);
    ui->lineedit_terminalInput->setReadOnly(!grubTerminal.isEmpty());
    ui->lineedit_terminalOutput->setReadOnly(!grubTerminal.isEmpty());
    ui->lineedit_terminalInput->setText(!grubTerminal.isEmpty() ? grubTerminal : unquoteWord(m_settings.value(QStringLiteral("GRUB_TERMINAL_INPUT"))));
    ui->lineedit_terminalOutput->setText(!grubTerminal.isEmpty() ? grubTerminal : unquoteWord(m_settings.value(QStringLiteral("GRUB_TERMINAL_OUTPUT"))));

    ui->lineedit_distributor->setText(unquoteWord(m_settings.value(QStringLiteral("GRUB_DISTRIBUTOR"))));
    ui->lineedit_serial->setText(unquoteWord(m_settings.value(QStringLiteral("GRUB_SERIAL_COMMAND"))));
    ui->lineedit_initTune->setText(unquoteWord(m_settings.value(QStringLiteral("GRUB_INIT_TUNE"))));
    ui->checkBox_uuid->setChecked(unquoteWord(m_settings.value(QStringLiteral("GRUB_DISABLE_LINUX_UUID"))).compare(QLatin1String("true")) != 0);

    m_dirtyBits.fill(0);
    setNeedsSave(false);
}
void KCMGRUB2::save()
{
    QString grubDefault;
    if (!m_entries.isEmpty()) {
        m_settings[QStringLiteral("GRUB_DEFAULT")] = QStringLiteral("saved");
        QStandardItemModel *model = qobject_cast<QStandardItemModel *>(ui->combobox_default->model());
        QTreeView *view = qobject_cast<QTreeView *>(ui->combobox_default->view());
        // Ugly, ugly hack. The view's current QModelIndex is invalidated
        // while the view is hidden and there is no access to the internal
        // QPersistentModelIndex (it is hidden in QComboBox's pimpl).
        // While the popup is shown, the QComboBox selects the current entry.
        // TODO: Maybe move away from the QComboBox+QTreeView implementation?
        ui->combobox_default->showPopup();
        grubDefault = model->itemFromIndex(view->currentIndex())->data().toString();
        ui->combobox_default->hidePopup();
    }
    if (m_dirtyBits.testBit(grubSavedefaultDirty)) {
        if (ui->checkBox_savedefault->isChecked()) {
            m_settings[QStringLiteral("GRUB_SAVEDEFAULT")] = QStringLiteral("true");
        } else {
            m_settings.remove(QStringLiteral("GRUB_SAVEDEFAULT"));
        }
    }
    if (m_dirtyBits.testBit(grubHiddenTimeoutDirty)) {
        if (ui->checkBox_hiddenTimeout->isChecked()) {
            m_settings[QStringLiteral("GRUB_HIDDEN_TIMEOUT")] = QString::number(ui->spinBox_hiddenTimeout->value());
        } else {
            m_settings.remove(QStringLiteral("GRUB_HIDDEN_TIMEOUT"));
        }
    }
    if (m_dirtyBits.testBit(grubHiddenTimeoutQuietDirty)) {
        if (ui->checkBox_hiddenTimeoutShowTimer->isChecked()) {
            m_settings.remove(QStringLiteral("GRUB_HIDDEN_TIMEOUT_QUIET"));
        } else {
            m_settings[QStringLiteral("GRUB_HIDDEN_TIMEOUT_QUIET")] = QStringLiteral("true");
        }
    }
    if (m_dirtyBits.testBit(grubTimeoutDirty)) {
        if (ui->checkBox_timeout->isChecked()) {
            if (ui->radioButton_timeout0->isChecked()) {
                m_settings[QStringLiteral("GRUB_TIMEOUT")] = QLatin1Char('0');
            } else {
                m_settings[QStringLiteral("GRUB_TIMEOUT")] = QString::number(ui->spinBox_timeout->value());
            }
        } else {
            m_settings[QStringLiteral("GRUB_TIMEOUT")] = QStringLiteral("-1");
        }
    }
    if (m_dirtyBits.testBit(grubLocaleDirty)) {
        int langIndex = ui->combobox_language->currentIndex();
        if (langIndex > 0) {
            m_settings[QStringLiteral("LANGUAGE")] = ui->combobox_language->itemData(langIndex).toString();
        } else {
            m_settings.remove(QStringLiteral("LANGUAGE"));
        }
    }
    if (m_dirtyBits.testBit(grubDisableRecoveryDirty)) {
        if (ui->checkBox_recovery->isChecked()) {
            m_settings.remove(QStringLiteral("GRUB_DISABLE_RECOVERY"));
        } else {
            m_settings[QStringLiteral("GRUB_DISABLE_RECOVERY")] = QStringLiteral("true");
        }
    }
    if (m_dirtyBits.testBit(grubDisableOsProberDirty)) {
        if (ui->checkBox_osProber->isChecked()) {
            m_settings.remove(QStringLiteral("GRUB_DISABLE_OS_PROBER"));
        } else {
            m_settings[QStringLiteral("GRUB_DISABLE_OS_PROBER")] = QStringLiteral("true");
        }
    }
    if (m_dirtyBits.testBit(grubGfxmodeDirty)) {
        if (ui->combobox_gfxmode->currentIndex() <= 0) {
            qCritical() << "Something went terribly wrong!";
        } else {
            m_settings[QStringLiteral("GRUB_GFXMODE")] = quoteWord(ui->combobox_gfxmode->itemData(ui->combobox_gfxmode->currentIndex()).toString());
        }
    }
    if (m_dirtyBits.testBit(grubGfxpayloadLinuxDirty)) {
        if (ui->combobox_gfxpayload->currentIndex() <= 0) {
            qCritical() << "Something went terribly wrong!";
        } else if (ui->combobox_gfxpayload->currentIndex() == 1) {
            m_settings.remove(QStringLiteral("GRUB_GFXPAYLOAD_LINUX"));
        } else if (ui->combobox_gfxpayload->currentIndex() > 1) {
            m_settings[QStringLiteral("GRUB_GFXPAYLOAD_LINUX")] =
                quoteWord(ui->combobox_gfxpayload->itemData(ui->combobox_gfxpayload->currentIndex()).toString());
        }
    }
    if (m_dirtyBits.testBit(grubColorNormalDirty)) {
        QString normalForeground = ui->combobox_normalForeground->itemData(ui->combobox_normalForeground->currentIndex()).toString();
        QString normalBackground = ui->combobox_normalBackground->itemData(ui->combobox_normalBackground->currentIndex()).toString();
        if (normalForeground.compare(QLatin1String("light-gray")) != 0 || normalBackground.compare(QLatin1String("black")) != 0) {
            m_settings[QStringLiteral("GRUB_COLOR_NORMAL")] = normalForeground + QLatin1Char('/') + normalBackground;
        } else {
            m_settings.remove(QStringLiteral("GRUB_COLOR_NORMAL"));
        }
    }
    if (m_dirtyBits.testBit(grubColorHighlightDirty)) {
        QString highlightForeground = ui->combobox_highlightForeground->itemData(ui->combobox_highlightForeground->currentIndex()).toString();
        QString highlightBackground = ui->combobox_highlightBackground->itemData(ui->combobox_highlightBackground->currentIndex()).toString();
        if (highlightForeground.compare(QLatin1String("black")) != 0 || highlightBackground.compare(QLatin1String("light-gray")) != 0) {
            m_settings[QStringLiteral("GRUB_COLOR_HIGHLIGHT")] = highlightForeground + QLatin1Char('/') + highlightBackground;
        } else {
            m_settings.remove(QStringLiteral("GRUB_COLOR_HIGHLIGHT"));
        }
    }
    if (m_dirtyBits.testBit(grubBackgroundDirty)) {
        QString background = ui->kurlrequester_background->url().toLocalFile();
        if (!background.isEmpty()) {
            m_settings[QStringLiteral("GRUB_BACKGROUND")] = quoteWord(background);
        } else {
            m_settings.remove(QStringLiteral("GRUB_BACKGROUND"));
        }
    }
    if (m_dirtyBits.testBit(grubThemeDirty)) {
        QString theme = ui->kurlrequester_theme->url().toLocalFile();
        if (!theme.isEmpty()) {
            m_settings[QStringLiteral("GRUB_THEME")] = quoteWord(theme);
        } else {
            m_settings.remove(QStringLiteral("GRUB_THEME"));
        }
    }
    if (m_dirtyBits.testBit(grubCmdlineLinuxDefaultDirty)) {
        QString cmdlineLinuxDefault = ui->lineedit_cmdlineDefault->text();
        if (!cmdlineLinuxDefault.isEmpty()) {
            m_settings[QStringLiteral("GRUB_CMDLINE_LINUX_DEFAULT")] = quoteWord(cmdlineLinuxDefault);
        } else {
            m_settings.remove(QStringLiteral("GRUB_CMDLINE_LINUX_DEFAULT"));
        }
    }
    if (m_dirtyBits.testBit(grubCmdlineLinuxDirty)) {
        QString cmdlineLinux = ui->lineedit_cmdline->text();
        if (!cmdlineLinux.isEmpty()) {
            m_settings[QStringLiteral("GRUB_CMDLINE_LINUX")] = quoteWord(cmdlineLinux);
        } else {
            m_settings.remove(QStringLiteral("GRUB_CMDLINE_LINUX"));
        }
    }
    if (m_dirtyBits.testBit(grubTerminalDirty)) {
        QString terminal = ui->lineedit_terminal->text();
        if (!terminal.isEmpty()) {
            m_settings[QStringLiteral("GRUB_TERMINAL")] = quoteWord(terminal);
        } else {
            m_settings.remove(QStringLiteral("GRUB_TERMINAL"));
        }
    }
    if (m_dirtyBits.testBit(grubTerminalInputDirty)) {
        QString terminalInput = ui->lineedit_terminalInput->text();
        if (!terminalInput.isEmpty()) {
            m_settings[QStringLiteral("GRUB_TERMINAL_INPUT")] = quoteWord(terminalInput);
        } else {
            m_settings.remove(QStringLiteral("GRUB_TERMINAL_INPUT"));
        }
    }
    if (m_dirtyBits.testBit(grubTerminalOutputDirty)) {
        QString terminalOutput = ui->lineedit_terminalOutput->text();
        if (!terminalOutput.isEmpty()) {
            m_settings[QStringLiteral("GRUB_TERMINAL_OUTPUT")] = quoteWord(terminalOutput);
        } else {
            m_settings.remove(QStringLiteral("GRUB_TERMINAL_OUTPUT"));
        }
    }
    if (m_dirtyBits.testBit(grubDistributorDirty)) {
        QString distributor = ui->lineedit_distributor->text();
        if (!distributor.isEmpty()) {
            m_settings[QStringLiteral("GRUB_DISTRIBUTOR")] = quoteWord(distributor);
        } else {
            m_settings.remove(QStringLiteral("GRUB_DISTRIBUTOR"));
        }
    }
    if (m_dirtyBits.testBit(grubSerialCommandDirty)) {
        QString serialCommand = ui->lineedit_serial->text();
        if (!serialCommand.isEmpty()) {
            m_settings[QStringLiteral("GRUB_SERIAL_COMMAND")] = quoteWord(serialCommand);
        } else {
            m_settings.remove(QStringLiteral("GRUB_SERIAL_COMMAND"));
        }
    }
    if (m_dirtyBits.testBit(grubInitTuneDirty)) {
        QString initTune = ui->lineedit_initTune->text();
        if (!initTune.isEmpty()) {
            m_settings[QStringLiteral("GRUB_INIT_TUNE")] = quoteWord(initTune);
        } else {
            m_settings.remove(QStringLiteral("GRUB_INIT_TUNE"));
        }
    }
    if (m_dirtyBits.testBit(grubDisableLinuxUuidDirty)) {
        if (ui->checkBox_uuid->isChecked()) {
            m_settings.remove(QStringLiteral("GRUB_DISABLE_LINUX_UUID"));
        } else {
            m_settings[QStringLiteral("GRUB_DISABLE_LINUX_UUID")] = QStringLiteral("true");
        }
    }

    QString configFileContents;
    QTextStream stream(&configFileContents, QIODevice::WriteOnly | QIODevice::Text);
    QHash<QString, QString>::const_iterator it = m_settings.constBegin();
    QHash<QString, QString>::const_iterator end = m_settings.constEnd();
    for (; it != end; ++it) {
        stream << it.key() << '=' << it.value() << Qt::endl;
    }

    Action saveAction(QStringLiteral("org.kde.kcontrol.kcmgrub2.save"));
    saveAction.setHelperId(QStringLiteral("org.kde.kcontrol.kcmgrub2"));
    saveAction.addArgument(QStringLiteral("rawConfigFileContents"), configFileContents.toUtf8());
    saveAction.addArgument(QStringLiteral("rawDefaultEntry"),
                           !m_entries.isEmpty() ? grubDefault.toUtf8() : m_settings.value(QStringLiteral("GRUB_DEFAULT")).toUtf8());
    if (ui->combobox_language->currentIndex() > 0) {
        saveAction.addArgument(QStringLiteral("LANG"), qgetenv("LANG"));
        saveAction.addArgument(QStringLiteral("LANGUAGE"), m_settings.value(QStringLiteral("LANGUAGE")));
    }
    if (m_dirtyBits.testBit(memtestDirty)) {
        saveAction.addArgument(QStringLiteral("memtest"), ui->checkBox_memtest->isChecked());
    }
    saveAction.setParentWidget(widget());
    saveAction.setTimeout(60000);

    KAuth::ExecuteJob *saveJob = saveAction.execute(KAuth::Action::AuthorizeOnlyMode);
    if (!saveJob->exec()) {
        return;
    }
    saveJob = saveAction.execute();

    QProgressDialog progressDlg(widget());
    progressDlg.setWindowTitle(i18nc("@title:window Verb (gerund). Refers to current status.", "Saving"));
    progressDlg.setLabelText(i18nc("@info:progress", "Saving GRUB settings..."));
    progressDlg.setCancelButton(nullptr);
    progressDlg.setModal(true);
    progressDlg.setMinimum(0);
    progressDlg.setMaximum(0);
    progressDlg.show();
    connect(saveJob, &KJob::finished, &progressDlg, &QWidget::hide);

    if (saveJob->exec()) {
        QDialog *dialog = new QDialog(widget());
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

        KMessageBox::createKMessageBox(dialog,
                                       buttonBox,
                                       QMessageBox::Information,
                                       i18nc("@info", "Successfully saved GRUB settings."),
                                       QStringList(),
                                       QString(),
                                       nullptr,
                                       KMessageBox::Notify,
                                       QString::fromUtf8(saveJob->data().value(QStringLiteral("output")).toByteArray().constData())); // krazy:exclude=qclasses
        load();
    } else {
        KMessageBox::detailedError(widget(), i18nc("@info", "Failed to save GRUB settings."), saveJob->errorText());
    }
}

void KCMGRUB2::slotRemoveOldEntries()
{
#if HAVE_QAPT || HAVE_QPACKAGEKIT
    QPointer<RemoveDialog> removeDlg = new RemoveDialog(m_entries, widget());
    if (removeDlg->exec()) {
        load();
    }
    delete removeDlg;
#endif
}
void KCMGRUB2::slotGrubSavedefaultChanged()
{
    m_dirtyBits.setBit(grubSavedefaultDirty);
    markAsChanged();
}
void KCMGRUB2::slotGrubHiddenTimeoutToggled(bool checked)
{
    ui->spinBox_hiddenTimeout->setEnabled(checked);
    ui->checkBox_hiddenTimeoutShowTimer->setEnabled(checked);
}
void KCMGRUB2::slotGrubHiddenTimeoutChanged()
{
    m_dirtyBits.setBit(grubHiddenTimeoutDirty);
    markAsChanged();
}
void KCMGRUB2::slotGrubHiddenTimeoutQuietChanged()
{
    m_dirtyBits.setBit(grubHiddenTimeoutQuietDirty);
    markAsChanged();
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
    markAsChanged();
}
void KCMGRUB2::slotGrubLanguageChanged()
{
    m_dirtyBits.setBit(grubLocaleDirty);
    markAsChanged();
}
void KCMGRUB2::slotGrubDisableRecoveryChanged()
{
    m_dirtyBits.setBit(grubDisableRecoveryDirty);
    markAsChanged();
}
void KCMGRUB2::slotMemtestChanged()
{
    m_dirtyBits.setBit(memtestDirty);
    markAsChanged();
}
void KCMGRUB2::slotGrubDisableOsProberChanged()
{
    m_dirtyBits.setBit(grubDisableOsProberDirty);
    markAsChanged();
}
void KCMGRUB2::slotGrubGfxmodeChanged()
{
    if (ui->combobox_gfxmode->currentIndex() == 0) {
        bool ok;
        QRegExpValidator regExp(QRegExp(QLatin1String("\\d{3,4}x\\d{3,4}(x\\d{1,2})?")), this);
        QString resolution = TextInputDialog::getText(widget(),
                                                      i18nc("@title:window", "Enter screen resolution"),
                                                      i18nc("@label:textbox", "Please enter a GRUB resolution:"),
                                                      QString(),
                                                      &regExp,
                                                      &ok);
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
    markAsChanged();
}
void KCMGRUB2::slotGrubGfxpayloadLinuxChanged()
{
    if (ui->combobox_gfxpayload->currentIndex() == 0) {
        bool ok;
        QRegExpValidator regExp(QRegExp(QLatin1String("\\d{3,4}x\\d{3,4}(x\\d{1,2})?")), this);
        QString resolution = TextInputDialog::getText(widget(),
                                                      i18nc("@title:window", "Enter screen resolution"),
                                                      i18nc("@label:textbox", "Please enter a Linux boot resolution:"),
                                                      QString(),
                                                      &regExp,
                                                      &ok);
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
    markAsChanged();
}
void KCMGRUB2::slotResolutionsRefresh()
{
    m_resolutionsForceRead = true;
    load();
}
void KCMGRUB2::slotGrubColorNormalChanged()
{
    m_dirtyBits.setBit(grubColorNormalDirty);
    markAsChanged();
}
void KCMGRUB2::slotGrubColorHighlightChanged()
{
    m_dirtyBits.setBit(grubColorHighlightDirty);
    markAsChanged();
}
void KCMGRUB2::slowGrubBackgroundChanged()
{
    ui->pushbutton_preview->setEnabled(!ui->kurlrequester_background->text().isEmpty());
    m_dirtyBits.setBit(grubBackgroundDirty);
    markAsChanged();
}
void KCMGRUB2::slotPreviewGrubBackground()
{
    QFile file(ui->kurlrequester_background->url().toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    // TODO: Need something more elegant.
    QDialog *dialog = new QDialog(widget());
    QLabel *label = new QLabel(dialog);
    label->setPixmap(QPixmap::fromImage(QImage::fromData(file.readAll())).scaled(QDesktopWidget().screenGeometry(widget()).size()));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->showFullScreen();
    KMessageBox::information(dialog,
                             i18nc("@info", "Press <shortcut>Escape</shortcut> to exit fullscreen mode."),
                             QString(),
                             QStringLiteral("GRUBFullscreenPreview"));
}
void KCMGRUB2::slotCreateGrubBackground()
{
#if HAVE_IMAGEMAGICK
    QPointer<ConvertDialog> convertDlg = new ConvertDialog(widget());
    QString resolution = ui->combobox_gfxmode->itemData(ui->combobox_gfxmode->currentIndex()).toString();
    convertDlg->setResolution(resolution.section(QLatin1Char('x'), 0, 0).toInt(), resolution.section(QLatin1Char('x'), 1, 1).toInt());
    connect(convertDlg.data(), &ConvertDialog::splashImageCreated, ui->kurlrequester_background, &KUrlRequester::setText);
    convertDlg->exec();
    delete convertDlg;
#endif
}
void KCMGRUB2::slotGrubThemeChanged()
{
    m_dirtyBits.setBit(grubThemeDirty);
    markAsChanged();
}
void KCMGRUB2::slotGrubCmdlineLinuxDefaultChanged()
{
    m_dirtyBits.setBit(grubCmdlineLinuxDefaultDirty);
    markAsChanged();
}
void KCMGRUB2::slotGrubCmdlineLinuxChanged()
{
    m_dirtyBits.setBit(grubCmdlineLinuxDirty);
    markAsChanged();
}
void KCMGRUB2::slotGrubTerminalChanged()
{
    QString grubTerminal = ui->lineedit_terminal->text();
    ui->lineedit_terminalInput->setReadOnly(!grubTerminal.isEmpty());
    ui->lineedit_terminalOutput->setReadOnly(!grubTerminal.isEmpty());
    ui->lineedit_terminalInput->setText(!grubTerminal.isEmpty() ? grubTerminal : unquoteWord(m_settings.value(QStringLiteral("GRUB_TERMINAL_INPUT"))));
    ui->lineedit_terminalOutput->setText(!grubTerminal.isEmpty() ? grubTerminal : unquoteWord(m_settings.value(QStringLiteral("GRUB_TERMINAL_OUTPUT"))));
    m_dirtyBits.setBit(grubTerminalDirty);
}
void KCMGRUB2::slotGrubTerminalInputChanged()
{
    m_dirtyBits.setBit(grubTerminalInputDirty);
    markAsChanged();
}
void KCMGRUB2::slotGrubTerminalOutputChanged()
{
    m_dirtyBits.setBit(grubTerminalOutputDirty);
    markAsChanged();
}
void KCMGRUB2::slotGrubDistributorChanged()
{
    m_dirtyBits.setBit(grubDistributorDirty);
    markAsChanged();
}
void KCMGRUB2::slotGrubSerialCommandChanged()
{
    m_dirtyBits.setBit(grubSerialCommandDirty);
    markAsChanged();
}
void KCMGRUB2::slotGrubInitTuneChanged()
{
    m_dirtyBits.setBit(grubInitTuneDirty);
    markAsChanged();
}
void KCMGRUB2::slotGrubDisableLinuxUuidChanged()
{
    m_dirtyBits.setBit(grubDisableLinuxUuidDirty);
    markAsChanged();
}
void KCMGRUB2::slotInstallBootloader()
{
    QPointer<InstallDialog> installDlg = new InstallDialog(widget());
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

    const auto actions = qobject_cast<const QWidget *>(sender())->actions();
    for (QAction *action : actions) {
        if (!action->isCheckable()) {
            action->setCheckable(true);
        }
        action->setChecked(lineEdit->text().contains(QRegExp(QString(QLatin1String("\\b%1\\b")).arg(action->data().toString()))));
    }
}
void KCMGRUB2::slotTriggeredSuggestion(QAction *action)
{
    QLineEdit *lineEdit = nullptr;
    void (KCMGRUB2::*updateFunction)() = nullptr;
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

void KCMGRUB2::setupColors(std::initializer_list<ColorInfo> colorInfos)
{
    for (const auto &ci : colorInfos) {
        QPixmap colorPixmap(16, 16);
        colorPixmap.fill(ci.color);
        const QIcon color(colorPixmap);
        ui->combobox_normalForeground->addItem(color, ci.text, ci.grubColor);
        ui->combobox_highlightForeground->addItem(color, ci.text, ci.grubColor);
        ui->combobox_normalBackground->addItem(color, ci.text, ci.grubColor);
        ui->combobox_highlightBackground->addItem(color, ci.text, ci.grubColor);
    }
}

void KCMGRUB2::setupObjects()
{
    setButtons(Default | Apply);

    m_dirtyBits.resize(lastDirtyBit);
    m_resolutionsEmpty = true;
    m_resolutionsForceRead = false;

    QTreeView *view = new QTreeView(ui->combobox_default);
    view->setHeaderHidden(true);
    view->setItemsExpandable(false);
    view->setRootIsDecorated(false);
    ui->combobox_default->setView(view);

    ui->pushbutton_remove->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));
    ui->pushbutton_remove->setVisible(HAVE_QAPT || HAVE_QPACKAGEKIT);

    ui->toolButton_refreshGfxmode->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    ui->toolButton_refreshGfxpayload->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));

    QPixmap black(16, 16), transparent(16, 16);
    black.fill(Qt::black);
    transparent.fill(Qt::transparent);
    ui->combobox_normalForeground->addItem(QIcon(black), i18nc("@item:inlistbox Refers to color.", "Black"), QLatin1String("black"));
    ui->combobox_highlightForeground->addItem(QIcon(black), i18nc("@item:inlistbox Refers to color.", "Black"), QLatin1String("black"));
    ui->combobox_normalBackground->addItem(QIcon(transparent), i18nc("@item:inlistbox Refers to color.", "Transparent"), QLatin1String("black"));
    ui->combobox_highlightBackground->addItem(QIcon(transparent), i18nc("@item:inlistbox Refers to color.", "Transparent"), QLatin1String("black"));

    setupColors({
        {QStringLiteral("blue"), i18nc("@item:inlistbox Refers to color.", "Blue"), QColorConstants::Svg::blue},
        {QStringLiteral("cyan"), i18nc("@item:inlistbox Refers to color.", "Cyan"), QColorConstants::Svg::cyan},
        {QStringLiteral("dark-gray"), i18nc("@item:inlistbox Refers to color.", "Dark Gray"), QColorConstants::Svg::darkgray},
        {QStringLiteral("green"), i18nc("@item:inlistbox Refers to color.", "Green"), QColorConstants::Svg::green},
        {QStringLiteral("light-cyan"), i18nc("@item:inlistbox Refers to color.", "Light Cyan"), QColorConstants::Svg::lightcyan},
        {QStringLiteral("light-blue"), i18nc("@item:inlistbox Refers to color.", "Light Blue"), QColorConstants::Svg::lightblue},
        {QStringLiteral("light-green"), i18nc("@item:inlistbox Refers to color.", "Light Green"), QColorConstants::Svg::lightgreen},
        {QStringLiteral("light-gray"), i18nc("@item:inlistbox Refers to color.", "Light Gray"), QColorConstants::Svg::lightgray},
        {QStringLiteral("light-magenta"), i18nc("@item:inlistbox Refers to color.", "Light Magenta"), QColorConstants::Svg::magenta},
        {QStringLiteral("light-red"), i18nc("@item:inlistbox Refers to color.", "Light Red"), QColorConstants::Svg::orangered},
        {QStringLiteral("magenta"), i18nc("@item:inlistbox Refers to color.", "Magenta"), QColorConstants::Svg::darkmagenta},
        {QStringLiteral("red"), i18nc("@item:inlistbox Refers to color.", "Red"), QColorConstants::Svg::red},
        {QStringLiteral("white"), i18nc("@item:inlistbox Refers to color.", "White"), QColorConstants::Svg::white},
        {QStringLiteral("yellow"), i18nc("@item:inlistbox Refers to color.", "Yellow"), QColorConstants::Svg::yellow},
    });

    ui->combobox_normalForeground->setCurrentIndex(ui->combobox_normalForeground->findData(QLatin1String("light-gray")));
    ui->combobox_normalBackground->setCurrentIndex(ui->combobox_normalBackground->findData(QLatin1String("black")));
    ui->combobox_highlightForeground->setCurrentIndex(ui->combobox_highlightForeground->findData(QLatin1String("black")));
    ui->combobox_highlightBackground->setCurrentIndex(ui->combobox_highlightBackground->findData(QLatin1String("light-gray")));

    ui->pushbutton_preview->setIcon(QIcon::fromTheme(QStringLiteral("image-png")));
    ui->pushbutton_create->setIcon(QIcon::fromTheme(QStringLiteral("insert-image")));
    ui->pushbutton_create->setVisible(HAVE_IMAGEMAGICK);

    ui->pushbutton_cmdlineDefaultSuggestions->setIcon(QIcon::fromTheme(QStringLiteral("tools-wizard")));
    ui->pushbutton_cmdlineDefaultSuggestions->setMenu(new QMenu(ui->pushbutton_cmdlineDefaultSuggestions));
    ui->pushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Quiet Boot"))->setData(QLatin1String("quiet"));
    ui->pushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Show Splash Screen"))->setData(QLatin1String("splash"));
    ui->pushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Disable Plymouth"))->setData(QLatin1String("noplymouth"));
    ui->pushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off ACPI"))->setData(QLatin1String("acpi=off"));
    ui->pushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off APIC"))->setData(QLatin1String("noapic"));
    ui->pushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off Local APIC"))->setData(QLatin1String("nolapic"));
    ui->pushbutton_cmdlineDefaultSuggestions->menu()->addAction(i18nc("@action:inmenu", "Single User Mode"))->setData(QLatin1String("single"));
    ui->pushbutton_cmdlineSuggestions->setIcon(QIcon::fromTheme(QStringLiteral("tools-wizard")));
    ui->pushbutton_cmdlineSuggestions->setMenu(new QMenu(ui->pushbutton_cmdlineSuggestions));
    ui->pushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Quiet Boot"))->setData(QLatin1String("quiet"));
    ui->pushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Show Splash Screen"))->setData(QLatin1String("splash"));
    ui->pushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Disable Plymouth"))->setData(QLatin1String("noplymouth"));
    ui->pushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off ACPI"))->setData(QLatin1String("acpi=off"));
    ui->pushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off APIC"))->setData(QLatin1String("noapic"));
    ui->pushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Turn Off Local APIC"))->setData(QLatin1String("nolapic"));
    ui->pushbutton_cmdlineSuggestions->menu()->addAction(i18nc("@action:inmenu", "Single User Mode"))->setData(QLatin1String("single"));
    ui->pushbutton_terminalSuggestions->setIcon(QIcon::fromTheme(QStringLiteral("tools-wizard")));
    ui->pushbutton_terminalSuggestions->setMenu(new QMenu(ui->pushbutton_terminalSuggestions));
    ui->pushbutton_terminalSuggestions->menu()->addAction(i18nc("@action:inmenu", "PC BIOS && EFI Console"))->setData(QLatin1String("console"));
    ui->pushbutton_terminalSuggestions->menu()->addAction(i18nc("@action:inmenu", "Serial Terminal"))->setData(QLatin1String("serial"));
    ui->pushbutton_terminalSuggestions->menu()
        ->addAction(i18nc("@action:inmenu 'Open' is an adjective here, not a verb. 'Open Firmware' is a former IEEE standard.", "Open Firmware Console"))
        ->setData(QLatin1String("ofconsole"));
    ui->pushbutton_terminalInputSuggestions->setIcon(QIcon::fromTheme(QStringLiteral("tools-wizard")));
    ui->pushbutton_terminalInputSuggestions->setMenu(new QMenu(ui->pushbutton_terminalInputSuggestions));
    ui->pushbutton_terminalInputSuggestions->menu()->addAction(i18nc("@action:inmenu", "PC BIOS && EFI Console"))->setData(QLatin1String("console"));
    ui->pushbutton_terminalInputSuggestions->menu()->addAction(i18nc("@action:inmenu", "Serial Terminal"))->setData(QLatin1String("serial"));
    ui->pushbutton_terminalInputSuggestions->menu()
        ->addAction(i18nc("@action:inmenu 'Open' is an adjective here, not a verb. 'Open Firmware' is a former IEEE standard.", "Open Firmware Console"))
        ->setData(QLatin1String("ofconsole"));
    ui->pushbutton_terminalInputSuggestions->menu()->addAction(i18nc("@action:inmenu", "PC AT Keyboard (Coreboot)"))->setData(QLatin1String("at_keyboard"));
    ui->pushbutton_terminalInputSuggestions->menu()
        ->addAction(i18nc("@action:inmenu", "USB Keyboard (HID Boot Protocol)"))
        ->setData(QLatin1String("usb_keyboard"));
    ui->pushbutton_terminalOutputSuggestions->setIcon(QIcon::fromTheme(QStringLiteral("tools-wizard")));
    ui->pushbutton_terminalOutputSuggestions->setMenu(new QMenu(ui->pushbutton_terminalOutputSuggestions));
    ui->pushbutton_terminalOutputSuggestions->menu()->addAction(i18nc("@action:inmenu", "PC BIOS && EFI Console"))->setData(QLatin1String("console"));
    ui->pushbutton_terminalOutputSuggestions->menu()->addAction(i18nc("@action:inmenu", "Serial Terminal"))->setData(QLatin1String("serial"));
    ui->pushbutton_terminalOutputSuggestions->menu()
        ->addAction(i18nc("@action:inmenu 'Open' is an adjective here, not a verb. 'Open Firmware' is a former IEEE standard.", "Open Firmware Console"))
        ->setData(QLatin1String("ofconsole"));
    ui->pushbutton_terminalOutputSuggestions->menu()->addAction(i18nc("@action:inmenu", "Graphics Mode Output"))->setData(QLatin1String("gfxterm"));
    ui->pushbutton_terminalOutputSuggestions->menu()->addAction(i18nc("@action:inmenu", "VGA Text Output (Coreboot)"))->setData(QLatin1String("vga_text"));

    ui->pushbutton_install->setIcon(QIcon::fromTheme(QStringLiteral("system-software-update")));
}
void KCMGRUB2::setupConnections()
{
    connect(ui->combobox_default, qOverload<int>(&QComboBox::activated), this, &KCMGRUB2::markAsChanged);
    connect(ui->pushbutton_remove, &QAbstractButton::clicked, this, &KCMGRUB2::slotRemoveOldEntries);
    connect(ui->checkBox_savedefault, &QAbstractButton::clicked, this, &KCMGRUB2::slotGrubSavedefaultChanged);

    connect(ui->checkBox_hiddenTimeout, &QAbstractButton::toggled, this, &KCMGRUB2::slotGrubHiddenTimeoutToggled);
    connect(ui->checkBox_hiddenTimeout, &QAbstractButton::clicked, this, &KCMGRUB2::slotGrubHiddenTimeoutChanged);
    connect(ui->spinBox_hiddenTimeout, qOverload<int>(&QSpinBox::valueChanged), this, &KCMGRUB2::slotGrubHiddenTimeoutChanged);
    connect(ui->checkBox_hiddenTimeoutShowTimer, &QAbstractButton::clicked, this, &KCMGRUB2::slotGrubHiddenTimeoutQuietChanged);

    connect(ui->checkBox_timeout, &QAbstractButton::toggled, this, &KCMGRUB2::slotGrubTimeoutToggled);
    connect(ui->checkBox_timeout, &QAbstractButton::clicked, this, &KCMGRUB2::slotGrubTimeoutChanged);
    connect(ui->radioButton_timeout0, &QAbstractButton::clicked, this, &KCMGRUB2::slotGrubTimeoutChanged);
    connect(ui->radioButton_timeout, &QAbstractButton::toggled, ui->spinBox_timeout, &QWidget::setEnabled);
    connect(ui->radioButton_timeout, &QAbstractButton::clicked, this, &KCMGRUB2::slotGrubTimeoutChanged);
    connect(ui->spinBox_timeout, qOverload<int>(&QSpinBox::valueChanged), this, &KCMGRUB2::slotGrubTimeoutChanged);

    connect(ui->combobox_language, qOverload<int>(&QComboBox::activated), this, &KCMGRUB2::slotGrubLanguageChanged);
    connect(ui->checkBox_recovery, &QAbstractButton::clicked, this, &KCMGRUB2::slotGrubDisableRecoveryChanged);
    connect(ui->checkBox_memtest, &QAbstractButton::clicked, this, &KCMGRUB2::slotMemtestChanged);
    connect(ui->checkBox_osProber, &QAbstractButton::clicked, this, &KCMGRUB2::slotGrubDisableOsProberChanged);

    connect(ui->combobox_gfxmode, qOverload<int>(&QComboBox::activated), this, &KCMGRUB2::slotGrubGfxmodeChanged);
    connect(ui->toolButton_refreshGfxmode, &QAbstractButton::clicked, this, &KCMGRUB2::slotResolutionsRefresh);
    connect(ui->combobox_gfxpayload, qOverload<int>(&QComboBox::activated), this, &KCMGRUB2::slotGrubGfxpayloadLinuxChanged);
    connect(ui->toolButton_refreshGfxpayload, &QAbstractButton::clicked, this, &KCMGRUB2::slotResolutionsRefresh);

    connect(ui->combobox_normalForeground, qOverload<int>(&QComboBox::activated), this, &KCMGRUB2::slotGrubColorNormalChanged);
    connect(ui->combobox_normalBackground, qOverload<int>(&QComboBox::activated), this, &KCMGRUB2::slotGrubColorNormalChanged);
    connect(ui->combobox_highlightForeground, qOverload<int>(&QComboBox::activated), this, &KCMGRUB2::slotGrubColorHighlightChanged);
    connect(ui->combobox_highlightBackground, qOverload<int>(&QComboBox::activated), this, &KCMGRUB2::slotGrubColorHighlightChanged);

    connect(ui->kurlrequester_background, &KUrlRequester::textChanged, this, &KCMGRUB2::slowGrubBackgroundChanged);
    connect(ui->pushbutton_preview, &QAbstractButton::clicked, this, &KCMGRUB2::slotPreviewGrubBackground);
    connect(ui->pushbutton_create, &QAbstractButton::clicked, this, &KCMGRUB2::slotCreateGrubBackground);
    connect(ui->kurlrequester_theme, &KUrlRequester::textChanged, this, &KCMGRUB2::slotGrubThemeChanged);

    connect(ui->lineedit_cmdlineDefault, &QLineEdit::textEdited, this, &KCMGRUB2::slotGrubCmdlineLinuxDefaultChanged);
    connect(ui->pushbutton_cmdlineDefaultSuggestions->menu(), &QMenu::aboutToShow, this, &KCMGRUB2::slotUpdateSuggestions);
    connect(ui->pushbutton_cmdlineDefaultSuggestions->menu(), &QMenu::triggered, this, &KCMGRUB2::slotTriggeredSuggestion);
    connect(ui->lineedit_cmdline, &QLineEdit::textEdited, this, &KCMGRUB2::slotGrubCmdlineLinuxChanged);
    connect(ui->pushbutton_cmdlineSuggestions->menu(), &QMenu::aboutToShow, this, &KCMGRUB2::slotUpdateSuggestions);
    connect(ui->pushbutton_cmdlineSuggestions->menu(), &QMenu::triggered, this, &KCMGRUB2::slotTriggeredSuggestion);

    connect(ui->lineedit_terminal, &QLineEdit::textEdited, this, &KCMGRUB2::slotGrubTerminalChanged);
    connect(ui->pushbutton_terminalSuggestions->menu(), &QMenu::aboutToShow, this, &KCMGRUB2::slotUpdateSuggestions);
    connect(ui->pushbutton_terminalSuggestions->menu(), &QMenu::triggered, this, &KCMGRUB2::slotTriggeredSuggestion);
    connect(ui->lineedit_terminalInput, &QLineEdit::textEdited, this, &KCMGRUB2::slotGrubTerminalInputChanged);
    connect(ui->pushbutton_terminalInputSuggestions->menu(), &QMenu::aboutToShow, this, &KCMGRUB2::slotUpdateSuggestions);
    connect(ui->pushbutton_terminalInputSuggestions->menu(), &QMenu::triggered, this, &KCMGRUB2::slotTriggeredSuggestion);
    connect(ui->lineedit_terminalOutput, &QLineEdit::textEdited, this, &KCMGRUB2::slotGrubTerminalOutputChanged);
    connect(ui->pushbutton_terminalOutputSuggestions->menu(), &QMenu::aboutToShow, this, &KCMGRUB2::slotUpdateSuggestions);
    connect(ui->pushbutton_terminalOutputSuggestions->menu(), &QMenu::triggered, this, &KCMGRUB2::slotTriggeredSuggestion);

    connect(ui->lineedit_distributor, &QLineEdit::textEdited, this, &KCMGRUB2::slotGrubDistributorChanged);
    connect(ui->lineedit_serial, &QLineEdit::textEdited, this, &KCMGRUB2::slotGrubSerialCommandChanged);
    connect(ui->lineedit_initTune, &QLineEdit::textEdited, this, &KCMGRUB2::slotGrubInitTuneChanged);
    connect(ui->checkBox_uuid, &QAbstractButton::clicked, this, &KCMGRUB2::slotGrubDisableLinuxUuidChanged);

    connect(ui->pushbutton_install, &QAbstractButton::clicked, this, &KCMGRUB2::slotInstallBootloader);
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
        m_locales = QDir(grubLocalePath())
                        .entryList(QStringList() << QStringLiteral("*.mo"), QDir::Files)
                        .replaceInStrings(QRegExp(QLatin1String("\\.mo$")), QString());
    } else {
        operations |= Locales;
    }

    // Do not prompt for password if only the VBE operation is required, unless forced.
    if (operations && ((operations & (~Vbe)) || m_resolutionsForceRead)) {
        Action loadAction(QStringLiteral("org.kde.kcontrol.kcmgrub2.load"));
        loadAction.setHelperId(QStringLiteral("org.kde.kcontrol.kcmgrub2"));
        loadAction.addArgument(QStringLiteral("operations"), (int)(operations));
        loadAction.setParentWidget(this);

        KAuth::ExecuteJob *loadJob = loadAction.execute();
        if (!loadJob->exec()) {
            qCritical() << "KAuth error!";
            qCritical() << "Error code:" << loadJob->error();
            qCritical() << "Error description:" << loadJob->errorText();
            return;
        }

        if (operations.testFlag(MenuFile)) {
            if (loadJob->data().value(QStringLiteral("menuSuccess")).toBool()) {
                parseEntries(QString::fromUtf8(loadJob->data().value(QStringLiteral("menuContents")).toByteArray().constData()));
            } else {
                qCritical() << "Helper failed to read file:" << grubMenuPath();
                qCritical() << "Error code:" << loadJob->data().value(QStringLiteral("menuError")).toInt();
                qCritical() << "Error description:" << loadJob->data().value(QStringLiteral("menuErrorString")).toString();
            }
        }
        if (operations.testFlag(ConfigurationFile)) {
            if (loadJob->data().value(QStringLiteral("configSuccess")).toBool()) {
                parseSettings(QString::fromUtf8(loadJob->data().value(QStringLiteral("configContents")).toByteArray().constData()));
            } else {
                qCritical() << "Helper failed to read file:" << grubConfigPath();
                qCritical() << "Error code:" << loadJob->data().value(QStringLiteral("configError")).toInt();
                qCritical() << "Error description:" << loadJob->data().value(QStringLiteral("configErrorString")).toString();
            }
        }
        if (operations.testFlag(EnvironmentFile)) {
            if (loadJob->data().value(QStringLiteral("envSuccess")).toBool()) {
                parseEnv(QString::fromUtf8(loadJob->data().value(QStringLiteral("envContents")).toByteArray().constData()));
            } else {
                qCritical() << "Helper failed to read file:" << grubEnvPath();
                qCritical() << "Error code:" << loadJob->data().value(QStringLiteral("envError")).toInt();
                qCritical() << "Error description:" << loadJob->data().value(QStringLiteral("envErrorString")).toString();
            }
        }
        if (operations.testFlag(MemtestFile)) {
            m_memtest = loadJob->data().value(QStringLiteral("memtest")).toBool();
            if (m_memtest) {
                m_memtestOn = loadJob->data().value(QStringLiteral("memtestOn")).toBool();
            }
        }
        if (operations.testFlag(Vbe)) {
            m_resolutions = loadJob->data().value(QStringLiteral("gfxmodes")).toStringList();
            m_resolutionsEmpty = false;
            m_resolutionsForceRead = false;
        }
        if (operations.testFlag(Locales)) {
            m_locales = loadJob->data().value(QStringLiteral("locales")).toStringList();
        }
    }
}

void KCMGRUB2::showLocales()
{
    ui->combobox_language->clear();
    ui->combobox_language->addItem(i18nc("@item:inlistbox", "No translation"), QString());

    for (const QString &locale : qAsConst(m_locales)) {
        QString language = QLocale(locale).nativeLanguageName();
        if (language.isEmpty()) {
            language = QLocale(locale.split(QLatin1Char('@')).first()).nativeLanguageName();
            if (language.isEmpty()) {
                language = QLocale(locale.split(QLatin1Char('@')).first().split(QLatin1Char('_')).first()).nativeLanguageName();
            }
        }
        ui->combobox_language->addItem(QStringLiteral("%1 (%2)").arg(language, locale), locale);
    }
}
void KCMGRUB2::sortResolutions()
{
    for (int i = 0; i < m_resolutions.size(); i++) {
        if (m_resolutions.at(i).contains(QRegExp(QLatin1String("^\\d{3,4}x\\d{3,4}$")))) {
            m_resolutions[i] = QStringLiteral("%1x%2x0").arg(m_resolutions.at(i).section(QLatin1Char('x'), 0, 0).rightJustified(4, QLatin1Char('0')),
                                                             m_resolutions.at(i).section(QLatin1Char('x'), 1).rightJustified(4, QLatin1Char('0')));
        } else if (m_resolutions.at(i).contains(QRegExp(QLatin1String("^\\d{3,4}x\\d{3,4}x\\d{1,2}$")))) {
            m_resolutions[i] = QStringLiteral("%1x%2x%3")
                                   .arg(m_resolutions.at(i).section(QLatin1Char('x'), 0, 0).rightJustified(4, QLatin1Char('0')),
                                        m_resolutions.at(i).section(QLatin1Char('x'), 1, 1).rightJustified(4, QLatin1Char('0')),
                                        m_resolutions.at(i).section(QLatin1Char('x'), 2).rightJustified(2, QLatin1Char('0')));
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

    for (const QString &resolution : qAsConst(m_resolutions)) {
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
        entry.chop(1); // remove trailing space
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
        // Read the first word of the line
        stream >> word;
        if (stream.atEnd()) {
            return;
        }
        // If the first word is known, process the rest of the line
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
        // Drop the rest of the line
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
