#define TINY_GSM_MODEM_SIM800
#define SerialMon Serial
#define SerialGPRS Serial1
#define Serial_GPS Serial2
#define TINY_GSM_TEST_BATTERY true

#include <TinyGsmClient.h>
#include <TinyGPS++.h>
#include "utilities.h"

// Configurações gerais
const char phone_number[] = "+5547988414805";
static const uint32_t BAUD_RATE = 9600;
static const uint32_t GSM_RATE = 115200;
static const uint32_t GPS_RATE = 230400;

const unsigned long RESET_INTERVAL = 6UL * 60UL * 60UL * 1000UL; // 6 horas
const unsigned long STATUS_CHECK_INTERVAL = 60000;

unsigned long lastCheckTime = 0;
unsigned long lastResetTime = 0;

int satellites;
int signalQuality;
float vBateria;

TinyGsm modem(SerialGPRS);
TinyGsmClient client(modem);
TinyGPSPlus gps;

bool checkForSMS()
{
  modem.sendAT("+CMGL=\"REC UNREAD\"");
  String response;

  while (modem.stream.available())
  {
    response = modem.stream.readStringUntil('\n');
    SerialMon.println(response);

    if (response.indexOf("Localizar") >= 0)
    {
      SerialMon.println("\nEnviando Localização!");
      return true;
    }
  }
  return false;
}

void sendLocation()
{
  String sendActualLocation;
  if (gps.location.isValid())
  {
    double latitude = gps.location.lat();
    double longitude = gps.location.lng();
    double speed = gps.speed.kmph();
    const char *direction = gps.course.isValid() ? TinyGPSPlus::cardinal(gps.course.value()) : "N/A";
    int satellites = gps.satellites.isValid() ? gps.satellites.value() : 0;
    double altitude = gps.altitude.isValid() ? gps.altitude.meters() : 0.0;
    double hdop = gps.hdop.value();
    int signalQuality = modem.getSignalQuality();
    float vBateria = modem.getBattVoltage() / 1000.0F;

    String movement = (speed <= 5.00) ? " Parado" : "Em Movimento";
    String situation = (satellites <= 0) ? "Sem sinal de GPS!\nUltima localizacao! " : "GPS Ativo!";
   
    String dateTime;
    if (gps.date.isValid() && gps.time.isValid())
    {
      dateTime = String(gps.date.day()) + "/" + String(gps.date.month()) + "/" + String(gps.date.year()) +
                 " - " + String(gps.time.hour()) + ":" + String(gps.time.minute()) + ":" + String(gps.time.second());
    }
    else
    {
      dateTime = "Data e Hora não disponíveis";
    }

    String googleMapsLink = "https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6);
    sendActualLocation = "Localizacao --> "   + googleMapsLink +
                         "\nEstado --> "      + movement +
                         "\nVelocidade --> "  + String(speed, 2) + " km/h" +
                         "\nSentido -->: "    + String(direction) +
                         "\nSatelites --> "   + String(satellites) +
                         "\nAltitude--> "     + String(altitude, 2) + 
                         "\nHDOP --> "        + String(hdop) +
                         "\nGSM RSSI --> "    + String(signalQuality) +
                         "\nBateria --> "     + String(vBateria, 2) + " Volts" +
                         "\n"                 +
                         "\n"                 + situation +
                         "\nData --> "        + dateTime;
  }
  else
  {
    sendActualLocation = "Falha ao obter a localização do GPS.";
  }
  modem.sendSMS(phone_number, sendActualLocation);
  Serial.print("Enviando SMS: ");
  Serial.println(sendActualLocation);
  SerialMon.println("\nMensagens Apagadas!");
  modem.sendAT("+CMGD=1,4");
  
}

void checkStatus()
{

  if (millis() - lastCheckTime >= STATUS_CHECK_INTERVAL)
  {
    if (gps.charsProcessed() < 10)
    {
      SerialMon.println("GPS com erro!");
    }
    else
    {
      satellites = gps.satellites.value();
      SerialMon.print("Satélites: " + String(satellites));
    }

    vBateria = modem.getBattVoltage() / 1000.0F;
    SerialMon.print("\nBateria: " + String(vBateria, 2) + " Volts");

    signalQuality = modem.getSignalQuality();
    SerialMon.print("\nRSSI: " + String(signalQuality));

    lastCheckTime = millis();
  }
}

void ledStatus()
{
  pinMode(LED_GPIO, OUTPUT);
  digitalWrite(LED_GPIO, LED_ON);
  delay(1000);
  digitalWrite(LED_GPIO, LED_OFF);
}

void resetModulo()
{
  unsigned long currentTime = millis();
  if (currentTime - lastResetTime >= RESET_INTERVAL)
  {
    SerialMon.println("Reiniciando ESP32...");
    lastResetTime = currentTime;
    ESP.restart();
  }
}

void setup()
{
  SerialMon.begin(BAUD_RATE);
  setupModem();
  delay(10);

  Serial_GPS.begin(GPS_RATE, SERIAL_8N1, GPS_RX, GPS_TX);
  SerialMon.println("Inicializando GPS...");
  delay(1000);

  SerialGPRS.begin(GSM_RATE, SERIAL_8N1, MODEM_RX, MODEM_TX);
  SerialMon.println("Inicializando GSM...");
  delay(1000);

  SerialMon.println("Reinisiando Modem...");
  modem.restart();
  delay(3000);

  modem.sendAT("+CMGF=1");  
  modem.sendAT("+CTZU=1");
}

void loop()
{
  // Processa dados do GPS
  while (Serial_GPS.available() > 0)
  {
    gps.encode(Serial_GPS.read());
  }
    if (checkForSMS())
    {
      sendLocation();
      ledStatus();
    }
  
  checkStatus();
  resetModulo();
}
