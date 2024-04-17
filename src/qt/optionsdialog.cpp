// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "optionsdialog.h"
#include "ui_optionsdialog.h"

#include "komodounits.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "optionsmodel.h"

#include "netbase.h"
#include "txdb.h" // for -dbcache defaults
#include "main.h"

#include <QDataWidgetMapper>
#include <QDir>
#include <QIntValidator>
#include <QLocale>
#include <QMessageBox>
#include <QTimer>
#include <QApplication>
#include <QFile>
#include <QSettings>
#include <QPalette>

OptionsDialog::OptionsDialog(QWidget *parent, bool enableWallet) :
    QDialog(parent),
    ui(new Ui::OptionsDialog),
    model(0),
    mapper(0)
{
    ui->setupUi(this);

    /* Main elements init */
    ui->databaseCache->setMinimum(nMinDbCache);
    ui->databaseCache->setMaximum(nMaxDbCache);
    ui->threadsScriptVerif->setMinimum(-GetNumCores());
    ui->threadsScriptVerif->setMaximum(MAX_SCRIPTCHECK_THREADS);

    /* Network elements init */
#ifndef USE_UPNP
    ui->mapPortUpnp->setEnabled(false);
#endif

    //Not working....
    ui->mapPortUpnp->hide();

    ui->proxyIp->setEnabled(false);
    ui->proxyPort->setEnabled(false);
    ui->proxyPort->setValidator(new QIntValidator(1, 65535, this));

    ui->proxyIpTor->setEnabled(false);
    ui->proxyPortTor->setEnabled(false);
    ui->proxyPortTor->setValidator(new QIntValidator(1, 65535, this));

    ui->proxyIpI2P->setEnabled(false);
    ui->proxyPortI2P->setEnabled(false);
    ui->proxyPortI2P->setValidator(new QIntValidator(1, 65535, this));

    //diable tor with default proxy in GUI
    ui->proxyReachTor->setEnabled(false);
    ui->proxyReachTor->setVisible(false);
    ui->proxyReachTorLabel->setVisible(false);

    connect(ui->connectSocks, SIGNAL(toggled(bool)), ui->proxyIp, SLOT(setEnabled(bool)));
    connect(ui->connectSocks, SIGNAL(toggled(bool)), ui->proxyPort, SLOT(setEnabled(bool)));
    connect(ui->connectSocks, SIGNAL(toggled(bool)), this, SLOT(updateProxyValidationState()));

    connect(ui->connectSocksTor, SIGNAL(toggled(bool)), ui->proxyIpTor, SLOT(setEnabled(bool)));
    connect(ui->connectSocksTor, SIGNAL(toggled(bool)), ui->proxyPortTor, SLOT(setEnabled(bool)));
    connect(ui->connectSocksTor, SIGNAL(toggled(bool)), this, SLOT(updateProxyValidationState()));

    connect(ui->connectSocksI2P, SIGNAL(toggled(bool)), ui->proxyIpI2P, SLOT(setEnabled(bool)));
    connect(ui->connectSocksI2P, SIGNAL(toggled(bool)), ui->proxyPortI2P, SLOT(setEnabled(bool)));
    connect(ui->connectSocksI2P, SIGNAL(toggled(bool)), this, SLOT(updateProxyValidationStateI2P()));

    ui->i2pLink->setText("<a href=\"https://github.com/ElosysNetwork/elosys/blob/master/doc/i2p.md/\">I2P Guide</a>");
    ui->i2pLink->setTextFormat(Qt::RichText);
    ui->i2pLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ui->i2pLink->setOpenExternalLinks(true);

    ui->torLink->setText("<a href=\"https://github.com/ElosysNetwork/elosys/blob/master/doc/tor.md/\">TOR Guide</a>");
    ui->torLink->setTextFormat(Qt::RichText);
    ui->torLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ui->torLink->setOpenExternalLinks(true);

    /* Window elements init */
#ifdef Q_OS_MAC
    /* remove Window tab on Mac */
    ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabWindow));
#endif

    /* remove Wallet tab in case of -disablewallet */
    if (!enableWallet) {
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabWallet));
    }

    for(int i = 0; i < ui->tabWidget->count(); ++i) {
        ui->tabWidget->widget(i)->setAutoFillBackground(true);
    }

    /* Display elements init */
    QDir translations(":translations");

    ui->komodoAtStartup->setToolTip(ui->komodoAtStartup->toolTip().arg(tr(PACKAGE_NAME)));
    ui->komodoAtStartup->setText(ui->komodoAtStartup->text().arg(tr(PACKAGE_NAME)));

    ui->openKomodoConfButton->setToolTip(ui->openKomodoConfButton->toolTip().arg(tr(PACKAGE_NAME)));

    //Add Wallet themes available
    ui->theme->addItem("Armada", QVariant("armada"));
    ui->theme->addItem("Ghost Ship", QVariant("ghostship"));
    ui->theme->addItem("Night Ship", QVariant("night"));
    ui->theme->addItem("Elosys", QVariant("elosys"));
    ui->theme->addItem("Elosys Map", QVariant("elosysmap"));
    ui->theme->addItem("Treasure", QVariant("treasure"));
    ui->theme->addItem("Treasure Map", QVariant("treasuremap"));
    ui->theme->addItem("Dark", QVariant("dark"));
    ui->theme->addItem("Light", QVariant("light"));

    ui->lang->setToolTip(ui->lang->toolTip().arg(tr(PACKAGE_NAME)));
    ui->lang->addItem(QString("(") + tr("default") + QString(")"), QVariant(""));
    for (const QString &langStr : translations.entryList())
    {
        QLocale locale(langStr);

        /** check if the locale name consists of 2 parts (language_country) */
        if(langStr.contains("_"))
        {
#if QT_VERSION >= 0x040800
            /** display language strings as "native language - native country (locale name)", e.g. "Deutsch - Deutschland (de)" */
            ui->lang->addItem(locale.nativeLanguageName() + QString(" - ") + locale.nativeCountryName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
#else
            /** display language strings as "language - country (locale name)", e.g. "German - Germany (de)" */
            ui->lang->addItem(QLocale::languageToString(locale.language()) + QString(" - ") + QLocale::countryToString(locale.country()) + QString(" (") + langStr + QString(")"), QVariant(langStr));
#endif
        }
        else
        {
#if QT_VERSION >= 0x040800
            /** display language strings as "native language (locale name)", e.g. "Deutsch (de)" */
            ui->lang->addItem(locale.nativeLanguageName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
#else
            /** display language strings as "language (locale name)", e.g. "German (de)" */
            ui->lang->addItem(QLocale::languageToString(locale.language()) + QString(" (") + langStr + QString(")"), QVariant(langStr));
#endif
        }
    }
#if QT_VERSION >= 0x040700
    ui->thirdPartyTxUrls->setPlaceholderText("https://example.com/tx/%s");
#endif

    ui->unit->setModel(new KomodoUnits(this));

    /* Widget-to-option mapper */
    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    mapper->setOrientation(Qt::Vertical);

    /* setup/change UI elements when proxy IPs are invalid/valid */
    ui->proxyIp->setCheckValidator(new ProxyAddressValidator(parent));
    ui->proxyIpTor->setCheckValidator(new ProxyAddressValidator(parent));
    ui->proxyIpI2P->setCheckValidator(new ProxyAddressValidatorI2P(parent));

    connect(ui->proxyIp, SIGNAL(validationDidChange(QValidatedLineEdit *)), this, SLOT(updateProxyValidationState()));
    connect(ui->proxyIpTor, SIGNAL(validationDidChange(QValidatedLineEdit *)), this, SLOT(updateProxyValidationState()));
    connect(ui->controlIpTor, SIGNAL(validationDidChange(QValidatedLineEdit *)), this, SLOT(updateProxyValidationState()));
    connect(ui->proxyIpI2P, SIGNAL(validationDidChange(QValidatedLineEdit *)), this, SLOT(updateProxyValidationStateI2P()));

    connect(ui->proxyPort, SIGNAL(textChanged(const QString&)), this, SLOT(updateProxyValidationState()));
    connect(ui->proxyPortTor, SIGNAL(textChanged(const QString&)), this, SLOT(updateProxyValidationState()));
    connect(ui->controlPortTor, SIGNAL(textChanged(const QString&)), this, SLOT(updateProxyValidationState()));
    connect(ui->proxyPortI2P, SIGNAL(textChanged(const QString&)), this, SLOT(updateProxyValidationStateI2P()));

    /* Offline signing */
    connect(ui->enableOfflineSigning,  SIGNAL(clicked(bool)), this, SLOT(enableOfflineSigningClick(bool)));
    connect(ui->rbOfflineSigning_Sign, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->rbOfflineSigning_Spend,SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
}

OptionsDialog::~OptionsDialog()
{
    delete ui;
}

void OptionsDialog::setModel(OptionsModel *_model)
{

    this->model = _model;

    if(_model)
    {
        /* check if client restart is needed and show persistent message */
        if (_model->isRestartRequired())
            showRestartWarning(true);

        QString strLabel = _model->getOverriddenByCommandLine();
        if (strLabel.isEmpty())
            strLabel = tr("none");
        ui->overriddenByCommandLineLabel->setText(strLabel);

        mapper->setModel(_model);
        setMapper();
        mapper->toFirst();

        updateDefaultProxyNets();

        evaluateOfflineSigning( ui->enableOfflineSigning->isChecked() );
    }
    /* Change without restarting */
    connect(ui->theme, SIGNAL(valueChanged()), this, SLOT(setTheme()));

    /* warn when one of the following settings changes by user action (placed here so init via mapper doesn't trigger them) */

    /* Main */
    connect(ui->databaseCache, SIGNAL(valueChanged(int)), this, SLOT(showRestartWarning()));
    connect(ui->threadsScriptVerif, SIGNAL(valueChanged(int)), this, SLOT(showRestartWarning()));
    /* Wallet */
    connect(ui->enableDeleteTx, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->saplingConsolidationEnabled, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->chkReindex, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->chkBootstrap, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->chkZapWalletTxes, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    /* Network */
    connect(ui->allowIncoming, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->requireTLS, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->connectSocks, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->connectSocks, SIGNAL(clicked(bool)), this, SLOT(enableProxyTypes()));
    connect(ui->connectSocksTor, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->controlPasswordTor, SIGNAL(textChanged(const QString &)), this, SLOT(showRestartWarning()));
    connect(ui->connectSocksI2P, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->allowIncomingI2P, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->Ipv4Disable, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->Ipv6Disable, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));

    /* Display */
    connect(ui->lang, SIGNAL(valueChanged()), this, SLOT(showRestartWarning()));
    connect(ui->thirdPartyTxUrls, SIGNAL(textChanged(const QString &)), this, SLOT(showRestartWarning()));

}

void OptionsDialog::setMapper()
{
    /* Main */
    mapper->addMapping(ui->komodoAtStartup, OptionsModel::StartAtStartup);
    mapper->addMapping(ui->threadsScriptVerif, OptionsModel::ThreadsScriptVerif);
    mapper->addMapping(ui->databaseCache, OptionsModel::DatabaseCache);

    /* Wallet */
    mapper->addMapping(ui->enableOfflineSigning,   OptionsModel::EnableZSigning);
    mapper->addMapping(ui->rbOfflineSigning_Spend, OptionsModel::EnableZSigning_Spend);
    mapper->addMapping(ui->rbOfflineSigning_Sign,  OptionsModel::EnableZSigning_Sign);

    mapper->addMapping(ui->saplingConsolidationEnabled, OptionsModel::SaplingConsolidationEnabled);
    mapper->addMapping(ui->enableDeleteTx, OptionsModel::EnableDeleteTx);
    mapper->addMapping(ui->chkReindex, OptionsModel::EnableReindex);
    mapper->addMapping(ui->chkBootstrap, OptionsModel::EnableBootstrap);
    mapper->addMapping(ui->chkZapWalletTxes, OptionsModel::ZapWalletTxes);

    /* Network */
    mapper->addMapping(ui->mapPortUpnp, OptionsModel::MapPortUPnP);
    mapper->addMapping(ui->allowIncoming, OptionsModel::Listen);
    mapper->addMapping(ui->requireTLS, OptionsModel::EncryptedP2P);

    mapper->addMapping(ui->connectSocks, OptionsModel::ProxyUse);
    mapper->addMapping(ui->proxyIp, OptionsModel::ProxyIP);
    mapper->addMapping(ui->proxyPort, OptionsModel::ProxyPort);

    mapper->addMapping(ui->connectSocksTor, OptionsModel::ProxyUseTor);
    mapper->addMapping(ui->proxyIpTor, OptionsModel::ProxyIPTor);
    mapper->addMapping(ui->proxyPortTor, OptionsModel::ProxyPortTor);

    mapper->addMapping(ui->controlIpTor, OptionsModel::ControlIPTor);
    mapper->addMapping(ui->controlPortTor, OptionsModel::ControlPortTor);
    mapper->addMapping(ui->controlPasswordTor, OptionsModel::ControlPasswordTor);

    mapper->addMapping(ui->allowIncomingI2P, OptionsModel::IncomingI2P);
    mapper->addMapping(ui->connectSocksI2P, OptionsModel::ProxyUseI2P);
    mapper->addMapping(ui->proxyIpI2P, OptionsModel::ProxyIPI2P);
    mapper->addMapping(ui->proxyPortI2P, OptionsModel::ProxyPortI2P);

    mapper->addMapping(ui->Ipv4Disable, OptionsModel::IPv4Disable);
    mapper->addMapping(ui->Ipv6Disable, OptionsModel::IPv6Disable);

    /* Window */
#ifndef Q_OS_MAC
    mapper->addMapping(ui->hideTrayIcon, OptionsModel::HideTrayIcon);
    mapper->addMapping(ui->minimizeToTray, OptionsModel::MinimizeToTray);
    mapper->addMapping(ui->minimizeOnClose, OptionsModel::MinimizeOnClose);
#endif

    /* Display */
    mapper->addMapping(ui->lang, OptionsModel::Language);
    mapper->addMapping(ui->unit, OptionsModel::DisplayUnit);
    mapper->addMapping(ui->enableHexEncoding, OptionsModel::EnableHexMemo);
    mapper->addMapping(ui->theme, OptionsModel::Theme);
    mapper->addMapping(ui->thirdPartyTxUrls, OptionsModel::ThirdPartyTxUrls);


}

void OptionsDialog::setOkButtonState(bool fState)
{
    ui->okButton->setEnabled(fState);
}

bool OptionsDialog::restartPrompt(QString sHeading)
{
    if(model)
    {
        // confirmation dialog
        QMessageBox::StandardButton btnRetVal = QMessageBox::question(this, tr(sHeading.toStdString().c_str() ),
            tr("Client restart required to activate changes.") + "<br><br>" + tr("Client will be shut down. Do you want to proceed?"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

        if(btnRetVal == QMessageBox::Cancel)
        {
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        return false;
    }

}

void OptionsDialog::on_resetButton_clicked()
{
    bool bResult;
    bResult = restartPrompt("Confirm options reset");

    if (bResult==true)
    {
        /* reset all options and close GUI */
        model->Reset();
        QApplication::quit();
    }
}

void OptionsDialog::on_openKomodoConfButton_clicked()
{
    /* explain the purpose of the config file */
    QMessageBox::information(this, tr("Configuration options"),
        tr("The configuration file is used to specify advanced user options which override GUI settings. "
           "Additionally, any command-line options will override this configuration file."));

    /* show an error if there was some problem opening the file */
    if (!GUIUtil::openKomodoConf())
        QMessageBox::critical(this, tr("Error"), tr("The configuration file could not be opened."));
}

void OptionsDialog::on_okButton_clicked()
{
    bool bResult=false;
    if (model->isRestartRequired())
    {
        bResult = restartPrompt("Confirm options change");
        if (bResult == false) {
          /* Restart is required, but user does not want to continue. */
          /* Return to the options dialog. User must select 'cancel' to */
          /* then close the options dialog without applying the changes */
          return;
        }
    }

    mapper->submit();
    accept();
    updateDefaultProxyNets();

    if (bResult == true) {
        QApplication::quit();
    }
}

void OptionsDialog::on_cancelButton_clicked()
{
    reject();
}

void OptionsDialog::on_hideTrayIcon_stateChanged(int fState)
{
    if(fState)
    {
        ui->minimizeToTray->setChecked(false);
        ui->minimizeToTray->setEnabled(false);
    }
    else
    {
        ui->minimizeToTray->setEnabled(true);
    }
}

void OptionsDialog::enableOfflineSigningClick(bool bChecked)
{
  showRestartWarning(false);
  evaluateOfflineSigning(bChecked);
}

void OptionsDialog::evaluateOfflineSigning(bool bChecked)
{
  if (bChecked==true)
  {
    ui->frameOfflineSigning->setVisible(true);
    if (
       (ui->rbOfflineSigning_Sign->isChecked()==false) &&
       (ui->rbOfflineSigning_Spend->isChecked()==false)
       )
    {
      ui->rbOfflineSigning_Sign->setChecked(true);
    }
  }
  else
  {
    ui->frameOfflineSigning->setVisible(false);
  }
}

void OptionsDialog::setTheme()
{
      //Set the theme in the settings
      QSettings settings;
      QString strTheme = ui->theme->itemData(ui->theme->currentIndex()).toString();
      settings.setValue("strTheme", strTheme);

      //Set the Theme in the app
      LogPrintf("Setting Theme: %s %s\n", strTheme.toStdString(),__func__);
      QFile file(":/stylesheets/" + strTheme);
      file.open(QFile::ReadOnly);
      QString stylesheet = QLatin1String(file.readAll());
      qApp->setStyleSheet(stylesheet);

      QPalette newPal(qApp->palette());
      newPal.setColor(QPalette::Link, COLOR_POSITIVE_DARK);
      newPal.setColor(QPalette::LinkVisited, COLOR_NEGATIVE_DARK);
      qApp->setPalette(newPal);
}

void OptionsDialog::enableProxyTypes()
{
      if (ui->connectSocks->isEnabled()) {
          ui->proxyReachIPv4->setEnabled(true);
          ui->proxyReachIPv6->setEnabled(true);
      } else {
          ui->proxyReachIPv4->setEnabled(true);
          ui->proxyReachIPv6->setEnabled(true);
      }

      //disable default proxy tor
      ui->proxyReachTor->setEnabled(false);
      ui->proxyReachTor->setVisible(false);
      ui->proxyReachTorLabel->setVisible(false);
}

void OptionsDialog::showRestartWarning(bool fPersistent)
{
    ui->statusLabel->setStyleSheet("QLabel { color: red; }");

    if(fPersistent)
    {
        ui->statusLabel->setText(tr("Client restart required to activate changes."));
    }
    else
    {
        ui->statusLabel->setText(tr("This change would require a client restart."));
        // clear non-persistent status label after 10 seconds
        // Todo: should perhaps be a class attribute, if we extend the use of statusLabel
        QTimer::singleShot(10000, this, SLOT(clearStatusLabel()));
    }

    //Items that require restart-on-change were connected to showRestartWarning() in ::setModel()
    //Note: The model implementation is misleading: RestartRequired is not evaluated with every
    //      item change. Therefore isRestartRequired always returns false while you're busy
    //      editing the options.
    //      The model only runs setData(), which updates the model to the new values. If a data
    //      change requires a restart then the variable is updated, but its way too late in the
    //      process to do anything usefull with it. The internal data of the model is already
    //      updated. So, selecting Cancel at this point won't 'undo' the changes
    //
    //      A quick fix is to force the restart when a relevant item is changed, regardless if
    //      the user sets it back to its original value, which should actually have to 'undo'
    //      the forced restart.
    model->setRestartRequired(true);

}

void OptionsDialog::clearStatusLabel()
{
    ui->statusLabel->clear();
}

void OptionsDialog::updateProxyValidationState()
{
    QValidatedLineEdit *pUiProxyIp = ui->proxyIp;
    QValidatedLineEdit *otherProxyWidget = (pUiProxyIp == ui->proxyIpTor) ? ui->proxyIp : ui->proxyIpTor;
    if (pUiProxyIp->isValid()
        && (!ui->proxyPort->isEnabled() || ui->proxyPort->text().toInt() > 0)
        && (!ui->proxyPortTor->isEnabled() || ui->proxyPortTor->text().toInt() > 0)
        && ((ui->connectSocksTor->isChecked() && ui->controlIpTor->isValid() && ui->proxyPortTor->text().toInt() > 0) || !ui->connectSocksTor->isChecked()))
    {
        setOkButtonState(otherProxyWidget->isValid()); //only enable ok button if both proxys are valid
        clearStatusLabel();
    }
    else
    {
        setOkButtonState(false);
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(tr("The supplied proxy address is invalid."));
    }
}

void OptionsDialog::updateProxyValidationStateI2P()
{
    QValidatedLineEdit *pUiProxyIp = ui->proxyIpI2P;
    if (pUiProxyIp->isValid() && (!ui->proxyPortI2P->isEnabled() || ui->proxyPortI2P->text().toInt() > 0))
    {
        setOkButtonState(true);
        clearStatusLabel();
    }
    else
    {
        setOkButtonState(false);
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(tr("The supplied I2P sam address is invalid."));
    }
}

void OptionsDialog::updateDefaultProxyNets()
{
    proxyType proxy;
    std::string strProxy;
    QString strDefaultProxyGUI;

    GetProxy(NET_IPV4, proxy);
    strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
    strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
    (strProxy == strDefaultProxyGUI.toStdString()) ? ui->proxyReachIPv4->setChecked(true) : ui->proxyReachIPv4->setChecked(false);

    GetProxy(NET_IPV6, proxy);
    strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
    strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
    (strProxy == strDefaultProxyGUI.toStdString()) ? ui->proxyReachIPv6->setChecked(true) : ui->proxyReachIPv6->setChecked(false);

    GetProxy(NET_ONION, proxy);
    strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
    strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
    (strProxy == strDefaultProxyGUI.toStdString()) ? ui->proxyReachTor->setChecked(true) : ui->proxyReachTor->setChecked(false);
}

ProxyAddressValidator::ProxyAddressValidator(QObject *parent) :
QValidator(parent)
{
}

QValidator::State ProxyAddressValidator::validate(QString &input, int &pos) const
{
    Q_UNUSED(pos);
    // Validate the proxy
    CService serv(LookupNumeric(input.toStdString().c_str(), 9050));
    proxyType addrProxy = proxyType(serv, true);
    if (addrProxy.IsValid())
        return QValidator::Acceptable;

    return QValidator::Invalid;
}

ProxyAddressValidatorI2P::ProxyAddressValidatorI2P(QObject *parent) :
QValidator(parent)
{
}

QValidator::State ProxyAddressValidatorI2P::validate(QString &input, int &pos) const
{
    Q_UNUSED(pos);
    // Validate the proxy
    CService serv(LookupNumeric(input.toStdString().c_str(), 7656));
    proxyType addrProxy = proxyType(serv, true);
    if (addrProxy.IsValid())
        return QValidator::Acceptable;

    return QValidator::Invalid;
}
