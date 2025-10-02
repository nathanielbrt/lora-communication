#include "LoRa_E22.h"
#include "Arduino.h"
#include <Preferences.h>
#include <nvs_flash.h>

#define RX_PIN 16
#define TX_PIN 17
LoRa_E22 e22(&Serial2, 15, 21, 19); // AUX, M0, M1

Preferences controle;
Preferences transmissao;
Preferences leitor;

int contador = 0;
String nameSpc;

//ENDEREÇO deste RECEPTOR
#define MY_ADDH 0x00
#define MY_ADDL 0x0C // Endereço deste receptor

// Struct idêntica à do transmissor!
typedef struct __attribute__((packed)) {
  float time;
  float lat; 
  float lon;
  float alt; 
  float veld; 
  float veln;
  float vele; 
  float yaw;
  float pitch;
  float roll;
  float bar_out;
  uint8_t gnss_fix;
  uint8_t parachute;
} CompactPayload;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  e22.begin();
  delay(500);

  Serial2.setTimeout(200);

  Serial.print("\n[RECEPTOR] sizeof(CompactPayload) = ");
  Serial.println(sizeof(CompactPayload));
  Serial.println("Esperando por pacotes de " + String(sizeof(CompactPayload)) + " bytes...");

  // Configuração do modulo receptor
  ResponseStructContainer c = e22.getConfiguration();
  if (c.status.code == E22_SUCCESS) {
    Configuration config = *(Configuration*) c.data;

    // Define o endereco Receptor
    config.ADDH = MY_ADDH;
    config.ADDL = MY_ADDL;
    
    //Configure o modo de transmissao para FIXO
    config.TRANSMISSION_MODE.fixedTransmission = FT_FIXED_TRANSMISSION;
    
    // Habilita o RSSI no RECEPTOR    
    config.TRANSMISSION_MODE.enableRSSI = RSSI_ENABLED;

    ResponseStatus rs = e22.setConfiguration(config, WRITE_CFG_PWR_DWN_SAVE);
    if (rs.code == E22_SUCCESS) {
      Serial.println("Modulo LoRa RECEPTOR configurado com sucesso.");
      Serial.print(MY_ADDH, HEX);
      Serial.print(MY_ADDL, HEX);
      Serial.println(", Fixed Transmission e RSSI");    
    } else {
      Serial.println("Erro ao configurar modulo: " + rs.getResponseDescription());
    }
    c.close();
  } else {
    Serial.print("Erro ao ler configuração inicial do RECEPTOR: ");
    Serial.println(c.status.getResponseDescription());
  }

  controle.begin("controle", false);
  int numTransm = controle.getInt("prox", 0);
  nameSpc = "transmissao" + String(numTransm);
  controle.putInt("prox", numTransm + 1);
  controle.end();

  transmissao.begin(nameSpc.c_str(), false);
  Serial.println("Receptor iniciado. Namespace: " + nameSpc);
}

void loop() {
  if (e22.available() > 0) {
    Serial.println("\n[RECEPTOR] Dados disponíveis!");
    
    // Tente receber com o tamanho correto
    ResponseStructContainer rsc = e22.receiveMessageRSSI(sizeof(CompactPayload));
    
    Serial.print("[RECEPTOR] sizeof(CompactPayload) = ");
    Serial.println(sizeof(CompactPayload));
    
    Serial.print("[RECEPTOR] Status code: ");
    Serial.println(rsc.status.code);
    
    Serial.print("[RECEPTOR] Status description: ");
    Serial.println(rsc.status.getResponseDescription());

    if (rsc.status.code == E22_SUCCESS) {
      CompactPayload* pacote = (CompactPayload*) rsc.data;
      
      // Seu código de exibição...
      Serial.println("--------------------------------");
      Serial.println("Novo Pacote Recebido!");
      Serial.print("  Timestamp: ");      Serial.println(pacote->time, 2);
      Serial.print("  Latitude: ");       Serial.println(pacote->lat, 6);
      Serial.print("  Longitude: ");      Serial.println(pacote->lon, 6);
      Serial.print("  Altitude: ");       Serial.println(pacote->alt, 2);
      Serial.print("  V. Down: ");        Serial.println(pacote->veld, 2);
      Serial.print("  V. North: ");       Serial.println(pacote->veln, 2);
      Serial.print("  V. East: ");        Serial.println(pacote->vele, 2);
      Serial.print("  Yaw: ");            Serial.println(pacote->yaw, 2);
      Serial.print("  Pitch: ");          Serial.println(pacote->pitch, 2);
      Serial.print("  Roll: ");           Serial.println(pacote->roll, 2);
      Serial.print("  Barometer: ");      Serial.println(pacote->bar_out, 2);
      Serial.print("  Fixed: ");          Serial.println(pacote->gnss_fix);
      Serial.print("  Parachute Open: "); Serial.println(pacote->parachute ? "SIM" : "NAO");

      Serial.print("  RSSI: ");
      int rssiVal = (int)rsc.rssi;
      if (rssiVal > 127) rssiVal -= 256;
      Serial.print(rssiVal);              Serial.println(" dBm");

      Serial.println("--------------------------------");

      // Salvando no Preferences
      String chave = "pacote" + String(contador);
      String valor = String(pacote->time, 2) + ";" +
                     String(pacote->lat, 6) + ";" +
                     String(pacote->lon, 6) + ";" +
                     String(pacote->alt, 2) + ";" +
                     String(pacote->veld, 2) + ";" +
                     String(pacote->veln, 2) + ";" +
                     String(pacote->vele, 2) + ";" +
                     String(pacote->yaw, 2) + ";" +
                     String(pacote->pitch, 2) + ";" +
                     String(pacote->roll, 2) + ";" +
                     String(pacote->bar_out, 2) + ";" +
                     String(pacote->gnss_fix) + ";" +
                     String(pacote->parachute);
      transmissao.putString(chave.c_str(), valor);

      contador++;

    } else {
      Serial.print("ERRO: ");
      Serial.println(rsc.status.getResponseDescription());
    }
    rsc.close();
  }

  // Comandos via Serial
if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();

    if (comando == "listar") {
      list();
    } else if (comando.startsWith("transmissao")) {
      read(comando);
    } else if (comando == "limpar") {
      limparMemoria();
    } else {
      Serial.println("Comando invalido. Use 'listar' ou 'transmissaoX'.");
    }
  }
}

void read(const String& nome) {
  if (leitor.begin(nome.c_str(), true)) {
    Serial.println("Lendo dados de: " + nome);

    for (int i = 0; i < 10000; i++) {
      String chave = "pacote" + String(i);
      String valor = leitor.getString(chave.c_str(), "");
      if (valor == "") break;
      Serial.println("  " + chave + ": " + valor);
    }
    leitor.end();
  } 
  else {
    Serial.println("Namespace nao encontrado.");
  }
}

void list() {
  controle.begin("controle", true);
  int total = controle.getInt("prox", 0);
  controle.end();

  Serial.println("Namespaces disponiveis:");
  for (int i = 0; i < total; i++) {
    Serial.println("  transmissao" + String(i));
  }
}

void limparMemoria() {
  Serial.println("Apagando toda a memória NVS (Preferences)...");
  delay(500);
  esp_err_t status = nvs_flash_erase(); // Limpa a NVS inteira
  if (status == ESP_OK) {
    Serial.println("Memória NVS apagada com sucesso!");
  } else {
    Serial.print("Erro ao apagar NVS: ");
    Serial.println(status);
  }
  Serial.println("Reiniciando ESP...");
  delay(2000);
  ESP.restart(); // Reinicia o ESP
}