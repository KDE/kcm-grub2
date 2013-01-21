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

#ifndef REMOVEDLG_H
#define REMOVEDLG_H

//KDE
class KProgressDialog;

//Project
#include <config.h>
#if HAVE_QAPT
#include "qaptBackend.h"
#elif HAVE_QPACKAGEKIT
#include "qPkBackend.h"
#endif

//Ui
#include "ui_removeDlg.h"

class RemoveDialog : public KDialog
{
    Q_OBJECT
public:
    explicit RemoveDialog(const QStringList &entries, const QHash<QString, QString> &kernels, QWidget *parent = 0, Qt::WFlags flags = 0);
    virtual ~RemoveDialog();
protected Q_SLOTS:
    virtual void slotButtonClicked(int button);
private Q_SLOTS:
    void slotItemChanged();
    void slotProgress(const QString &status, int percentage);
    void slotFinished(bool success);
private:
    void detectCurrentKernelImage();

#if HAVE_QAPT
    QAptBackend *m_backend;
#elif HAVE_QPACKAGEKIT
    QPkBackend *m_backend;
#endif
    QString m_currentKernelImage;
    KProgressDialog *m_progressDlg;
    Ui::RemoveDialog ui;
};

#endif
