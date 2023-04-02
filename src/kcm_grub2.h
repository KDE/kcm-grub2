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

#ifndef KCMGRUB2_H
#define KCMGRUB2_H

#include <KCModule>
#include <QBitArray>

// Project
class Entry;

// Ui
namespace Ui
{
class KCMGRUB2;
}

class KCMGRUB2 : public KCModule
{
    Q_OBJECT
public:
    explicit KCMGRUB2(QObject *parent, const KPluginMetaData &data, const QVariantList &list);
    virtual ~KCMGRUB2();

    void defaults() override;
    void load() override;
    void save() override;
private Q_SLOTS:
    void slotRemoveOldEntries();
    void slotGrubSavedefaultChanged();
    void slotGrubHiddenTimeoutToggled(bool checked);
    void slotGrubHiddenTimeoutChanged();
    void slotGrubHiddenTimeoutQuietChanged();
    void slotGrubTimeoutToggled(bool checked);
    void slotGrubTimeoutChanged();
    void slotGrubLanguageChanged();
    void slotGrubDisableRecoveryChanged();
    void slotMemtestChanged();
    void slotGrubDisableOsProberChanged();
    void slotGrubGfxmodeChanged();
    void slotGrubGfxpayloadLinuxChanged();
    void slotResolutionsRefresh();
    void slotGrubColorNormalChanged();
    void slotGrubColorHighlightChanged();
    void slowGrubBackgroundChanged();
    void slotPreviewGrubBackground();
    void slotCreateGrubBackground();
    void slotGrubThemeChanged();
    void slotGrubCmdlineLinuxDefaultChanged();
    void slotGrubCmdlineLinuxChanged();
    void slotGrubTerminalChanged();
    void slotGrubTerminalInputChanged();
    void slotGrubTerminalOutputChanged();
    void slotGrubDistributorChanged();
    void slotGrubSerialCommandChanged();
    void slotGrubInitTuneChanged();
    void slotGrubDisableLinuxUuidChanged();
    void slotInstallBootloader();

    void slotUpdateSuggestions();
    void slotTriggeredSuggestion(QAction *action);

private:
    struct ColorInfo {
        QString grubColor;
        QString text;
        QColor color;
    };
    void setupColors(std::initializer_list<ColorInfo> colors);
    void setupObjects();
    void setupConnections();

    bool readFile(const QString &fileName, QByteArray &fileContents);
    void readAll();

    void showLocales();
    void sortResolutions();
    void showResolutions();

    QString parseTitle(const QString &line);
    void parseEntries(const QString &config);
    void parseSettings(const QString &config);
    void parseEnv(const QString &config);

    Ui::KCMGRUB2 *ui;

    enum {
        grubSavedefaultDirty,
        grubHiddenTimeoutDirty,
        grubHiddenTimeoutQuietDirty,
        grubTimeoutDirty,
        grubLocaleDirty,
        grubDisableRecoveryDirty,
        memtestDirty,
        grubDisableOsProberDirty,
        grubGfxmodeDirty,
        grubGfxpayloadLinuxDirty,
        grubColorNormalDirty,
        grubColorHighlightDirty,
        grubBackgroundDirty,
        grubThemeDirty,
        grubCmdlineLinuxDefaultDirty,
        grubCmdlineLinuxDirty,
        grubTerminalDirty,
        grubTerminalInputDirty,
        grubTerminalOutputDirty,
        grubDistributorDirty,
        grubSerialCommandDirty,
        grubInitTuneDirty,
        grubDisableLinuxUuidDirty,
        lastDirtyBit
    };
    QBitArray m_dirtyBits;

    QList<Entry> m_entries;
    QHash<QString, QString> m_settings;
    QHash<QString, QString> m_env;
    bool m_memtest;
    bool m_memtestOn;
    QHash<QString, QString> m_devices;
    QStringList m_resolutions;
    bool m_resolutionsEmpty;
    bool m_resolutionsForceRead;
    QStringList m_locales;
};

#endif
