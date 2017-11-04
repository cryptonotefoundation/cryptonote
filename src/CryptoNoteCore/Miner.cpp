// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#include "Miner.h"

#include <future>
#include <numeric>
#include <sstream>
#include <thread>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem.hpp>
#include <boost/limits.hpp>
#include <boost/utility/value_init.hpp>

#include "crypto/crypto.h"
#include "Common/CommandLine.h"
#include "Common/StringTools.h"
#include "Serialization/SerializationTools.h"

#include "CryptoNoteFormatUtils.h"
#include "TransactionExtra.h"

using namespace Logging;

namespace CryptoNote
{

  miner::miner(const Currency& currency, IMinerHandler& handler, Logging::ILogger& log) :
    m_currency(currency),
    logger(log, "miner"),
    m_stop(true),
    m_template(boost::value_initialized<BlockTemplate>()),
    m_template_no(0),
    m_diffic(0),
    m_handler(handler),
    m_pausers_count(0),
    m_threads_total(0),
    m_starter_nonce(0),
    m_last_hr_merge_time(0),
    m_hashes(0),
    m_do_print_hashrate(false),
    m_do_mining(false),
    m_current_hash_rate(0),
    m_update_block_template_interval(5),
    m_update_merge_hr_interval(2)
  {
  }
  //-----------------------------------------------------------------------------------------------------
  miner::~miner() {
    stop();
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::set_block_template(const BlockTemplate& bl, const Difficulty& di) {
    std::lock_guard<decltype(m_template_lock)> lk(m_template_lock);

    m_template = bl;

    if (m_template.majorVersion >= BLOCK_MAJOR_VERSION_2) {
      CachedBlock cachedBlk(m_template);
      CryptoNote::TransactionExtraMergeMiningTag mmTag;
      mmTag.depth = 0;
      try {
        m_template.parentBlock.baseTransaction.extra.clear();
        mmTag.merkleRoot = cachedBlk.getAuxiliaryBlockHeaderHash();
        if (!CryptoNote::appendMergeMiningTagToExtra(m_template.parentBlock.baseTransaction.extra, mmTag)) {
          return false;
        }
      } catch (std::exception&) {
        return false;
      }
    }

    m_diffic = di;
    ++m_template_no;
    m_starter_nonce = Crypto::rand<uint32_t>();
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::on_block_chain_update() {
    if (!is_mining()) {
      return true;
    }

    return request_block_template();
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::request_block_template() {
    BlockTemplate bl = boost::value_initialized<BlockTemplate>();
    Difficulty di = 0;
    uint32_t height;
    CryptoNote::BinaryArray extra_nonce;

    if(m_extra_messages.size() && m_config.current_extra_message_index < m_extra_messages.size()) {
      extra_nonce = m_extra_messages[m_config.current_extra_message_index];
    }

    if(!m_handler.get_block_template(bl, m_mine_address, di, height, extra_nonce)) {
      logger(ERROR) << "Failed to get_block_template(), stopping mining";
      return false;
    }

    set_block_template(bl, di);
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::on_idle()
  {
    m_update_block_template_interval.call([&](){
      if(is_mining()) 
        request_block_template();
      return true;
    });

    m_update_merge_hr_interval.call([&](){
      merge_hr();
      return true;
    });

    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::do_print_hashrate(bool do_hr)
  {
    m_do_print_hashrate = do_hr;
  }

  uint64_t millisecondsSinceEpoch() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
  }

  //-----------------------------------------------------------------------------------------------------
  void miner::merge_hr()
  {
    if(m_last_hr_merge_time && is_mining()) {
      m_current_hash_rate = m_hashes * 1000 / (millisecondsSinceEpoch() - m_last_hr_merge_time + 1);
      std::lock_guard<std::mutex> lk(m_last_hash_rates_lock);
      m_last_hash_rates.push_back(m_current_hash_rate);
      if(m_last_hash_rates.size() > 19)
        m_last_hash_rates.pop_front();

      if(m_do_print_hashrate) {
        uint64_t total_hr = std::accumulate(m_last_hash_rates.begin(), m_last_hash_rates.end(), static_cast<uint64_t>(0));
        float hr = static_cast<float>(total_hr)/static_cast<float>(m_last_hash_rates.size());
        std::cout << "hashrate: " << std::setprecision(4) << std::fixed << hr << ENDL;
      }
    }
    
    m_last_hr_merge_time = millisecondsSinceEpoch();
    m_hashes = 0;
  }

  bool miner::init(const MinerConfig& config) {
    if (!config.extraMessages.empty()) {
      std::string buff;
      if (!Common::loadFileToString(config.extraMessages, buff)) {
        logger(ERROR, BRIGHT_RED) << "Failed to load file with extra messages: " << config.extraMessages; 
        return false; 
      }
      std::vector<std::string> extra_vec;
      boost::split(extra_vec, buff, boost::is_any_of("\n"), boost::token_compress_on );
      m_extra_messages.resize(extra_vec.size());
      for(size_t i = 0; i != extra_vec.size(); i++) {
        boost::algorithm::trim(extra_vec[i]);
        if(!extra_vec[i].size())
          continue;
        BinaryArray ba = Common::asBinaryArray(Common::base64Decode(extra_vec[i]));
        if(buff != "0")
          m_extra_messages[i] = ba;
      }
      m_config_folder_path = boost::filesystem::path(config.extraMessages).parent_path().string();
      m_config = boost::value_initialized<decltype(m_config)>();

      std::string filebuf;
      if (Common::loadFileToString(m_config_folder_path + "/" + CryptoNote::parameters::MINER_CONFIG_FILE_NAME, filebuf)) {
        loadFromJson(m_config, filebuf);
      }

      logger(INFO) << "Loaded " << m_extra_messages.size() << " extra messages, current index " << m_config.current_extra_message_index;
    }

    if(!config.startMining.empty()) {
      if (!m_currency.parseAccountAddressString(config.startMining, m_mine_address)) {
        logger(ERROR) << "Target account address " << config.startMining << " has wrong format, starting daemon canceled";
        return false;
      }
      m_threads_total = 1;
      m_do_mining = true;
      if(config.miningThreads > 0) {
        m_threads_total = config.miningThreads;
      }
    }

    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::is_mining()
  {
    return !m_stop;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::start(const AccountPublicAddress& adr, size_t threads_count)
  {   
    if (is_mining()) {
      logger(ERROR) << "Starting miner but it's already started";
      return false;
    }

    std::lock_guard<std::mutex> lk(m_threads_lock);

    if(!m_threads.empty()) {
      logger(ERROR) << "Unable to start miner because there are active mining threads";
      return false;
    }

    m_mine_address = adr;
    m_threads_total = static_cast<uint32_t>(threads_count);
    m_starter_nonce = Crypto::rand<uint32_t>();

    if (!m_template_no) {
      request_block_template(); //lets update block template
    }

    m_stop = false;

    for (uint32_t i = 0; i != threads_count; i++) {
      m_threads.push_back(std::thread(std::bind(&miner::worker_thread, this, i)));
    }

    logger(INFO) << "Mining has started with " << threads_count << " threads, good luck!";
    return true;
  }
  
  //-----------------------------------------------------------------------------------------------------
  uint64_t miner::get_speed()
  {
    if(is_mining())
      return m_current_hash_rate;
    else
      return 0;
  }
  
  //-----------------------------------------------------------------------------------------------------
  void miner::send_stop_signal() 
  {
    m_stop = true;
  }

  //-----------------------------------------------------------------------------------------------------
  bool miner::stop()
  {
    send_stop_signal();
    std::lock_guard<std::mutex> lk(m_threads_lock);

    for (auto& th : m_threads) {
      th.join();
    }

    m_threads.clear();
    logger(INFO) << "Mining has been stopped, " << m_threads.size() << " finished" ;
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::find_nonce_for_given_block(Crypto::cn_context &context, BlockTemplate& bl, const Difficulty& diffic) {

    unsigned nthreads = std::thread::hardware_concurrency();

    if (nthreads > 0 && diffic > 5) {
      std::vector<std::future<void>> threads(nthreads);
      std::atomic<uint32_t> foundNonce;
      std::atomic<bool> found(false);
      uint32_t startNonce = Crypto::rand<uint32_t>();

      for (unsigned i = 0; i < nthreads; ++i) {
        threads[i] = std::async(std::launch::async, [&, i]() {
          Crypto::cn_context localctx;
          Crypto::Hash h;

          BlockTemplate lb(bl); // copy to local block

          for (uint32_t nonce = startNonce + i; !found; nonce += nthreads) {
            lb.nonce = nonce;

            CachedBlock cb(lb);
            try {
              h = cb.getBlockLongHash(localctx);
            } catch (std::exception&) {
              return;
            }

            if (check_hash(h, diffic)) {
              foundNonce = nonce;
              found = true;
              return;
            }
          }
        });
      }

      for (auto& t : threads) {
        t.wait();
      }

      if (found) {
        bl.nonce = foundNonce.load();
      }

      return found;
    } else {
      for (; bl.nonce != std::numeric_limits<uint32_t>::max(); bl.nonce++) {
        Crypto::Hash h;
        CachedBlock cb(bl);
        try {
          h = cb.getBlockLongHash(context);
        } catch (std::exception&) {
          return false;
        }

        if (check_hash(h, diffic)) {
          return true;
        }
      }
    }

    return false;
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::on_synchronized()
  {
    if(m_do_mining) {
      start(m_mine_address, m_threads_total);
    }
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::pause()
  {
    std::lock_guard<std::mutex> lk(m_miners_count_lock);
    ++m_pausers_count;
    if(m_pausers_count == 1 && is_mining())
      logger(TRACE) << "MINING PAUSED";
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::resume()
  {
    std::lock_guard<std::mutex> lk(m_miners_count_lock);
    --m_pausers_count;
    if(m_pausers_count < 0)
    {
      m_pausers_count = 0;
      logger(ERROR) << "Unexpected miner::resume() called";
    }
    if(!m_pausers_count && is_mining())
      logger(TRACE) << "MINING RESUMED";
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::worker_thread(uint32_t th_local_index)
  {
    logger(INFO) << "Miner thread was started ["<< th_local_index << "]";
    uint32_t nonce = m_starter_nonce + th_local_index;
    Difficulty local_diff = 0;
    uint32_t local_template_ver = 0;
    Crypto::cn_context context;
    BlockTemplate b;

    while(!m_stop)
    {
      if(m_pausers_count) //anti split workaround
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      if(local_template_ver != m_template_no) {
        std::unique_lock<std::mutex> lk(m_template_lock);
        b = m_template;
        local_diff = m_diffic;
        lk.unlock();

        local_template_ver = m_template_no;
        nonce = m_starter_nonce + th_local_index;
      }

      if(!local_template_ver)//no any set_block_template call
      {
        logger(TRACE) << "Block template not set yet";
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        continue;
      }

      b.nonce = nonce;
      Crypto::Hash h;

      CachedBlock cb(b);
      if (!m_stop) {
        try {
          h = cb.getBlockLongHash(context);
        } catch (std::exception& e) {
          logger(ERROR) << "getBlockLongHash failed: " << e.what();
          m_stop = true;
        }
      }

      if (!m_stop && check_hash(h, local_diff))
      {
        //we lucky!
        ++m_config.current_extra_message_index;

        logger(INFO, GREEN) << "Found block for difficulty: " << local_diff;

        if(!m_handler.handle_block_found(b)) {
          --m_config.current_extra_message_index;
        } else {
          //success update, lets update config
          Common::saveStringToFile(m_config_folder_path + "/" + CryptoNote::parameters::MINER_CONFIG_FILE_NAME, storeToJson(m_config));
        }
      }

      nonce += m_threads_total;
      ++m_hashes;
    }
    logger(INFO) << "Miner thread stopped ["<< th_local_index << "]";
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
}
