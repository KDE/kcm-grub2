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
#include "common.h"

//Qt
#include <QTextStream>

//KDE
#include <KProcess>
#include <KShell>

QString quoteWord(const QString &word)
{
    return !word.startsWith(QLatin1Char('`')) || !word.endsWith(QLatin1Char('`')) ? KShell::quoteArg(word) : word;
}
QString unquoteWord(const QString &word)
{
    if (word.isEmpty()) {
        return QString();
    }

    KProcess echo;
    echo.setShellCommand(QStringLiteral("echo -n %1").arg(word));
    echo.setOutputChannelMode(KProcess::OnlyStdoutChannel);
    if (echo.execute() == 0) {
        return QString::fromLocal8Bit(echo.readAllStandardOutput().constData());
    }

    QChar ch;
    QString quotedWord = word, unquotedWord;
    QTextStream stream(&quotedWord, QIODevice::ReadOnly | QIODevice::Text);
    while (!stream.atEnd()) {
        stream >> ch;
        if (ch == QLatin1Char('\'')) {
            Q_FOREVER {
                if (stream.atEnd()) {
                    return QString();
                }
                stream >> ch;
                if (ch == QLatin1Char('\'')) {
                    return unquotedWord;
                } else {
                    unquotedWord.append(ch);
                }
            }
        } else if (ch == QLatin1Char('"')) {
            Q_FOREVER {
                if (stream.atEnd()) {
                    return QString();
                }
                stream >> ch;
                if (ch == QLatin1Char('\\')) {
                    if (stream.atEnd()) {
                        return QString();
                    }
                    stream >> ch;
                    switch (ch.toLatin1()) {
                        case '$':
                        case '"':
                        case '\\':
                            unquotedWord.append(ch);
                            break;
                        case '\n':
                            unquotedWord.append(QLatin1Char(' '));
                            break;
                        default:
                            unquotedWord.append(QLatin1Char('\\')).append(ch);
                            break;
                    }
                } else if (ch == QLatin1Char('"')) {
                    return unquotedWord;
                } else {
                    unquotedWord.append(ch);
                }
            }
        } else {
            Q_FOREVER {
                if (ch == QLatin1Char('\\')) {
                    if (stream.atEnd()) {
                        return unquotedWord;
                    }
                    stream >> ch;
                    switch (ch.toLatin1()) {
                        case '\n':
                            break;
                        default:
                            unquotedWord.append(ch);
                            break;
                    }
                } else if (ch.isSpace()) {
                    return unquotedWord;
                } else {
                    unquotedWord.append(ch);
                }
                if (stream.atEnd()) {
                    return unquotedWord;
                }
                stream >> ch;
            }
        }
    }
    return QString();
}
