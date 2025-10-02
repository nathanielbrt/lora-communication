#include <Preferences.h>
#include "Arduino.h"
#include "LoRa_E22.h"

#define RX_PIN 16
#define TX_PIN 17
LoRa_E22 e22(&Serial2, 15, 19, 21); // AUX, M0, M1

Preferences controle;
Preferences transmissao;
Preferences leitor;

//contador da quantidade de pacotes recebidos
int contador = 0;
String nameSpc;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);  
  e22.begin();

  //controle do numero de transmissoes
  controle.begin("controle", false);
  int numTransm = controle.getInt("prox", 0);
  nameSpc = "transmissao" + String(numTransm);
  controle.putInt("prox", numTransm + 1);
  controle.end();

  Serial.print("Namespace atual: ");
  Serial.println(nameSpc);

  transmissao.begin(nameSpc.c_str(), false);
}

void loop() {
  if (e22.available() > 1) {
    ResponseContainer rc = e22.receiveMessage();

    if (rc.status.code == 1) {
      String msg = rc.data;
      String chave = "pacote" + String(contador);

      //Serial.print("Status Code: "); // Adicionado para debug
      //Serial.println(rc.status.code); // Adicionado para debug

      transmissao.putString(chave.c_str(), msg);

      Serial.print("Salvou: ");
      Serial.print(chave);
      Serial.print(" = ");
      Serial.println(msg);

      contador++;
    }
  }

  //comandos via Serial
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();

    if (comando == "listar") {
      list();
    } else if (comando.startsWith("transmissao")) {
      read(comando);
    } else {
      Serial.println("Comando inválido. Use 'listar' ou 'transmissaoX'.");
    }
  }
}

void read(const String& nome) {
  if (leitor.begin(nome.c_str(), true)) {
    Serial.print("Lendo mensagens de: ");
    Serial.println(nome);

    for (int i = 0; i < 10000; i++) {
      String chave = "pacote" + String(i);
      String valor = leitor.getString(chave.c_str(), "");
      if (valor == "") break;

      Serial.print("  ");
      Serial.print(chave);
      Serial.print(": ");
      Serial.println(valor);
    }

    leitor.end();
  } else {
    Serial.println("Namespace não encontrado ou inválido.");
  }
}

void list() {
  controle.begin("controle", true);
  int totalTransmissoes = controle.getInt("prox", 0);
  controle.end();

  Serial.println("Namespaces salvos:");
  for (int i = 0; i < totalTransmissoes; i++) {
    Serial.print("transmissao");
    Serial.println(i);
  }
}