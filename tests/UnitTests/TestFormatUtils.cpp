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

#include "gtest/gtest.h"

#include <vector>

#include "Common/Util.h"
#include "CryptoNoteConfig.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/TransactionExtra.h"
#include "Common/StringTools.h"

#include <Logging/LoggerGroup.h>

#define AUTO_VAL_INIT(n) boost::value_initialized<decltype(n)>()

TEST(parseTransactionExtra, handles_empty_extra)
{
  std::vector<uint8_t> extra;;
  std::vector<CryptoNote::TransactionExtraField> tx_extra_fields;
  ASSERT_TRUE(CryptoNote::parseTransactionExtra(extra, tx_extra_fields));
  ASSERT_TRUE(tx_extra_fields.empty());
}

TEST(parseTransactionExtra, handles_padding_only_size_1)
{
  const uint8_t extra_arr[] = {0};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<CryptoNote::TransactionExtraField> tx_extra_fields;
  ASSERT_TRUE(CryptoNote::parseTransactionExtra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(CryptoNote::TransactionExtraPadding), tx_extra_fields[0].type());
  ASSERT_EQ(1, boost::get<CryptoNote::TransactionExtraPadding>(tx_extra_fields[0]).size);
}

TEST(parseTransactionExtra, handles_padding_only_size_2)
{
  const uint8_t extra_arr[] = {0, 0};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<CryptoNote::TransactionExtraField> tx_extra_fields;
  ASSERT_TRUE(CryptoNote::parseTransactionExtra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(CryptoNote::TransactionExtraPadding), tx_extra_fields[0].type());
  ASSERT_EQ(2, boost::get<CryptoNote::TransactionExtraPadding>(tx_extra_fields[0]).size);
}

TEST(parseTransactionExtra, handles_padding_only_max_size)
{
  std::vector<uint8_t> extra(TX_EXTRA_NONCE_MAX_COUNT, 0);
  std::vector<CryptoNote::TransactionExtraField> tx_extra_fields;
  ASSERT_TRUE(CryptoNote::parseTransactionExtra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(CryptoNote::TransactionExtraPadding), tx_extra_fields[0].type());
  ASSERT_EQ(TX_EXTRA_NONCE_MAX_COUNT, boost::get<CryptoNote::TransactionExtraPadding>(tx_extra_fields[0]).size);
}

TEST(parseTransactionExtra, handles_padding_only_exceed_max_size)
{
  std::vector<uint8_t> extra(TX_EXTRA_NONCE_MAX_COUNT + 1, 0);
  std::vector<CryptoNote::TransactionExtraField> tx_extra_fields;
  ASSERT_FALSE(CryptoNote::parseTransactionExtra(extra, tx_extra_fields));
}

TEST(parseTransactionExtra, handles_invalid_padding_only)
{
  std::vector<uint8_t> extra(2, 0);
  extra[1] = 42;
  std::vector<CryptoNote::TransactionExtraField> tx_extra_fields;
  ASSERT_FALSE(CryptoNote::parseTransactionExtra(extra, tx_extra_fields));
}

TEST(parseTransactionExtra, handles_pub_key_only)
{
  const uint8_t extra_arr[] = {1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
    80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<CryptoNote::TransactionExtraField> tx_extra_fields;
  ASSERT_TRUE(CryptoNote::parseTransactionExtra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(CryptoNote::TransactionExtraPublicKey), tx_extra_fields[0].type());
}

TEST(parseTransactionExtra, handles_extra_nonce_only)
{
  const uint8_t extra_arr[] = {2, 1, 42};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<CryptoNote::TransactionExtraField> tx_extra_fields;
  ASSERT_TRUE(CryptoNote::parseTransactionExtra(extra, tx_extra_fields));
  ASSERT_EQ(1, tx_extra_fields.size());
  ASSERT_EQ(typeid(CryptoNote::TransactionExtraNonce), tx_extra_fields[0].type());
  CryptoNote::TransactionExtraNonce extra_nonce = boost::get<CryptoNote::TransactionExtraNonce>(tx_extra_fields[0]);
  ASSERT_EQ(1, extra_nonce.nonce.size());
  ASSERT_EQ(42, extra_nonce.nonce[0]);
}

TEST(parseTransactionExtra, handles_pub_key_and_padding)
{
  const uint8_t extra_arr[] = {1, 30, 208, 98, 162, 133, 64, 85, 83, 112, 91, 188, 89, 211, 24, 131, 39, 154, 22, 228,
    80, 63, 198, 141, 173, 111, 244, 183, 4, 149, 186, 140, 230, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  std::vector<uint8_t> extra(&extra_arr[0], &extra_arr[0] + sizeof(extra_arr));
  std::vector<CryptoNote::TransactionExtraField> tx_extra_fields;
  ASSERT_TRUE(CryptoNote::parseTransactionExtra(extra, tx_extra_fields));
  ASSERT_EQ(2, tx_extra_fields.size());
  ASSERT_EQ(typeid(CryptoNote::TransactionExtraPublicKey), tx_extra_fields[0].type());
  ASSERT_EQ(typeid(CryptoNote::TransactionExtraPadding), tx_extra_fields[1].type());
}

TEST(parse_and_validate_tx_extra, is_valid_tx_extra_parsed)
{
  Logging::LoggerGroup logger;
  CryptoNote::Currency currency = CryptoNote::CurrencyBuilder(logger).currency();
  CryptoNote::Transaction tx = AUTO_VAL_INIT(tx);
  CryptoNote::AccountBase acc;
  acc.generate();
  CryptoNote::BinaryArray b = Common::asBinaryArray("dsdsdfsdfsf");
  ASSERT_TRUE(currency.constructMinerTx(CryptoNote::BLOCK_MAJOR_VERSION_1, 0, 0, 10000000000000, 1000, currency.minimumFee(), acc.getAccountKeys().address, tx, b, 1));
  Crypto::PublicKey tx_pub_key = CryptoNote::getTransactionPublicKeyFromExtra(tx.extra);
  ASSERT_NE(tx_pub_key, CryptoNote::NULL_PUBLIC_KEY);
}
TEST(parse_and_validate_tx_extra, fails_on_big_extra_nonce)
{
  Logging::LoggerGroup logger;
  CryptoNote::Currency currency = CryptoNote::CurrencyBuilder(logger).currency();
  CryptoNote::Transaction tx = AUTO_VAL_INIT(tx);
  CryptoNote::AccountBase acc;
  acc.generate();
  CryptoNote::BinaryArray b(TX_EXTRA_NONCE_MAX_COUNT + 1, 0);
  ASSERT_FALSE(currency.constructMinerTx(CryptoNote::BLOCK_MAJOR_VERSION_1, 0, 0, 10000000000000, 1000, currency.minimumFee(), acc.getAccountKeys().address, tx, b, 1));
}
TEST(parse_and_validate_tx_extra, fails_on_wrong_size_in_extra_nonce)
{
  CryptoNote::Transaction tx = AUTO_VAL_INIT(tx);
  tx.extra.resize(20, 0);
  tx.extra[0] = TX_EXTRA_NONCE;
  tx.extra[1] = 255;
  std::vector<CryptoNote::TransactionExtraField> tx_extra_fields;
  ASSERT_FALSE(CryptoNote::parseTransactionExtra(tx.extra, tx_extra_fields));
}
TEST(validate_parse_amount_case, validate_parse_amount)
{
  Logging::LoggerGroup logger;
  CryptoNote::Currency currency = CryptoNote::CurrencyBuilder(logger).numberOfDecimalPlaces(8).currency();
  uint64_t res = 0;
  bool r = currency.parseAmount("0.0001", res);
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 10000);

  r = currency.parseAmount("100.0001", res);
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 10000010000);

  r = currency.parseAmount("000.0000", res);
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 0);

  r = currency.parseAmount("0", res);
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 0);


  r = currency.parseAmount("   100.0001    ", res);
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 10000010000);

  r = currency.parseAmount("   100.0000    ", res);
  ASSERT_TRUE(r);
  ASSERT_EQ(res, 10000000000);

  r = currency.parseAmount("   100. 0000    ", res);
  ASSERT_FALSE(r);

  r = currency.parseAmount("100. 0000", res);
  ASSERT_FALSE(r);

  r = currency.parseAmount("100 . 0000", res);
  ASSERT_FALSE(r);

  r = currency.parseAmount("100.00 00", res);
  ASSERT_FALSE(r);

  r = currency.parseAmount("1 00.00 00", res);
  ASSERT_FALSE(r);
}
