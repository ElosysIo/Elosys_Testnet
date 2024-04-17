// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef KOMODO_QT_ZADDRESSBOOKPAGE_H
#define KOMODO_QT_ZADDRESSBOOKPAGE_H

#include <QDialog>

class ZAddressTableModel;
class WalletModel;
class PlatformStyle;

namespace Ui {
    class ZAddressBookPage;
}

QT_BEGIN_NAMESPACE
class QItemSelection;
class QMenu;
class QModelIndex;
class QSortFilterProxyModel;
QT_END_NAMESPACE

/** Widget that shows a list of sending or receiving z-addresses.
  */
class ZAddressBookPage : public QWidget
{
    Q_OBJECT

public:
    enum Tabs {
        SendingTab = 0,
        ReceivingTab = 1
    };

    enum Mode {
        ForSelection, /**< Open address book to pick address */
        ForEditing  /**< Open address book for editing */
    };

    explicit ZAddressBookPage(const PlatformStyle *platformStyle, Mode mode, Tabs tab, QWidget *parent);
    ~ZAddressBookPage();


    void setModel(ZAddressTableModel *model);
    void setWalletModel(WalletModel *walletModel);

public Q_SLOTS:
    // void done(int retval);
    void exportSK();
    void exportVK();

private:
    Ui::ZAddressBookPage *ui;
    ZAddressTableModel *model;
    WalletModel *walletModel;
    Mode mode;
    Tabs tab;
    QSortFilterProxyModel *proxyModel;
    QMenu *contextMenu;
    QString newAddressToSelect;

private Q_SLOTS:
    /** Create a new address for receiving coins and / or add a new address book entry */
    void on_newAddress_clicked();
    /** Copy z_sendmany template to send funds to selected address entry to clipboard */
    void onCopyZSendManyToAction();
    /** Copy z_sendmany template to send funds from selected address entry to clipboard */
    void onCopyZSendManyFromAction();
    /** Copy address of currently selected address entry to clipboard */
    void on_copyAddress_clicked();
    /** Copy label of currently selected address entry to clipboard (no button) */
    void onCopyLabelAction();
    /** Edit currently selected address entry (no button) */
    void onEditAction();
    /** Export button clicked */
    void on_exportButton_clicked();

    /** Spawn contextual menu (right mouse menu) for address book entry */
    void contextualMenu(const QPoint &point);
    /** New entry/entries were added to address table */
    void selectNewAddress(const QModelIndex &parent, int begin, int /*end*/);

Q_SIGNALS:
    void sendCoins(QString addr);
    /** Activity detected in the GUI, reset the lock timer */
    void resetUnlockTimerEvent();
};

#endif // KOMODO_QT_ZADDRESSBOOKPAGE_H
