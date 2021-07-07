// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2019, The Karbo Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <Common/ConsoleTools.h>
#include <iomanip>
#include <ostream>
#include <string>

class ColouredMsg {
public:
  ColouredMsg(std::string msg, Common::Console::Color colour)
    : msg(msg), colour(colour) {}

  ColouredMsg(std::string msg, int padding,
    Common::Console::Color colour)
    : msg(msg), colour(colour), padding(padding), pad(true) {}


  /* Set the text colour, write the message, then reset. We use a class
     as it seems the only way to have a valid << operator. We need this
     so we can nicely do something like:

     std::cout << "Hello " << GreenMsg("user") << std::endl;

     Without having to write:

     std::cout << "Hello ";
     GreenMsg("user");
     std::cout << std::endl; */

  friend std::ostream& operator<<(std::ostream& os, const ColouredMsg &m) {
    Common::Console::setTextColor(m.colour);

    if (m.pad) {
      os << std::left << std::setw(m.padding) << m.msg;
    } else {
      os << m.msg;
    }

    Common::Console::setTextColor(Common::Console::Color::Default);
    return os;
  }

protected:
  std::string msg;
  const Common::Console::Color colour;
  const int padding = 0;
  const bool pad = false;
};

class SuccessMsg : public ColouredMsg {
public:
  explicit SuccessMsg(std::string msg)
    : ColouredMsg(msg, Common::Console::Color::Green) {}

  explicit SuccessMsg(std::string msg, int padding)
    : ColouredMsg(msg, padding, Common::Console::Color::Green) {}
};

class InformationMsg : public ColouredMsg {
public:
  explicit InformationMsg(std::string msg)
    : ColouredMsg(msg, Common::Console::Color::BrightYellow) {}

  explicit InformationMsg(std::string msg, int padding)
    : ColouredMsg(msg, padding,
      Common::Console::Color::BrightYellow) {}
};

class SuggestionMsg : public ColouredMsg {
public:
  explicit SuggestionMsg(std::string msg)
    : ColouredMsg(msg, Common::Console::Color::BrightBlue) {}

  explicit SuggestionMsg(std::string msg, int padding)
    : ColouredMsg(msg, padding,
      Common::Console::Color::BrightBlue) {}
};

class WarningMsg : public ColouredMsg {
public:
  explicit WarningMsg(std::string msg)
    : ColouredMsg(msg, Common::Console::Color::BrightRed) {}

  explicit WarningMsg(std::string msg, int padding)
    : ColouredMsg(msg, padding,
      Common::Console::Color::BrightRed) {}
};
