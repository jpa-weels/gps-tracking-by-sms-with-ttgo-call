#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_TEST_BATTERY true

#include <TinyGsmClient.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

// Configurações gerais
const char phone_number[] = "+5547988414805";
static const uint32_t BAUD_RATE = 9600;
static const uint32_t GSM_RATE = 115200;  // Ajustando para um valor padrão suportado pelo Uno
static const uint32_t GPS_RATE = 57600;  // Mesma taxa para o GPS

const unsigned long RESET_INTERVAL = 6UL * 60UL * 60UL * 1000UL; // 6 horas
unsigned long lastResetTime = 0;

// Definindo pinos para o SoftwareSerial
SoftwareSerial SerialGPRS(7, 8); // RX, TX para GSM
SoftwareSerial SerialGPS(4, 3);  // RX, TX para GPS

TinyGsm modem(SerialGPRS);
TinyGsmClient client(modem);
TinyGPSPlus gps;

// Função para obter o último dia de um mês
int getLastDayOfMonth(int month, int year)
{
  if (month == 2) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
  } else if (month == 4 || month == 6 || month == 9 || month == 11) {
    return 30;
  } else {
    return 31;
  }
}

bool checkForSMS()
{
  modem.sendAT("+CMGL=\"REC UNREAD\"");
  String response;

  while (modem.stream.available()) {
    response = modem.stream.readStringUntil('\n');
    Serial.println(response);

    if (response.indexOf("Localizar") >= 0) {
      Serial.println("\nEnviando Localização!");
      return true;
    }
  }
  return false;
}

void sendLocation()
{
  String sendActualLocation;
  if (gps.location.isValid() && gps.location.isUpdated()) {
    double latitude = gps.location.lat();
    double longitude = gps.location.lng();
    double speed = gps.speed.kmph();
    const char *direction = gps.course.isValid() ? TinyGPSPlus::cardinal(gps.course.value()) : "N/A";
    int satellites = gps.satellites.isValid() ? gps.satellites.value() : 0;
    double altitude = gps.altitude.isValid() ? gps.altitude.meters() : 0.0;
    int signalQuality = modem.getSignalQuality();
    float vBateria = modem.getBattVoltage() / 1000.0F;

    String movement = (speed <= 5.00) ? " Parado" : "Em Movimento";
    String situation = (satellites <= 0) ? "Sem sinal de GPS!\nUltima localizacao! " : "GPS Ativo!";

    // Ajuste do fuso horário (UTC-3)
    int utcOffset = -3;
    String dateTime;
    if (gps.date.isValid() && gps.time.isValid()) {
      int hour = gps.time.hour() + utcOffset;
      int day = gps.date.day();
      int month = gps.date.month();
      int year = gps.date.year();

      if (hour < 0) {
        hour += 24;
        day--;
        if (day < 1) {
          month--;
          if (month < 1) {
            month = 12;
            year--;
          }
          day = getLastDayOfMonth(month, year);
        }
      } else if (hour >= 24) {
        hour -= 24;
        day++;
        if (day > getLastDayOfMonth(month, year)) {
          day = 1;
          month++;
          if (month > 12) {
            month = 1;
            year++;
          }
        }
      }

      dateTime = String(day) + "/" + String(month) + "/" + String(year) +
                 " - " + String(hour) + ":" + String(gps.time.minute()) + ":" + String(gps.time.second());
    } else {
      dateTime = "Data e Hora não disponíveis";
    }

    String googleMapsLink = "https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6);
    sendActualLocation = situation +
                         "\nData --> " + dateTime +
                         "\nLocalizacao --> " + googleMapsLink +
                         "\nEstado --> " + movement +
                         "\nVelocidade --> " + String(speed, 2) + " km/h" +
                         "\nSentido --> " + String(direction) +
                         "\nSatelites --> " + String(satellites) +
                         "\nAltitude --> " + String(altitude, 2) +
                         "\nRSSI --> " + String(signalQuality) +
                         "\nBateria --> " + String(vBateria, 2) + " Volts";
  } else {
    sendActualLocation = "Encontrando Satelites!\nAguarde...";
  }

  modem.sendSMS(phone_number, sendActualLocation);
  Serial.print("Enviando SMS: ");
  Serial.println(sendActualLocation);
  modem.sendAT("+CMGD=1,4");
}

void resetModulo()
{
  unsigned long currentTime = millis();
  if (currentTime - lastResetTime >= RESET_INTERVAL) {
    Serial.println("Reiniciando Arduino...");
    lastResetTime = currentTime;
    // No Arduino Uno não há função para reiniciar, então apenas registre o tempo
  }
}

void setup()
{
  Serial.begin(BAUD_RATE);

  SerialGPS.begin(GPS_RATE);
  Serial.println("Inicializando GPS...");
  delay(1000);

  SerialGPRS.begin(GSM_RATE);
  Serial.println("Inicializando GSM...");
  delay(1000);

  modem.restart();
  delay(3000);

  modem.sendAT("+CMGF=1");
  modem.sendAT("+CTZU=1");
}

void loop()
{
  // Processa dados do GPS
  while (SerialGPS.available() > 0) {
    gps.encode(SerialGPS.read());
  }

  if (checkForSMS()) {
    sendLocation();
  }

 // resetModulo();
}
