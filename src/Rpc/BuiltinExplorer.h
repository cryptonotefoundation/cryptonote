// Copyright (c) 2016-2026, The Karbo developers
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

#include "CoreRpcServerCommandsDefinitions.h"
#include "BlockchainExplorer/BlockchainExplorerDataBuilder.h"

namespace CryptoNote {

class Core;
class NodeServer;
class ICryptoNoteProtocolQuery;
class RpcServer;

// Built-in block explorer + simple status pages served as HTML over HTTP.
//
// Pulled out of RpcServer to keep that class focused on JSON / binary RPC
// dispatch; everything HTML and the one explorer-specific JSON-RPC method
// (`search`) lives here.
//
// The routing layer in RpcServer still owns URL→handler dispatch
// (see RpcServer::processRequest); RpcServer's HTML on_get_* methods are
// thin forwarders that call into the BuiltinExplorer instance held as a
// member. This lets the routing table — including the templated method
// pointers in `s_handlers` — stay unchanged.
class BuiltinExplorer {
public:
  BuiltinExplorer(Core& core,
                  NodeServer& p2p,
                  const ICryptoNoteProtocolQuery& protocolQuery,
                  BlockchainExplorerDataBuilder& explorerBuilder,
                  RpcServer& parent);

  // Status pages.
  bool on_get_index(const COMMAND_HTTP::request& req, COMMAND_HTTP::response& res);
  bool on_get_supply(const COMMAND_HTTP::request& req, COMMAND_HTTP::response& res);
  bool on_get_payment_id(const COMMAND_HTTP::request& req, COMMAND_HTTP::response& res);

  // Block-explorer pages.
  bool on_get_explorer(const COMMAND_EXPLORER::request& req, COMMAND_EXPLORER::response& res);
  bool on_get_explorer_block_by_hash(const COMMAND_EXPLORER_GET_BLOCK_DETAILS_BY_HASH::request& req,
                                     COMMAND_EXPLORER_GET_BLOCK_DETAILS_BY_HASH::response& res);
  bool on_get_explorer_tx_by_hash(const COMMAND_EXPLORER_GET_TRANSACTION_DETAILS_BY_HASH::request& req,
                                  COMMAND_EXPLORER_GET_TRANSACTION_DETAILS_BY_HASH::response& res);
  bool on_get_explorer_txs_by_payment_id(const COMMAND_EXPLORER_GET_TRANSACTIONS_BY_PAYMENT_ID::request& req,
                                         COMMAND_EXPLORER_GET_TRANSACTIONS_BY_PAYMENT_ID::response& res);
  bool on_get_explorer_account_number(const COMMAND_EXPLORER_GET_ACCOUNT_NUMBER::request& req,
                                      COMMAND_EXPLORER_GET_ACCOUNT_NUMBER::response& res);
  bool on_get_explorer_address(const COMMAND_EXPLORER_GET_ADDRESS::request& req,
                               COMMAND_EXPLORER_GET_ADDRESS::response& res);

  // Explorer-only JSON RPC: client-side search dispatcher invoked from
  // the JS in the explorer pages.
  bool on_explorer_search(const COMMAND_RPC_EXPLORER_SEARCH::request& req,
                          COMMAND_RPC_EXPLORER_SEARCH::response& res);

private:
  Core& m_core;
  NodeServer& m_p2p;
  const ICryptoNoteProtocolQuery& m_protocolQuery;
  BlockchainExplorerDataBuilder& m_explorerBuilder;
  RpcServer& m_parent;  // for getRpcConnectionsCount(); see on_get_index
};

} // namespace CryptoNote
