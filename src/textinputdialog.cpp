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

#include "textinputdialog.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegExpValidator>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QStyle>

TextInputDialog::TextInputDialog(QWidget *parent)
    : QDialog(parent)
{
    m_label = new QLabel;
    m_lineEdit = new QLineEdit;

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *okButon = buttonBox->button(QDialogButtonBox::Ok);
    okButon->setEnabled(false);

    connect(m_lineEdit, &QLineEdit::textEdited, this, [this, okButon]() {
        okButon->setEnabled(m_lineEdit->hasAcceptableInput());
    });

    auto *layout = new QVBoxLayout;
    layout->addWidget(m_label);
    layout->addWidget(m_lineEdit);
    layout->addSpacing(style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing));
    layout->addWidget(buttonBox);
    setLayout(layout);
}

void TextInputDialog::setLabelText(const QString &text)
{
    m_label->setText(text);
}

QString TextInputDialog::textValue() const
{
    return m_lineEdit->text();
}

void TextInputDialog::setTextValue(const QString &resolution)
{
    m_lineEdit->setText(resolution);
}

void TextInputDialog::setValidator(const QValidator *v)
{
    m_lineEdit->setValidator(v);
}

QString TextInputDialog::getText(QWidget *parent, const QString &title, const QString &label, const QString &text, const QValidator *v, bool *ok)
{
    TextInputDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.setLabelText(label);
    dlg.setTextValue(text);
    dlg.setValidator(v);
    *ok = dlg.exec();
    return dlg.textValue();
}
