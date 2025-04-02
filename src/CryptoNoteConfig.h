#pragma once

#include <cstdint>
#include <string>

namespace CryptoNote {
  const char CRYPTONOTE_NAME[] = "ThunderHub";
  const uint64_t MONEY_SUPPLY = UINT64_C(50000000000000000000); // 50 miliardi di THUB
  const unsigned EMISSION_SPEED_FACTOR = 20;
  const uint64_t GENESIS_BLOCK_REWARD = UINT64_C(5000000000); // 50 THUB preminati
  const uint64_t DIFFICULTY_TARGET = 90; // 90 secondi per blocco
  const uint8_t CRYPTONOTE_DISPLAY_DECIMAL_POINT = 8;
  const uint64_t MINIMUM_FEE = UINT64_C(10000); // 0.0001 THUB
  const uint64_t DEFAULT_DUST_THRESHOLD = UINT64_C(10000);
  const uint64_t CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW = 10;
  const uint64_t CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE = 100000;

  const int P2P_DEFAULT_PORT = 13000;
  const int RPC_DEFAULT_PORT = 13001;
  const char GENESIS_COINBASE_TX_HEX[] = "013c01ff0001aabbccddee029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd0880712101e444827ebec7bfe1938c8505128cbcf59343e9651afb0893d3d664f560fd216f";
  const uint8_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 88;
  const char BYTECOIN_NETWORK[] = "thunderhub-net-2025-04-02";

  const char* const SEED_NODES[] = {
    "your-public-ip:13000", // Sostituisci con il tuo IP pubblico più tardi
    nullptr
  };
}
