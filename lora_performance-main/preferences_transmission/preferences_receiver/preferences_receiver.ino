#include "LoRa_E220.h"
#include "Arduino.h"
#include <Preferences.h>

#define RX_PIN 16
#define TX_PIN 17
LoRa_E220 e220(&Serial2, 19, 21, 15); // M0, M1, AUX

Preferences preferencias;

uint16_t ultimo_contador = 0;
bool primeiro_pacote = true;
int pacote_index = 0; // Sempre inicia em 0

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);  
  e220.begin();

  preferencias.begin("loraDados", false);  // Namespace

  // Limpa os dados anteriores e reinicia o índice
  preferencias.clear(); // ⚠️ CUIDADO: apaga todos os dados no namespace!
  pacote_index = 0;

  Serial.println("Receptor iniciado, memória limpa e pronto para nova amostra");
}

void loop() {
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    if (comando == "listar") {
      lerPacotesSalvos();
    }
  }

  if (e220.available() > 0) {
    ResponseContainer rc = e220.receiveMessage();
    if (rc.status.code == 1) {
      String mensagem = rc.data;

      salvarDados(mensagem); // Salva no índice atual

      // Detecção de perda
      int pos = mensagem.lastIndexOf(' ');
      uint16_t contador_recebido = mensagem.substring(pos + 1).toInt();

      if (primeiro_pacote) {
        ultimo_contador = contador_recebido;
        primeiro_pacote = false;
      } else {
        if (contador_recebido != ultimo_contador + 1) {
          String perda = "Pacote(s) perdido(s) entre " + String(ultimo_contador) + " e " + String(contador_recebido);
          Serial.println(perda);
          salvarDados(perda);  // Salva info da perda também
        }
        ultimo_contador = contador_recebido;
      }
    } else {
      Serial.println("Erro ao receber: " + rc.status.getResponseDescription());
    }
  }
}

void salvarDados(const String& dados) {
  String chave = "recv" + String(pacote_index);
  preferencias.putString(chave.c_str(), dados);
  pacote_index++;
}

void lerPacotesSalvos() {
  Serial.println("[DEBUG] Função listar chamada");
  Serial.println("Conteúdo salvo no Preferences:");
  for (int i = 0; i < pacote_index; i++) {
    String chave = "recv" + String(i);
    String valor = preferencias.getString(chave.c_str(), "Nenhum dado");
    Serial.println(chave + ": " + valor);
  }
}
