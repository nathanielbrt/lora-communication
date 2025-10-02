#include <Preferences.h>
#include "Arduino.h"
#include "LoRa_E22.h"
#include <nvs_flash.h>

#define RX_PIN 16
#define TX_PIN 17
LoRa_E22 e22(&Serial2, 15, 21, 19); // AUX, M0, M1

Preferences preferencias;
Preferences controle;
Preferences leitor;

uint16_t contador = 0;
String nameSpc;

// Endereço do Transmissor
#define MY_ADDH 0x00
#define MY_ADDL 0x01

// Endereço e Canal de DESTINO (o broker)
#define DEST_ADDH 0x00
#define DEST_ADDL 0x02

#define BROKER_ADDRESS 0x0002
#define LORA_CHANNEL 0x12

// Enum para tipos de pacote
enum PacketType : uint8_t {
  CONNECT = 0x01,
  CONNACK = 0x02,
  DATA    = 0x03
};

// Pacotes handshake MQTT-like - AGORA COM PADDING DE 5 BYTES
struct ConnectPacket {
  uint8_t type = CONNECT;
  uint16_t client_id;
  uint16_t keep_alive;
  byte padding[5]; // Adiciona 5 bytes para totalizar 10
} __attribute__((packed));

struct ConnAckPacket {
  uint8_t type = CONNACK;
  uint8_t return_code;
} __attribute__((packed));

// Pacote de dados enviado após o handshake
struct DataPacket {
  uint8_t type;         
  uint16_t id;
  uint8_t tipo;
  char nome[6];
} __attribute__((packed));

bool connected = false;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  e22.begin();
  delay(500);

  // Configuração do módulo LoRa
  ResponseStructContainer c = e22.getConfiguration();
  if (c.status.code == E22_SUCCESS) {
    Configuration configuration = *(Configuration*) c.data;

    configuration.ADDH = MY_ADDH;
    configuration.ADDL = MY_ADDL;
    configuration.TRANSMISSION_MODE.fixedTransmission = FT_FIXED_TRANSMISSION;

    ResponseStatus rs = e22.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
    if (rs.code == E22_SUCCESS) {
      Serial.println("Modulo LoRa configurado para Fixed Transmission.");
      Serial.print("Endereco: ");
      Serial.print(MY_ADDH, HEX);
      Serial.println(MY_ADDL, HEX);
    } else {
      Serial.print("Erro ao configurar: ");
      Serial.println(rs.getResponseDescription());
    }
    c.close();
  }

  controle.begin("controle", false);
  int numTransm = controle.getInt("prox", 0);
  nameSpc = "LoRaTX" + String(numTransm);
  controle.putInt("prox", numTransm + 1);
  controle.end();

  preferencias.begin(nameSpc.c_str(), false);
  Serial.println("Transmissor iniciado no namespace: " + nameSpc);
}

void loop() {
  if (!connected) {
    enviarConnect();

    unsigned long start_time = millis();
    bool connackReceived = false;

    while (millis() - start_time < 2000) {
      if (e22.available() > 0) {
        ResponseStructContainer rsc = e22.receiveMessage(sizeof(ConnAckPacket));
        if (rsc.status.code == E22_SUCCESS) {
          ConnAckPacket connAck;
          memcpy(&connAck, rsc.data, sizeof(connAck));
          if (connAck.type == CONNACK && connAck.return_code == 0) {
            Serial.println("Conexão estabelecida com o receptor.");
            connected = true;
            connackReceived = true;
            rsc.close();
            delay(100);
            break;
          }
        }
        rsc.close();
      }
    }

    if (!connackReceived) {
      Serial.println("Timeout: CONNACK não recebido, tentando novamente...");
      delay(1000);
      return;
    }
  } else {
    DataPacket pacote;
    pacote.type = DATA;
    pacote.id = contador;
    strncpy(pacote.nome, "TESTE", sizeof(pacote.nome));
    pacote.nome[sizeof(pacote.nome) - 1] = '\0';

    ResponseStatus rs = e22.sendFixedMessage(DEST_ADDH, DEST_ADDL, LORA_CHANNEL, &pacote, sizeof(DataPacket));

    if (rs.code == E22_SUCCESS) {
      salvarDados(pacote);
      Serial.println("Enviado: Pacote " + String(contador));
    } else {
      Serial.println("Erro ao enviar: " + rs.getResponseDescription());
    }

    contador++;
    delay(1000);
  }

  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();

    if (comando == "listar") {
      list();
    } else if (comando.startsWith("LoRaTX")) {
      read(comando);
    } else if (comando == "limpar") {
      limparMemoria();
    } else {
      Serial.println("Comando invalido.");
    }
  }
}

void enviarConnect() {
  ConnectPacket connect = {CONNECT, 0x0001, 60, {0,0,0,0,0}}; // Inicializa padding
  ResponseStatus rs = e22.sendFixedMessage(highByte(BROKER_ADDRESS), lowByte(BROKER_ADDRESS), LORA_CHANNEL, &connect, sizeof(connect));
  if (rs.code == E22_SUCCESS) {
    Serial.println("CONNECT enviado.");
  } else {
    Serial.println("Erro ao enviar CONNECT.");
  }
}

void salvarDados(const DataPacket& pacote) {
  String chave = "pacote" + String(pacote.id);
  String valor = "|id:" + String(pacote.id) +
                 "|tipo:" + String(pacote.tipo) +
                 "|dados:" + String(pacote.nome);
  preferencias.putString(chave.c_str(), valor);
}

void list() {
  controle.begin("controle", true);
  int total = controle.getInt("prox", 0);
  controle.end();

  Serial.println("Namespaces disponíveis:");
  for (int i = 0; i < total; i++) {
    Serial.println("  LoRaTX" + String(i));
  }
}

void read(const String& nome) {
  if (leitor.begin(nome.c_str(), true)) {
    Serial.println("Lendo pacotes de: " + nome);
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
