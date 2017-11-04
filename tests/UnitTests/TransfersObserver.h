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

#include <tuple>
#include <vector>
#include "ITransfersSynchronizer.h"

namespace CryptoNote {

class TransfersObserver : public ITransfersObserver {
public:
  virtual void onError(ITransfersSubscription* object, uint32_t height, std::error_code ec) override {
    errors.emplace_back(height, ec);
  }

  virtual void onTransactionUpdated(ITransfersSubscription* object, const Hash& transactionHash) override {
    updated.push_back(transactionHash);
  }

  virtual void onTransactionDeleted(ITransfersSubscription* object, const Hash& transactionHash) override {
    deleted.push_back(transactionHash);
  }

  std::vector<std::tuple<uint64_t, std::error_code>> errors;
  std::vector<Hash> updated;
  std::vector<Hash> deleted;
};


}
