#include "Buzzer.h"
#include "Config.h"

bool alarmSequenceActive = false;
bool lowBattBeepActive = false;
int lowBattBeepRemaining = 0;
unsigned long lowBattBeepTimer = 0;

// Active buzzer: hold pin high for the duration.
#define CHIRP_MS        90
#define CHIRP_GAP_MS   110

static void chirpOnce(unsigned int ms) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(ms);
  digitalWrite(BUZZER_PIN, LOW);
}

void buzzerOn()  { chirpOnce(CHIRP_MS); }
void buzzerOff() { digitalWrite(BUZZER_PIN, LOW); }

void buzzerChirpPair() {
  chirpOnce(CHIRP_MS);
  delay(CHIRP_GAP_MS);
  chirpOnce(CHIRP_MS);
}

void updateBeeps(unsigned long now) {
  if (alarmSequenceActive) {
    buzzerChirpPair();
    alarmSequenceActive = false;
  }
  if (lowBattBeepActive) {
    buzzerChirpPair();
    lowBattBeepActive = false;
    lowBattBeepRemaining = 0;
  }
}
