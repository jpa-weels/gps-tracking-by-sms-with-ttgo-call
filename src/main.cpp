#define TINY_GSM_MODEM_SIM800
#define SerialMon Serial
#define SerialGPRS Serial1
#define Serial_GPS Serial2
#define TINY_GSM_TEST_BATTERY true

#include <TinyGsmClient.h>
#include <TinyGPS++.h>
#include <TimeLib.h>  // Biblioteca para manipular tempo
#include "TTGOTCALL.h" // Para esp32 ttgo t Call sim800l
//#include "ESP32.h" // Para modulos separados esp32, sim800l, ublox m9

// Configurações gerais
const char phone_number[] = "+55479884xxxx";

static const uint32_t BAUD_RATE = 9600;
static const uint32_t GSM_RATE = 115200; // Padrão, nao deve ser alterado
static const uint32_t GPS_RATE = 230400; // O padrão dos modulos gps é de 9600. para alterar vc precisa de um software https://content.u-blox.com/sites/default/files/2024-06/u-centersetup_v24.05.zip e uma plada de conexao usb to ttl FT232l

const unsigned long RESET_INTERVAL = 6UL * 60UL * 60UL * 1000UL; //Reset modulo a cada 6 horas
unsigned long lastResetTime = 0;

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

float ajustSpeed(float speed) {
    return speed <= 3.00 ? 0.00 : speed;
}
 // Converter a data/hora do GPS para timestamp Unix
unsigned long getUnixTimestamp() {
    if (gps.date.isValid() && gps.time.isValid()) {
        tmElements_t tm;
        tm.Year = gps.date.year() - 1970;
        tm.Month = gps.date.month();
        tm.Day = gps.date.day();
        tm.Hour = gps.time.hour();
        tm.Minute = gps.time.minute();
        tm.Second = gps.time.second();
        return makeTime(tm) - 3 * 3600; // Ajustar o fuso horário, aqui na minha localidade não foi necessario  (-3 horas para UTC-3)
    }
    return 0;
}

// Função para converter timestamp Unix para o formato DD-MM-YYYY HH:MM:SS
String formatDateTime(unsigned long timestamp) {
    tmElements_t tm;
    breakTime(timestamp, tm);

    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%02d-%02d-%04d %02d:%02d:%02d", 
             tm.Day, tm.Month, tm.Year + 1970, tm.Hour, tm.Minute, tm.Second);

    return String(buffer);
}

void sendLocation()
{
  String sendActualLocation;
  if (gps.location.isValid() && gps.location.isUpdated()){
   
    String movement = (gps.speed.kmph() <= 5.00) ? " Nao" : "Sim";
    String situation = (gps.satellites.value() <= 0) ? "Sem sinal de GPS!\nÚltima localização!" : "GPS Ativo!";

    
    unsigned long unixTime = getUnixTimestamp();  // Obtenha o timestamp Unix
        String dateTimeStr = "";  // Declare a variável fora do bloco
        if (unixTime > 0) {
            dateTimeStr = formatDateTime(unixTime);  // Inicialize aqui
        }
  
    String googleMapsLink = "https://maps.google.com/?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
       sendActualLocation = "\n"                 + situation +
                            "\nData --> "        + String(dateTimeStr) +
                            "\n"                 +
                            "\nLocalizacao --> " + googleMapsLink +
                            "\nMovimento --> "   + movement +
                            "\nVelocidade --> "  + String(ajustSpeed(gps.speed.kmph()), 1) + " km/h" +
                            "\nSatelites --> "   + String(gps.satellites.value()) +
                            "\nAltitude--> "     + String(gps.altitude.meters(), 1) +
                            "\nRSSI --> "        + String(modem.getSignalQuality()) +
                            "\nIMEI --> "        + String(modem.getIMEI()) +
                            "\nBateria --> "     + String(modem.getBattVoltage() / 1000.0F) + " Volts" ;

  } else {
    sendActualLocation = "Encontrando Satelites!\nAguarde... ";
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
