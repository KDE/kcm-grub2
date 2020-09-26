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
#include "removeDlg.h"

//Qt
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QIcon>
#include <QProgressDialog>
#include <QPushButton>

//KDE
#include <KLocalizedString>
#include <KMessageBox>

//Project
#include "entry.h"

//Ui
#include "ui_removeDlg.h"

RemoveDialog::RemoveDialog(const QList<Entry> &entries, QWidget *parent) : QDialog(parent)
{
    QWidget *widget = new QWidget(this);
    ui = new Ui::RemoveDialog;
    ui->setupUi(widget);
    ui->gridLayout->setContentsMargins(0, 0, 0, 0);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &RemoveDialog::slotAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    m_okButton = buttonBox->button(QDialogButtonBox::Ok);
    m_okButton->setEnabled(false);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    setLayout(mainLayout);
    mainLayout->addWidget(widget);
    mainLayout->addWidget(buttonBox);

    setWindowTitle(i18nc("@title:window", "Remove Old Entries"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("list-remove")));

    m_progressDlg = 0;

#if HAVE_QAPT
    m_backend = new QAptBackend;
#elif HAVE_QPACKAGEKIT
    m_backend = new QPkBackend;
#endif

    detectCurrentKernelImage();
    QProgressDialog progressDlg(this);
    progressDlg.setWindowTitle(i18nc("@title:window", "Finding Old Entries"));
    progressDlg.setLabelText(i18nc("@info:progress", "Finding Old Entries..."));
    progressDlg.setCancelButton(nullptr);
    progressDlg.setModal(true);
    progressDlg.show();
    bool found = false;
    for (int i = 0; i < entries.size(); i++) {
        progressDlg.setValue(100. / entries.size() * (i + 1));
        QString file = entries.at(i).kernel();
        if (file.isEmpty() || file == m_currentKernelImage) {
            continue;
        }
        QStringList package = m_backend->ownerPackage(file);
        if (package.size() < 2) {
            continue;
        }
        found = true;
        const QString packageName = package.takeFirst();
        const QString packageVersion = package.takeFirst();
        QTreeWidgetItem *item = 0;
        for (int j = 0; j < ui->treeWidget->topLevelItemCount(); j++) {
            if (ui->treeWidget->topLevelItem(j)->data(0, Qt::UserRole).toString() == packageName && ui->treeWidget->topLevelItem(j)->data(0, Qt::UserRole + 1).toString() == packageVersion) {
                item = ui->treeWidget->topLevelItem(j);
                break;
            }
        }
        if (!item) {
            item = new QTreeWidgetItem(ui->treeWidget, QStringList(i18nc("@item:inlistbox", "Kernel %1", packageVersion)));
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            item->setData(0, Qt::UserRole, packageName);
            item->setData(0, Qt::UserRole + 1, packageVersion);
            item->setCheckState(0, Qt::Checked);
            ui->treeWidget->addTopLevelItem(item);
        }
        item->addChild(new QTreeWidgetItem(QStringList(entries.at(i).prettyTitle())));
    }
    if (found) {
        ui->treeWidget->expandAll();
        ui->treeWidget->resizeColumnToContents(0);
        ui->treeWidget->setMinimumWidth(ui->treeWidget->columnWidth(0) + ui->treeWidget->sizeHintForRow(0));
        connect(ui->treeWidget, &QTreeWidget::itemChanged, this, &RemoveDialog::slotItemChanged);
        m_okButton->setEnabled(true);
    } else {
        KMessageBox::sorry(this, i18nc("@info", "No removable entries were found."));
        QTimer::singleShot(0, this, &QDialog::reject);
    }
}
RemoveDialog::~RemoveDialog()
{
    delete m_backend;
    delete ui;
}
void RemoveDialog::slotAccepted()
{
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++) {
        if (ui->treeWidget->topLevelItem(i)->checkState(0) == Qt::Checked) {
            QString packageName = ui->treeWidget->topLevelItem(i)->data(0, Qt::UserRole).toString();
            m_backend->markForRemoval(packageName);
            if (ui->checkBox_headers->isChecked()) {
                packageName.replace(QLatin1String("image"), QLatin1String("headers"));
                m_backend->markForRemoval(packageName);
            }
        }
    }
    if (KMessageBox::questionYesNoList(this, i18nc("@info", "Are you sure you want to remove the following packages?"), m_backend->markedForRemoval()) == KMessageBox::Yes) {
        connect(m_backend, &QPkBackend::progress, this, &RemoveDialog::slotProgress);
        connect(m_backend, &QPkBackend::finished, this, &RemoveDialog::slotFinished);
        m_backend->removePackages();
    } else {
        m_backend->undoChanges();
    }

    accept();
}
void RemoveDialog::slotItemChanged()
{
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++) {
        if (ui->treeWidget->topLevelItem(i)->checkState(0) == Qt::Checked) {
            m_okButton->setEnabled(true);
            return;
        }
    }
    m_okButton->setEnabled(false);
}
void RemoveDialog::slotProgress(const QString &status, int percentage)
{
    if (!m_progressDlg) {
        m_progressDlg = new QProgressDialog(this);
        m_progressDlg->setWindowTitle(i18nc("@title:window", "Removing Old Entries"));
        m_progressDlg->setCancelButton(nullptr);
        m_progressDlg->setModal(true);
        m_progressDlg->show();
    }
    m_progressDlg->setLabelText(status);
    m_progressDlg->setValue(percentage);
}
void RemoveDialog::slotFinished(bool success)
{
    if (success) {
        accept();
    } else {
        KMessageBox::error(this, i18nc("@info", "Package removal failed."));
        reject();
    }
}
void RemoveDialog::detectCurrentKernelImage()
{
    QFile file(QStringLiteral("/proc/cmdline"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    const QStringList args = QTextStream(&file).readAll().split(QRegExp(QLatin1String("\\s+")));
    for (const QString &argument : args) {
        if (argument.startsWith(QLatin1String("BOOT_IMAGE"))) {
            m_currentKernelImage = argument.section(QLatin1Char('='), 1);
            return;
        }
    }
}
