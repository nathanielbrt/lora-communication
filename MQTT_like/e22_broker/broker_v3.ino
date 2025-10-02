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

// ENDEREÇO deste RECEPTOR
#define MY_ADDH 0x00
#define MY_ADDL 0x02

#define CLIENT_ADDRESS 0x0001
#define LORA_CHANNEL 0x12

// Enum para os tipos de pacotes (IDÊNTICO AO CLIENT)
enum PacketType : uint8_t {
  CONNECT = 0x01,
  CONNACK = 0x02,
  DATA    = 0x03
};

// Pacotes handshake (IDÊNTICO AO CLIENT) - AGORA COM PADDING
struct ConnectPacket {
  uint8_t type = CONNECT;
  uint16_t client_id;
  uint16_t keep_alive;
  uint8_t size = 10;
} __attribute__((packed));

struct ConnAckPacket {
  uint8_t type = CONNACK;
  uint8_t return_code;
} __attribute__((packed));

// Pacote de dados (pós-handshake) (IDÊNTICO AO CLIENT)
struct DataPacket {
  uint8_t type;         
  uint16_t id;
  uint8_t tipo;
  char nome[6];
} __attribute__((packed));

// Tamanho máximo do pacote que o broker espera receber (ambos ConnectPacket e DataPacket terão 10 bytes)
const int PACKET_SIZE = sizeof(ConnectPacket); 
const int MAX_BROKER_PACKET_SIZE = sizeof(DataPacket); 

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  e22.begin();
  delay(500);

  // Configuração do módulo receptor
  ResponseStructContainer c = e22.getConfiguration();
  if (c.status.code == E22_SUCCESS) {
    Configuration config = *(Configuration*) c.data;

    config.ADDH = MY_ADDH;
    config.ADDL = MY_ADDL;
    config.TRANSMISSION_MODE.fixedTransmission = FT_FIXED_TRANSMISSION;
    config.TRANSMISSION_MODE.enableRSSI = RSSI_ENABLED;

    ResponseStatus rs = e22.setConfiguration(config, WRITE_CFG_PWR_DWN_SAVE);
    if (rs.code == E22_SUCCESS) {
      Serial.println("Modulo LoRa RECEPTOR configurado com sucesso.");
      Serial.print(MY_ADDH, HEX);
      Serial.print(MY_ADDL, HEX);
      Serial.println(", Fixed Transmission e RSSI");    
    } else {
      Serial.println("Erro ao configurar módulo: " + rs.getResponseDescription());
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
// ResponseStructContainer open = e22.receiveMessageRSSI(PACKET_SIZE);

void loop() {
  if (e22.available() > 0) {
    // Tenta ler o tamanho padronizado do pacote, incluindo o RSSI.
    ResponseStructContainer rsc = e22.receiveMessageRSSI(MAX_BROKER_PACKET_SIZE);

    if (rsc.status.code == E22_SUCCESS && rsc.data != nullptr) {
      uint8_t packet_type = *(uint8_t*)rsc.data;
      int rssiVal = (int)rsc.rssi;
      if (rssiVal > 127) rssiVal -= 256;

      switch (packet_type) {
        case CONNECT: {
          ConnectPacket connect;
          memcpy(&connect, rsc.data, sizeof(connect));
          Serial.print("Recebido CONNECT, client_id=");
          Serial.println(connect.client_id);

          ConnAckPacket connAck = {CONNACK, 0};
          ResponseStatus rs = e22.sendFixedMessage(highByte(connect.client_id), lowByte(connect.client_id), LORA_CHANNEL, &connAck, sizeof(connAck));
          delay(200);

          if (rs.code == E22_SUCCESS) {
            Serial.println("CONNACK enviado."); 
          } else {
            Serial.println("Erro ao enviar CONNACK: " + rs.getResponseDescription());
          }
          break;
        }
        case DATA: {
          DataPacket pacote;
          memcpy(&pacote, rsc.data, sizeof(DataPacket));

          Serial.print("PACOTE " + String(contador));
          Serial.print(" | ID: ");
          Serial.print(pacote.id);
          Serial.print(" | Nome: ");
          Serial.print(pacote.nome);
          Serial.print(" | RSSI: ");
          Serial.print(rssiVal);
          Serial.println(" dBm");

          String chave = "pacote " + String(contador);
          String valor = "ID :" + String(pacote.id) + "; NOME: " + String(pacote.nome) + "; RSSI: " + String(rssiVal);
          transmissao.putString(chave.c_str(), valor);
          contador++;
          break;
        }
        default: {
          Serial.print("Recebido pacote de tipo desconhecido: ");
          Serial.println(packet_type, HEX);
          Serial.println("Tamanho do buffer lido: " + String(MAX_BROKER_PACKET_SIZE));
          break;
        }
      }
      rsc.close();
    } else {
      Serial.println("Erro ao receber pacote LoRa: " + rsc.status.getResponseDescription());
    }
  }

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
      Serial.println("Comando inválido. Use 'listar' ou 'transmissaoX' ou 'limpar'.");
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
  } else {
    Serial.println("Namespace não encontrado.");
  }
}

void list() {
  controle.begin("controle", true);
  int total = controle.getInt("prox", 0);
  controle.end();

  Serial.println("Namespaces disponíveis:");
  for (int i = 0; i < total; i++) {
    Serial.print("  transmissao" + String(i));
    Serial.println(i);
  }
}

void limparMemoria() {
  Serial.println("Apagando a memória NVS...");
  delay(500);
  esp_err_t status = nvs_flash_erase();
  if (status == ESP_OK) {
    Serial.println("Memória NVS apagada com sucesso!");
  } else {
    Serial.print("Erro ao apagar NVS: ");
    Serial.println(status);
  }
  Serial.println("Reiniciando ESP...");
  delay(2000);
  ESP.restart();
}
