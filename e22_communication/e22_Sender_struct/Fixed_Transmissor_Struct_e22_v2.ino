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

//Endereco do Transmissor
#define MY_ADDH 0x00
#define MY_ADDL 0x01

//Endereco e Canal de DESTINO
#define DEST_ADDH 0x00
#define DEST_ADDL 0x0C
#define DEST_CHAN 0x17

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
  float bar;
  bool parachute;
} CompactPayload;


void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  e22.begin();
  delay(500);

  // Configuração do módulo LoRa
  ResponseStructContainer c = e22.getConfiguration();
  if (c.status.code == E22_SUCCESS) {
    Configuration configuration = *(Configuration*) c.data;

    // Define o endereço Transmissor
    configuration.ADDH = MY_ADDH;
    configuration.ADDL = MY_ADDL;

    // Define o modo de transmissão para FIXO
    configuration.TRANSMISSION_MODE.fixedTransmission = FT_FIXED_TRANSMISSION;
    
    // Salva a configuração
    ResponseStatus rs = e22.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
    if (rs.code == E22_SUCCESS) {
      Serial.println("Modulo LoRa configurado para Fixed Transmission.");
      Serial.print("Endereco: ");
      Serial.print(MY_ADDH, HEX);
      Serial.println(MY_ADDL, HEX);
    } else {
      Serial.print("Erro ao configurar Fixed Transmission: ");
      Serial.println(rs.getResponseDescription());
    }
    c.close();
  } else {
    Serial.print("Erro ao ler configuracao do modulo: ");
    Serial.println(c.status.getResponseDescription());
  }

  controle.begin("controle", false);
  int numTransm = controle.getInt("prox", 0);
  nameSpc = "LoraTX" + String(numTransm);
  controle.putInt("prox", numTransm + 1);
  controle.end();

  preferencias.begin(nameSpc.c_str(), false);
  Serial.println("Transmissor iniciado no namespace: " + nameSpc);
}

void loop() {
  CompactPayload pacote;
  pacote.time = contador;
  pacote.lat = contador;
  pacote.lon = contador;
  pacote.alt = contador;
  pacote.veld = contador;
  pacote.veln = contador;
  pacote.vele = contador;
  pacote.yaw = contador;
  pacote.pitch = contador;
  pacote.roll = contador;
  pacote.bar = contador;
  pacote.parachute = contador;

  ResponseStatus rs = e22.sendFixedMessage(DEST_ADDH, DEST_ADDL, DEST_CHAN, (uint8_t*)&pacote, sizeof(CompactPayload));
  
  if (rs.code == E22_SUCCESS) { // [cite: 16]
        salvarDados(pacote); // [cite: 16]
        Serial.print("Pacote #");
        Serial.print(contador);
        Serial.print(" enviado para Endereço "); // [cite: 17]
        Serial.print(DEST_ADDH, HEX); // [cite: 18]
        Serial.print(DEST_ADDL, HEX); // [cite: 18]
        Serial.print(", Canal "); // [cite: 18]
        Serial.println(DEST_CHAN, HEX); // [cite: 18]
    } else {
        Serial.print("Erro ao enviar: " + rs.getResponseDescription()); // [cite: 18]
  }

  contador++;
  delay(1000);

  // Comandos via Serial
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();

    if (comando == "listar") {
      list();
    } else if (comando.startsWith("LoraTX")) {
      read(comando);
    } else if (comando == "limpar") {
      limparMemoria();
    } else {
      Serial.println("Comando invalido. Use 'listar' ou 'LoRaTXx'.");
    }
  }
}


void salvarDados(const CompactPayload& pacote) {
  String chave = "pacote" + String(contador);
  String valor = String(pacote.time, 2) + ";" +
                 String(pacote.lat, 6) + ";" +
                 String(pacote.lon, 6) + ";" +
                 String(pacote.alt, 2) + ";" +
                 String(pacote.veld, 2) + ";" +
                 String(pacote.veln, 2) + ";" +
                 String(pacote.vele, 2) + ";" +
                 String(pacote.yaw, 2) + ";" +
                 String(pacote.pitch, 2) + ";" +
                 String(pacote.roll, 2) + ";" +
                 String(pacote.bar, 2) + ";" +
                 String(pacote.parachute);
  preferencias.putString(chave.c_str(), valor);
}


// Lista todos os namespaces existentes
void list() {
  controle.begin("controle", true);
  int total = controle.getInt("prox", 0);
  controle.end();

  Serial.println("Namespaces disponiveis:");
  for (int i = 0; i < total; i++) {
    Serial.println("  LrRaTX" + String(i));
  }
}

// Le os dados salvos dentro de um namespace específico
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
    Serial.println("Namespace nao encontrado.");
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