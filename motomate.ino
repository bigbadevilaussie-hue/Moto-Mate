#include "Config.h"
#include "BenchCommission.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#if __has_include("Ota.local.h")
#include "Ota.local.h"
#endif
#ifndef MOTO_OTA_KEY
#error Run tools/setup_ota.py before building V1.10 OTA bench
#endif

namespace {
WebServer server(80);
enum class Mode { Connecting, Ready, Retry };
Mode mode = Mode::Connecting;
uint64_t retryAt = 0, rebootAt = 0;
uint32_t bootId = 0;
bool receiving = false, complete = false, sawFile = false;
size_t expected = 0, written = 0;
String failure;
const char* HEADERS[] = {"X-Moto-Key", "X-Moto-Size", "X-Moto-MD5"};

bool authorised() {
  const String value = server.header("X-Moto-Key");
  const char* secret = MOTO_OTA_KEY;
  if (value.length() != strlen(secret)) return false;
  unsigned diff = 0;
  for (size_t i=0; i<value.length(); ++i) diff |= uint8_t(value[i]) ^ uint8_t(secret[i]);
  return diff == 0;
}

void abortUpload(const char* reason) {
  if (Update.isRunning()) Update.abort();
  receiving = complete = false;
  failure = reason;
  Serial.printf("[OTA] FAIL %s\n", reason);
}

bool decimalSize(const String& s, size_t& n) {
  if (!s.length() || s.length()>7) return false;
  n = 0;
  for (size_t i=0;i<s.length();++i) {
    if (s[i]<'0'||s[i]>'9') return false;
    n = n*10 + s[i]-'0';
  }
  return n >= 24;
}

bool validMD5(const String& s) {
  if (s.length()!=32) return false;
  for (size_t i=0;i<s.length();++i)
    if (!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f'))) return false;
  return true;
}

void status() {
  const auto* p = esp_ota_get_running_partition();
  const auto* next = esp_ota_get_next_update_partition(nullptr);
  char out[320];
  snprintf(out,sizeof(out),"{\"device\":\"Moto-Mate\",\"version\":\"%s\",\"mode\":\"ota_bench\",\"boot_id\":\"%08lx\",\"slot\":\"%s\",\"slot_capacity\":%lu,\"update_capacity\":%lu,\"ota_busy\":%s}",
      MOTO_MATE_VERSION,(unsigned long)bootId,p?p->label:"unknown",
      p?(unsigned long)p->size:0,next?(unsigned long)next->size:0,
      (receiving||rebootAt)?"true":"false");
  server.send(200,"application/json",out);
}

void upload() {
  HTTPUpload& u = server.upload();
  switch (u.status) {
    case UPLOAD_FILE_START: {
      if (sawFile) { abortUpload("multiple files"); return; }
      sawFile = true;
      failure = "";
      expected = written = 0;
      complete = receiving = false;
      if (rebootAt || !authorised()) { abortUpload("unauthorised or reboot pending"); return; }
      const auto* target = esp_ota_get_next_update_partition(nullptr);
      const auto* running = esp_ota_get_running_partition();
      const String md5 = server.header("X-Moto-MD5");
      if (!target || !running || target->address==running->address ||
          !decimalSize(server.header("X-Moto-Size"),expected) ||
          expected>target->size || !validMD5(md5)) {
        abortUpload("invalid size, slot or checksum"); return;
      }
      // Do not start flash writes until first chunk's S3 header is checked.
      receiving = true;
      break;
    }
    case UPLOAD_FILE_WRITE:
      if (!receiving) return;
      if (u.currentSize > expected-written) { abortUpload("length overflow"); return; }
      if (!written) {
        // ESP image header chip ID at bytes 12..13 (ESP32-S3 = 9).
        if (u.currentSize<24 || u.buf[0]!=0xe9 || u.buf[12]!=9 || u.buf[13]!=0) {
          abortUpload("not an S3 application image"); return;
        }
        if (!Update.begin(expected,U_FLASH) || !Update.setMD5(server.header("X-Moto-MD5").c_str())) {
          abortUpload("Update.begin or checksum failed"); return;
        }
        Serial.printf("[OTA] start bytes=%lu\n",(unsigned long)expected);
      }
      if (Update.write(u.buf,u.currentSize)!=u.currentSize) { abortUpload("short flash write"); return; }
      written += u.currentSize;
      break;
    case UPLOAD_FILE_END:
      if (receiving && written==expected && u.totalSize==expected) {
        receiving = false;
        complete = true;
      } else if (receiving) abortUpload("incomplete image");
      break;
    case UPLOAD_FILE_ABORTED:
      abortUpload("client disconnected");
      sawFile = false;
      break;
    default: break;
  }
}

void finishRequest() {
  if (!authorised()) {
    abortUpload("unauthorised");
    server.send(403,"text/plain","unauthorised");
  } else if (!complete || !Update.isRunning()) {
    abortUpload(failure.length()?failure.c_str():"missing or incomplete image");
    server.send(400,"text/plain",failure);
  } else if (!Update.end(false)) {
    abortUpload("image validation or commit failed");
    server.send(500,"text/plain",failure);
  } else {
    complete = false;
    server.send(200,"application/json","{\"accepted\":true,\"reboot_pending\":true}");
    rebootAt = esp_timer_get_time()+500000;
    Serial.println("[OTA] image committed; reboot pending, verify new boot remotely");
  }
  sawFile = false;
}

void startServer() {
  server.collectHeaders(HEADERS,3);
  server.on("/status",HTTP_GET,status);
  server.on("/ota",HTTP_POST,finishRequest,upload);
  server.onNotFound([](){ server.send(404,"text/plain","not found"); });
  server.begin();
  Serial.printf("[OTA] READY http://%s/status /ota (authenticated)\n",WiFi.localIP().toString().c_str());
  mode=Mode::Ready;
}
}

void setup() {
  Serial.begin(115200);
  bootId=esp_random();
  Serial.println("[BOOT] MOTO-MATE V" MOTO_MATE_VERSION " OTA BENCH / stays awake");
  commissionRadioBegin(true);
}

void loop() {
  if (rebootAt && uint64_t(esp_timer_get_time())>=rebootAt) ESP.restart();
  if (mode==Mode::Connecting && commissionRadioPoll()) {
    if (commissionWifiConnected()) {
      // Handlers registered once; begin again only after stop on a lost link.
      static bool configured=false;
      if (!configured) { startServer(); configured=true; }
      else { server.begin(); mode=Mode::Ready; }
    } else {
      retryAt=esp_timer_get_time()+30000000ULL;
      mode=Mode::Retry;
    }
  } else if (mode==Mode::Ready) {
    if (WiFi.status()!=WL_CONNECTED && !rebootAt) {
      if (receiving || complete || Update.isRunning()) abortUpload("Wi-Fi lost");
      sawFile=false;
      server.stop();
      commissionRadioBegin(true);
      mode=Mode::Connecting;
    } else {
      server.handleClient();
      // The synchronous WebServer upload handler has returned. If it never
      // reached the final request handler, discard any partial staged image.
      if (!rebootAt && (receiving || complete || Update.isRunning() || sawFile)) {
        abortUpload("request ended without commit");
        sawFile=false;
      }
    }
  } else if (mode==Mode::Retry && uint64_t(esp_timer_get_time())>=retryAt) {
    commissionRadioBegin(true);
    mode=Mode::Connecting;
  }
  vTaskDelay(1);
}
