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
#include "removeDlg.h"

//Qt
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>

//KDE
#include <KMessageBox>
#include <KProgressDialog>

RemoveDialog::RemoveDialog(const QStringList &entries, const QHash<QString, QString> &kernels, QWidget *parent, Qt::WFlags flags) : KDialog(parent, flags)
{
    QWidget *widget = new QWidget(this);
    ui.setupUi(widget);
    setMainWidget(widget);
    enableButtonOk(false);
    setWindowTitle(i18nc("@title:window", "Remove Old Entries"));

    m_progressDlg = 0;

#if HAVE_QAPT
    m_backend = new QAptBackend;
#elif HAVE_QPACKAGEKIT
    m_backend = new QPkBackend;
#endif

    KProgressDialog progressDlg(this, i18nc("@title:window", "Finding Old Entries"), i18nc("@info:progress", "Finding Old Entries..."));
    progressDlg.setAllowCancel(false);
    progressDlg.setModal(true);
    progressDlg.show();
    bool found = false;
    for (int i = 0; i < entries.size(); i++) {
        progressDlg.progressBar()->setValue(100. / entries.size() * (i + 1));
        QString file = kernels.value(entries.at(i));
        if (file.isEmpty()) {
            continue;
        }
        QString packageName = m_backend->ownerPackage(file);
        if (!packageName.isEmpty()) {
            QListWidgetItem *item = new QListWidgetItem(entries.at(i), ui.klistwidget);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            item->setData(Qt::UserRole, packageName);
            item->setData(Qt::UserRole + 1, file);
            item->setCheckState(Qt::Unchecked);
            ui.klistwidget->addItem(item);
            found = true;
        }
    }
    if (found) {
        ui.klistwidget->setMinimumSize(ui.klistwidget->sizeHintForColumn(0) + ui.klistwidget->sizeHintForRow(0), ui.klistwidget->sizeHintForRow(0) * ui.klistwidget->count());
        connect(ui.klistwidget, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(slotItemChanged(QListWidgetItem*)));
        detectCurrentKernelImage();
    } else {
        KMessageBox::sorry(this, i18nc("@info", "No removable entries were found."));
        QTimer::singleShot(0, this, SLOT(reject()));
    }
}
RemoveDialog::~RemoveDialog()
{
    delete m_backend;
}
void RemoveDialog::slotButtonClicked(int button)
{
    if (button == KDialog::Ok) {
        QStringList packageNames;
        for (int i = 0; i < ui.klistwidget->count(); i++) {
            if (ui.klistwidget->item(i)->checkState() == Qt::Checked) {
                QString packageName = ui.klistwidget->item(i)->data(Qt::UserRole).toString();
                m_backend->markForRemoval(packageName);
                if (ui.checkBox_headers->isChecked()) {
                    packageName.replace("image", "headers");
                    m_backend->markForRemoval(packageName);
                }
            }
        }
        if (KMessageBox::questionYesNoList(this, i18nc("@info", "Are you sure you want to remove the following packages?"), m_backend->markedForRemoval()) == KMessageBox::Yes) {
            connect(m_backend, SIGNAL(progress(QString,int)), this, SLOT(slotProgress(QString,int)));
            connect(m_backend, SIGNAL(finished(bool)), this, SLOT(slotFinished(bool)));
            m_backend->removePackages();
        } else {
            m_backend->undoChanges();
        }
        return;
    }
    KDialog::slotButtonClicked(button);
}
void RemoveDialog::slotItemChanged(QListWidgetItem *item)
{
    bool state = false;
    ui.klistwidget->blockSignals(true);
    for (int i = 0; i < ui.klistwidget->count(); i++) {
        if (ui.klistwidget->item(i)->data(Qt::UserRole) == item->data(Qt::UserRole)) {
            ui.klistwidget->item(i)->setCheckState(item->checkState());
        }
        if (ui.klistwidget->item(i)->checkState() == Qt::Checked) {
            state = true;
        }
    }
    ui.klistwidget->blockSignals(false);
    enableButtonOk(state);
    if (item->checkState() == Qt::Checked && item->data(Qt::UserRole + 1).toString().compare(m_currentKernelImage) == 0) {
        if (KMessageBox::warningYesNo(this, i18nc("@info", "This is your current kernel!<br/>Are you sure you want to remove it?")) == KMessageBox::No) {
            item->setCheckState(Qt::Unchecked);
        }
    }
}
void RemoveDialog::slotProgress(const QString &status, int percentage)
{
    if (!m_progressDlg) {
        m_progressDlg = new KProgressDialog(this, i18nc("@title:window", "Removing Old Entries"));
        m_progressDlg->setAllowCancel(false);
        m_progressDlg->setModal(true);
        m_progressDlg->show();
    }
    m_progressDlg->setLabelText(status);
    m_progressDlg->progressBar()->setValue(percentage);
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
    QFile file("/proc/cmdline");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QTextStream stream(&file);
    Q_FOREACH(const QString &argument, stream.readAll().split(QRegExp("\\s+"))) {
        if (argument.startsWith("BOOT_IMAGE")) {
            m_currentKernelImage = argument.section('=', 1);
            return;
        }
    }
}
