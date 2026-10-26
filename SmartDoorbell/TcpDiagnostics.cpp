#include "TcpDiagnostics.h"

extern "C" {
#include <lwip/priv/tcp_priv.h>
}

namespace {

template <typename Pcb>
uint16_t countPcbs(const Pcb* head) {
  uint16_t count = 0;
  for (const Pcb* pcb = head; pcb != nullptr && count < UINT16_MAX;
       pcb = pcb->next) {
    ++count;
  }
  return count;
}

}  // namespace

TcpDiagnosticSnapshot TcpDiagnostics::snapshot() {
  TcpDiagnosticSnapshot result{};
  result.activePcbs = countPcbs(tcp_active_pcbs);
  result.timeWaitPcbs = countPcbs(tcp_tw_pcbs);
  result.boundPcbs = countPcbs(tcp_bound_pcbs);
  result.listenPcbs = countPcbs(tcp_listen_pcbs.listen_pcbs);
  return result;
}
