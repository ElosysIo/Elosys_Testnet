// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef KOMODO_QT_OPTIONSMODEL_H
#define KOMODO_QT_OPTIONSMODEL_H

#include "amount.h"

#include <QAbstractListModel>

// QT_BEGIN_NAMESPACE
// // #ifdef ENABLE_BIP70
// // class QNetworkProxy;
// // #endif
// QT_END_NAMESPACE

/** Interface from Qt to configuration data structure for Komodo client.
   To Qt, the options are presented as a list with the different options
   laid out vertically.
   This can be changed to a tree once the settings become sufficiently
   complex.
 */
class OptionsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit OptionsModel(QObject *parent = 0, bool resetSettings = false);

    enum OptionID {
        StartAtStartup,                 // bool
        HideTrayIcon,                   // bool
        MinimizeToTray,                 // bool
        MapPortUPnP,                    // bool
        MinimizeOnClose,                // bool
        ProxyUse,                       // bool
        ProxyIP,                        // QString
        ProxyPort,                      // int
        ProxyUseTor,                    // bool
        ProxyIPTor,                     // QString
        ProxyPortTor,                   // int
        ControlIPTor,                   // QString
        ControlPortTor,                 // int
        ControlPasswordTor,             // QString
        DisplayUnit,                    // KomodoUnits::Unit
        Theme,                          // QString
        ThirdPartyTxUrls,               // QString
        Language,                       // QString
        EnableDeleteTx,                 // bool
        EnableReindex,                  // bool
        EnableZSigning,                 // bool
        EnableZSigning_Spend,           // bool
        EnableZSigning_Sign,            // bool
        EnableHexMemo,                  // bool
        EnableBootstrap,                // bool
        ZapWalletTxes,                  // bool
        ThreadsScriptVerif,             // int
        DatabaseCache,                  // int
        SaplingConsolidationEnabled,    // bool
        Listen,                         // bool
        EncryptedP2P,                   // bool
        IncomingI2P,                    // bool
        ProxyUseI2P,                    // bool
        ProxyIPI2P,                     // QString
        ProxyPortI2P,                   // int
        IPv4Disable,                    // bool
        IPv6Disable,                    // bool
        OptionIDRowCount,
    };

    void Init(bool resetSettings = false);
    void Reset();

    int rowCount(const QModelIndex & parent = QModelIndex()) const;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole);
    /** Updates current unit in memory, settings and emits displayUnitChanged(newUnit) signal */
    void setDisplayUnit(const QVariant &value);
    void setHexMemo(const QVariant &value);

    /* Explicit getters */
    bool getHideTrayIcon() const { return fHideTrayIcon; }
    bool getMinimizeToTray() const { return fMinimizeToTray; }
    bool getMinimizeOnClose() const { return fMinimizeOnClose; }
    int getDisplayUnit() const { return nDisplayUnit; }
    bool getHexMemo() const { return fEnableHexMemo; }
    QString getThirdPartyTxUrls() const { return "http://explorer.elosyschain.com/tx/%s"; }
    // #ifdef ENABLE_BIP70
    // bool getProxySettings(QNetworkProxy& proxy) const;
    // #endif
    const QString& getOverriddenByCommandLine() { return strOverriddenByCommandLine; }

    /* Restart flag helper */
    void setRestartRequired(bool fRequired);
    bool isRestartRequired() const;

private:
    /* Qt-only settings */
    bool fHideTrayIcon;
    bool fMinimizeToTray;
    bool fMinimizeOnClose;
    QString language;
    int nDisplayUnit;
    bool fEnableHexMemo;
    QString strThirdPartyTxUrls;
    QString strTheme;
    /* settings that were overridden by command-line */
    QString strOverriddenByCommandLine;

    // Add option to list of GUI options overridden through command line/config file
    void addOverriddenOption(const std::string &option);

    // Check settings version and upgrade default values if required
    void checkAndMigrate();
Q_SIGNALS:
    void displayUnitChanged(int unit);
    void hideTrayIconChanged(bool);
    void optionHexMemo(bool);
};

#endif // KOMODO_QT_OPTIONSMODEL_H
