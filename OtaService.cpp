#include "Config.h"
#include "OtaService.h"
#include "Hardware.h"
#include "Settings.h"
#include "MotoClock.h"
#include "ClockPolicy.h"
#include "BenchSensors.h"
#include "Storage.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_timer.h>

#if __has_include("Ota.local.h")
#include "Ota.local.h"
#endif
#ifndef MOTO_OTA_KEY
#error Run tools/setup_ota.py before building V1.10 OTA bench
#endif

namespace {
WebServer server(80);
uint64_t rebootAt = 0;
uint64_t leaseEnd = 0;
bool leaseOpen(uint64_t margin=0) {
  const uint64_t now=esp_timer_get_time();
  return now<leaseEnd && leaseEnd-now>margin;
}
uint32_t bootId = 0;
bool receiving = false, complete = false, sawFile = false;
size_t expected = 0, written = 0;
String failure;
const char* HEADERS[] = {"X-Moto-Key", "X-Moto-Size", "X-Moto-MD5"};

bool authorised() {
  if (!leaseOpen()) return false;
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
  if (!leaseOpen()) { server.send(403,"text/plain","maintenance expired"); return; }
  const auto* p = esp_ota_get_running_partition();
  const auto* next = esp_ota_get_next_update_partition(nullptr);
  char out[320];
  snprintf(out,sizeof(out),"{\"device\":\"Moto-Mate\",\"version\":\"%s\",\"mode\":\"ota_bench\",\"boot_id\":\"%08lx\",\"slot\":\"%s\",\"slot_capacity\":%lu,\"update_capacity\":%lu,\"ota_busy\":%s}",
      MOTO_MATE_VERSION,(unsigned long)bootId,p?p->label:"unknown",
      p?(unsigned long)p->size:0,next?(unsigned long)next->size:0,
      (receiving||rebootAt)?"true":"false");
  String body(out);
  body.remove(body.length()-1);
  body += hardwareStatus();
  body += ",\"settings\":";
  body += settingsJson();
  body += ",\"clock\":";
  body += clockJson();
  body += ",\"sensors\":";
  body += sensorsJson();
  body += ",\"storage\":";
  body += storageJson();
  body += ",\"resources\":";
  body += resourcesJson();
  body += "}";
  server.send(200,"application/json",body);
}

void upload() {
  HTTPUpload& u = server.upload();
  if (!leaseOpen(2000000ULL)) { abortUpload("maintenance deadline"); return; }
  switch (u.status) {
    case UPLOAD_FILE_START: {
      if (sawFile) { abortUpload("multiple files"); return; }
      sawFile = true;
      failure = "";
      expected = written = 0;
      complete = receiving = false;
      if (rebootAt || !authorised() || !leaseOpen(30000000ULL)) { abortUpload("unauthorised or reboot pending"); return; }
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
  if (!authorised() || !leaseOpen(2000000ULL)) {
    abortUpload("unauthorised or maintenance deadline");
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

void getSettings() {
  if (!authorised()) { server.send(403,"text/plain","unauthorised"); return; }
  server.send(200,"application/json",settingsJson());
}
void setSettings() {
  if (!authorised()) { server.send(403,"text/plain","unauthorised"); return; }
  if (receiving || complete || rebootAt || Update.isRunning()) {
    server.send(409,"text/plain","OTA transaction active"); return;
  }
  if (server.args()!=3 || !server.hasArg("profile") ||
      !server.hasArg("bench_sleep") || !server.hasArg("verbose")) {
    server.send(400,"text/plain","expected profile, bench_sleep, verbose"); return;
  }
  const String profile=server.arg("profile");
  const String sleep=server.arg("bench_sleep");
  const String verbose=server.arg("verbose");
  if ((profile!="BENCH" && profile!="FIELD") ||
      (sleep!="0" && sleep!="1") || (verbose!="0" && verbose!="1")) {
    server.send(400,"text/plain","invalid settings"); return;
  }
  auto next=settingsCurrent();
  next.benchSleepLog=false; // legacy POST disarms one-shot logging as before
  next.profile=profile=="BENCH"?mm::Profile::Bench:mm::Profile::Field;
  next.benchSleep=sleep=="1";
  next.verbose=verbose=="1";
  if (!settingsSave(next)) {
    server.send(503,"application/json",settingsJson()); return;
  }
  server.send(200,"application/json",settingsJson());
}

void getClock() {
  if (!authorised()) { server.send(403,"text/plain","unauthorised"); return; }
  server.send(200,"application/json",clockJson());
}
void setClock() {
  if (!authorised()) { server.send(403,"text/plain","unauthorised"); return; }
  if (receiving || complete || rebootAt || Update.isRunning()) {
    server.send(409,"text/plain","OTA transaction active"); return;
  }
  if (!settingsHealthy() || settingsCurrent().profile!=mm::Profile::Bench) {
    server.send(409,"text/plain","BENCH profile required"); return;
  }
  uint64_t utc=0;
  if (server.args()!=1 || !server.hasArg("epoch_us")) {
    server.send(400,"text/plain","expected epoch_us"); return;
  }
  const String value=server.arg("epoch_us");
  if (!mm::parseUtcUs(value.c_str(),value.length(),utc)) {
    server.send(400,"text/plain","invalid UTC epoch_us"); return;
  }
  if (!clockApplyHostUtc(utc)) {
    server.send(500,"text/plain","clock apply/readback failed"); return;
  }
  server.send(200,"application/json",clockJson());
}

bool parseGeneration(const String& s,uint64_t& value) {
  if(s.length()!=16) return false;
  uint64_t n=0;
  for(size_t i=0;i<s.length();++i) {
    const char c=s[i];
    if(c>='0' && c<='9') n=(n<<4)|uint64_t(c-'0');
    else if(c>='a' && c<='f') n=(n<<4)|uint64_t(c-'a'+10);
    else return false;
  }
  if(!n) return false;
  value=n; return true;
}
bool logCommissionAllowed(bool monitorControl=false) {
  if(!authorised()) { server.send(403,"text/plain","unauthorised"); return false; }
  if(receiving || complete || rebootAt || Update.isRunning() ||
      !settingsHealthy() || settingsCurrent().profile!=mm::Profile::Bench ||
      (!monitorControl && storageMonitorActive())) {
    server.send(409,"text/plain","idle BENCH maintenance required"); return false;
  }
  return true;
}
void getLogIdentity() {
  if(!logCommissionAllowed()) return;
  server.send(200,"application/json",storageLogIdentityJson());
}
void setLogIdentity() {
  if(!logCommissionAllowed()) return;
  uint64_t generation=0;
  if(server.args()!=1 || !server.hasArg("generation") ||
      !parseGeneration(server.arg("generation"),generation)) {
    server.send(400,"text/plain","expected nonzero 16 lowercase hex generation"); return;
  }
  if(!storageProvisionLogIdentity(generation)) {
    server.send(409,"application/json",storageLogIdentityJson()); return;
  }
  server.send(200,"application/json",storageLogIdentityJson());
}
void testLogSequence() {
  if(!logCommissionAllowed()) return;
  uint64_t generation=0,seq=0;
  if(server.args()!=1 || !server.hasArg("generation") ||
      !parseGeneration(server.arg("generation"),generation)) {
    server.send(400,"text/plain","expected generation"); return;
  }
  if(!storageAllocateTestSequence(generation,seq)) {
    server.send(409,"application/json",storageLogIdentityJson()); return;
  }
  String body=storageLogIdentityJson();
  body.remove(body.length()-1);
  char b[48]; snprintf(b,sizeof(b),",\"allocated\":\"%016llx\"}",(unsigned long long)seq);
  body+=b; server.send(200,"application/json",body);
}
void testLogWrite() {
  if(!logCommissionAllowed()) return;
  uint64_t generation=0;
  const String mode=server.arg("mode");
  if(server.args()!=2 || !parseGeneration(server.arg("generation"),generation) ||
     (mode!="open" && mode!="seal")) {
    server.send(400,"text/plain","expected generation and mode=open or seal"); return;
  }
  String result;
  const bool ok=storageTestLog(generation,bootId,mode=="open",result);
  server.send(ok?200:409,"application/json",result.length()?result:"{\"ok\":false}");
}
void testLogRecover() {
  if(!logCommissionAllowed()) return;
  uint64_t generation=0,seq=0;
  if(server.args()!=2 || !parseGeneration(server.arg("generation"),generation) ||
     !parseGeneration(server.arg("sequence"),seq)) {
    server.send(400,"text/plain","expected generation and sequence"); return;
  }
  String result;
  const bool ok=storageRecoverTestLog(generation,seq,result);
  server.send(ok?200:409,"application/json",result.length()?result:"{\"ok\":false}");
}
void getLogTransfer(bool data) {
  if(!logCommissionAllowed()) return;
  uint64_t generation=0,seq=0;
  if(server.args()!=2 || !parseGeneration(server.arg("generation"),generation) ||
     !parseGeneration(server.arg("sequence"),seq)) {
    server.send(400,"text/plain","expected generation and sequence"); return;
  }
  fs::File file;
  String descriptor;
  if(!storageOpenBenchTransfer(generation,seq,file,descriptor)) {
    server.send(409,"text/plain","sealed BENCH file unavailable or invalid"); return;
  }
  if(!data) {
    file.close();
    server.send(200,"application/json",descriptor);
    return;
  }
  const size_t expectedBytes=file.size();
  const size_t sent=server.streamFile(file,"application/octet-stream");
  file.close();
  Serial.printf("[LOG] BENCH read seq=%016llx sent=%u expected=%u; retained\n",
    (unsigned long long)seq,unsigned(sent),unsigned(expectedBytes));
}
void postLogAck() {
  if(!logCommissionAllowed()) return;
  uint64_t generation=0,seq=0;
  if(server.args()!=4 || !parseGeneration(server.arg("generation"),generation) ||
     !parseGeneration(server.arg("sequence"),seq) || server.arg("status")!="durable" ||
     !server.hasArg("descriptor") || server.arg("descriptor").length()>511 ||
     server.arg("descriptor").length()==0) {
    server.send(400,"application/json","{\"state\":\"invalid_receipt\"}"); return;
  }
  const char* state=storageBenchAck(generation,seq,server.arg("descriptor"));
  char b[120];
  snprintf(b,sizeof(b),"{\"state\":\"%s\",\"sequence\":\"%016llx\"}",state,(unsigned long long)seq);
  server.send(!strcmp(state,"removed")?200:409,"application/json",b);
}
void getMonitor() {
  if(!logCommissionAllowed(true)) return;
  server.send(200,"application/json",storageMonitorJson());
}
void startMonitor() {
  if(!logCommissionAllowed(true)) return;
  uint64_t generation=0;
  const String count=server.arg("count");
  if(server.args()!=2 || !parseGeneration(server.arg("generation"),generation) ||
     count.length()!=1 || count[0]<'1' || count[0]>'4' || settingsCurrent().benchSleep) {
    server.send(400,"text/plain","expected generation, count=1..4 and bench sleep not armed"); return;
  }
  const bool ok=storageMonitorStart(generation,bootId,unsigned(count[0]-'0'));
  server.send(ok?202:409,"application/json",storageMonitorJson());
}
void sealMonitor() {
  if(!logCommissionAllowed(true)) return;
  if(server.args()) { server.send(400,"text/plain","no arguments expected"); return; }
  const bool ok=storageMonitorSeal();
  server.send(ok?200:409,"application/json",storageMonitorJson());
}
void armSleepLog() {
  if(!logCommissionAllowed()) return;
  uint64_t generation=0;
  if(server.args()!=1 || !parseGeneration(server.arg("generation"),generation) ||
     settingsCurrent().benchSleep || !storageSleepLogReady(generation)) {
    server.send(409,"text/plain","idle provisioned BENCH and free space required"); return;
  }
  auto next=settingsCurrent();
  next.benchSleep=true; next.benchSleepLog=true;
  if(!settingsSave(next)) { server.send(503,"application/json",settingsJson()); return; }
  server.send(200,"application/json",settingsJson());
}
void getInventory() {
  if(!logCommissionAllowed()) return;
  if(server.args()) { server.send(400,"text/plain","no arguments expected"); return; }
  String body;
  if(!storageInventory(body)) { server.send(409,"text/plain","inventory unavailable"); return; }
  server.send(200,"application/json",body);
}
void startServer() {
  server.collectHeaders(HEADERS,3);
  server.on("/status",HTTP_GET,status);
  server.on("/settings",HTTP_GET,getSettings);
  server.on("/settings",HTTP_POST,setSettings);
  server.on("/ota",HTTP_POST,finishRequest,upload);
  server.on("/clock",HTTP_GET,getClock);
  server.on("/clock",HTTP_POST,setClock);
  server.on("/log/inventory",HTTP_GET,getInventory);
  server.on("/log/identity",HTTP_GET,getLogIdentity);
  server.on("/log/identity",HTTP_POST,setLogIdentity);
  server.on("/log/test-sequence",HTTP_POST,testLogSequence);
  server.on("/log/test-write",HTTP_POST,testLogWrite);
  server.on("/log/test-recover",HTTP_POST,testLogRecover);
  server.on("/log/descriptor",HTTP_GET,[](){ getLogTransfer(false); });
  server.on("/log/data",HTTP_GET,[](){ getLogTransfer(true); });
  server.on("/log/ack",HTTP_POST,postLogAck);
  server.on("/log/monitor",HTTP_GET,getMonitor);
  server.on("/log/monitor/start",HTTP_POST,startMonitor);
  server.on("/log/monitor/seal",HTTP_POST,sealMonitor);
  server.on("/log/sleep-arm",HTTP_POST,armSleepLog);
  server.onNotFound([](){ server.send(404,"text/plain","not found"); });
  server.begin();
  Serial.printf("[OTA] READY http://%s/status /ota (authenticated)\n",WiFi.localIP().toString().c_str());
}
}

void otaInit() { bootId=esp_random(); }
uint32_t otaBootId() { return bootId; }
void otaStart(uint64_t deadlineUs) {
  leaseEnd=deadlineUs;
  static bool configured=false;
  if (!configured) { startServer(); configured=true; }
  else server.begin();
}
void otaStop() {
  leaseEnd=0;
  storageMonitorCancel("wifi_lost_preserved");
  if (receiving || complete || Update.isRunning()) abortUpload("Wi-Fi lost");
  sawFile=false;
  server.stop();
}
bool otaRebootPending() { return rebootAt != 0; }
void otaPoll() {
  if (rebootAt) {
    if (uint64_t(esp_timer_get_time())>=rebootAt) ESP.restart();
    return;
  }
  server.handleClient();
  if (!rebootAt && (receiving || complete || Update.isRunning() || sawFile)) {
    abortUpload("request ended without commit");
    sawFile=false;
  }
  if(rebootAt || !settingsHealthy() || settingsCurrent().profile!=mm::Profile::Bench || settingsCurrent().benchSleep) {
    storageMonitorCancel("maintenance_ended_preserved");
  } else if(!receiving && !complete && !Update.isRunning()) storageMonitorPoll(esp_timer_get_time());
}
