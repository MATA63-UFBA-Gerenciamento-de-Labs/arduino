/*
  Projeto baseado no BasicHTTPSClient.ino do ESP8266 e no urlencode.ino
*/

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <SPI.h>
#include <MFRC522.h>

// Seta os pinos que serão utilizados para leitura UID
#define SS_PIN 2  // D4 pin
#define RST_PIN 4 // D2 pin
// Seta os pinos que serão utilizados para indicar abertura/bloqueio da porta
#define LED_Verde 5 // Led Verde -> D1 no ESP
#define LED_Vermelho 0 // Led Vermelho -> D3 no ESP
#define BUZZER 16 // Buzzer -> D0 no ESP

// Define as variáveis necessárias para identificar leituras e envios
bool isRead = false;
bool novoRFID = false;
String rfidTag = "";
String currentRfidTag = "";

int INTERVAL = 2000; // Seta um intervalo antes de processar o mesmo cartão/tag
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;

// Instancia a classe do módulo MFRC 522
MFRC522 mfrc522(SS_PIN, RST_PIN); 

// Define as credenciais para conexão com o Wi-Fi
const char* ssid = ""; // Insira aqui o nome de sua rede 2.4Ghz
const char* password = "";  // Insira aqui a senha de seu Wi-Fi

void setup() {
  Serial.begin(115200);
  // Inicia o SPI bus
  SPI.begin();
  // Inicia o MFRC522
  mfrc522.PCD_Init();

  // Coloca os referidos pinos em modo de saída
  pinMode(LED_Verde, OUTPUT);
  pinMode(LED_Vermelho, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  Serial.println();
  Serial.println();
  Serial.println();

  // Conecta à rede Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }

  Serial.println();
  Serial.println("Pronto! Apresente seu cartão/tag.");
}

void loop() {
  if(mfrc522.PICC_IsNewCardPresent()) {
    if(mfrc522.PICC_ReadCardSerial()) {
      isRead = true;

      Serial.print("Tag UID:");
      Serial.print(rfidTag);
      for(byte i = 0; i < mfrc522.uid.size; i++) {
        rfidTag.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
        rfidTag.concat(String(mfrc522.uid.uidByte[i], HEX));
      }

      // Remover os espaços em branco no início
      rfidTag.remove(0, 1);

      rfidTag.toLowerCase();

      Serial.println();
      mfrc522.PICC_HaltA();
    }
  }

  if (isRead) {
      currentMillis = millis();
      if (currentRfidTag != rfidTag) {
        currentRfidTag =  rfidTag;
        novoRFID = true;
      } else {
        if (currentMillis - previousMillis >= INTERVAL) {
          novoRFID = true;
        } else {
          novoRFID = false;
        }
      }

      // Se for um novo cartão/tag:
      if(novoRFID) {
        if(rfidTag != "") {
          desligaLEDVermelho();
          noTone(BUZZER);

          previousMillis = currentMillis;

          Serial.print("Código UID:");
          Serial.println(rfidTag);

          String encodedRFIDCode = urlencode(rfidTag);
          Serial.println();

          // Envia o RFID Uid
          bool isAuthorized = usuarioAutorizado(encodedRFIDCode);

          if (!isAuthorized) {
            naoAutorizado();
          } else {
            autorizado();
          }
        }
      }
  }

  rfidTag = "";
  novoRFID = false;
}

bool usuarioAutorizado(String rfIdCode){
  bool sendDataAutorizado = false;
  // Aguarda pela conexão Wi-Fi
  if ((WiFi.status() == WL_CONNECTED)) {

    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

    // Ignora a validação do certificado SSL
    client->setInsecure();
    
    // Cria a instância do HTTPClient
    HTTPClient https;
    
    // Inicializa uma comunicação via HTTPS com o client
    String url = "https://back-end-orcin-theta.vercel.app/aluno/autorizado/?rf_id_code="; // Host da rota de autorização na API
    url+=rfIdCode;

    https.setTimeout(20000);

    Serial.print("[HTTPS] Conectando API...\n");

    if (https.begin(*client, url)) {
      int httpCode = https.GET();

      if (httpCode > 0) {
        Serial.println("[HTTPS] GET...");
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          sendDataAutorizado = retornaAutorizacao(payload);
        }
      } else {
        Serial.printf("[HTTPS] O GET falhou. Erro: %s\n", https.errorToString(httpCode).c_str());
      }
      https.end();
    } else {
      Serial.printf("[HTTPS] Incapaz de conectar.\n");
    }
  }
  return sendDataAutorizado;
}

String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
      }
      yield();
    }
    return encodedString;
}


bool retornaAutorizacao(String payload){
  const size_t capacity = JSON_OBJECT_SIZE(1) + 30;
  DynamicJsonDocument doc(capacity);

  deserializeJson(doc, payload);

  const char* autorizado = doc["autorizado"];

  bool estaAutorizado = (doc["autorizado"] == "true");
  Serial.printf("Autorizado: ");
  Serial.println(autorizado);

  return estaAutorizado;
}



void naoAutorizado() {
  Serial.println("Você não está autorizado!");
  tone(BUZZER, 2000);
  ligaLEDVermelho();
  noTone(BUZZER);
  desligaLEDVermelho();
}

void autorizado() {
  Serial.println("Pode entrar. Lá ele!");
  ligaLEDVerde();
}

void desligaLEDVerde() {
  digitalWrite(LED_Verde, LOW);
}

void desligaLEDVermelho(){
  digitalWrite(LED_Vermelho, LOW);
}

void ligaLEDVermelho() {
  digitalWrite(LED_Vermelho, HIGH);
  delay(1000);
  desligaLEDVermelho();
}

void ligaLEDVerde() {
  digitalWrite(LED_Verde, HIGH);
  tone(BUZZER, 500);
  delay(100);
  noTone(BUZZER);
  delay(900);
  desligaLEDVerde();
}