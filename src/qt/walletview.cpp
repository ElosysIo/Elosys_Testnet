// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "utiltime.h"
#include "walletview.h"

#include "addressbookpage.h"
#include "zaddressbookpage.h"
#include "askpassphrasedialog.h"
#include "elosysoceangui.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "overviewpage.h"
#include "platformstyle.h"
#include "receivecoinsdialog.h"
//#include "sendcoinsdialog.h"
#include "zaddressbookpage.h"
#include "zsendcoinsdialog.h"
#include "zsigndialog.h"
#include "signverifymessagedialog.h"
#include "transactiontablemodel.h"
#include "transactionview.h"
#include "walletmodel.h"
#include "importkeydialog.h"
#include "openphrasedialog.h"
#include "unlocktimerdialog.h"

#include "ui_interface.h"

#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QProgressDialog>
#include <QPushButton>
#include <QVBoxLayout>

WalletView::WalletView(const PlatformStyle *_platformStyle, QWidget *parent):
    QStackedWidget(parent),
    clientModel(0),
    walletModel(0),
    platformStyle(_platformStyle)
{
    // Create tabs
    overviewPage = new OverviewPage(platformStyle);

    transactionsPage = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout();
    QHBoxLayout *hbox_buttons = new QHBoxLayout();
    transactionView = new TransactionView(platformStyle, this);
    vbox->addWidget(transactionView);
    QPushButton *exportButton = new QPushButton(tr("&Export"), this);
    exportButton->setToolTip(tr("Export the data in the current tab to a file"));
    if (platformStyle->getImagesOnButtons()) {
        exportButton->setIcon(platformStyle->SingleColorIcon(":/icons/export"));
    }
    hbox_buttons->addStretch();
    hbox_buttons->addWidget(exportButton);
    vbox->addLayout(hbox_buttons);
    transactionsPage->setLayout(vbox);

    receiveCoinsPage = new QWidget(this);
    QVBoxLayout *rvbox = new QVBoxLayout();
    receiveCoinsView = new ZAddressBookPage(platformStyle, ZAddressBookPage::ForEditing, ZAddressBookPage::ReceivingTab, this);
    rvbox->addWidget(receiveCoinsView);
    receiveCoinsPage->setLayout(rvbox);


    //sendCoinsPage = new SendCoinsDialog(platformStyle);
    zsendCoinsPage = new ZSendCoinsDialog(platformStyle);
    zsignPage      = new ZSignDialog(platformStyle);

    usedSendingAddressesPage = new AddressBookPage(platformStyle, AddressBookPage::ForEditing, AddressBookPage::SendingTab, this);
    usedReceivingAddressesPage = new AddressBookPage(platformStyle, AddressBookPage::ForEditing, AddressBookPage::ReceivingTab, this);
    // usedReceivingZAddressesPage = new ZAddressBookPage(platformStyle, ZAddressBookPage::ForEditing, ZAddressBookPage::ReceivingTab, this);

    addWidget(overviewPage);
    addWidget(transactionsPage);
    addWidget(receiveCoinsPage);
    //addWidget(sendCoinsPage);
    addWidget(zsendCoinsPage);
    addWidget(zsignPage);

    // Clicking on a transaction on the overview pre-selects the transaction on the transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));
    connect(overviewPage, SIGNAL(outOfSyncWarningClicked()), this, SLOT(requestedSyncWarningInfo()));

    // Double-clicking on a transaction on the transaction history page shows details
    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    // Clicking on "Export" allows to export the transaction list
    connect(exportButton, SIGNAL(clicked()), transactionView, SLOT(exportClicked()));

    // Pass through messages from sendCoinsPage
    //connect(sendCoinsPage, SIGNAL(message(QString,QString,unsigned int)), this, SIGNAL(message(QString,QString,unsigned int)));
    // Pass through messages from zsendCoinsPage
    connect(zsendCoinsPage, SIGNAL(message(QString,QString,unsigned int)), this, SIGNAL(message(QString,QString,unsigned int)));
//    connect(zsignPage,      SIGNAL(message(QString,QString,unsigned int)), this, SIGNAL(message(QString,QString,unsigned int)));
    // Pass through messages from transactionView
    connect(transactionView, SIGNAL(message(QString,QString,unsigned int)), this, SIGNAL(message(QString,QString,unsigned int)));

    // Detect Activity and reset unlock timer
    connect(overviewPage, SIGNAL(resetUnlockTimerEvent()), this, SLOT(resetUnlockTimer()));
    connect(zsendCoinsPage, SIGNAL(resetUnlockTimerEvent()), this, SLOT(resetUnlockTimer()));
    connect(zsignPage, SIGNAL(resetUnlockTimerEvent()), this, SLOT(resetUnlockTimer()));
    connect(transactionView, SIGNAL(resetUnlockTimerEvent()), this, SLOT(resetUnlockTimer()));
    connect(receiveCoinsView, SIGNAL(resetUnlockTimerEvent()), this, SLOT(resetUnlockTimer()));

    // This timer will be fired repeatedly to update the locked message
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(setLockMessage()));
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(openUnlockTimerDialog()));
    pollTimer->start(250);

}

WalletView::~WalletView()
{
}

void WalletView::setElosysOceanGUI(ElosysOceanGUI *gui)
{
    if (gui)
    {
        // Clicking on a transaction on the overview page simply sends you to transaction history page
        connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), gui, SLOT(gotoHistoryPage()));

        // Receive and report messages
        connect(this, SIGNAL(message(QString,QString,unsigned int)), gui, SLOT(message(QString,QString,unsigned int)));

        // Pass through encryption status changed signals
        connect(this, SIGNAL(encryptionStatusChanged(int)), gui, SLOT(setEncryptionStatus(int)));

        // Pass through transaction notifications
        connect(this, SIGNAL(incomingTransaction(QString,int,CAmount,QString,QString,QString)), gui, SLOT(incomingTransaction(QString,int,CAmount,QString,QString,QString)));

        // Connect HD enabled state signal
        connect(this, SIGNAL(hdEnabledStatusChanged(int)), gui, SLOT(setHDStatus(int)));
    }
}

void WalletView::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;

    overviewPage->setClientModel(_clientModel);
    //sendCoinsPage->setClientModel(_clientModel);
    zsendCoinsPage->setClientModel(_clientModel);
    zsignPage->setClientModel(_clientModel);
}

void WalletView::setWalletModel(WalletModel *_walletModel)
{
    this->walletModel = _walletModel;

    // Put transaction list in tabs
    transactionView->setModel(_walletModel);
    overviewPage->setWalletModel(_walletModel);
    receiveCoinsView->setModel(_walletModel->getZAddressTableModel());
    receiveCoinsView->setWalletModel(_walletModel);
    //sendCoinsPage->setModel(_walletModel);
    zsendCoinsPage->setModel(_walletModel);
    zsignPage->setModel(_walletModel);
    usedReceivingAddressesPage->setModel(_walletModel->getAddressTableModel());
    // usedReceivingZAddressesPage->setModel(_walletModel->getZAddressTableModel());
    usedSendingAddressesPage->setModel(_walletModel->getAddressTableModel());

    if (_walletModel)
    {
        // Receive and pass through messages from wallet model
        connect(_walletModel, SIGNAL(message(QString,QString,unsigned int)), this, SIGNAL(message(QString,QString,unsigned int)));

        // Handle changes in encryption status
        connect(_walletModel, SIGNAL(encryptionStatusChanged(int)), this, SIGNAL(encryptionStatusChanged(int)));
        updateEncryptionStatus();

        // update HD status
        Q_EMIT hdEnabledStatusChanged(_walletModel->hdEnabled());

        // Balloon pop-up for new transaction
        connect(_walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),
                this, SLOT(processNewTransaction(QModelIndex,int,int)));

        // Ask for passphrase if needed
        connect(_walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));

    }
}

void WalletView::processNewTransaction(const QModelIndex& parent, int start, int /*end*/)
{
    // Prevent balloon-spam when initial block download is in progress or rescanning
    if (!walletModel || walletModel->startedRescan || !clientModel || clientModel->inInitialBlockDownload())
        return;

    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    if (!ttm || ttm->processingQueuedTransactions())
        return;

    QString date = ttm->index(start, TransactionTableModel::Date, parent).data().toString();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent).data(Qt::EditRole).toULongLong();
    QString type = ttm->index(start, TransactionTableModel::Type, parent).data().toString();
    QModelIndex index = ttm->index(start, 0, parent);
    QString address = ttm->data(index, TransactionTableModel::AddressRole).toString();
    QString label = ttm->data(index, TransactionTableModel::LabelRole).toString();

    Q_EMIT incomingTransaction(date, walletModel->getOptionsModel()->getDisplayUnit(), amount, type, address, label);
}

void WalletView::setLockMessage() {
    if (overviewPage) {
        WalletModel::EncryptionStatus encryptionStatus = walletModel->getEncryptionStatus();

        //update the UI based on encryption status
        setUnlockButton();

        if (walletModel->startedRescan) {
            overviewPage->setLockMessage(tr("Wallet will relock after rescan."));
            return;
        }

        if (encryptionStatus == WalletModel::Unlocked) {
            if (walletModel->relockTime > 0) {
                std::string timeLeft = DateTimeStrFormat("%M:%S%F", walletModel->relockTime - GetTime());
                QString message = tr("Wallet will relock in ") + QString::fromStdString(timeLeft);
                overviewPage->setLockMessage(message);
            } else {
                overviewPage->setLockMessage(tr("Wallet unlocked by external RPC command."));
            }
        }

        if (encryptionStatus == WalletModel::Locked) {
              int walletHeight;
              int chainHeight;
              QString message = tr("In-Memory Wallet is synced with the chain.\n");
              walletModel->getWalletChainHeights(walletHeight, chainHeight);
              if (chainHeight < walletHeight + 30) { //Report in sync until the chain is at least 30 blocks higher, approx 2x setbestchain interval
                  message = message + tr("On-Disk Wallet is synced with the chain\n");
              } else {
                  message = message + tr("On-Disk Wallet is ") + QString::number(chainHeight - walletHeight) + tr(" blocks behind the chain.\n");
                  message = message + tr("Unlock to write to Disk.");
              }
              overviewPage->setLockMessage(message);
        }
    }
}

void WalletView::setUnlockButton()
{
    if (overviewPage) {

      switch (walletModel->getEncryptionStatus())
      {
      case WalletModel::Locked:
          overviewPage->setUiVisible(true, true, 0);
          break;
      case WalletModel::Unlocked:
          overviewPage->setUiVisible(false, true, walletModel->relockTime);
          break;
      case WalletModel::Unencrypted:
          overviewPage->setUiVisible(false, false, 0);
          break;
      default:
          break;
      }
    }
}

void WalletView::gotoOverviewPage()
{
    setCurrentWidget(overviewPage);
}

void WalletView::gotoHistoryPage()
{
    setCurrentWidget(transactionsPage);
}

void WalletView::gotoReceiveCoinsPage()
{
    setCurrentWidget(receiveCoinsPage);
}
/*
void WalletView::gotoSendCoinsPage(QString addr)
{
    setCurrentWidget(sendCoinsPage);

    if (!addr.isEmpty())
        sendCoinsPage->setAddress(addr);
}
*/
void WalletView::gotoZSendCoinsPage(QString addr)
{
    setCurrentWidget(zsendCoinsPage);

    if (!addr.isEmpty())
        zsendCoinsPage->setAddress(addr);

    zsendCoinsPage->updatePayFromList();
    //Clear the page upon entry:
    zsendCoinsPage->clear();
}

void WalletView::gotoZSignPage( )
{
    setCurrentWidget(zsignPage);
    //Clear the page upon enter.
    zsignPage->clear();
}

void WalletView::gotoSignMessageTab(QString addr)
{
    // calls show() in showTab_SM()
    SignVerifyMessageDialog *signVerifyMessageDialog = new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_SM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void WalletView::gotoVerifyMessageTab(QString addr)
{
    // calls show() in showTab_VM()
    SignVerifyMessageDialog *signVerifyMessageDialog = new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_VM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}
/*
bool WalletView::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    return sendCoinsPage->handlePaymentRequest(recipient);
}
*/
void WalletView::showOutOfSyncWarning(bool fShow)
{
    overviewPage->showOutOfSyncWarning(fShow);
}

void WalletView::updateEncryptionStatus()
{
    Q_EMIT encryptionStatusChanged(walletModel->getEncryptionStatus());
}

void WalletView::importSK()
{
    if(!walletModel)
        return;

    WalletModel::EncryptionStatus encryptionStatus = walletModel->getEncryptionStatus();

    QMessageBox msgBox;
    msgBox.setStyleSheet("QLabel{min-width: 350px;}");
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);

    if (encryptionStatus == WalletModel::Busy) {
        //Should never happen
        msgBox.setText("Encryption Status: Busy");
        msgBox.setInformativeText("Keys cannot be imported while the wallet is Busy, try again in a few minutes.");
        int ret = msgBox.exec();
        return;
    }

    if (encryptionStatus == WalletModel::Locked) {
        walletModel->requireUnlock();
    }

    if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
      msgBox.setText("Encryption Status: Locked");
      msgBox.setInformativeText("Keys cannot be imported while the wallet is Locked.");
      int ret = msgBox.exec();
      return;
    }

    OpenSKDialog dlg(this);
    QString privateKey;
    dlg.exec();
    if (dlg.result() == QDialog::Accepted) {
          privateKey = dlg.privateKey;
          walletModel->importSpendingKey(privateKey);
    }
    dlg.close();
}

void WalletView::importVK()
{
    if(!walletModel)
        return;

    WalletModel::EncryptionStatus encryptionStatus = walletModel->getEncryptionStatus();

    QMessageBox msgBox;
    msgBox.setStyleSheet("QLabel{min-width: 350px;}");
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);

    if (encryptionStatus == WalletModel::Busy) {
        //Should never happen
        msgBox.setText("Encryption Status: Busy");
        msgBox.setInformativeText("Keys cannot be imported while the wallet is Busy, try again in a few minutes.");
        int ret = msgBox.exec();
        return;
    }

    if (encryptionStatus == WalletModel::Locked) {
        walletModel->requireUnlock();
    }

    if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
      msgBox.setText("Encryption Status: Locked");
      msgBox.setInformativeText("Keys cannot be imported while the wallet is Locked.");
      int ret = msgBox.exec();
      return;
    }

    OpenVKDialog dlg(this);
    QString privateKey;
    dlg.exec();
    if (dlg.result() == QDialog::Accepted) {
          privateKey = dlg.privateKey;
          walletModel->importViewingKey(privateKey);
    }
    dlg.close();
}

void WalletView::showSeedPhrase()
{
    if(!walletModel)
        return;

    WalletModel::EncryptionStatus encryptionStatus = walletModel->getEncryptionStatus();

    QMessageBox msgBox;
    msgBox.setStyleSheet("QLabel{min-width: 350px;}");
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);

    if (encryptionStatus == WalletModel::Busy) {
        //Should never happen
        msgBox.setText("Encryption Status: Busy");
        msgBox.setInformativeText("Seed Phrase cannot be displayed while the wallet is Busy, try again in a few minutes.");
        int ret = msgBox.exec();
        return;
    }

    if (encryptionStatus == WalletModel::Locked) {
        walletModel->requireUnlock();
    }

    if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
      msgBox.setText("Encryption Status: Locked");
      msgBox.setInformativeText("Seed Phrase can not be displayed while the wallet is Locked.");
      int ret = msgBox.exec();
      return;
    }

    QString phrase = "";
    std::string recoverySeedPhrase = "";
    if (walletModel->getSeedPhrase(recoverySeedPhrase)) {
        phrase = QString::fromStdString(recoverySeedPhrase);
    }

    OpenPhraseDialog dlg(this, phrase);
    dlg.exec();
    dlg.close();

}

void WalletView::rescan()
{
    if(!walletModel)
        return;

    WalletModel::EncryptionStatus encryptionStatus = walletModel->getEncryptionStatus();

    QMessageBox msgBox;
    msgBox.setStyleSheet("QLabel{min-width: 350px;}");
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);

    if (encryptionStatus == WalletModel::Busy) {
        //Should never happen
        msgBox.setText("Encryption Status: Busy");
        msgBox.setInformativeText("Wallet can not be rescanned while the wallet is Busy, try again in a few minutes.");
        int ret = msgBox.exec();
        return;
    }

    if (encryptionStatus == WalletModel::Locked) {
        walletModel->requireUnlock();
    }

    if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
      msgBox.setText("Encryption Status: Locked");
      msgBox.setInformativeText("Wallet can not be rescanned while the wallet is Locked.");
      int ret = msgBox.exec();
      return;
    }

    walletModel->rescan();
}

void WalletView::encryptWallet(bool status)
{
    if(!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt : AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    updateEncryptionStatus();
}

void WalletView::backupWallet()
{
    QString filename = GUIUtil::getSaveFileName(this,
        tr("Backup Wallet"), QString(),
        tr("Wallet Data (*.dat)"), nullptr);

    if (filename.isEmpty())
        return;

    if (!walletModel->backupWallet(filename)) {
        Q_EMIT message(tr("Backup Failed"), tr("There was an error trying to save the wallet data to %1.").arg(filename),
            CClientUIInterface::MSG_ERROR);
        }
    else {
        Q_EMIT message(tr("Backup Successful"), tr("The wallet data was successfully saved to %1.").arg(filename),
            CClientUIInterface::MSG_INFORMATION);
    }
}

void WalletView::changePassphrase()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void WalletView::unlockWallet()
{
    if(!walletModel)
        return;
    // Unlock wallet when requested by wallet model
    if (walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
}

void WalletView::resetUnlockTimer() {
    if(!walletModel)
        return;

    if (walletModel->getEncryptionStatus() == WalletModel::Unlocked)
    {
        walletModel->relockTime = GetTime() + 300; //relock after 5 minutes
    }
}

void WalletView::usedSendingAddresses()
{
    if(!walletModel)
        return;

    usedSendingAddressesPage->show();
    usedSendingAddressesPage->raise();
    usedSendingAddressesPage->activateWindow();
}

void WalletView::usedReceivingAddresses()
{
    if(!walletModel)
        return;

    usedReceivingAddressesPage->show();
    usedReceivingAddressesPage->raise();
    usedReceivingAddressesPage->activateWindow();
}

void WalletView::usedReceivingZAddresses()
{
    if(!walletModel)
        return;

    // usedReceivingZAddressesPage->show();
    // usedReceivingZAddressesPage->raise();
    // usedReceivingZAddressesPage->activateWindow();
}

void WalletView::requestedSyncWarningInfo()
{
    Q_EMIT outOfSyncWarningClicked();
}

void WalletView::openUnlockTimerDialog() {
    if (ShutdownRequested) {
        return;
    }

    if (!fUnlocking
        && this->walletModel->relockTime < GetTime() + 31
        && this->walletModel->relockTime > 0 ) {
        fUnlocking = true;
        UnlockTimerDialog dlg(this);
        dlg.exec();
        if (dlg.result() == QDialog::Accepted) {
            // Keep Unlocked
            this->walletModel->relockTime = GetTime() + 300; //relock after 5 minutes
        } else {
            this->walletModel->lockWallet();
        }
        dlg.close();
        fUnlocking = false;

    }

}
