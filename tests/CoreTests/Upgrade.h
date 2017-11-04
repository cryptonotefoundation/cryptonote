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

#pragma once 
#include "Chaingen.h"

struct gen_upgrade : public test_chain_unit_base
{
  gen_upgrade();

  bool generate(std::vector<test_event_entry>& events) const;

  bool check_block_verification_context(std::error_code bve, size_t eventIdx, const CryptoNote::BlockTemplate& blk);

  bool markInvalidBlock(CryptoNote::Core& c, size_t evIndex, const std::vector<test_event_entry>& events);
  bool checkBlockTemplateVersionIsV1(CryptoNote::Core& c, size_t evIndex, const std::vector<test_event_entry>& events);
  bool checkBlockTemplateVersionIsV2(CryptoNote::Core& c, size_t evIndex, const std::vector<test_event_entry>& events);
  bool checkBlockRewardEqFee(CryptoNote::Core& c, size_t evIndex, const std::vector<test_event_entry>& events);
  bool checkBlockRewardIsZero(CryptoNote::Core& c, size_t evIndex, const std::vector<test_event_entry>& events);
  bool rememberCoinsInCirculationBeforeUpgrade(CryptoNote::Core& c, size_t evIndex, const std::vector<test_event_entry>& events);
  bool rememberCoinsInCirculationAfterUpgrade(CryptoNote::Core& c, size_t evIndex, const std::vector<test_event_entry>& events);

private:
  bool checkBeforeUpgrade(std::vector<test_event_entry>& events, test_generator& generator,
                          const CryptoNote::BlockTemplate& parentBlock, const CryptoNote::AccountBase& minerAcc, bool checkReward) const;
  bool checkAfterUpgrade(std::vector<test_event_entry>& events, test_generator& generator,
                         const CryptoNote::BlockTemplate& parentBlock, const CryptoNote::AccountBase& minerAcc) const;
  bool checkBlockTemplateVersion(CryptoNote::Core& c, uint8_t expectedMajorVersion, uint8_t expectedMinorVersion);

private:
  size_t m_invalidBlockIndex;
  size_t m_checkBlockTemplateVersionCallCounter;
  uint64_t m_coinsInCirculationBeforeUpgrade;
  uint64_t m_coinsInCirculationAfterUpgrade;
};
