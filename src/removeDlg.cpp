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
#include <QtCore/QTimer>

//KDE
#include <KMessageBox>
#include <KProgressDialog>

//QApt
#include <libqapt/backend.h>

RemoveDialog::RemoveDialog(const QStringList &entries, const QHash<QString, QString> &kernels, QWidget *parent, Qt::WFlags flags) : KDialog(parent, flags)
{
    QWidget *widget = new QWidget(this);
    ui.setupUi(widget);
    setMainWidget(widget);
    enableButtonOk(false);
    setWindowTitle(i18nc("@title:window", "Remove Old Entries"));

    m_backend = new QApt::Backend;
    m_backend->init();
    m_progressDlg = 0;

    bool found = false;
    QApt::Package *package;
    foreach(const QString &entry, entries) {
        QString file = kernels.value(entry);
        if (!file.isEmpty() && (package = m_backend->packageForFile(file))) {
            QListWidgetItem *item = new QListWidgetItem(entry, ui.klistwidget);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            item->setData(Qt::UserRole, package->name());
            item->setCheckState(Qt::Unchecked);
            ui.klistwidget->addItem(item);
            found = true;
        }
    }
    if (!found) {
        KMessageBox::sorry(this, i18nc("@info", "No removable entries were found."));
        QTimer::singleShot(0, this, SLOT(reject()));
    }
    ui.klistwidget->setMinimumSize(ui.klistwidget->sizeHintForColumn(0) + ui.klistwidget->sizeHintForRow(0), ui.klistwidget->sizeHintForRow(0) * ui.klistwidget->count());
    connect(ui.klistwidget, SIGNAL(itemChanged(QListWidgetItem *)), this, SLOT(slotItemChanged(QListWidgetItem *)));
}
RemoveDialog::~RemoveDialog()
{
    delete m_backend;
}
void RemoveDialog::slotButtonClicked(int button)
{
    if (button == KDialog::Ok) {
        for (int i = 0; i < ui.klistwidget->count(); i++) {
            if (ui.klistwidget->item(i)->checkState() == Qt::Checked) {
                QString packageName = ui.klistwidget->item(i)->data(Qt::UserRole).toString();
                m_backend->markPackageForRemoval(packageName);
                if (ui.checkBox_headers->isChecked()) {
                    packageName.replace("image", "headers");
                    packageName = packageName.left(packageName.lastIndexOf('-'));
                    m_backend->markPackageForRemoval(packageName);
                }
            }
        }
        connect(m_backend, SIGNAL(commitProgress(QString, int)), this, SLOT(slotCommitProgress(QString, int)));
        m_backend->commitChanges();
        return;
    }
    KDialog::slotButtonClicked(button);
}
void RemoveDialog::slotItemChanged(QListWidgetItem *item)
{
    bool state = false;
    for (int i = 0; i < ui.klistwidget->count(); i++) {
        if (ui.klistwidget->item(i)->data(Qt::UserRole) == item->data(Qt::UserRole)) {
            ui.klistwidget->item(i)->setCheckState(item->checkState());
        }
        if (ui.klistwidget->item(i)->checkState() == Qt::Checked) {
            state = true;
        }
    }
    enableButtonOk(state);
}
void RemoveDialog::slotCommitProgress(const QString &status, int percentage)
{
    if (percentage == 100) {
        accept();
    }
    if (!m_progressDlg) {
        m_progressDlg = new KProgressDialog(this, i18nc("@title:window", "Removing Old Entries"));
        m_progressDlg->setAllowCancel(false);
        m_progressDlg->setModal(true);
        m_progressDlg->show();
    }
    m_progressDlg->setLabelText(status);
    m_progressDlg->progressBar()->setValue(percentage);
}
