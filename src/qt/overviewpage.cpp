// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "komodounits.h"
#include "clientmodel.h"
#include "clientversion.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "transactionfilterproxy.h"
#include "transactiontablemodel.h"
#include "walletmodel.h"
#include "updatedialog.h"
#include "util.h" // for KOMODO_ASSETCHAIN_MAXLEN

#include "params.h" //curl for price check

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QSettings>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLocale>
#include <QDesktopServices>
#include <QUrl>
#include <QVersionNumber>

#define DECORATION_SIZE 54
#define NUM_ITEMS 5


extern int nMaxConnections; //From net.h

extern char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN];

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    explicit TxViewDelegate(const PlatformStyle *_platformStyle, QObject *parent=nullptr):
        QAbstractItemDelegate(parent), unit(KomodoUnits::ELO),
        platformStyle(_platformStyle)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(TransactionTableModel::RawDecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon = platformStyle->SingleColorIcon(icon);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(value.canConvert<QBrush>())
        {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        painter->setPen(foreground);
        QRect boundingRect;
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address, &boundingRect);

        if (index.data(TransactionTableModel::WatchonlyRole).toBool())
        {
            QIcon iconWatchonly = qvariant_cast<QIcon>(index.data(TransactionTableModel::WatchonlyDecorationRole));
            iconWatchonly = platformStyle->SingleColorIcon(iconWatchonly);
            QRect watchonlyRect(boundingRect.right() + 5, mainRect.top()+ypad+halfheight, 16, halfheight);
            iconWatchonly.paint(painter, watchonlyRect);
        }

        if(amount < 0)
        {
            QSettings settings;
            if (settings.value("strTheme", "armada").toString() == "dark") {
                foreground = COLOR_NEGATIVE_DARK;
            } else if (settings.value("strTheme", "armada").toString() == "elosys") {
                foreground = COLOR_NEGATIVE;
            } else if (settings.value("strTheme", "armada").toString() == "elosysmap") {
                foreground = COLOR_NEGATIVE_DARK;
            } else if (settings.value("strTheme", "armada").toString() == "armada") {
                foreground = COLOR_NEGATIVE_DARK;
            } else if (settings.value("strTheme", "armada").toString() == "treasure") {
                foreground = COLOR_NEGATIVE_DARK;
            } else if (settings.value("strTheme", "armada").toString() == "treasuremap") {
                foreground = COLOR_NEGATIVE_DARK;
            } else if (settings.value("strTheme", "armada").toString() == "ghostship") {
                foreground = COLOR_NEGATIVE_DARK;
            } else if (settings.value("strTheme", "armada").toString() == "night") {
                foreground = COLOR_NEGATIVE_DARK;
            } else {
                foreground = COLOR_NEGATIVE;
            }
        }
        else if(amount > 0)
        {
            QSettings settings;
            if (settings.value("strTheme", "armada").toString() == "dark") {
                foreground = COLOR_POSITIVE_DARK;
            } else if (settings.value("strTheme", "armada").toString() == "elosys") {
                foreground = COLOR_POSITIVE_ELOSYS;
            } else if (settings.value("strTheme", "armada").toString() == "elosysmap") {
                foreground = COLOR_POSITIVE_ELOSYS;
            } else if (settings.value("strTheme", "armada").toString() == "armada") {
                foreground = COLOR_POSITIVE_ELOSYS;
            } else if (settings.value("strTheme", "armada").toString() == "treasure") {
                foreground = COLOR_POSITIVE_ELOSYS;
            } else if (settings.value("strTheme", "armada").toString() == "treasuremap") {
                foreground = COLOR_POSITIVE_ELOSYS;
            } else if (settings.value("strTheme", "armada").toString() == "ghostship") {
                foreground = COLOR_POSITIVE_ELOSYS;
            } else if (settings.value("strTheme", "armada").toString() == "night") {
                foreground = COLOR_POSITIVE_ELOSYS;
            } else {
                foreground = COLOR_POSITIVE;
            }
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = KomodoUnits::formatWithUnit(unit, amount, true, KomodoUnits::separatorAlways);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;
    const PlatformStyle *platformStyle;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    clientModel(0),
    walletModel(0),
    currentBalance(-1),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    currentWatchOnlyBalance(-1),
    currentWatchUnconfBalance(-1),
    currentWatchImmatureBalance(-1),
    currentPrivateWatchBalance(-1),
    currentPrivateBalance(-1),
    currentInterestBalance(-1),
    txdelegate(new TxViewDelegate(platformStyle, this))
{
    ui->setupUi(this);

    // use a SingleColorIcon for the "out of sync warning" icon
    QIcon icon = platformStyle->SingleColorIcon(":/icons/warning");
    icon.addPixmap(icon.pixmap(QSize(64,64), QIcon::Normal), QIcon::Disabled); // also set the disabled icon because we are using a disabled QPushButton to work around missing HiDPI support of QLabel (https://bugreports.qt.io/browse/QTBUG-42503)
    ui->labelTransactionsStatus->setIcon(icon);
    ui->labelWalletStatus->setIcon(icon);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    // start with displaying the "out of sync" warnings
    if (nMaxConnections>0) //On-line
    {
        showOutOfSyncWarning(true);
        connect(ui->labelWalletStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));
        connect(ui->labelTransactionsStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));
    }

    //connect Unlock wallet button
    connect(ui->btnUnlock, SIGNAL(clicked()), this, SLOT(unlockWallet()));

    //set labal name to style
    ui->lblLockedMessage->setObjectName("lockedMessage");

    updateJSONtimer = new QTimer(this);
    updateGUItimer = new QTimer(this);
    gitJSONtimer = new QTimer(this);
    gitGUItimer = new QTimer(this);

    gitReply = new JsonDownload;
    cmcReply = new JsonDownload;

    connect(updateJSONtimer, SIGNAL(timeout()), SLOT(getPrice()));
    connect(updateGUItimer, SIGNAL(timeout()), SLOT(replyPriceFinished()));
    connect(gitJSONtimer, SIGNAL(timeout()), SLOT(getGitRelease()));
    connect(gitGUItimer, SIGNAL(timeout()), SLOT(replyGitRelease()));

    updateJSONtimer->setInterval(300000); //Check every 5 minutes.
    updateJSONtimer->start();

    updateGUItimer->setInterval(5000); //Check every 15 seconds.
    updateGUItimer->start();

    gitJSONtimer->setInterval(21600000); //Check every 6 hours.
    gitJSONtimer->start();

    gitGUItimer->setInterval(5000); //Check every 15 seconds.
    gitGUItimer->start();

    getGitRelease();
    getPrice();
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    Q_EMIT resetUnlockTimerEvent();

    if(filter)
        Q_EMIT transactionClicked(filter->mapToSource(index));
}

void OverviewPage::handleOutOfSyncWarningClicks()
{
    Q_EMIT outOfSyncWarningClicked();
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::getGitRelease()
{
    getHttpsJson("https://api.github.com/repos/ElosysNetwork/Elosys/releases", gitReply, GITHUB_HEADERS);
}

void OverviewPage::replyGitRelease()
{
    if (gitReply->failed == false && gitReply->complete == true) {
        try {
            QJsonDocument response = QJsonDocument::fromJson(gitReply->response.c_str());
            const QJsonArray responseArray  = response.array();
            const QJsonObject firstRecord  = responseArray[0].toObject();
            QString gitStrVersion = firstRecord["tag_name"].toString();

            if (gitStrVersion.startsWith("v"))
                gitStrVersion = gitStrVersion.right(gitStrVersion.length() - 1);

            QVersionNumber gitVersion = QVersionNumber::fromString(gitStrVersion);

            QVersionNumber clientVersion = QVersionNumber::fromString(QString::fromStdString(FormatGitVersion()));

            if (gitVersion > clientVersion) {

                  // Check Setting for update ignore, only ignore update notification for 1 week.
                  QSettings settings;
                  uint64_t ignoreTime = settings.value("timeIgnoreVersion", "0").toULongLong();
                  uint64_t currentTime = GetTime();
                  QString strIgnoreVersion = "0.0.0";

                  if (currentTime - ignoreTime < 604800) {
                      //Get ignored version if ignored time is less than 7 days
                      strIgnoreVersion = settings.value("strIgnoreVersion", "0.0.0").toString();
                  }

                  QVersionNumber ignoreVersion = QVersionNumber::fromString(strIgnoreVersion);

                  if (gitVersion > ignoreVersion) {
                      //Open Update Dialog
                      UpdateDialog dlg(this, clientVersion, gitVersion);
                      dlg.exec();
                      if (dlg.result() == QDialog::Accepted) {
                          //Open Elosys Github release page
                          QDesktopServices::openUrl(QUrl("https://github.com/Elosysnetwork/Elosys/releases"));
                      }
                      // Set IgnoreVersion
                      qint64 newIgnoreTime = GetTime();
                      settings.setValue("strIgnoreVersion", gitVersion.toString());
                      settings.setValue("timeIgnoreVersion", QString::number(newIgnoreTime));
                      dlg.close();

                  }

            }

        } catch (...) {
            LogPrintf("Github Releases JSON Parsing error\n");
        }
    }
}

void OverviewPage::getPrice()
{
    getHttpsJson("https://api.coingecko.com/api/v3/simple/price?ids=elosys-chain&vs_currencies=btc%2Cusd%2Ceur&include_market_cap=true&include_24hr_vol=true&include_24hr_change=true", cmcReply, CMC_HEADERS);
}

void OverviewPage::replyPriceFinished()
{
    if (cmcReply->failed == false && cmcReply->complete == true) {
        try {
            QJsonDocument response = QJsonDocument::fromJson(cmcReply->response.c_str());

            const QJsonObject item  = response.object();
            const QJsonObject usd  = item["elosys-chain"].toObject();
            auto fiatValue = usd["usd"].toDouble();

            double currentFiat = currentPrivateBalance * fiatValue;
            double watchFiat = currentPrivateWatchBalance * fiatValue;

            //TODO: Setup multiple currencies
            QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
            QLocale dollar;

            //Set Total Value
            ui->labelFiat->setText(dollar.toCurrencyString(currentFiat/1e8));
            ui->labelWatchFiat->setText(dollar.toCurrencyString(watchFiat/1e8));
            ui->labelFiatTotal->setText(dollar.toCurrencyString((currentFiat+watchFiat)/1e8));

            //Set Exchange Rate
            ui->labelExchange->setText(dollar.toCurrencyString(fiatValue));

        } catch (...) {
            LogPrintf("Coin Gecko JSON Parsing error\n");
        }
    }
}

void OverviewPage::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance, const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance, const CAmount& privateWatchBalance, const CAmount& privateBalance, const CAmount& interestBalance)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    currentWatchOnlyBalance = watchOnlyBalance;
    currentWatchUnconfBalance = watchUnconfBalance;
    currentWatchImmatureBalance = watchImmatureBalance;
    currentPrivateWatchBalance = privateWatchBalance;
    currentPrivateBalance = privateBalance;
    currentInterestBalance = interestBalance;
    ui->labelBalance->setText(KomodoUnits::formatWithUnit(unit, balance, false, KomodoUnits::separatorAlways));
    ui->labelUnconfirmed->setText(KomodoUnits::formatWithUnit(unit, unconfirmedBalance, false, KomodoUnits::separatorAlways));
    ui->labelImmature->setText(KomodoUnits::formatWithUnit(unit, immatureBalance, false, KomodoUnits::separatorAlways));
    ui->labelTotal->setText(KomodoUnits::formatWithUnit(unit, balance + unconfirmedBalance + immatureBalance + privateBalance + interestBalance, false, KomodoUnits::separatorAlways));
    ui->labelWatchAvailable->setText(KomodoUnits::formatWithUnit(unit, watchOnlyBalance, false, KomodoUnits::separatorAlways));
    ui->labelWatchPending->setText(KomodoUnits::formatWithUnit(unit, watchUnconfBalance, false, KomodoUnits::separatorAlways));
    ui->labelWatchImmature->setText(KomodoUnits::formatWithUnit(unit, watchImmatureBalance, false, KomodoUnits::separatorAlways));
    ui->labelWatchTotal->setText(KomodoUnits::formatWithUnit(unit, watchOnlyBalance + watchUnconfBalance + watchImmatureBalance + privateWatchBalance, false, KomodoUnits::separatorAlways));
    ui->labelPrivateWatchBalance->setText(KomodoUnits::formatWithUnit(unit, privateWatchBalance, false, KomodoUnits::separatorAlways));
    ui->labelPrivateBalance->setText(KomodoUnits::formatWithUnit(unit, privateBalance, false, KomodoUnits::separatorAlways));
    ui->labelInterestBalance->setText(KomodoUnits::formatWithUnit(unit, interestBalance, false, KomodoUnits::separatorAlways));
    ui->labelWalletTotal->setText(KomodoUnits::formatWithUnit(unit, balance + unconfirmedBalance + immatureBalance + privateBalance + interestBalance +watchOnlyBalance + watchUnconfBalance + watchImmatureBalance + privateWatchBalance , false, KomodoUnits::separatorAlways));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = immatureBalance != 0;
    bool showWatchOnlyImmature = watchImmatureBalance != 0;
    bool showInterest = (chainName.isKMD());

    bool showTransparent = balance !=0;
    bool showWatchOnlyTransaparent = watchOnlyBalance != 0;

    // for symmetry reasons also show immature label when the watch-only one is shown
    ui->labelBalance->setVisible(showTransparent || showWatchOnlyTransaparent);
    ui->labelBalanceText->setVisible(showTransparent || showWatchOnlyTransaparent);
    ui->labelWatchAvailable->setVisible(showWatchOnlyTransaparent); // show watch-only immature balance

    // for symmetry reasons also show immature label when the watch-only one is shown
    ui->labelImmature->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelImmatureText->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelWatchImmature->setVisible(showWatchOnlyImmature); // show watch-only immature balance
    // we should show interest only for KMD, so we need to use setVisible with condition
    ui->labelInterestBalance->setVisible(showInterest);
    ui->labelInterestTotalText->setVisible(showInterest);

}

// show/hide watch-only labels
void OverviewPage::updateWatchOnlyLabels(bool showWatchOnly)
{


    if (showWatchOnly) {
        ui->labelSpendable->setVisible(showWatchOnly);            // show spendable label (only when watch-only is active)
        ui->labelWatchonly->setVisible(showWatchOnly);            // show watch-only label
        ui->lineWatchBalance->setVisible(showWatchOnly);          // show watch-only balance separator line
        ui->lineWatchFiat->setVisible(showWatchOnly);             // show watch-only fiat separator line
        ui->labelWatchPending->setVisible(showWatchOnly);         // show watch-only pending balance
        ui->labelPrivateWatchBalance->setVisible(showWatchOnly);  // show watch-only private balance
        ui->labelWatchTotal->setVisible(showWatchOnly);           // show watch-only total balance
        ui->labelWatchFiat->setVisible(showWatchOnly);            // Show watch-only fiat balance

        ui->labelFiatTotalText->setVisible(showWatchOnly);
        ui->labelWalletTotalText->setVisible(showWatchOnly);
        ui->labelFiatTotal->setVisible(showWatchOnly);
        ui->labelWalletTotal->setVisible(showWatchOnly);

        ui->verticalSpacerCombined->changeSize(10, 10, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->labelCombinedText->setVisible(showWatchOnly);

        bool showTransparent = (currentBalance + currentWatchOnlyBalance) != 0;
        ui->labelBalance->setVisible(showTransparent);
        ui->labelBalanceText->setVisible(showTransparent);
        ui->labelWatchAvailable->setVisible(showTransparent);

        bool showImmature = (currentImmatureBalance + currentWatchImmatureBalance) != 0;
        ui->labelImmature->setVisible(showImmature);
        ui->labelImmatureText->setVisible(showImmature);
        ui->labelWatchImmature->setVisible(showImmature); // show watch-only immature balance


    } else {
        ui->labelSpendable->setVisible(showWatchOnly);            // show spendable label (only when watch-only is active)
        ui->labelWatchonly->setVisible(showWatchOnly);            // show watch-only label
        ui->lineWatchBalance->setVisible(showWatchOnly);          // show watch-only balance separator line
        ui->lineWatchFiat->setVisible(showWatchOnly);          // show watch-only fiat separator line
        ui->labelWatchAvailable->setVisible(showWatchOnly);       // show watch-only available balance
        ui->labelWatchPending->setVisible(showWatchOnly);         // show watch-only pending balance
        ui->labelWatchTotal->setVisible(showWatchOnly);           // show watch-only total balance
        ui->labelWatchFiat->setVisible(showWatchOnly);            // Show watch-only fiat balance
        ui->labelWatchImmature->setVisible(showWatchOnly);        // show watch-only immature balance
        ui->labelPrivateWatchBalance->setVisible(showWatchOnly);  // show watch-only private balance

        ui->labelFiatTotalText->setVisible(showWatchOnly);
        ui->labelWalletTotalText->setVisible(showWatchOnly);
        ui->labelFiatTotal->setVisible(showWatchOnly);
        ui->labelWalletTotal->setVisible(showWatchOnly);

        ui->verticalSpacerCombined->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->labelCombinedText->setVisible(showWatchOnly);
    }

    ui->lineWatchFiat->setVisible(false);
    ui->lineFiat->setVisible(false);
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter.get());
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getUnconfirmedBalance(), model->getImmatureBalance(),
                   model->getWatchBalance(), model->getWatchUnconfirmedBalance(), model->getWatchImmatureBalance(),
                   model->getPrivateWatchBalance(), model->getPrivateBalance(),model->getInterestBalance());
        connect(model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this, SLOT(setBalance(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

        updateWatchOnlyLabels(model->haveWatchOnly());
        connect(model, SIGNAL(notifyWatchonlyChanged(bool)), this, SLOT(updateWatchOnlyLabels(bool)));
    }

    // update the display unit, to not use the default ("KMD")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, currentUnconfirmedBalance, currentImmatureBalance,
                       currentWatchOnlyBalance, currentWatchUnconfBalance, currentWatchImmatureBalance,
                       currentPrivateWatchBalance, currentPrivateBalance, currentInterestBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();

    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
  if (nMaxConnections>0) //On-line
  {
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
  }
}

void OverviewPage::setLockMessage(QString message) {
    ui->lblLockedMessage->setText(message);
}

void OverviewPage::setUiVisible(bool visible, bool isCrypted, int64_t relockTime) {
    if (!isCrypted) {
        //Always hide on an unencrypted wallet
        ui->lblLockedMessage->setVisible(false);
        ui->btnUnlock->setVisible(false);
        if (nMaxConnections>0) 	//Online
        {
            ui->frame->setVisible(true);
            ui->frame_2->setVisible(true);
        }
        else			//Offline
        {
            //Hide the balances frame.
            ui->frame->setVisible(false);
            //Hide the transaction summary frame
            ui->frame_2->setVisible(false);
            //Give a message on the empty page that we're in offline mode
            OverviewPage::updateAlerts("<b>Cold storage offline mode");
        }
        return;
    }

    //Alway Show on a crypted wallet
    ui->btnUnlock->setVisible(true);

    if (visible) {
        ui->btnUnlock->setText("Unlock");
        ui->frame->setVisible(false);
        ui->frame_2->setVisible(false);
    } else {
        ui->btnUnlock->setText("Lock");
        if (nMaxConnections>0) //Online
        {
            ui->frame->setVisible(true);
            ui->frame_2->setVisible(true);
        }
        else //Cold storagage offline
        {
            //Hide the balances frame.
            ui->frame->setVisible(false);
            //Hide the transaction summary frame
            ui->frame_2->setVisible(false);
            //Give a message on the empty page that we're in offline mode
            OverviewPage::updateAlerts("<b>Cold storage offline mode");
        }
    }

    ui->lblLockedMessage->setVisible(visible);

}

void OverviewPage::unlockWallet() {
    if (walletModel) {
        if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
            walletModel->requireUnlock();
        } else {
            walletModel->lockWallet();
        }

    }
}
