#pragma once

#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "ConnectionContext.h"
#include "System/Context.h"
#include "System/Dispatcher.h"
#include "System/Event.h"
#include "Logging/LoggerRef.h"
#include "System/TcpConnection.h"
#include "P2pProtocolTypes.h"

namespace CryptoNote {

  class LevinProtocol;
  class ISerializer;

  struct P2pMessage {
    enum Type {
      COMMAND,
      REPLY,
      NOTIFY
    };

    P2pMessage(Type type, uint32_t command, const BinaryArray& buffer, int32_t returnCode = 0) :
      type(type), command(command), buffer(buffer), returnCode(returnCode) {
    }

    size_t size() const {
      return buffer.size();
    }

    Type type;
    uint32_t command;
    const BinaryArray buffer;
    int32_t returnCode;
  };

  struct P2pConnectionContext : public CryptoNoteConnectionContext {
  public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    System::Context<void>* context = nullptr;
    PeerIdType peerId = 0;
    System::TcpConnection connection;
    std::set<NetworkAddress> sent_addresses;

    P2pConnectionContext(System::Dispatcher& dispatcher, Logging::ILogger& log, System::TcpConnection&& conn) :
      connection(std::move(conn)),
      logger(log, "node_server"),
      queueEvent(dispatcher) {
    }

    bool pushMessage(P2pMessage&& msg);
    std::vector<P2pMessage> popBuffer();
    void interrupt();

    uint64_t writeDuration(TimePoint now) const;

  private:
    Logging::LoggerRef logger;
    TimePoint writeOperationStartTime;
    System::Event queueEvent;
    std::vector<P2pMessage> writeQueue;
    size_t writeQueueSize = 0;
    bool stopped = false;
  };

}  // namespace CryptoNote