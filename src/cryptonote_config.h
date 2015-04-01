// Copyright (c) 2011-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#define CRYPTONOTE_MAX_BLOCK_NUMBER                     500000000
#define CRYPTONOTE_MAX_BLOCK_SIZE                       500000000  // block header blob limit, never used!
#define CRYPTONOTE_MAX_TX_SIZE                          1000000000
#define CRYPTONOTE_PUBLIC_ADDRESS_TEXTBLOB_VER          0
//TODO Currency-specific address prefix
#define CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX        
//TODO Choose maturity period for your currency
#define CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW            
#define CURRENT_TRANSACTION_VERSION                     1
#define CURRENT_BLOCK_MAJOR_VERSION                     1
#define CURRENT_BLOCK_MINOR_VERSION                     0
#define CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT              60*60*2

#define BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW               60

//TODO Specify total number of available coins
//TODO ((uint64_t)(-1)) equals to 18446744073709551616 coins
//TODO or you can define number explicitly UINT64_C(858986905600000000)
#define MONEY_SUPPLY                                   
#define EMISSION_SPEED_FACTOR                           (18)

//TODO Define number of blocks for block size median calculation
#define CRYPTONOTE_REWARD_BLOCKS_WINDOW                 100
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE       10000 //size of block (bytes) after which reward for block calculated using block size
#define CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE          600
//TODO Define number of digits
#define CRYPTONOTE_DISPLAY_DECIMAL_POINT                
//TODO Define number of smallest units in one coin
#define COIN                                            
//TODO Define default fee for transactions
#define DEFAULT_FEE                                     

//TODO Define preferred block's target time
#define DIFFICULTY_TARGET                               120 // seconds
//TODO There are options to tune CryptoNote's difficulty retargeting function. 
//TODO We recommend not to change it.
#define DIFFICULTY_WINDOW                               720 // blocks
#define DIFFICULTY_LAG                                  15
#define DIFFICULTY_CUT                                  60  // timestamps to cut after sorting
#define DIFFICULTY_BLOCKS_COUNT                         DIFFICULTY_WINDOW + DIFFICULTY_LAG


#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS      DIFFICULTY_TARGET * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS
#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS       1


#define DIFFICULTY_BLOCKS_ESTIMATE_TIMESPAN             DIFFICULTY_TARGET


#define BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT          10000  //by default, blocks ids count in synchronizing
#define BLOCKS_SYNCHRONIZING_DEFAULT_COUNT              200    //by default, blocks count in blocks downloading
#define CRYPTONOTE_PROTOCOL_HOP_RELAX_COUNT             3      //value of hop, after which we use only announce of new block

//TODO This port will be used by the daemon to establish connections with p2p network
#define P2P_DEFAULT_PORT                                
//TODO This port will be used by the daemon to interact with simlewallet
#define RPC_DEFAULT_PORT                                
#define COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT           1000

#define P2P_LOCAL_WHITE_PEERLIST_LIMIT                  1000
#define P2P_LOCAL_GRAY_PEERLIST_LIMIT                   5000

#define P2P_DEFAULT_CONNECTIONS_COUNT                   8
#define P2P_DEFAULT_HANDSHAKE_INTERVAL                  60           //secondes
#define P2P_DEFAULT_PACKET_MAX_SIZE                     50000000     //50000000 bytes maximum packet size
#define P2P_DEFAULT_PEERS_IN_HANDSHAKE                  250
#define P2P_DEFAULT_CONNECTION_TIMEOUT                  5000       //5 seconds
#define P2P_DEFAULT_PING_CONNECTION_TIMEOUT             2000       //2 seconds
#define P2P_DEFAULT_INVOKE_TIMEOUT                      60*2*1000  //2 minutes
#define P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT            5000       //5 seconds
#define P2P_STAT_TRUSTED_PUB_KEY                        "8f80f9a5a434a9f1510d13336228debfee9c918ce505efe225d8c94d045fa115"
#define P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT       70

#define ALLOW_DEBUG_COMMANDS

//TODO Put here the name of your currency
#define CRYPTONOTE_NAME                         "worldnote"
#define CRYPTONOTE_BLOCKS_FILENAME              "blocks.dat"
#define CRYPTONOTE_BLOCKINDEXES_FILENAME        "blockindexes.dat"
#define CRYPTONOTE_BLOCKSCACHE_FILENAME         "blockscache.dat"
#define CRYPTONOTE_POOLDATA_FILENAME            "poolstate.bin"
#define P2P_NET_DATA_FILENAME                   "p2pstate.bin"
#define MINER_CONFIG_FILE_NAME                  "miner_conf.json"

#define THREAD_STACK_SIZE                       5 * 1024 * 1024

#define GENESIS_COINBASE_TX_HEX                 ""

