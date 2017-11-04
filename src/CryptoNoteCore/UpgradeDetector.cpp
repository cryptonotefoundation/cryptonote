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

#include "UpgradeDetector.h"
#include "IUpgradeDetector.h"

namespace CryptoNote {

class SimpleUpgradeDetector : public IUpgradeDetector {
public:
  SimpleUpgradeDetector(uint8_t targetVersion, uint32_t upgradeIndex) :
    m_targetVersion(targetVersion), m_upgradeIndex(upgradeIndex) {
  }
 
  uint8_t targetVersion() const override {
    return m_targetVersion;
  }
  
  uint32_t upgradeIndex() const override {
    return m_upgradeIndex;
  }

  ~SimpleUpgradeDetector() override {
  }

private:
  uint8_t m_targetVersion;
  uint32_t m_upgradeIndex;
};

std::unique_ptr<IUpgradeDetector> makeUpgradeDetector(uint8_t targetVersion, uint32_t upgradeIndex) {
  return std::unique_ptr<SimpleUpgradeDetector>(new SimpleUpgradeDetector(targetVersion, upgradeIndex));
}

}
