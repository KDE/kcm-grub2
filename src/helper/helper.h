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

#ifndef HELPER_CPP
#define HELPER_CPP

//KDE
#include <KAuth/ActionReply>
using namespace KAuth;

class Helper : public QObject
{
Q_OBJECT
public Q_SLOTS:
    ActionReply load(QVariantMap args);
    ActionReply probe(QVariantMap args);
    ActionReply save(QVariantMap args);
    ActionReply update(QVariantMap args);
};

#endif
