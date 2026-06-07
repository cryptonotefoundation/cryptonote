// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2016-2019, The Karbo developers
//
// Please see the included LICENSE file for more information.


//////////////////////////////////
#include "Common/FormatTools.h"
//////////////////////////////////

#include <cstdio>
#include <ctime>
#include <boost/algorithm/string/trim.hpp>
#include <boost/math/special_functions/round.hpp>

#include "../CryptoNoteConfig.h"
#include "Common/StringTools.h"
#include "CryptoNoteCore/Core.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"

namespace {
  const size_t numberOfDecimalPlaces = CryptoNote::parameters::CRYPTONOTE_DISPLAY_DECIMAL_POINT;
}

namespace Common {
  namespace Format {

    std::string get_mining_speed(const uint64_t hashrate)
    {
      std::stringstream stream;

      stream << std::setprecision(2) << std::fixed;

      if (hashrate > 1e9)
      {
        stream << hashrate / 1e9 << " GH/s";
      }
      else if (hashrate > 1e6)
      {
        stream << hashrate / 1e6 << " MH/s";
      }
      else if (hashrate > 1e3)
      {
        stream << hashrate / 1e3 << " KH/s";
      }
      else
      {
        stream << hashrate << " H/s";
      }

      return stream.str();
    }

    std::string get_sync_percentage(
      uint64_t height,
      const uint64_t target_height)
    {
      /* Don't divide by zero */
      if (height == 0 || target_height == 0)
      {
        return "0.00";
      }

      /* So we don't have > 100% */
      if (height > target_height)
      {
        height = target_height;
      }

      float percent = 100.0f * height / target_height;

      if (height < target_height && percent > 99.99f)
      {
        percent = 99.99f; // to avoid 100% when not fully synced
      }

      std::stringstream stream;

      stream << std::setprecision(2) << std::fixed << percent;

      return stream.str();
    }

    std::string prettyPrintBytes(uint64_t input)
    {
      /* Store as a double so we can have 12.34 kb for example */
      double numBytes = static_cast<double>(input);

      std::vector<std::string> suffixes = { "B", "KB", "MB", "GB", "TB" };

      uint64_t selectedSuffix = 0;

      while (numBytes >= 1024 && selectedSuffix < suffixes.size() - 1)
      {
        selectedSuffix++;

        numBytes /= 1024;
      }

      std::stringstream msg;

      msg << std::fixed << std::setprecision(2) << numBytes << " "
        << suffixes[selectedSuffix];

      return msg.str();
    }

    std::string unixTimeToDate(const uint64_t timestamp)
    {
      const std::time_t time = timestamp;
      char buffer[100];
      std::strftime(buffer, sizeof(buffer), "%F %R", std::localtime(&time));
      return std::string(buffer);
    }

    std::string formatAmount(uint64_t amount) {
      std::string s = std::to_string(amount);
      if (s.size() < numberOfDecimalPlaces + 1) {
        s.insert(0, numberOfDecimalPlaces + 1 - s.size(), '0');
      }
      s.insert(s.size() - numberOfDecimalPlaces, ".");
      return s;
    }

    std::string formatAmount(int64_t amount) {
      std::string s = formatAmount(static_cast<uint64_t>(std::abs(amount)));

      if (amount < 0) {
        s.insert(0, "-");
      }

      return s;
    }

    bool parseAmount(const std::string& str, uint64_t& amount) {
      std::string strAmount = str;
      boost::algorithm::trim(strAmount);

      size_t pointIndex = strAmount.find_first_of('.');
      size_t fractionSize;
      if (std::string::npos != pointIndex) {
        fractionSize = strAmount.size() - pointIndex - 1;
        while (numberOfDecimalPlaces < fractionSize && '0' == strAmount.back()) {
          strAmount.erase(strAmount.size() - 1, 1);
          --fractionSize;
        }
        if (numberOfDecimalPlaces < fractionSize) {
          return false;
        }
        strAmount.erase(pointIndex, 1);
      }
      else {
        fractionSize = 0;
      }

      if (strAmount.empty()) {
        return false;
      }

      if (!std::all_of(strAmount.begin(), strAmount.end(), ::isdigit)) {
        return false;
      }

      if (fractionSize < numberOfDecimalPlaces) {
        strAmount.append(numberOfDecimalPlaces - fractionSize, '0');
      }

      return Common::fromString(strAmount, amount);
    }

  } // namespace Format
} // namespace Common
