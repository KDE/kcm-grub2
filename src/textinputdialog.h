/*******************************************************************************
 * Copyright (C) 2018 Alexander Volkov <a.volkov@rusbitech.ru>                 *
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

#ifndef TEXTINPUTDIALOG_H
#define TEXTINPUTDIALOG_H

#include <QDialog>

class QLabel;
class QLineEdit;
class QValidator;

class TextInputDialog : public QDialog
{
public:
    explicit TextInputDialog(QWidget *parent = nullptr);

    void setLabelText(const QString &text);

    QString textValue() const;
    void setTextValue(const QString &resolution);

    void setValidator(const QValidator *v);

    static QString getText(QWidget *parent, const QString &title, const QString &label, const QString &text, const QValidator *v, bool *ok);

private:
    QLabel *m_label;
    QLineEdit *m_lineEdit;
};

#endif // TEXTINPUTDIALOG_H
