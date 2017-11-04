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

#include "UpgradeManager.h"

#include "CryptoNoteConfig.h"

namespace CryptoNote {

UpgradeManager::UpgradeManager() {
}

UpgradeManager::~UpgradeManager() {
}

void UpgradeManager::addMajorBlockVersion(uint8_t targetVersion, uint32_t upgradeHeight) {
  assert(m_upgradeDetectors.empty() || m_upgradeDetectors.back()->targetVersion() < targetVersion);
  m_upgradeDetectors.emplace_back(makeUpgradeDetector(targetVersion, upgradeHeight));
}

uint8_t UpgradeManager::getBlockMajorVersion(uint32_t blockIndex) const {
  for (auto it = m_upgradeDetectors.rbegin(); it != m_upgradeDetectors.rend(); ++it) {
    if (it->get()->upgradeIndex() < blockIndex) {
      return it->get()->targetVersion();
    }
  }

  return BLOCK_MAJOR_VERSION_1;
}

}
