/* Conexiones entre el RFID RC522 y la nodemcu
RC522     ARDUINO   NODEMCU
VCC       3.3V      3V
RST       GPIO5     D1
GND       GND       GND
MISO      GPIO12    D6
MOSI      GPIO13    D7
SCK       GPIO14    D5
NSS       GPIO4     D2
*/

/* Conexiones entre el reproductor mp3 DFPlayer Mini y la nodemcu
DFPlayer  ARDUINO   NODEMCU
VCC       3.3V      3V
GND       GND       GND
RX        GPIO0     D4
TX        GPIO2     D3
*/

//Librerías WiFi
#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

//Librerías RFID
#include <SPI.h>
#include <MFRC522.h>

//Librerías MP3
#include "Arduino.h"
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"



//LED de la nodemcu
#define LED_GPIO 2



//Variables globales para el WiFi
#define WLAN_SSID       "WLAN_F5L2"
#define WLAN_PASS       "H496N64Vf56H6iR4DVi9"
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                   //8883 para SSL
#define AIO_USERNAME    "jesusmanuel"
#define AIO_KEY         "a4d7b866794045618bbcaa59fc877092"

WiFiClient client;                                                                                          //Crea un cliente ESP8266. Usar WiFiClientSecure para cliente SSL
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);                      //Crea el cliente MQTT
Adafruit_MQTT_Publish canalRFID = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/canalRFID");            //Publicar en un canal el nombre del usuario. El nombre del canal es "canalRFID"
Adafruit_MQTT_Subscribe canalMP3 = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/canalMP3");          //Suscribirse a un canal que permite encender y apagar la música. El nombre del canal es "canalMP3"
Adafruit_MQTT_Subscribe canalVolume = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/canalVolume");    //Suscribirse a un canal que regula el volumen del altavoz. El nombre del canal es "canalVolume"
Adafruit_MQTT_Subscribe canalNext = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/canalNext");        //Suscribirse a un canal que pase a la siguiente canción. El nombre del canal es "canalNext"



//Conexiones del RFID
const int NSS_PIN = 4;
const int RST_PIN = 5;

//Conexiones del MP3
SoftwareSerial mySoftwareSerial(0, 2); // RX, TX



//Variables globales para el RFID
byte actualSerial[4];                             //Serial de la tarjeta leída
byte jesusSerial[4] = {0xFB, 0x1F, 0xDD, 0x5F};   //Serial de la tarjeta de Jesus Manuel
byte azaharaSerial[4] = {0xB5, 0x89, 0x6D, 0x7B}; //Serial de la tarjeta de Azahara
MFRC522 mfrc522(NSS_PIN, RST_PIN);                //Instancia del MFRC522

//Variables globales para el MP3
DFRobotDFPlayerMini myDFPlayer;



/*
 * Conexión a la red WiFi.
 */
 void WIFI_connect()
{
  //Poner el módulo WiFi en modo station (este modo permite de serie el "light sleep" para consumir menos y desconectar de cualquier red a la que pudiese estar previamente conectado
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(1000);

  //Conectamos a la WiFi
  Serial.print("Conectando a la red WiFi...");

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED)       //Nos quedamos esperando hasta que conecte
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" conectado.");
}


/*
 * Funcción para conectar inicialmente y reconectar cuando se haya perdido la conexión al servidor MQTT.
 */
void MQTT_connect()
{
  int8_t ret;

  if (!mqtt.connected())
  {
    Serial.print("Conectando al servidor MQTT...");
    uint8_t intentos = 3;
    while ((ret = mqtt.connect()) != 0)
    {
      Serial.println(mqtt.connectErrorString(ret));
      Serial.println("Reintentando dentro de 3 segundos...");
      mqtt.disconnect();
      delay(3000);
      if (!intentos--)
      { 
        while (1);
      }
    }
    Serial.println(" conectado.");
  }
}


/*
 * Reproduce las canciones de la tarjeta micro SD.
 */
void playMP3()
{
  myDFPlayer.loopFolder(1);
}


/*
 * Detiene la reproducción.
 */
void stopMP3()
{
  myDFPlayer.pause();
}


/*
 * Pasa a la siguiente canción.
 */
void nextSong()
{
  myDFPlayer.next();
}


/*
 * Publica el nombre del usuario leído en el canal "canalRFID".
 */
void publishInChannel(String user)
{
  unsigned int errorPublicacion;    //Variable para el control de errores de envío al publicar
  char userArray[user.length()+1];
  user.toCharArray(userArray, user.length()+1);

  //PUBLICACION: Si no hay error de publicación, la función devuelve 0, sino el código de error correspondiente (sólo interesante para debug)
  if (!(errorPublicacion = canalRFID.publish(userArray)))
  {
    Serial.print("\nError de publicación (error ");
    Serial.print(errorPublicacion);
    Serial.println(")");
  }
}


/*
 * Recibe la orden de encencer o apagar la música del canal "canalMP3", el valor del volumen del canal "canalVolume" y si se pasa a la siguiente canción con el canal "canalNext".
 */
void subscribeFromChannel()
{
  //SUSCRIPCION
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(500)))
  {
    if (subscription == &canalMP3)
    {
      if (strcmp((char *)canalMP3.lastread, "ON"))
      {
        Serial.println("PLAY");
        playMP3();
      }
      else
      {
        Serial.println("STOP");
        stopMP3();
      }
    }
    else if (subscription == &canalVolume)
    {
      Serial.print("VOLUME = ");
      Serial.println(atoi((char *)canalVolume.lastread));
      myDFPlayer.volume(atoi((char *)canalVolume.lastread));
    }
    else if (subscription == &canalNext)
    {
      if (strcmp((char *)canalNext.lastread, "0"))
      {
        Serial.println("NEXT SONG");
        nextSong();
      }
    }
  }
}


/*
 * Almacena en actualSerial el serial de la tarjeta leída.
 */
void saveSerial(byte *buffer, byte bufferSize)
{
  for (byte i = 0; i < bufferSize; i++)
    actualSerial[i] = buffer[i];
}


/*
 * Imprime el serial de la tarjeta leída.
 */
void printSerial(byte *buffer, byte bufferSize)
{
  myDFPlayer.playMp3Folder(3);
  Serial.print("\nUsuario no identificado. Serial:");
  for (byte i = 0; i < bufferSize; i++)
  {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
  Serial.println();
}


/*
 * Enciende y apaga el LED dos veces para notificar que se ha leído una tarjeta.
 */
void notifyLed()
{
  int timeOn = 50;
  digitalWrite(LED_GPIO, LOW);
  delay(timeOn);
  digitalWrite(LED_GPIO, HIGH);
  delay(timeOn);
  digitalWrite(LED_GPIO, LOW);
  delay(timeOn);
  digitalWrite(LED_GPIO, HIGH);
}


/*
 * Comprueba si el serial del user1 y el serial del user2 son iguales.
 */
boolean checkUser(byte user1[], byte user2[])
{
  for (byte i = 0; i < 4; i++)
    if (user1[i] != user2[i])
      return false;
      
  return true;
}


/*
 * Reproduce el sonido de bienvenida al usuario correspondiente o de usuario no identificado.
 */
void playWelcome(int user)
{
  myDFPlayer.playMp3Folder(user);
  delay(3000);
}



void setup()
{
  Serial.begin(115200);

  //Inicializa la WiFi
  WIFI_connect();

  //Suscripción a los canales "canalMP3", "canalVolume" y "canalNext"
  mqtt.subscribe(&canalMP3);
  mqtt.subscribe(&canalVolume);
  mqtt.subscribe(&canalNext);

  //Inicializa el LED
  pinMode(LED_GPIO, OUTPUT);
  digitalWrite(LED_GPIO, HIGH);
  
  //Inicializa el RFID
  SPI.begin();
  mfrc522.PCD_Init();

  //Inicializa el MP3
  mySoftwareSerial.begin(9600);
  if (!myDFPlayer.begin(mySoftwareSerial))
  {
    while(true);
  }
  myDFPlayer.volume(10);


  Serial.println("RFID y MP3 conectados.\n");
}

void loop()
{
  //Conexión al servidor MQTT
  MQTT_connect();
  
  subscribeFromChannel();
  
  //Detecta tarjeta
  if (mfrc522.PICC_IsNewCardPresent())
  {
    //Lee el serial de la tarjeta
    if (mfrc522.PICC_ReadCardSerial())
    {
      //Guarda el serial de la tarjeta
      saveSerial(mfrc522.uid.uidByte, mfrc522.uid.size);
      
      if (checkUser(actualSerial, jesusSerial) == true)
      {
        publishInChannel("Jesus Manuel");
        Serial.println("\nBienvenido, Jesus Manuel.");
        playWelcome(1);
        playMP3();
      }
      else if (checkUser(actualSerial, azaharaSerial) == true)
      {
        publishInChannel("Azahara");
        Serial.println("\nBienvenida, Azahara.");
        playWelcome(2);
        playMP3();
      }
      else
      {
        publishInChannel("Usuario no identificado");
        printSerial(mfrc522.uid.uidByte, mfrc522.uid.size); 
      }

      notifyLed();

      //Finaliza la lectura actual
      mfrc522.PICC_HaltA();

      delay(500);
    }
  }
}
