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
#include "convertDlg.h"

//KDE
#include <KFileDialog>
#include <KMessageBox>

//ImageMagick
#include <Magick++.h>

ConvertDialog::ConvertDialog(QWidget *parent, Qt::WFlags flags) : KDialog(parent, flags)
{
    QWidget *widget = new QWidget(this);
    ui.setupUi(widget);
    setMainWidget(widget);
    //TODO: Query ImageMagick for supported mimetypes
    ui.kurlrequester_image->setMode(KFile::File | KFile::ExistingOnly | KFile::LocalOnly);
    ui.kurlrequester_image->fileDialog()->setOperationMode(KFileDialog::Opening);
    ui.kurlrequester_converted->setMode(KFile::File | KFile::LocalOnly);
    ui.kurlrequester_converted->fileDialog()->setOperationMode(KFileDialog::Saving);
    ui.kurlrequester_converted->fileDialog()->setFilter(QString("*%1|PNG %5 (%1)\n*%2|TGA %5 (%2)\n*%3 *%4|JPEG %5 (%3 %4)").arg(".png", ".tga", ".jpg", ".jpeg", i18nc("@item:inlistbox", "Image")));
}
void ConvertDialog::setResolution(int width, int height)
{
    if (width > 0 && height > 0) {
        ui.spinBox_width->setValue(width);
        ui.spinBox_height->setValue(height);
    }
}
void ConvertDialog::slotButtonClicked(int button)
{
    if (button == KDialog::Ok) {
        if (ui.kurlrequester_image->text().isEmpty() || ui.kurlrequester_converted->text().isEmpty()) {
            KMessageBox::information(this, i18nc("@info", "Please fill in both <interface>Image</interface> and <interface>Convert To</interface> fields."));
            return;
        } else if (ui.spinBox_width->value() == 0 || ui.spinBox_height->value() == 0) {
            KMessageBox::information(this, i18nc("@info", "Please fill in both <interface>Width</interface> and <interface>Height</interface> fields."));
            return;
        }
        Magick::Geometry resolution(ui.spinBox_width->value(), ui.spinBox_height->value());
        resolution.aspect(ui.checkBox_force->isChecked());
        Magick::Image image(ui.kurlrequester_image->url().toLocalFile().toStdString());
        image.zoom(resolution);
        image.depth(8);
        image.classType(Magick::DirectClass);
        image.write(ui.kurlrequester_converted->url().toLocalFile().toStdString());
        if (ui.checkBox_wallpaper->isChecked()) {
            emit splashImageCreated(ui.kurlrequester_converted->url().toLocalFile());
        }
    }
    KDialog::slotButtonClicked(button);
}
