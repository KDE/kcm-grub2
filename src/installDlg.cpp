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

//Own
#include "installDlg.h"

//Qt
#include <QFile>
#include <QRadioButton>
#include <QPushButton>

//KDE
#include <KGlobal>
#include <KLocalizedString>
#include <KIcon>
#include <KMessageBox>
#include <KProgressDialog>
#include <KAuthAction>
#include <KAuthExecuteJob>
using namespace KAuth;
#include <Solid/Device>
#include <Solid/StorageAccess>
#include <Solid/StorageVolume>

//Ui
#include "ui_installDlg.h"

InstallDialog::InstallDialog(QWidget *parent, Qt::WFlags flags) : KDialog(parent, flags)
{
    QWidget *widget = new QWidget(this);
    ui = new Ui::InstallDialog;
    ui->setupUi(widget);
    setMainWidget(widget);
    enableButtonOk(false);
    setWindowTitle(i18nc("@title:window", "Install/Recover Bootloader"));
    setWindowIcon(KIcon(QLatin1String("system-software-update")));
    if (parent) {
        setInitialSize(parent->size());
    }

    ui->treeWidget_recover->headerItem()->setText(0, QString());
    ui->treeWidget_recover->header()->setResizeMode(QHeaderView::Stretch);
    ui->treeWidget_recover->header()->setResizeMode(0, QHeaderView::ResizeToContents);
    Q_FOREACH(const Solid::Device &device, Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess)) {
        if (!device.is<Solid::StorageAccess>() || !device.is<Solid::StorageVolume>()) {
            continue;
        }
        const Solid::StorageAccess *partition = device.as<Solid::StorageAccess>();
        if (!partition) {
            continue;
        }
        const Solid::StorageVolume *volume = device.as<Solid::StorageVolume>();
        if (!volume || volume->usage() != Solid::StorageVolume::FileSystem) {
            continue;
        }

        QString uuidDir = QLatin1String("/dev/disk/by-uuid/"), uuid = volume->uuid(), name;
        name = (QFile::exists((name = uuidDir + uuid)) || QFile::exists((name = uuidDir + uuid.toLower())) || QFile::exists((name = uuidDir + uuid.toUpper())) ? QFile::symLinkTarget(name) : QString());
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeWidget_recover, QStringList() << QString() << name << partition->filePath() << volume->label() << volume->fsType() << KGlobal::locale()->formatByteSize(volume->size()));
        item->setIcon(1, KIcon(device.icon()));
        item->setTextAlignment(5, Qt::AlignRight | Qt::AlignVCenter);
        ui->treeWidget_recover->addTopLevelItem(item);
        QRadioButton *radio = new QRadioButton(ui->treeWidget_recover);
        connect(radio, SIGNAL(clicked(bool)), this, SLOT(enableButtonOk(bool)));
        ui->treeWidget_recover->setItemWidget(item, 0, radio);
    }
}
InstallDialog::~InstallDialog()
{
    delete ui;
}

void InstallDialog::slotButtonClicked(int button)
{
    if (button == KDialog::Ok) {
        Action installAction(QLatin1String("org.kde.kcontrol.kcmgrub2.install"));
        installAction.setHelperId(QLatin1String("org.kde.kcontrol.kcmgrub2"));
        for (int i = 0; i < ui->treeWidget_recover->topLevelItemCount(); i++) {
            QRadioButton *radio = qobject_cast<QRadioButton *>(ui->treeWidget_recover->itemWidget(ui->treeWidget_recover->topLevelItem(i), 0));
            if (radio && radio->isChecked()) {
                installAction.addArgument(QLatin1String("partition"), ui->treeWidget_recover->topLevelItem(i)->text(1));
                installAction.addArgument(QLatin1String("mountPoint"), ui->treeWidget_recover->topLevelItem(i)->text(2));
                installAction.addArgument(QLatin1String("mbrInstall"), !ui->checkBox_partition->isChecked());
                break;
            }
        }
        if (installAction.arguments().value(QLatin1String("partition")).toString().isEmpty()) {
            KMessageBox::sorry(this, i18nc("@info", "Sorry, you have to select a partition with a proper name!"));
            return;
        }
        installAction.setParentWidget(this);

        KAuth::ExecuteJob *installJob = installAction.execute(KAuth::Action::AuthorizeOnlyMode);
        if (!installJob->exec()) {
            return;
        }

        KProgressDialog progressDlg(this, i18nc("@title:window", "Installing"), i18nc("@info:progress", "Installing GRUB..."));
        progressDlg.setAllowCancel(false);
        progressDlg.setModal(true);
        progressDlg.progressBar()->setMinimum(0);
        progressDlg.progressBar()->setMaximum(0);
        progressDlg.show();
        installJob = installAction.execute();
        connect(installJob, SIGNAL(finished(KJob*)), &progressDlg, SLOT(hide()));

        if (installJob->exec()) {
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

            KMessageBox::createKMessageBox(dialog, buttonBox, QMessageBox::Information, i18nc("@info", "Successfully installed GRUB."),
                                           QStringList(), QString(), nullptr, KMessageBox::Notify,
                                           QString::fromUtf8(installJob->data().value(QLatin1String("output")).toByteArray())); // krazy:exclude=qclasses
        } else {
            KMessageBox::detailedError(this, i18nc("@info", "Failed to install GRUB."), installJob->errorText());
        }
    }
    KDialog::slotButtonClicked(button);
}
