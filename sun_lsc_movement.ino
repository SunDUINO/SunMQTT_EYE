/*
    Name:       LSC_Movement_smart_sensor.ino
    Created:	14.02.2021 14:32:48
    Author:     SunRiver 
                https://suniotprojects.blogspot.com/
*/

// Biblioteki ---------------------------------------------------------------------------------------------------
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Ustawienia WIFI -----------------------------------------------------------------------------------------------
const char* ssid = "wifi_ssid";
const char* pass = "wifi_paswd";

// Ustawienia MQTT -----------------------------------------------------------------------------------------------
const char* mqtt_srw = "addres_mqtt";  // np. 192.168.2.134
const int   mqtt_prt = 1883;           // port
const char* mqtt_usr = "user";         // user mqtt
const char* mqtt_pwd = "pasword";      // pasword mqtt

const char* mqtt_cid = "Sotton_EYE";   // mqtt client id  


WiFiClient espClient;
PubSubClient client(espClient);

String temperatureString = "";
String timestring = "";

// Ustawienia GPIO ------------------------------------------------------------------------------------------------
// zmienne, status LED, PIR Motion Sensor, LDR, temperatury
const int output = 14; 
const int statusLed = 16;   
const int motionSensor = 5;
const int ldr = A0;

String outputState = "off";

// One Wire --------------------------------------------------------------------------------------------------------
const int oneWireBus = 4;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);  // DS18B20

// Timery ----------------------------------------------------------------------------------------------------------
unsigned long now = millis();
unsigned long lastMeasure = 0;
boolean startTimer = false;
unsigned long currentTime = millis();
unsigned long previousTime = 0;

// Utalenie połaczenia z WIFI --------------------------------------------------------------------------------------
void setup_wifi() 
{
    delay(10);
    // Informacje debugowe na uarcie ....
    Serial.println();
    Serial.print("Lacze do sieci:  ");
    Serial.println(ssid);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("WiFi polaczone - IP address: ");
    Serial.println(WiFi.localIP());
}

// --> Publikowanie wiadomości -- mozna personalizować zarówno wiadomość 
//     jak i akcje w urządzeniach subskrybującyc
void callback(String topic, byte* message, unsigned int length) 
{
   
    String messageTemp;

    for (int i = 0; i < length; i++) 
    {
        Serial.print((char)message[i]);
        messageTemp += (char)message[i];
    }
    Serial.println();

// -->  Kontrola GPIO -- może się prrzydać  do sygnalizacji 
    if (topic == "SoTToN_Eye/output") 
    {
        Serial.print("Ustawiam wyjście na");
        if (messageTemp == "on") 
        {
            digitalWrite(output, LOW);
            client.publish("SoTToN_Eye/out_sts", "ON");
            Serial.print("on");
        }
        else if (messageTemp == "off") 
        {
            digitalWrite(output, HIGH);
            client.publish("SoTToN_Eye/out_sts", "OFF");
            Serial.print("off");
        }
    }
    Serial.println();
}

// Realizacja połączenia z MQTT  ----------------------------------------------------------------------------------
void reconnect() 
{
    
    while (!client.connected()) 
    {
        
        // Załadowanie CID  --  /clientID/
        String clientId = mqtt_cid;
        clientId += String(random(0xffff), HEX);
        // Podjęcie próby połaczenia
        if (client.connect(clientId.c_str(),mqtt_usr, mqtt_pwd)) 
        {
            Serial.println("connected");
            // -- subskrybowanie topiku -- pomaga kontrolować gpio 
            client.subscribe("SoTToN_Eye/output");
        }
        else 
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            //5sek oczekiwania na ponowną próbę łączenia
            delay(5000);
        }
    }
}

// --> Sprawdzanie stanu czujnika ruchu  i uruchoimienie timera
ICACHE_RAM_ATTR void detectsMovement() 
{
    Serial.println("Wykryto Ruch!");
    client.publish("SoTToN_Eye/motion", "RUCH");
    previousTime = millis();
    startTimer = true;
}

void setup() 
{
    // --> uruchamiam 1Wire 
    sensors.begin();

    // --> Ustawiam serial na 115200bps
    Serial.begin(115200);

    // --> Ustawiam Czujnik Ruchu
    pinMode(motionSensor, INPUT_PULLUP); 
    attachInterrupt(digitalPinToInterrupt(motionSensor), detectsMovement, RISING);

    // --> Ustawiam piny stanów i wyjść 
    pinMode(output, OUTPUT);
    pinMode(statusLed, OUTPUT);
    digitalWrite(output, LOW); //HIGH or LOW
    digitalWrite(statusLed, HIGH);

    setup_wifi();
    client.setServer(mqtt_srw, mqtt_prt);
    client.setCallback(callback);
}

void loop() {
    if (!client.connected()) 
    {
        reconnect();
    }
    client.loop();

    // --> Ustawiam Timer na TERAZ
    now = millis();

    // --> co 5 minut wysyłka czasu działania w sek -------------------------------------------------------------
    // --> taki mały testowy uptime
    if (now - lastMeasure > 300000)
    {
        timestring = String(now / 1000);
        client.publish("SoTToN_Eye/time_sec", timestring.c_str());
        Serial.println("==>> Aktualizacja czasu");
    }

    // --> Odczyt LDR i Temperatury co 30sekund  ---------------------------------------------------------------- 
    if (now - lastMeasure > 30000) 
    {
        lastMeasure = now;
        sensors.requestTemperatures();
        // ---> Stopnie Celsiusza 
        temperatureString = String(sensors.getTempCByIndex(0));
        // ---> Stopnie Fahrenheita 
        // temperatureString = String(sensors.getTempFByIndex(0));
        // ---> Publikacja temperatury 
        client.publish("SoTToN_Eye/temperature", temperatureString.c_str());
        Serial.println("==>> Aktualizacja Temperatury");

        // ---> Odczyt czujnika LDR 
        client.publish("SoTToN_Eye/ldr", String(analogRead(ldr)).c_str());
        Serial.println("==>> Zmiana parametrow LDR"); 
    }

    // ---> Czujnik Ruchu ----------------------------------------------------------------------------------------
    // ---> Stan czujnika po wykryciu ruchu jest aktywny 10 sekund  jeśli wystąpi zmiana  ustawiamy Brak Ruchu
    // ---> w przeciwnym wpadku utrzymnujemy alarm ....
    if ((now - previousTime > 10000) && startTimer) 
    {
        client.publish("SoTToN_Eye/motion", "BRAK");
        Serial.println("Brak Ruchu");
        startTimer = false;
    }
}
