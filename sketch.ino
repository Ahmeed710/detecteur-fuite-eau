#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Broches
const int PIN_ENTREE     = 34;  // debitmetre entree (potentiometre)
const int PIN_SORTIE     = 35;  // debitmetre sortie (potentiometre)
const int PIN_BUZZER     = 26;
const int PIN_BOUTON     = 27;
const int PIN_LED_VERTE  = 25;
const int PIN_LED_ROUGE  = 33;
const int PIN_RELAIS     = 32;  // electrovanne (HIGH = vanne fermee)

// Reglages
const int SEUIL_FUITE = 500;
const unsigned long DUREE_CONFIRMATION = 3000;  // fuite confirmee apres 3 s
const unsigned long PERIODE_AFFICHAGE  = 500;
const unsigned long ANTI_REBOND        = 200;   // ignore les rebonds du bouton

enum Etat { NORMAL, SUSPICION, ALARME, ACQUITTEE };
Etat etat = NORMAL;

unsigned long debutEcart = 0;
unsigned long dernierAffichage = 0;
unsigned long dernierAppui = 0;
bool boutonPrecedent = false;

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BOUTON, INPUT_PULLUP);
  pinMode(PIN_LED_VERTE, OUTPUT);
  pinMode(PIN_LED_ROUGE, OUTPUT);
  pinMode(PIN_RELAIS, OUTPUT);
  digitalWrite(PIN_RELAIS, LOW);  // vanne ouverte au demarrage
}

void afficher(int entree, int sortie, int ecart) {
  char ligne[17];

  snprintf(ligne, sizeof(ligne), "E:%4d S:%4d   ", entree, sortie);
  lcd.setCursor(0, 0);
  lcd.print(ligne);

  const char* message;
  switch (etat) {
    case NORMAL:    message = "OK";               break;
    case SUSPICION: message = "Ecart suspect..."; break;
    case ALARME:    message = "FUITE! Vanne OFF"; break;
    case ACQUITTEE: message = "Reparer + bouton"; break;
  }
  snprintf(ligne, sizeof(ligne), "%-16s", message);
  lcd.setCursor(0, 1);
  lcd.print(ligne);

  Serial.printf("E=%d S=%d ecart=%d | %s\n", entree, sortie, ecart, message);
}

void loop() {
  unsigned long maintenant = millis();

  // 1. LIRE
  int debitEntree = analogRead(PIN_ENTREE);
  int debitSortie = analogRead(PIN_SORTIE);
  int ecart = abs(debitEntree - debitSortie);
  bool fuite = ecart > SEUIL_FUITE;

  // Detection de l'instant de l'appui (front) + anti-rebond
  bool boutonEnfonce = digitalRead(PIN_BOUTON) == LOW;
  bool appui = false;
  if (boutonEnfonce && !boutonPrecedent && maintenant - dernierAppui > ANTI_REBOND) {
    appui = true;
    dernierAppui = maintenant;
  }
  boutonPrecedent = boutonEnfonce;

  // 2. DECIDER (machine a etats)
  switch (etat) {
    case NORMAL:
      if (fuite) {
        etat = SUSPICION;
        debutEcart = maintenant;
      }
      break;

    case SUSPICION:
      if (!fuite) etat = NORMAL;
      else if (maintenant - debutEcart >= DUREE_CONFIRMATION) etat = ALARME;
      break;

    case ALARME:
      if (appui) etat = ACQUITTEE;   // 1er appui : on coupe le son
      break;

    case ACQUITTEE:
      if (appui) etat = NORMAL;      // 2e appui : reparation faite, on rouvre
      break;
  }

  // 3. AGIR
  bool clignote = (maintenant / 250) % 2;  // alterne toutes les 250 ms

  digitalWrite(PIN_LED_VERTE, etat == NORMAL);
  digitalWrite(PIN_LED_ROUGE, etat == SUSPICION ? clignote
                             : (etat == ALARME || etat == ACQUITTEE));
  digitalWrite(PIN_RELAIS, etat == ALARME || etat == ACQUITTEE);  // vanne fermee

  if (etat == ALARME) tone(PIN_BUZZER, 1000);
  else noTone(PIN_BUZZER);

  if (maintenant - dernierAffichage >= PERIODE_AFFICHAGE) {
    dernierAffichage = maintenant;
    afficher(debitEntree, debitSortie, ecart);
  }
}