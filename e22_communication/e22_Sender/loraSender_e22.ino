#include "LoRa_E22.h"
#include "Arduino.h"
#include <Preferences.h>

#define RX_PIN 16
#define TX_PIN 17
LoRa_E22 e22(&Serial2, 15, 19, 21); // AUX, M0, M1 (Corrigido de Serial12 para Serial2)
Preferences preferencias;

uint16_t contador = 0; // Contador para sequência de pacotes

void setup() {
  Serial.begin(115200); // Para depuração básica
//  mySerial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN); // Inicializa Serial2 com pinos personalizados
  e22.begin();

  // Inicializa a memória Preferences
  preferencias.begin("loraDados", false); // Namespace "loraDados", modo read/write
  Serial.println("Transmissor iniciado e Preferences montado");
}

void loop() {
  // Cria a mensagem com contador
  String mensagem = "teste" + String(contador);

  // Envia o pacote
  ResponseStatus rs = e22.sendMessage(mensagem);
  if (rs.code == 1) { // Sucesso no envio
  
    //debug
    //Serial.print("Código: ");
    //Serial.println(rs.code);
    //Serial.print("Descrição: ");
    //Serial.println(rs.getResponseDescription());

    salvarDados(mensagem);
    Serial.println("Enviado: Seq " + String(contador)); 
    
  } else {
    Serial.println("Erro ao enviar: " + rs.getResponseDescription());
  }

  contador++; // Incrementa o contador
  delay(2000); // Intervalo entre envios (ajuste conforme necessário)
}

void salvarDados(const String& dados) {
  String chave = "sent" + String(contador);
  preferencias.putString(chave.c_str(), dados); // Salva como chave-valor
}