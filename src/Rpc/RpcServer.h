// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016, The Forknote developers
// Copyright (c) 2016-2020, The Karbo developers
//
// This file is part of Karbo.
//
// Karbo is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Karbo is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Karbo.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include "HttpServer.h"

#include <functional>
#include <unordered_map>

#include <Logging/LoggerRef.h>
#include "ITransaction.h"
#include "CoreRpcServerCommandsDefinitions.h"
#include "BlockchainExplorer/BlockchainExplorerDataBuilder.h"
#include "CryptoNoteCore/Core.h"
#include "Common/Math.h"

namespace CryptoNote {

class Core;
class NodeServer;
class BlockchainExplorer;
class ICryptoNoteProtocolQuery;

class RpcServer : public HttpServer {
public:
  RpcServer(System::Dispatcher& dispatcher, Logging::ILogger& log, Core& core, NodeServer& p2p, ICryptoNoteProtocolQuery& protocolQuery);

  typedef std::function<bool(RpcServer*, const HttpRequest& request, HttpResponse& response)> HandlerFunction;
  bool restrictRpc(const bool is_resctricted);
  bool enableCors(const std::string domain);
  bool setFeeAddress(const std::string& fee_address, const AccountPublicAddress& fee_acc);
  bool setFeeAmount(const uint64_t fee_amount);
  bool setViewKey(const std::string& view_key);
  bool setContactInfo(const std::string& contact);
  bool checkIncomingTransactionForFee(const BinaryArray& tx_blob);
  std::string getCorsDomain();

private:

  template <class Handler>
  struct RpcHandler {
    const Handler handler;
    const bool allowBusyCore;
  };

  typedef void (RpcServer::*HandlerPtr)(const HttpRequest& request, HttpResponse& response);
  static std::unordered_map<std::string, RpcHandler<HandlerFunction>> s_handlers;

  virtual void processRequest(const HttpRequest& request, HttpResponse& response) override;
  bool processJsonRpcRequest(const HttpRequest& request, HttpResponse& response);
  bool isCoreReady();

  // binary handlers
  bool on_get_blocks(const COMMAND_RPC_GET_BLOCKS_FAST::request& req, COMMAND_RPC_GET_BLOCKS_FAST::response& res);
  bool on_query_blocks(const COMMAND_RPC_QUERY_BLOCKS::request& req, COMMAND_RPC_QUERY_BLOCKS::response& res);
  bool on_query_blocks_lite(const COMMAND_RPC_QUERY_BLOCKS_LITE::request& req, COMMAND_RPC_QUERY_BLOCKS_LITE::response& res);
  bool on_get_indexes(const COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::request& req, COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response& res);
  bool on_get_random_outs(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res);
  bool on_get_pool_changes(const COMMAND_RPC_GET_POOL_CHANGES::request& req, COMMAND_RPC_GET_POOL_CHANGES::response& rsp);
  bool on_get_pool_changes_lite(const COMMAND_RPC_GET_POOL_CHANGES_LITE::request& req, COMMAND_RPC_GET_POOL_CHANGES_LITE::response& rsp);

  // http handlers
  bool on_get_index(const COMMAND_HTTP::request& req, COMMAND_HTTP::response& res);
  bool on_get_supply(const COMMAND_HTTP::request& req, COMMAND_HTTP::response& res);
  bool on_get_payment_id(const COMMAND_HTTP::request& req, COMMAND_HTTP::response& res);

  // json handlers
  bool on_get_info(const COMMAND_RPC_GET_INFO::request& req, COMMAND_RPC_GET_INFO::response& res);
  bool on_get_height(const COMMAND_RPC_GET_HEIGHT::request& req, COMMAND_RPC_GET_HEIGHT::response& res);
  bool on_get_transactions(const COMMAND_RPC_GET_TRANSACTIONS::request& req, COMMAND_RPC_GET_TRANSACTIONS::response& res);
  bool on_send_raw_transaction(const COMMAND_RPC_SEND_RAW_TRANSACTION::request& req, COMMAND_RPC_SEND_RAW_TRANSACTION::response& res);
  bool on_start_mining(const COMMAND_RPC_START_MINING::request& req, COMMAND_RPC_START_MINING::response& res);
  bool on_stop_mining(const COMMAND_RPC_STOP_MINING::request& req, COMMAND_RPC_STOP_MINING::response& res);
  bool on_stop_daemon(const COMMAND_RPC_STOP_DAEMON::request& req, COMMAND_RPC_STOP_DAEMON::response& res);
  bool on_get_fee_address(const COMMAND_RPC_GET_FEE_ADDRESS::request& req, COMMAND_RPC_GET_FEE_ADDRESS::response& res);
  bool on_get_peer_list(const COMMAND_RPC_GET_PEER_LIST::request& req, COMMAND_RPC_GET_PEER_LIST::response& res);
  bool on_get_connections(const COMMAND_RPC_GET_CONNECTIONS::request& req, COMMAND_RPC_GET_CONNECTIONS::response& res);
  
  bool on_get_blocks_details_by_heights(const COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HEIGHTS::request& req, COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HEIGHTS::response& rsp);
  bool on_get_blocks_details_by_hashes(const COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES::request& req, COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES::response& rsp);
  bool on_get_block_details_by_height(const COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::response& rsp);
  bool on_get_block_details_by_hash(const COMMAND_RPC_GET_BLOCK_DETAILS_BY_HASH::request& req, COMMAND_RPC_GET_BLOCK_DETAILS_BY_HASH::response& rsp);
  bool on_get_blocks_hashes_by_timestamps(const COMMAND_RPC_GET_BLOCKS_HASHES_BY_TIMESTAMPS::request& req, COMMAND_RPC_GET_BLOCKS_HASHES_BY_TIMESTAMPS::response& rsp);
  bool on_get_transactions_details_by_hashes(const COMMAND_RPC_GET_TRANSACTIONS_DETAILS_BY_HASHES::request& req, COMMAND_RPC_GET_TRANSACTIONS_DETAILS_BY_HASHES::response& rsp);
  bool on_get_transaction_details_by_hash(const COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASH::request& req, COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASH::response& rsp);
  bool on_get_transactions_details_by_heights(const COMMAND_RPC_GET_TRANSACTIONS_DETAILS_BY_HEIGHTS::request& req, COMMAND_RPC_GET_TRANSACTIONS_DETAILS_BY_HEIGHTS::response& rsp);
  bool on_get_transaction_hashes_by_paymentid(const COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID::request& req, COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID::response& rsp);
  bool on_get_transactions_with_output_global_indexes_by_heights(const COMMAND_RPC_GET_TRANSACTIONS_WITH_OUTPUT_GLOBAL_INDEXES_BY_HEIGHTS::request& req, COMMAND_RPC_GET_TRANSACTIONS_WITH_OUTPUT_GLOBAL_INDEXES_BY_HEIGHTS::response& rsp);


  // json rpc
  bool on_getblockcount(const COMMAND_RPC_GETBLOCKCOUNT::request& req, COMMAND_RPC_GETBLOCKCOUNT::response& res);
  bool on_getblockhash(const COMMAND_RPC_GETBLOCKHASH::request& req, COMMAND_RPC_GETBLOCKHASH::response& res);
  bool on_getblocktemplate(const COMMAND_RPC_GETBLOCKTEMPLATE::request& req, COMMAND_RPC_GETBLOCKTEMPLATE::response& res);
  bool on_get_currency_id(const COMMAND_RPC_GET_CURRENCY_ID::request& req, COMMAND_RPC_GET_CURRENCY_ID::response& res);
  bool on_submitblock(const COMMAND_RPC_SUBMITBLOCK::request& req, COMMAND_RPC_SUBMITBLOCK::response& res);
  bool on_blocks_list_json(const COMMAND_RPC_GET_BLOCKS_LIST::request& req, COMMAND_RPC_GET_BLOCKS_LIST::response& res);
  bool on_alt_blocks_list_json(const COMMAND_RPC_GET_ALT_BLOCKS_LIST::request& req, COMMAND_RPC_GET_ALT_BLOCKS_LIST::response& res);
  bool on_get_last_block_header(const COMMAND_RPC_GET_LAST_BLOCK_HEADER::request& req, COMMAND_RPC_GET_LAST_BLOCK_HEADER::response& res);
  bool on_get_block_header_by_hash(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::response& res);
  bool on_get_block_header_by_height(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::response& res);
  bool on_get_block_timestamp_by_height(const COMMAND_RPC_GET_BLOCK_TIMESTAMP_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCK_TIMESTAMP_BY_HEIGHT::response& res);
  bool on_get_transactions_pool_short(const COMMAND_RPC_GET_TRANSACTIONS_POOL_SHORT::request& req, COMMAND_RPC_GET_TRANSACTIONS_POOL_SHORT::response& res);
  bool on_get_transactions_pool_raw(const COMMAND_RPC_GET_RAW_TRANSACTIONS_POOL::request& req, COMMAND_RPC_GET_RAW_TRANSACTIONS_POOL::response& res);
  bool on_get_transactions_pool(const COMMAND_RPC_GET_TRANSACTIONS_POOL::request& req, COMMAND_RPC_GET_TRANSACTIONS_POOL::response& res);
  bool on_get_transactions_by_payment_id(const COMMAND_RPC_GET_TRANSACTIONS_BY_PAYMENT_ID::request& req, COMMAND_RPC_GET_TRANSACTIONS_BY_PAYMENT_ID::response& res);
  bool on_check_transaction_key(const COMMAND_RPC_CHECK_TRANSACTION_KEY::request& req, COMMAND_RPC_CHECK_TRANSACTION_KEY::response& res);
  bool on_check_transaction_with_view_key(const COMMAND_RPC_CHECK_TRANSACTION_WITH_PRIVATE_VIEW_KEY::request& req, COMMAND_RPC_CHECK_TRANSACTION_WITH_PRIVATE_VIEW_KEY::response& res);
  bool on_check_transaction_proof(const COMMAND_RPC_CHECK_TRANSACTION_PROOF::request& req, COMMAND_RPC_CHECK_TRANSACTION_PROOF::response& res);
  bool on_check_reserve_proof(const COMMAND_RPC_CHECK_RESERVE_PROOF::request& req, COMMAND_RPC_CHECK_RESERVE_PROOF::response& res);
  bool on_validate_address(const COMMAND_RPC_VALIDATE_ADDRESS::request& req, COMMAND_RPC_VALIDATE_ADDRESS::response& res);
  bool on_verify_message(const COMMAND_RPC_VERIFY_MESSAGE::request& req, COMMAND_RPC_VERIFY_MESSAGE::response& res);
  bool on_get_stats_by_heights(const COMMAND_RPC_GET_STATS_BY_HEIGHTS::request& req, COMMAND_RPC_GET_STATS_BY_HEIGHTS::response& res);
  bool on_get_stats_by_heights_range(const COMMAND_RPC_GET_STATS_BY_HEIGHTS_RANGE::request& req, COMMAND_RPC_GET_STATS_BY_HEIGHTS_RANGE::response& res);
  bool on_resolve_open_alias(const COMMAND_RPC_RESOLVE_OPEN_ALIAS::request& req, COMMAND_RPC_RESOLVE_OPEN_ALIAS::response& res);

  void fill_block_header_response(const Block& blk, bool orphan_status, uint32_t height, const Crypto::Hash& hash, block_header_response& responce);

  Logging::LoggerRef logger;
  CryptoNote::Core& m_core;
  CryptoNote::NodeServer& m_p2p;
  CryptoNote::BlockchainExplorerDataBuilder blockchainExplorerDataBuilder;
  const ICryptoNoteProtocolQuery& m_protocolQuery;
  bool m_restricted_rpc;
  std::string m_cors_domain;
  std::string m_fee_address;
  uint64_t    m_fee_amount;
  std::string m_contact_info;
  Crypto::SecretKey m_view_key = NULL_SECRET_KEY;
  CryptoNote::AccountPublicAddress m_fee_acc;
};

}
