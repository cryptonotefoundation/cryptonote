#include "P2pConnectionContext.h"
#include "../CryptoNoteConfig.h"
#include <chrono>

namespace CryptoNote {

  bool P2pConnectionContext::pushMessage(P2pMessage&& msg) {
    writeQueueSize += msg.size();

    if (writeQueueSize > P2P_CONNECTION_MAX_WRITE_BUFFER_SIZE) {
      logger(Logging::DEBUGGING) << *this << "Write queue overflows. Interrupt connection";
      interrupt();
      return false;
    }

    writeQueue.push_back(std::move(msg));
    queueEvent.set();
    return true;
  }

  std::vector<P2pMessage> P2pConnectionContext::popBuffer() {
    writeOperationStartTime = TimePoint();

    while (writeQueue.empty() && !stopped) {
      queueEvent.wait();
    }

    std::vector<P2pMessage> msgs(std::move(writeQueue));
    writeQueue.clear();
    writeQueueSize = 0;
    writeOperationStartTime = Clock::now();
    queueEvent.clear();
    return msgs;
  }

  uint64_t P2pConnectionContext::writeDuration(TimePoint now) const { // in milliseconds
    return writeOperationStartTime == TimePoint() ? 0 : std::chrono::duration_cast<std::chrono::milliseconds>(now - writeOperationStartTime).count();
  }

  void P2pConnectionContext::interrupt() {
    logger(Logging::DEBUGGING) << *this << "Interrupt connection";
    assert(context != nullptr);
    stopped = true;
    queueEvent.set();
    context->interrupt();
  }

}  // namespace CryptoNote