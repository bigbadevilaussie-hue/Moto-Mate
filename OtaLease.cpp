#include "OtaLease.h"
#include <esp_attr.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_ota_ops.h>
#include <esp_private/esp_clk.h>

namespace {
constexpr uint32_t MAGIC=0x4d4d4f54;
constexpr uint64_t LIMIT=180000000ULL;
struct Ticket { uint64_t expiry, inverse; uint32_t target, magic; };
RTC_NOINIT_ATTR Ticket ticket;
}
void otaLeaseArm(uint64_t deadlineUs,uint32_t targetAddress) {
  ticket.magic=0;
  const uint64_t now=esp_timer_get_time();
  if (deadlineUs<=now || deadlineUs-now>LIMIT || !targetAddress) return;
  const uint64_t rtc=esp_clk_rtc_time();
  if (!rtc) return;
  ticket.expiry=rtc+(deadlineUs-now);
  ticket.inverse=~ticket.expiry;
  ticket.target=targetAddress;
  ticket.magic=MAGIC;
}
uint64_t otaLeaseConsume() {
  const Ticket saved=ticket;
  ticket.magic=0; // consume before any radio or settings operations
  if (esp_reset_reason()!=ESP_RST_SW || saved.magic!=MAGIC ||
      saved.inverse!=~saved.expiry) return 0;
  const auto* running=esp_ota_get_running_partition();
  if (!running || running->address!=saved.target) return 0;
  const uint64_t rtc=esp_clk_rtc_time();
  if (!rtc || saved.expiry<=rtc || saved.expiry-rtc>LIMIT) return 0;
  // Conservative margin for RTC calibration and conversion uncertainty.
  const uint64_t remaining=saved.expiry-rtc;
  if (remaining<=2000000ULL) return 0;
  return uint64_t(esp_timer_get_time())+remaining-2000000ULL;
}
