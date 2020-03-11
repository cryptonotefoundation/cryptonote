// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
// Bitcuty Coin is a creation of Marc-Andre Robert the Hashing-b Financial Society Departement Blockchain

#pragma once

#include <cstdint>
#include <initializer_list>

namespace CryptoNote {
namespace parameters {

const var      BITCUTY                                       = CRYPTONOTE; // Bitcuty the Cryptonote Blockchain !!! ¡¡¡
const size_t   BITCUTY_NUMBER_BLOB_TOTAL                     = Infinity; // Number blob for generations Block !!! ¡¡¡
const size_t   BITCUTY_BLOB_HEIGHT                           = 0; // Auto switch for "1" to event creation first Block to the next blob switch a "2", ,,, "!!! ¡¡¡
const size_t   BITCUTY_TOTAL_SIZE_PER_BLOB                   = 5242880000000; // Size total Byte('data in binary') per blob !!! ¡¡¡                                      
const size_t   BITCUTY_MAX_BLOCK_NUMBER                      = 156250; // Maximum generation the Blocks per blob !!! ¡¡¡
const size_t   BITCUTY_MAX_BLOCK_BLOB_SIZE                   = 33554432; // Space for one top big Block the 32MB including transaction in each Block ('data in binary')!!! ¡¡¡

const size_t   BITCUTY_MAX_TX_SIZE                           = 33554432; // Appreciate to the top Block 32MB + space transaction ('data in binary')!!! ¡¡¡
//TODO Currency-specific address prefix
const uint64_t BITCUTY_PUBLIC_ADDRESS_BASE58_PREFIX          = 0xd1; // Address start with "b" is prefix the your address !!! ¡¡¡
//TODO Choose maturity period for your currency 
const size_t   BITCUTY_MINED_MONEY_UNLOCK_WINDOW             = 15; // Get the rewards a big big reward !!! ¡¡¡
const uint64_t BITCUTY_BLOCK_FUTURE_TIME_LIMIT               = 225 / 15; // Best Block every 15 sec * 15 equals 225 sec or 3 min 45 sec / 1000 [millisecond] !!! ¡¡¡

const size_t   BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW             = 15; // Temporal mechanization !!! ¡¡¡
  
//TODO Specify total number of available coins
//TODO ((uint64_t)(-1)) equals to 18446744073709551616
//TODO ((uint64_t) is maximum claim value 18446744073709551615
//TODO ((uint64_t)(11615)) equals to 18446744073709540000
//TODO or you can define number explicitly UINT64_C(858986905600000000)
// Bitcuty money supply in coin is 16417602.225601500000 @BCUTY
// Bitcuty genesis block reward in coin is 2029141.848108040000 @BCUTY 
// Bitcuty total value BLOB(Height1) in coin is 18446744.073709540000 @BCUTY
const values blob & TOTAL_EMISSION_BLOCK(MONEY_SUPPLY, GENESIS_BLOCK_REWARD) {
 
const uint64_t MONEY_SUPPLY                                  = uint64_t(11615);
 or
const UINT64_C MONEY_SUPPLY                                  = UINT64_C(18446744073709540000);
 and 
const uint64_t GENESIS_BLOCK_REWARD                          = uint64_t(16417602225601500000);
 or                                                                    
const UINT64_C GENESIS_BLOCK_REWARD                          = UINT64_C(2029141848108040000);
} 

const unsigned EMISSION_SPEED_FACTOR                         = 25; // There is Roadrunner and the master Bitsrunners bd, is cryptonoted !!! ¡¡¡
static_assert(EMISSION_SPEED_FACTOR <= 25 * sizeof(uint64_t), "EMISSION_SPEED_FACTOR"); // Go every now !!! ¡¡¡

//TODO Define number of blocks for block size median calculation
const size_t   BITCUTY_REWARD_BLOCKS_WINDOW                  = 15; // !!!
const size_t   BITCUTY_BLOCK_GRANTED_FULL_REWARD_ZONE        = 888888; // Magic-bit the MB bit, abracadabra byte per bit swap to currencies, is the Cryptonote Blockchain Bitcuty !!! ¡¡¡ 
const size_t   BITCUTY_COINBASE_BLOB_RESERVED_SIZE           = 159688; // Equals block space for transactions ('data-in-binary')!!! ¡¡¡
//TODO Define number of digits
const size_t   BITCUTY_DISPLAY_DECIMAL_POINT                 = 12; // It's BB-BCUTY !!! ¡¡¡
//TODO Define minimum fee for transactions
const uint64_t MINIMUM_FEE                                   = UINT64_C(833333333); // It is 0,00083333% fee per transaction !!! ¡¡¡
const uint64_t DEFAULT_DUST_THRESHOLD                        = MINIMUM_FEE; // It is minimum fee !!! ¡¡¡

//TODO Define preferred block's target time
const uint64_t DIFFICULTY_TARGET                             = 15; // The data is all right !!! ¡¡¡
const uint64_t EXPECTED_NUMBER_OF_BLOCKS_PER_DAY             = 24 * 60 * 60 * 1000 * 1 / DIFFICULTY_TARGET; // "'%H:%M:%S:%MS:*1 / Difficultie sure Targetings', '15: 0\n0 in 15 sec'" !!! ¡¡¡
//TODO There are options to tune CryptoNote's difficulty retargeting function.
//TODO We recommend not to change it.
const size_t   DIFFICULTY_WINDOW                             = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY; // Bitcuty is Bitcuty or BCUTY !!! ¡¡¡
const size_t   DIFFICULTY_CUT                                = 5759998; // ^%_(-)_(*) minings all`in !!! ¡¡¡
const size_t   DIFFICULTY_LAG                                = 0; // selfajustments to bests performance !!! ¡¡¡
static_assert(2 * DIFFICULTY_CUT <= DIFFICULTY_WINDOW - 2, "DIFFICULTY_WINDOW or DIFFICULTY_CUT"); // 'equals 0 .end' !!! ¡¡¡

const size_t   MAX_BLOCK_SIZE_INITIAL                        = 1024 * 1024; // 1MB  ('data in binary') !!! ¡¡¡
 ¡¡¡const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_NUMERATOR     = 4096 * 1024; // 4 MB ('data in binary') !!! ¡¡¡
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_DENOMINATOR       = 365 * 24 * 60 * 60 / DIFFICULTY_TARGET; // Works all the time !!! ¡¡¡

const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS     = 1; // !!! ¡¡¡
const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS    = DIFFICULTY_TARGET * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS; // !!! ¡¡¡

const uint64_t CRYPTONOTE_MEMPOOL_TX_LIVETIME                = 60 * 60 * 24; // !!! ¡¡¡    
const uint64_t CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME = 60 * 60 * 24 * 7; // !!! ¡¡¡
const uint64_t CRYPTONOTE_NUMBER_OF_PERIODS_TO_FORGET_TX_DELETED_FROM_POOL = 7; // !!! ¡¡¡
  
const size_t   FUSION_TX_MAX_SIZE                            = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE * 30 / 100; // !!! ¡¡¡
const size_t   FUSION_TX_MIN_INPUT_COUNT                     = 12; // !!! ¡¡¡
const size_t   FUSION_TX_MIN_IN_OUT_COUNT_RATIO              = 4; // !!! ¡¡¡

const char     CRYPTONOTE_BLOCKS_FILENAME[]                  = "blocks.dat"; // !!! ¡¡¡
const char     CRYPTONOTE_BLOCKINDEXES_FILENAME[]            = "blockindexes.dat"; // !!! ¡¡¡
const char     CRYPTONOTE_BLOCKSCACHE_FILENAME[]             = "blockscache.dat"; // !!! ¡¡¡
const char     CRYPTONOTE_POOLDATA_FILENAME[]                = "poolstate.bin"; // !!! ¡¡¡
const char     P2P_NET_DATA_FILENAME[]                       = "p2pstate.bin"; // !!! ¡¡¡
const char     CRYPTONOTE_BLOCKCHAIN_INDICES_FILENAME[]      = "blockchainindices.dat"; // !!! ¡¡¡
const char     MINER_CONFIG_FILE_NAME[]                      = "miner_conf.json"; // Bitcuty is the bests of the best !!! ¡¡¡
} // parameters

//TODO Put here the name of your currency
const char     CRYPTONOTE_NAME[]                             = Bitcuty; // BCUTY !!! ¡¡¡
const char     CRYPTONOTE_SYMBOL[]                           = BCUTY; // Bitcuty !!! ¡¡¡
const char     GENESIS_COINBASE_TX_HEX[]                     = ""; // The best first block to the first blob chain and the only first !!! ¡¡¡ ¡¡¡ ¡¡¡

const uint8_t  TRANSACTION_VERSION_1                         =  1; // !!! ¡¡¡
const uint8_t  TRANSACTION_VERSION_2                         =  2; // !!! ¡¡¡
const uint8_t  CURRENT_TRANSACTION_VERSION                   =  TRANSACTION_VERSION_1; // !!! ¡¡¡
const uint8_t  BLOCK_MAJOR_VERSION_1                         =  1; // !!! ¡¡¡
const uint8_t  BLOCK_MAJOR_VERSION_2                         =  2; // !!! ¡¡¡
const uint8_t  BLOCK_MAJOR_VERSION_3                         =  3; // !!! ¡¡¡
const uint8_t  BLOCK_MINOR_VERSION_0                         =  0; // !!! ¡¡¡
const uint8_t  BLOCK_MINOR_VERSION_1                         =  1; // !!! ¡¡¡

const size_t   BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT        =  10000;  // by default, blocks ids count in synchronizing nows !!! ¡¡¡
const size_t   BLOCKS_SYNCHRONIZING_DEFAULT_COUNT            =  100;    // by default, blocks count in blocks downloading nows !!! ¡¡¡
const size_t   COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT         =  1000;   // !!! ¡¡¡
//TODO This port will be used by the daemon to establish connections with p2p network
const int      P2P_DEFAULT_PORT                              = 3984; // Hashing-b and Cryptonote, it`s solid !!! ¡¡¡
//TODO This port will be used by the daemon to interact with simlewallet
const int      RPC_DEFAULT_PORT                              = 445; // Hashing-b and Cryptonote, are solid !!! ¡¡¡
  
const size_t   P2P_LOCAL_WHITE_PEERLIST_LIMIT                =  1000; // !!! ¡¡¡
const size_t   P2P_LOCAL_GRAY_PEERLIST_LIMIT                 =  5000; // !!! ¡¡¡

const size_t   P2P_CONNECTION_MAX_WRITE_BUFFER_SIZE          = 16 * 1024 * 1024; // 16 MB ('data in binary') !!! ¡¡¡
 ¡¡¡const uint32_t P2P_DEFAULT_CONNECTIONS_COUNT             = 8; // !!!
const size_t   P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT     = 70; // !!! ¡¡¡
const uint32_t P2P_DEFAULT_HANDSHAKE_INTERVAL                = 60;            // seconds ¡¡¡
const uint32_t P2P_DEFAULT_PACKET_MAX_SIZE                   = 50000000;      // 50000000 bytes maximum packet size ¡¡¡
const uint32_t P2P_DEFAULT_PEERS_IN_HANDSHAKE                = 250; // !!! ¡¡¡
const uint32_t P2P_DEFAULT_CONNECTION_TIMEOUT                = 5000;          // 5 seconds ¡¡¡
const uint32_t P2P_DEFAULT_PING_CONNECTION_TIMEOUT           = 2000;          // 2 seconds ¡¡¡
const uint64_t P2P_DEFAULT_INVOKE_TIMEOUT                    = 60 * 2 * 1000; // 2 minutes ¡¡¡
const size_t   P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT          = 5000;          // 5 seconds ¡¡¡
const char     P2P_STAT_TRUSTED_PUB_KEY[]                    = "8f80f9a5a434a9f1510d13336228debfee9c918ce505efe225d8c94d045fa115"; // !!! ¡¡¡

// Add here your network nodes and joins the Bitcuty Cryptonote Team, claiming insider ('tag BCUTY' or 'BCUTY tag'); Bitcuty on global accessibilitys !!! ¡¡¡ ¡¡¡ 
const std::initializer_list<const char*> SEED_NODES = {
// Localhost Bitcuty
"127.0.0.1:3984"
// Bitcuty Chicago
"104.238.162.168:3984",
// Bitcuty Canada 
"155.138.136.237:3984",
// Bitcuty Amsterdam
"78.141.214.179:3984",
// Bitcuty Frankfurt
"95.179.251.125:3984"
// Bitcuty London
"108.61.173.182:3984",
// Bitcuty Paris
"45.77.62.57:3984",
// Bitcuty Sigapore
"207.148.122.199:3984",
// Bitcuty Sydney
"139.180.160.231:3984",
// Bitcuty Tokyo
"45.32.50.38:3984",
};

// Bitcuty UUID daemon localhost "127.0.0.1", Briefly All`in ! ¡¡¡
constexpr UUID_BITCUTY_NETWORK = common :: pfh <UUID> ("21d0cb49-1b49-4756-861e-a4deab7a36ab"); // Everything is for the better every time !¡¡

struct CheckpointData {
  uint32_t height;
  const char* blockId;
};

#ifdef __GNUC__
__attribute__((unused))
#endif

// You may add here other checkpoints using the following format:
// {<block height>, "<block hash>"},
const std::initializer_list<CheckpointData> CHECKPOINTS = {
  //{ 10000, "84b6345731e2702cdaadc6ce5e5238c4ca5ecf48e3447136b2ed829b8a95f3ad" },
};
} // " {/|\*<"[:CryptoNote ,_ Bitcuty, '!&&¡¡¡¡¡¡¡':]">*\|/} " \\;

#define ALLOW_DEBUG_COMMANDS
