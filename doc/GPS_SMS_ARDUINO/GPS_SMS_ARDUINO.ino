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
unsigned long lastResetTime = 0;

TinyGsm modem(SerialGPRS);
TinyGsmClient client(modem);
TinyGPSPlus gps;

// Função para obter o último dia de um mês, levando em consideração anos bissextos
int getLastDayOfMonth(int month, int year)
{
  if (month == 2)  // Fevereiro
  {
    // Verifica se é ano bissexto
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
      return 29;
    else
      return 28;
  }
  else if (month == 4 || month == 6 || month == 9 || month == 11)
    return 30;  // Abril, Junho, Setembro, Novembro
  else
    return 31;  // Todos os outros meses
}

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
   
    // Ajuste do fuso horário (por exemplo, UTC-3 para horário de Brasília)
    int utcOffset = -3;  // Ajuste conforme o fuso horário desejado
    String dateTime;
    if (gps.date.isValid() && gps.time.isValid())
    {
      int hour = gps.time.hour() + utcOffset;
      int day = gps.date.day();
      int month = gps.date.month();
      int year = gps.date.year();

      // Ajustar se a hora ficar negativa ou ultrapassar 24 horas
      if (hour < 0) {
        hour += 24;
        day--;  // Subtrai um dia

        // Ajusta o mês e ano se o dia ficar menor que 1
        if (day < 1) {
          month--;
          if (month < 1) {
            month = 12;
            year--;  // Volta um ano se o mês ficar menor que janeiro
          }
          // Ajusta o último dia do mês anterior
          day = getLastDayOfMonth(month, year);
        }
      } else if (hour >= 24) {
        hour -= 24;
        day++;  // Adiciona um dia

        // Ajusta o mês e ano se o dia ultrapassar o último dia do mês
        if (day > getLastDayOfMonth(month, year)) {
          day = 1;
          month++;
          if (month > 12) {
            month = 1;
            year++;  // Avança um ano se o mês ultrapassar dezembro
          }
        }
      }

      dateTime = String(day) + "/" + String(month) + "/" + String(year) +
                 " - " + String(hour) + ":" + String(gps.time.minute()) + ":" + String(gps.time.second());
    }
    else
    {
      dateTime = "Data e Hora não disponíveis";
    }

    String googleMapsLink = "https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6);
       sendActualLocation = "\n"                 + situation +
                            "\nData --> "        + dateTime +
                            "\n"                 +
                            "Localizacao --> "   + googleMapsLink +
                            "\nEstado --> "      + movement +
                            "\nVelocidade --> "  + String(speed, 2) + " km/h" +
                            "\nSentido --> "     + String(direction) +
                            "\nSatelites --> "   + String(satellites) +
                            "\nAltitude--> "     + String(altitude, 2) + 
                            "\nHDOP --> "        + String(hdop) +
                            "\nRSSI --> "        + String(signalQuality) +
                            "\nBateria --> "     + String(vBateria, 2) + " Volts" ;

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

void ledStatus()
{
  pinMode(LED_GPIO, OUTPUT);
  digitalWrite(LED_GPIO, LED_ON);
  delay(1000);
  digitalWrite(LED_GPIO, LED_OFF);
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
  resetModulo();
}
