// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "validationinterface.h"

static CMainSignals g_signals;

CMainSignals& GetMainSignals()
{
    return g_signals;
}

void RegisterValidationInterface(CValidationInterface* pwalletIn) {
    g_signals.UpdatedBlockTip.connect(boost::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn, std::placeholders::_1));
    g_signals.SyncTransactions.connect(boost::bind(&CValidationInterface::SyncTransactions, pwalletIn, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    g_signals.EraseTransaction.connect(boost::bind(&CValidationInterface::EraseFromWallet, pwalletIn, std::placeholders::_1));
    g_signals.UpdatedTransaction.connect(boost::bind(&CValidationInterface::UpdatedTransaction, pwalletIn, std::placeholders::_1));
    g_signals.RescanWallet.connect(boost::bind(&CValidationInterface::RescanWallet, pwalletIn));
    g_signals.ChainTip.connect(boost::bind(&CValidationInterface::ChainTip, pwalletIn, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
    g_signals.Inventory.connect(boost::bind(&CValidationInterface::Inventory, pwalletIn, std::placeholders::_1));
    g_signals.Broadcast.connect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, std::placeholders::_1));
    g_signals.BlockChecked.connect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, std::placeholders::_1, std::placeholders::_2));
}

void UnregisterValidationInterface(CValidationInterface* pwalletIn) {
    g_signals.BlockChecked.disconnect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, std::placeholders::_1,std::placeholders:: _2));
    g_signals.Broadcast.disconnect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, std::placeholders::_1));
    g_signals.Inventory.disconnect(boost::bind(&CValidationInterface::Inventory, pwalletIn, std::placeholders::_1));
    g_signals.ChainTip.disconnect(boost::bind(&CValidationInterface::ChainTip, pwalletIn, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
    g_signals.UpdatedTransaction.disconnect(boost::bind(&CValidationInterface::UpdatedTransaction, pwalletIn, std::placeholders::_1));
    g_signals.EraseTransaction.disconnect(boost::bind(&CValidationInterface::EraseFromWallet, pwalletIn, std::placeholders::_1));
    g_signals.SyncTransactions.disconnect(boost::bind(&CValidationInterface::SyncTransactions, pwalletIn, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    g_signals.RescanWallet.disconnect(boost::bind(&CValidationInterface::RescanWallet, pwalletIn));
    g_signals.UpdatedBlockTip.disconnect(boost::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn, std::placeholders::_1));
}

void UnregisterAllValidationInterfaces() {
    g_signals.BlockChecked.disconnect_all_slots();
    g_signals.Broadcast.disconnect_all_slots();
    g_signals.Inventory.disconnect_all_slots();
    g_signals.ChainTip.disconnect_all_slots();
    g_signals.UpdatedTransaction.disconnect_all_slots();
    g_signals.EraseTransaction.disconnect_all_slots();
    g_signals.SyncTransactions.disconnect_all_slots();
    g_signals.RescanWallet.disconnect_all_slots();
    g_signals.UpdatedBlockTip.disconnect_all_slots();
}

void SyncWithWallets(const std::vector<CTransaction> &vtx, const CBlock *pblock, const int nHeight) {
    g_signals.SyncTransactions(vtx, pblock, nHeight);
}

void EraseFromWallets(const uint256 &hash) {
    g_signals.EraseTransaction(hash);
}

void RescanWallets() {
    g_signals.RescanWallet();
}
