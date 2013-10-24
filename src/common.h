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

#ifndef COMMON_H
#define COMMON_H

//Qt
#include <QFlags>

enum LoadOperation {
    NoOperation = 0x0,
    MenuFile = 0x1,
    ConfigurationFile = 0x2,
    EnvironmentFile = 0x4,
    MemtestFile = 0x8,
    Vbe = 0x10,
    Locales = 0x20
};
Q_DECLARE_FLAGS(LoadOperations, LoadOperation)
Q_DECLARE_OPERATORS_FOR_FLAGS(LoadOperations)

QString quoteWord(const QString &word);
QString unquoteWord(const QString &word);

#endif
