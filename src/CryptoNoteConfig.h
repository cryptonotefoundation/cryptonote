// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero project
// Copyright (c) 2014-2018, The Forknote developers
// Copyright (c) 2018, Ryo Currency Project
// Copyright (c) 2018-2019, The TurtleCoin developers
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

#include <cstddef>
#include <cstdint>

namespace CryptoNote {
namespace parameters {

const uint64_t DIFFICULTY_TARGET                             = 240; // seconds
const uint64_t EXPECTED_NUMBER_OF_BLOCKS_PER_DAY             = 24 * 60 * 60 / DIFFICULTY_TARGET;
const uint64_t CRYPTONOTE_MAX_BLOCK_NUMBER                   = 500000000;
const size_t   CRYPTONOTE_MAX_BLOCK_BLOB_SIZE                = 500000000;
const size_t   CRYPTONOTE_MAX_TX_SIZE                        = 1000000000;
const uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX       = 111; // addresses start with "K"
const uint64_t CRYPTONOTE_TX_PROOF_BASE58_PREFIX             = 3576968; // (0x369488), starts with "Proof..."
const uint64_t CRYPTONOTE_RESERVE_PROOF_BASE58_PREFIX        = 44907175188; // (0xa74ad1d14), starts with "RsrvPrf..."
const uint64_t CRYPTONOTE_KEYS_SIGNATURE_BASE58_PREFIX       = 176103705; // (0xa7f2119), starts with "SigV1..."
const size_t   CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW          = 10;
const size_t   CRYPTONOTE_TX_SPENDABLE_AGE                   = 6;
const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT            = DIFFICULTY_TARGET * 7;
const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V1         = DIFFICULTY_TARGET * 3;
const size_t   BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW             = 60;
const size_t   BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V1          = 11;

// MONEY_SUPPLY - total number coins to be generated
const uint64_t MONEY_SUPPLY                                  = UINT64_C(10000000000000000000);
const uint64_t COIN                                          = UINT64_C(1000000000000);
const uint64_t TAIL_EMISSION_REWARD                          = UINT64_C(1000000000000);
const size_t CRYPTONOTE_COIN_VERSION                         = 1;
const unsigned EMISSION_SPEED_FACTOR                         = 18;
static_assert(EMISSION_SPEED_FACTOR <= 8 * sizeof(uint64_t), "Bad EMISSION_SPEED_FACTOR");

const size_t   CRYPTONOTE_REWARD_BLOCKS_WINDOW               = 100;
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE     = 1000000; //size of block (bytes) after which reward for block calculated using block size
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2  = 1000000;
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1  = 100000;
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_CURRENT = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;
const size_t   CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE        = 600;
const size_t   CRYPTONOTE_DISPLAY_DECIMAL_POINT              = 12;

const uint64_t MINIMUM_FEE_V1                                = UINT64_C(100000000);
const uint64_t MINIMUM_FEE_V2                                = UINT64_C(100000000000);
const uint64_t MINIMUM_FEE                                   = MINIMUM_FEE_V2;
const uint64_t MAXIMUM_FEE                                   = UINT64_C(100000000000);

const uint64_t DEFAULT_DUST_THRESHOLD                        = UINT64_C(100000000);
const uint64_t MIN_TX_MIXIN_SIZE                             = 2;
const uint64_t MAX_TX_MIXIN_SIZE                             = 20;
const uint64_t MAX_EXTRA_SIZE                                = 1024;

const uint64_t MAX_TRANSACTION_SIZE_LIMIT                    = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_CURRENT / 4 - CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE;

const size_t   DANDELION_EPOCH                               = 600;
const size_t   DANDELION_STEMS                               = 2;
const size_t   DANDELION_STEM_EMBARGO                        = 173;
const uint8_t  DANDELION_STEM_TX_PROPAGATION_PROBABILITY     = 90;

const size_t   DIFFICULTY_WINDOW                             = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY; // blocks
const size_t   DIFFICULTY_WINDOW_V2                          = 17;  // blocks
const size_t   DIFFICULTY_WINDOW_V3                          = 60;  // blocks
const size_t   DIFFICULTY_WINDOW_V4                          = 120; // blocks
const size_t   DIFFICULTY_CUT                                = 60;  // timestamps to cut after sorting
const size_t   DIFFICULTY_LAG                                = 15;  // !!!
static_assert(2 * DIFFICULTY_CUT <= DIFFICULTY_WINDOW - 2, "Bad DIFFICULTY_WINDOW or DIFFICULTY_CUT");

const uint64_t POISSON_CHECK_TRIGGER = 10; // Reorg size that triggers poisson timestamp check
const uint64_t POISSON_CHECK_DEPTH = 60;   // Main-chain depth of the poisson check. The attacker will have to tamper 50% of those blocks
const double POISSON_LOG_P_REJECT = -75.0; // Reject reorg if the probablity that the timestamps are genuine is below e^x, -75 = 10^-33

const size_t   MAX_BLOCK_SIZE_INITIAL                        = 1000000;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_NUMERATOR         = 100 * 1024;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_DENOMINATOR       = 365 * 24 * 60 * 60 / DIFFICULTY_TARGET;

const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS     = 1;
const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS    = DIFFICULTY_TARGET * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS;

const uint64_t CRYPTONOTE_MEMPOOL_TX_LIVETIME                = 60 * 60 * 24;     //seconds, one day
const uint64_t CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME = 60 * 60 * 24 * 7; //seconds, one week
const uint64_t CRYPTONOTE_NUMBER_OF_PERIODS_TO_FORGET_TX_DELETED_FROM_POOL = 7;  // CRYPTONOTE_NUMBER_OF_PERIODS_TO_FORGET_TX_DELETED_FROM_POOL * CRYPTONOTE_MEMPOOL_TX_LIVETIME = time to forget tx

const size_t   FUSION_TX_MAX_SIZE                            = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1 * 30 / 100;
const size_t   FUSION_TX_MIN_INPUT_COUNT                     = 12;
const size_t   FUSION_TX_MIN_IN_OUT_COUNT_RATIO              = 4;

const uint32_t UPGRADE_HEIGHT_V2                             = 60000;  // Block v2, pre-LWMA
const uint32_t UPGRADE_HEIGHT_V3                             = 216000; // Block v3, LWMA1
const uint32_t UPGRADE_HEIGHT_V3_1                           = 216394; // Min fee v2, cap max mixin
const uint32_t UPGRADE_HEIGHT_V4                             = 266000; // Block v4, LWMA2, adaptive min fee, min mixin, disable slave merge mining
const uint32_t UPGRADE_HEIGHT_V4_1                           = 300000; // LWMA3
const uint32_t UPGRADE_HEIGHT_V4_2                           = 500000; // Fee per-byte for extra, ban unmixable denominations
const uint32_t UPGRADE_HEIGHT_V5                             = 4294967294; // Block v5, back to LWMA1+

const unsigned UPGRADE_VOTING_THRESHOLD                      = 90; // percent
const uint32_t UPGRADE_VOTING_WINDOW                         = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;  // blocks
const uint32_t UPGRADE_WINDOW                                = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;  // blocks
static_assert(0 < UPGRADE_VOTING_THRESHOLD && UPGRADE_VOTING_THRESHOLD <= 100, "Bad UPGRADE_VOTING_THRESHOLD");
static_assert(UPGRADE_VOTING_WINDOW > 1, "Bad UPGRADE_VOTING_WINDOW");

const char     CRYPTONOTE_BLOCKS_FILENAME[]                  = "blocks.dat";
const char     CRYPTONOTE_BLOCKINDEXES_FILENAME[]            = "blockindexes.dat";
const char     CRYPTONOTE_BLOCKSCACHE_FILENAME[]             = "blockscache.dat";
const char     CRYPTONOTE_POOLDATA_FILENAME[]                = "poolstate.bin";
const char     P2P_NET_DATA_FILENAME[]                       = "p2pstate.bin";
const char     CRYPTONOTE_BLOCKCHAIN_INDICES_FILENAME[]      = "blockchainindices.dat";
const char     MINER_CONFIG_FILE_NAME[]                      = "miner_conf.json";
} // parameters

const char     CRYPTONOTE_NAME[]                             = "karbowanec";
const char     GENESIS_COINBASE_TX_HEX[]                     = "010a01ff0001fac484c69cd608029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd0880712101f904925cc23f86f9f3565188862275dc556a9bdfb6aec22c5aca7f0177c45ba8";
const char     DNS_CHECKPOINTS_HOST[]                        = "checkpoints.karbo.org";

const uint8_t  CURRENT_TRANSACTION_VERSION                   =  1;
const uint8_t  BLOCK_MAJOR_VERSION_1                         =  1;
const uint8_t  BLOCK_MAJOR_VERSION_2                         =  2;
const uint8_t  BLOCK_MAJOR_VERSION_3                         =  3;
const uint8_t  BLOCK_MAJOR_VERSION_4                         =  4;
const uint8_t  BLOCK_MAJOR_VERSION_5                         =  5;
const uint8_t  BLOCK_MINOR_VERSION_0                         =  0;
const uint8_t  BLOCK_MINOR_VERSION_1                         =  1;

const size_t   BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT        =  10000;  //by default, blocks ids count in synchronizing
const size_t   BLOCKS_SYNCHRONIZING_DEFAULT_COUNT            =  128;    //by default, blocks count in blocks downloading
const size_t   COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT         =  1000;

const int      P2P_DEFAULT_PORT                              =  32347;
const int      RPC_DEFAULT_PORT                              =  32348;
const int      RPC_DEFAULT_SSL_PORT                          =  32448;
const int      WALLET_RPC_DEFAULT_PORT                       =  15000;
const int      WALLET_RPC_DEFAULT_SSL_PORT                   =  15100;
const int      GATE_RPC_DEFAULT_PORT                         =  16000;
const int      GATE_RPC_DEFAULT_SSL_PORT                     =  16100;
const char     RPC_DEFAULT_CHAIN_FILE[]                      = "rpc_server.crt";
const char     RPC_DEFAULT_KEY_FILE[]                        = "rpc_server.key";
const char     RPC_DEFAULT_DH_FILE[]                         = "rpc_server.pem";

const size_t   P2P_LOCAL_WHITE_PEERLIST_LIMIT                =  1000;
const size_t   P2P_LOCAL_GRAY_PEERLIST_LIMIT                 =  5000;

// This defines our current P2P network version
// and the minimum version for communication between nodes
const uint8_t  P2P_VERSION_1                                 = 1;
const uint8_t  P2P_VERSION_2                                 = 2;
const uint8_t  P2P_VERSION_3                                 = 3;
const uint8_t  P2P_VERSION_4                                 = 4;
const uint8_t  P2P_CURRENT_VERSION                           = P2P_VERSION_4;
const uint8_t  P2P_MINIMUM_VERSION                           = 1;

// This defines the number of versions ahead we must see peers before
// we start displaying warning messages that we need to upgrade our software
const uint8_t  P2P_UPGRADE_WINDOW                            = 2;

// This defines the minimum P2P version required for lite blocks propogation
const uint8_t  P2P_LITE_BLOCKS_PROPOGATION_VERSION           = 3;

const size_t   P2P_CONNECTION_MAX_WRITE_BUFFER_SIZE          = 64 * 1024 * 1024; // 64 MB
const uint32_t P2P_DEFAULT_CONNECTIONS_COUNT                 = 12;
const size_t   P2P_DEFAULT_ANCHOR_CONNECTIONS_COUNT          = 2;
const size_t   P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT     = 70;
const uint32_t P2P_DEFAULT_HANDSHAKE_INTERVAL                = 60;            // seconds
const uint32_t P2P_DEFAULT_PACKET_MAX_SIZE                   = 50000000;      // 50000000 bytes maximum packet size
const uint32_t P2P_DEFAULT_PEERS_IN_HANDSHAKE                = 250;
const uint32_t P2P_MAX_PEERS_IN_HANDSHAKE                    = 256;
const uint32_t P2P_DEFAULT_CONNECTION_TIMEOUT                = 5000;          // 5 seconds
const uint32_t P2P_DEFAULT_PING_CONNECTION_TIMEOUT           = 2000;          // 2 seconds
const uint64_t P2P_DEFAULT_INVOKE_TIMEOUT                    = 60 * 2 * 1000; // 2 minutes
const size_t   P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT          = 5000;          // 5 seconds
const uint32_t P2P_FAILED_ADDR_FORGET_SECONDS                = (60 * 60);     //1 hour
const uint32_t P2P_IP_BLOCKTIME                              = (60 * 60 * 24);//24 hour
const uint32_t P2P_IP_FAILS_BEFORE_BLOCK                     = 10;
const uint32_t P2P_IDLE_CONNECTION_KILL_INTERVAL             = (5 * 60);      //5 minutes

const char     P2P_STAT_TRUSTED_PUB_KEY[]                    = "8f80f9a5a434a9f1510d13336228debfee9c918ce505efe225d8c94d045fa115";

const char* const SEED_NODES[] = { 
  "seed1.karbowanec.com:32347",
  "seed2.karbowanec.com:32347",
  "seed.karbo.cloud:32347",
  "seed.karbo.org:32347",
  "seed.karbo.io:32347",
  "185.86.78.40:32347",
  "108.61.198.115:32347",
  "45.32.232.11:32347",
  "46.149.182.151:32347",
  "144.91.94.65:32347"
};

} // CryptoNote




