#include <Preferences.h>
#include "Arduino.h"
#include "LoRa_E22.h"

#define RX_PIN 16
#define TX_PIN 17
LoRa_E22 e22(&Serial2, 15, 21, 19); // AUX, M0, M1

Preferences preferencias;

uint16_t contador = 0; // Contador para sequencia de pacotes

//Endereco do transmissor
#define MY_ADDH 0x00
#define MY_ADDL 0x01

//Endereco e Canal do receptor
#define DEST_ADDH 0x00
#define DEST_ADDL 0x0C
#define DEST_CHAN 0x23 

void setup() {
  Serial.begin(115200); // Para depuração básica
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN); // Inicializa Serial2 com pinos personalizados
  e22.begin();

  delay(2000);

  // Definir o modulo em modo de transmissao fixa permanente
  // ler a configuracao atual.
  ResponseStructContainer c = e22.getConfiguration();
  if (c.status.code == E22_SUCCESS) {
    Configuration configuration = *(Configuration*) c.data;

    // Defina o endereço PRÓPRIO do módulo Transmissor
    configuration.ADDH = MY_ADDH;
    configuration.ADDL = MY_ADDL;

    // Defina o modo de transmissão para FIXO
    configuration.TRANSMISSION_MODE.fixedTransmission = FT_FIXED_TRANSMISSION; // 

    // Opcional: Manter o endereço e o canal do próprio módulo em 0x0000 e 0x12 (padrão de fábrica)
    // Se o seu módulo está com configurações de fábrica, ADDH, ADDL e CHAN já serão 0x00 e 0x12.
    // configuration.ADDH = 0x00;
    // configuration.ADDL = 0x00;
    // configuration.CHAN = 0x12; // 

    // Salvar a configuração de forma permanente (WRITE_CFG_PWR_DWN_SAVE)
    ResponseStatus rs = e22.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
    if (rs.code == E22_SUCCESS) {
      Serial.println("Módulo LoRa configurado para Fixed Transmission (Permanente).");
      Serial.print(MY_ADDH, HEX);
      Serial.print(MY_ADDL, HEX);
      Serial.println(" e Fixed Transmission (Permanente).");
    } else {
      Serial.print("Erro ao configurar Fixed Transmission: ");
      Serial.println(rs.getResponseDescription());
    }
    c.close(); // Liberar a memória alocada pela configuração
  } else {
    Serial.print("Erro ao ler configuração do módulo: ");
    Serial.println(c.status.getResponseDescription());
  }

  // Inicializa a memória Preferences
  preferencias.begin("loraDados", false); // Namespace "loraDados", modo read/write
  Serial.println("Transmissor iniciado e Preferences montado");
}

void loop() {
  // Cria a mensagem com contador
  String mensagem = "NATHANIEL" + String(contador);

  // Envia o pacote no modo de Transmissão Fixa

  ResponseStatus rs = e22.sendFixedMessage(DEST_ADDH, DEST_ADDL, DEST_CHAN, mensagem);
  
  if (rs.code == E22_SUCCESS) { // Sucesso no envio
    salvarDados(mensagem); // Salvar a mensagem que foi enviada
    Serial.println("Enviado (FIXED): Seq " + String(contador) + " para Endereço " + String(DEST_ADDH, HEX) + String(DEST_ADDL, HEX) + ", Canal " + String(DEST_CHAN, HEX)); 
  } else {
    Serial.println("Erro ao enviar (FIXED): " + rs.getResponseDescription());
  }

  contador++; // Incrementa o contador
  delay(1000); // Intervalo entre envios (ajuste conforme necessário)
}

void salvarDados(const String& dados) {
  String chave = "sent" + String(contador);
  preferencias.putString(chave.c_str(), dados); // Salva como chave-valor
}