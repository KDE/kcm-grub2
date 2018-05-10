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
#include "convertDlg.h"

//KDE
#include <KMessageBox>
#include <KMimeType>
#include <KLocalizedString>
#include <KUrl>

//ImageMagick
#include <Magick++.h>

//Ui
#include "ui_convertDlg.h"

ConvertDialog::ConvertDialog(QWidget *parent, Qt::WFlags flags) : KDialog(parent, flags)
{
    QWidget *widget = new QWidget(this);
    ui = new Ui::ConvertDialog;
    ui->setupUi(widget);
    setMainWidget(widget);

    QString readFilter;
    QList<Magick::CoderInfo> coderList;
    coderInfoList(&coderList, Magick::CoderInfo::TrueMatch, Magick::CoderInfo::AnyMatch, Magick::CoderInfo::AnyMatch);
    Q_FOREACH(const Magick::CoderInfo &coder, coderList) {
        readFilter.append(QString(QLatin1String(" *.%1")).arg(QString::fromStdString(coder.name()).toLower()));
    }
    readFilter.remove(0, 1);
    readFilter.append(QLatin1Char('|')).append(i18nc("@item:inlistbox", "ImageMagick supported image formats"));

    QString writeFilter = QString(QLatin1String("*%1|%5 (%1)\n*%2|%6 (%2)\n*%3 *%4|%7 (%3 %4)")).arg(QLatin1String(".png"), QLatin1String(".tga"), QLatin1String(".jpg"), QLatin1String(".jpeg"), KMimeType::mimeType(QLatin1String("image/png"))->comment(), KMimeType::mimeType(QLatin1String("image/x-tga"))->comment(), KMimeType::mimeType(QLatin1String("image/jpeg"))->comment());

    ui->kurlrequester_image->setMode(KFile::File | KFile::ExistingOnly | KFile::LocalOnly);
    ui->kurlrequester_image->setAcceptMode(QFileDialog::AcceptOpen);
    ui->kurlrequester_image->setFilter(readFilter);
    ui->kurlrequester_converted->setMode(KFile::File | KFile::LocalOnly);
    ui->kurlrequester_converted->setAcceptMode(QFileDialog::AcceptSave);
    ui->kurlrequester_converted->setFilter(writeFilter);
}
ConvertDialog::~ConvertDialog()
{
    delete ui;
}

void ConvertDialog::setResolution(int width, int height)
{
    if (width > 0 && height > 0) {
        ui->spinBox_width->setValue(width);
        ui->spinBox_height->setValue(height);
    }
}

void ConvertDialog::slotButtonClicked(int button)
{
    if (button == KDialog::Ok) {
        if (ui->kurlrequester_image->text().isEmpty() || ui->kurlrequester_converted->text().isEmpty()) {
            KMessageBox::information(this, i18nc("@info", "Please fill in both <interface>Image</interface> and <interface>Convert To</interface> fields."));
            return;
        } else if (ui->spinBox_width->value() == 0 || ui->spinBox_height->value() == 0) {
            KMessageBox::information(this, i18nc("@info", "Please fill in both <interface>Width</interface> and <interface>Height</interface> fields."));
            return;
        } else if (!QFileInfo(KUrl(ui->kurlrequester_converted->url()).directory()).isWritable()) {
            KMessageBox::information(this, i18nc("@info", "You do not have write permissions in this directory, please select another destination."));
            return;
        }
        Magick::Geometry resolution(ui->spinBox_width->value(), ui->spinBox_height->value());
        resolution.aspect(ui->checkBox_force->isChecked());
        Magick::Image image(std::string(ui->kurlrequester_image->url().toLocalFile().toUtf8().constData()));
        image.zoom(resolution);
        image.depth(8);
        image.classType(Magick::DirectClass);
        image.write(std::string(ui->kurlrequester_converted->url().toLocalFile().toUtf8().constData()));
        if (ui->checkBox_wallpaper->isChecked()) {
            Q_EMIT splashImageCreated(ui->kurlrequester_converted->url().toLocalFile());
        }
    }
    KDialog::slotButtonClicked(button);
}
