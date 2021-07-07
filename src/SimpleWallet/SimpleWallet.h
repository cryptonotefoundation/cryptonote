// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2016, XDN developers
// Copyright (c) 2014-2017, The Monero Project
// Copyright (c) 2016-2020, The Karbo developers
//
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>

#include <boost/program_options/variables_map.hpp>

#include "android.h"
#include "IWalletLegacy.h"
#include "Common/PasswordContainer.h"

#include "Common/ConsoleHandler.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/Currency.h"
#include "NodeRpcProxy/NodeRpcProxy.h"
#include "WalletLegacy/WalletHelper.h"
#include "WalletLegacy/WalletLegacy.h"

#include <Logging/LoggerRef.h>
#include <Logging/LoggerManager.h>

#include <System/Dispatcher.h>
#include <System/Ipv4Address.h>

namespace{
	Tools::PasswordContainer pwd_container;
}

namespace CryptoNote
{
  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class simple_wallet : public CryptoNote::INodeObserver, public CryptoNote::IWalletLegacyObserver, public CryptoNote::INodeRpcProxyObserver {
  public:
    simple_wallet(System::Dispatcher& dispatcher, const CryptoNote::Currency& currency, Logging::LoggerManager& log);

    bool init(const boost::program_options::variables_map& vm);
    bool deinit();
    bool run();
    void stop();

    bool process_command(const std::vector<std::string> &args);
    std::string get_commands_str();

    const CryptoNote::Currency& currency() const { return m_currency; }

  private:

    Logging::LoggerMessage success_msg_writer(bool color = false) {
      return logger(Logging::INFO, color ? Logging::GREEN : Logging::DEFAULT);
    }

    Logging::LoggerMessage fail_msg_writer() const {
      auto msg = logger(Logging::ERROR, Logging::BRIGHT_RED);
      msg << "Error: ";
      return msg;
    }

    void handle_command_line(const boost::program_options::variables_map& vm);

    bool new_wallet(const std::string &wallet_file, const std::string& password);
    bool new_wallet(const std::string &wallet_file, const std::string& password, const Crypto::SecretKey& spend_secret_key, const Crypto::SecretKey& view_secret_key);
    bool new_wallet(const std::string &wallet_file, const std::string& password, const AccountKeys& private_keys);
    bool new_tracking_wallet(AccountKeys &tracking_key, const std::string &wallet_file, const std::string& password);
    bool close_wallet();

    bool help(const std::vector<std::string> &args = std::vector<std::string>());
    bool seed(const std::vector<std::string> &args = std::vector<std::string>());
    bool exit(const std::vector<std::string> &args);
    bool start_mining(const std::vector<std::string> &args);
    bool stop_mining(const std::vector<std::string> &args);
    bool show_balance(const std::vector<std::string> &args = std::vector<std::string>());
    bool export_keys(const std::vector<std::string> &args = std::vector<std::string>());
    bool export_tracking_key(const std::vector<std::string> &args = std::vector<std::string>());
    bool show_incoming_transfers(const std::vector<std::string> &args);
    bool show_outgoing_transfers(const std::vector<std::string> &args);
    bool show_payments(const std::vector<std::string> &args);
    bool show_blockchain_height(const std::vector<std::string> &args);
    bool show_unlocked_outputs_count(const std::vector<std::string> &args);
    bool list_transfers(const std::vector<std::string> &args);
    bool transfer(const std::vector<std::string> &args);
    bool prepare_tx(const std::vector<std::string>& args);
    bool print_address(const std::vector<std::string> &args = std::vector<std::string>());
    bool save(const std::vector<std::string> &args);
    bool reset(const std::vector<std::string> &args);
    bool set_log(const std::vector<std::string> &args);
    bool payment_id(const std::vector<std::string> &args);
    bool change_password(const std::vector<std::string> &args);
    bool estimate_fusion(const std::vector<std::string> &args);
    bool optimize(const std::vector<std::string> &args);
    bool get_tx_key(const std::vector<std::string> &args);
    bool get_tx_proof(const std::vector<std::string> &args);
    bool get_reserve_proof(const std::vector<std::string> &args);
    bool sign_message(const std::vector<std::string> &args);
    bool verify_message(const std::vector<std::string> &args);

    void printConnectionError() const;
    uint64_t getMinimalFee();

    //---------------- IWalletLegacyObserver -------------------------
    virtual void initCompleted(std::error_code result) override;
    virtual void externalTransactionCreated(CryptoNote::TransactionId transactionId) override;
    virtual void synchronizationCompleted(std::error_code result) override;
    virtual void synchronizationProgressUpdated(uint32_t current, uint32_t total) override;
    //----------------------------------------------------------

    //----------------- INodeRpcProxyObserver --------------------------
    virtual void connectionStatusUpdated(bool connected) override;
    //----------------------------------------------------------

    friend class refresh_progress_reporter_t;

    class refresh_progress_reporter_t
    {
    public:
      refresh_progress_reporter_t(CryptoNote::simple_wallet& simple_wallet)
        : m_simple_wallet(simple_wallet)
        , m_blockchain_height(0)
        , m_blockchain_height_update_time()
        , m_print_time()
      {
      }

      void update(uint64_t height, bool force = false)
      {
        auto current_time = std::chrono::system_clock::now();
        if (std::chrono::seconds(m_simple_wallet.currency().difficultyTarget() / 2) < current_time - m_blockchain_height_update_time ||
            m_blockchain_height <= height) {
          update_blockchain_height();
          m_blockchain_height = (std::max)(m_blockchain_height, height);
        }

        if (std::chrono::milliseconds(1) < current_time - m_print_time || force) {
          std::cout << "Height " << height << " of " << m_blockchain_height << '\r';
          m_print_time = current_time;
        }
      }

    private:
      void update_blockchain_height()
      {
        uint64_t blockchain_height = m_simple_wallet.m_node->getLastLocalBlockHeight();
        m_blockchain_height = blockchain_height;
        m_blockchain_height_update_time = std::chrono::system_clock::now();
      }

    private:
      CryptoNote::simple_wallet& m_simple_wallet;
      uint64_t m_blockchain_height;
      std::chrono::system_clock::time_point m_blockchain_height_update_time;
      std::chrono::system_clock::time_point m_print_time;
    };

  private:
    std::string m_wallet_file_arg;
    std::string m_generate_new;
    std::string m_import_new;
    std::string m_restore_new;
    std::string m_track_new;
    std::string m_import_path;
    std::string m_daemon_address;
    std::string m_daemon_host;
    std::string m_daemon_path;
    std::string m_daemon_cert;
    std::string m_mnemonic_seed;
    std::string m_view_key;
    std::string m_spend_key;
    std::string m_wallet_file;
    uint16_t m_daemon_port;
    uint32_t m_scan_height;
    bool m_daemon_ssl;
    bool m_daemon_no_verify;
    bool m_do_not_relay_tx;
    
    std::unique_ptr<std::promise<std::error_code>> m_initResultPromise;

    Common::ConsoleHandler m_consoleHandler;
    const CryptoNote::Currency& m_currency;
    Logging::LoggerManager& m_logManager;
    System::Dispatcher& m_dispatcher;
    Logging::LoggerRef logger;

    std::unique_ptr<CryptoNote::NodeRpcProxy> m_node;
    std::unique_ptr<CryptoNote::IWalletLegacy> m_wallet;
    refresh_progress_reporter_t m_refresh_progress_reporter;

    bool m_walletSynchronized;
    bool m_trackingWallet;
    std::mutex m_walletSynchronizedMutex;
    std::condition_variable m_walletSynchronizedCV;
  };
}
