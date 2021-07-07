// Copyright (c) 2014-2017, The Monero Project
// Copyright (c) 2017-2018, Karbo developers
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/*!
 * \file electrum-words.h
 *
 * \brief Mnemonic seed generation and wallet restoration from them.
 *
 * This file and its cpp file are for translating Electrum-style word lists
 * into their equivalent byte representations for cross-compatibility with
 * that method of "backing up" one's wallet keys.
 */

#pragma once

#include <string>
#include <cstdint>
#include <map>
#include <boost/algorithm/string.hpp>
#include "crypto/crypto.h"  // for declaration of Crypto::SecretKey
#include "language_base.h"
#include "Common/ConsoleTools.h"

namespace Crypto {
namespace ElectrumWords
{

const int seed_length = 24;
const std::string old_language_name = "English";
/*!
	* \brief Converts seed words to bytes (secret key).
	* \param  words           String containing the words separated by spaces.
	* \param  dst             To put the secret key restored from the words.
	* \param  language_name   Language of the seed as found gets written here.
	* \return                 false if not a multiple of 3 words, or if word is not in the words list
	*/
bool words_to_bytes(std::string words, Crypto::SecretKey& dst,
	std::string &language_name);

/*!
	* \brief Converts bytes (secret key) to seed words.
	* \param  src           Secret key
	* \param  words         Space delimited concatenated words get written here.
	* \param  language_name Seed language name
	* \return               true if successful false if not. Unsuccessful if wrong key size.
	*/
bool bytes_to_words(const Crypto::SecretKey& src, std::string& words,
	const std::string &language_name);

/*!
	* \brief Gets a list of seed languages that are supported.
	* \param languages A vector is set to the list of languages.
	*/
void get_language_list(std::vector<std::string> &languages);

/*!
	* \brief Tells if the seed passed is an old style seed or not.
	* \param  seed The seed to check (a space delimited concatenated word list)
	* \return      true if the seed passed is a old style seed false if not.
	*/
bool get_is_old_style_seed(std::string seed);

    /* Templates have to be implemented in the header to be accessible
       elsewhere */

/*!
     * \brief Logs words not present in the english word list.
     * \param words   The words to check if they are present in the dictionary
     * \param stream  A type implementing << to have output written to
     */
    template <typename T>
    void log_incorrect_words(std::vector<std::string> words, T &stream)
    {
      //Language::Base *language = Language::Singleton<Language::English>::instance();
	  Language::Base *language = NULL;
	  
      const std::vector<std::string> &dictionary = language->get_word_list();

      Common::Console::setTextColor(Common::Console::Color::BrightRed);

      for (auto i : words)
      {
        if (std::find(dictionary.begin(), dictionary.end(), i) == dictionary.end())
        {
          stream << i << " is not in the english word list!" << std::endl;
        }
      }

      Common::Console::setTextColor(Common::Console::Color::Default);
    }

	template <typename T>
    bool is_valid_mnemonic(std::string mnemonic_phrase,
                           Crypto::SecretKey &private_spend_key,
                           T &stream)
	{
      /* Uncommenting these will allow importing of different languages, exporting
         in different languages however has not been added, as it will require
         changing the export_keys command to take an argument to specify what
         language the seed should be exported in. For now, multilanguage support
         has been disabled as there are a couple of issues - we can't print out
         what words aren't present in the dictionary if we don't know what
         dictionary they are using, and it's a lot more friendly to work that
         out automatically rather than asking, and secondly, it is possible that
         dictionaries of other words can overlap enough to allow an esperanto
         seed for example to be imported as an english seed */

      /*
      static std::string languages[] = {"English", "Nederlands", "Français",
                                        "Português", "Italiano", "Deutsch",
                                        "русский язык", "简体中文 (中国)",
                                        "Esperanto", "Lojban"};

      static const int num_of_languages = 10;
      */

      static std::string languages[] = {"English"};

      static const int num_of_languages = 1;

      static const int mnemonic_phrase_length = 25;

      std::vector<std::string> words;

      words = boost::split(words, mnemonic_phrase, ::isspace);

      if (words.size() != mnemonic_phrase_length)
      {
        Common::Console::setTextColor(Common::Console::Color::BrightRed);
        stream << "Invalid mnemonic phrase! Seed phrase is not 25 words! "
               << "Please try again." << std::endl;

        //log_incorrect_words(words, stream);

        Common::Console::setTextColor(Common::Console::Color::Default);

        return false;
      }

      /* Check every language for our phrase so the user doesn't have to specify
         it, this shouldn't be an issue as long as one language doesn't have enough
         of another languages words, might need some testing */
      for (int i = 0; i < num_of_languages; i++)
      {
        if (words_to_bytes(mnemonic_phrase, private_spend_key, languages[i]))
        {
          return true;
        }
      }

      /* The issue with this is if we try and automagically determine what language
         the seed phrase is in, then we can't log words which aren't in the x
         dictionary, we will have to take an argument to know what language they
         are in, but this is less user friendly. */
      Common::Console::setTextColor(Common::Console::Color::BrightRed);

      stream << "Invalid mnemonic phrase!" << std::endl;
      
      Common::Console::setTextColor(Common::Console::Color::Default);

      log_incorrect_words(words, stream);

      return false;
    }

} //ElectrumWords
} //Crypto

