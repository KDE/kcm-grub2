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

#ifndef INSTALLDLG_H
#define INSTALLDLG_H

//KDE
class KProgressDialog;

//Ui
#include "ui_installDlg.h"

class InstallDialog : public KDialog
{
    Q_OBJECT
public:
    explicit InstallDialog(const QString installExePath, QWidget *parent = 0, Qt::WFlags flags = 0);
protected Q_SLOTS:
    virtual void slotButtonClicked(int button);
private:
    QString m_installExePath;
    Ui::InstallDialog ui;
};

#endif
