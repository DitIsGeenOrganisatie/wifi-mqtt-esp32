#include "WiFi.h"
#include "PubSubClient.h" //pio lib install "knolleary/PubSubClient"
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

#define SSID          "IB3"
#define PWD           "ingenieursbeleving3"

#define MQTT_SERVER   "192.168.0.110"
#define MQTT_PORT     1883

#define LED_PIN       2

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

#define PN532_IRQ   4
#define PN532_RESET 5 

const int DELAY_BETWEEN_CARDS = 500;
long timeLastCardRead = 0;
boolean readerDisabled = false;
int irqCurr;
int irqPrev;

String card_ids[7] = {"0xF9 0x63 0x05 0xC2", "0x04 0xB9 0x14 0x02 0xFF 0x2C 0x80", "0x43 0xE0 0x01 0x03", "0x04 0x07 0xCC 0x52 0xA8 0x58 0x81", "0x04 0x84 0x7F 0xA2 0x2D 0x4D 0x80", "0xE9 0x39 0xB4 0xC2"};
//"0x89 0xD1 0x12 0xC3"

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

static void startListeningToNFC();
static void handleCardDetected();
void callback(char *topic, byte *message, unsigned int length);

void setup_wifi()
{
  delay(10);
  Serial.println("Connecting to WiFi..");

  WiFi.begin(SSID, PWD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup()
{

  Serial.begin(115200);

  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);

  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN532 board");
    while (1); // halt
  }

    // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  
  // configure board to read RFID tags
  nfc.SAMConfig();

  startListeningToNFC();

  pinMode(LED_PIN, OUTPUT);
}

void callback(char *topic, byte *message, unsigned int length)
{
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off".
  // Changes the output state according to the message
  if (String(topic) == "esp32/output")
  {
    Serial.print("Changing output to ");
    if (messageTemp == "on")
    {
      Serial.println("on");
      digitalWrite(LED_PIN, HIGH);
    }
    else if (messageTemp == "off")
    {
      Serial.println("off");
      digitalWrite(LED_PIN, LOW);
    }
  }
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // creat unique client ID
    // in Mosquitto broker enable anom. access
    if (client.connect("ESP8266Client2"))
    {
      Serial.println("connected");
      // Subscribe
      client.subscribe("nmbs/code");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop()
{
  // if (!client.connected())
  // {
  //   reconnect();
  // }
  // client.loop();

  // long now = millis();
  // if (now - lastMsg > 5000)
  // {
  //   lastMsg = now;
  // }

  if (readerDisabled) {
    if (millis() - timeLastCardRead > DELAY_BETWEEN_CARDS) {
      readerDisabled = false;
      startListeningToNFC();
    }
  } else {
    irqCurr = digitalRead(PN532_IRQ);

    // When the IRQ is pulled low - the reader has got something for us.
    if (irqCurr == LOW && irqPrev == HIGH) {
       //Serial.println("Got NFC IRQ");  
       handleCardDetected(); 
    }
  
    irqPrev = irqCurr;
  }
}


void startListeningToNFC() {
  // Reset our IRQ indicators
  irqPrev = irqCurr = HIGH;
  
  Serial.println("Present an ISO14443A Card ...");
  nfc.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A);
}

void handleCardDetected() {
    uint8_t success = false;
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
    uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

    // read the NFC tag's info
    success = nfc.readDetectedPassiveTargetID(uid, &uidLength);
    Serial.println(success ? "Read successful" : "Read failed (not a card?)");

    if (success) {
      // Display some basic information about the card
      //Serial.println("Found an ISO14443A card");
      //Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
      //Serial.print("  UID Value: ");
      Serial.print("Card ID HEX Value: ");
      nfc.PrintHex(uid, uidLength);

      //check id of card

      String card_id = "";

      //compare uid with card_ids

      for (int i = 0; i < 7; i++)
      {
        for (int j = 0; j < uidLength; j++)
        {
          card_id += "0x";
          if (uid[j] < 0x10) {
            card_id += "0"; // Add leading zero if necessary
          }
          card_id += String(uid[j], HEX);
          card_id += " ";
        }

        Serial.println("Card ID: " + card_id);
        
        card_ids[i].toLowerCase();

        Serial.println("Card ID: " + card_ids[i]);
        if (strcmp(card_id.c_str(), card_ids[i].c_str()) == 0)
        {
          Serial.println("Card ID MATCHED");
          digitalWrite(LED_PIN, HIGH);
          delay(1000);
          digitalWrite(LED_PIN, LOW);
          break;
        }
        else
        {
          Serial.println("Card ID NOT MATCHED");
        }
        card_id = "";
      }



      
      // if (uidLength == 4)
      // {
      //   // We probably have a Mifare Classic card ... 
      //   uint32_t cardid = uid[0];
      //   cardid <<= 8;
      //   cardid |= uid[1];
      //   cardid <<= 8;
      //   cardid |= uid[2];  
      //   cardid <<= 8;
      //   cardid |= uid[3]; 
      //   //Serial.print("Seems to be a Mifare Classic card #");
      //   Serial.print("Card ID NUMERIC Value: ");
      //   Serial.println(cardid);
      // }
      Serial.println("");

      timeLastCardRead = millis();
    }
    
    // The reader will be enabled again after DELAY_BETWEEN_CARDS ms will pass.
    readerDisabled = true;
}
