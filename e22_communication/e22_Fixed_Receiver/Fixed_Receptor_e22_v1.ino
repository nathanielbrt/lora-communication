#include "LoRa_E22.h"
#include "Arduino.h"
#include <Preferences.h>

#define RX_PIN 16
#define TX_PIN 17
LoRa_E22 e22(&Serial2, 15, 21, 19); // AUX, M0, M1

Preferences controle;
Preferences transmissao;
Preferences leitor;

//contador da quantidade de pacotes recebidos
int contador = 0; // Contador para pacotes recebidos
String nameSpc;

// --- Definições para o ENDEREÇO PRÓPRIO deste RECEPTOR ---
#define MY_ADDH 0x00
#define MY_ADDL 0x0C 

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);  
  e22.begin();

  delay(500); // Aumentado o delay para dar tempo ao módulo de inicializar

  ResponseStructContainer c = e22.getConfiguration();
  if (c.status.code == E22_SUCCESS) {
    Configuration configuration = *(Configuration*) c.data;

    // Defina o endereço PRÓPRIO do módulo Receptor
    configuration.ADDH = MY_ADDH;
    configuration.ADDL = MY_ADDL;

    // Configure o modo de transmissão para FIXO (para filtragem mais estrita e simetria)
    configuration.TRANSMISSION_MODE.fixedTransmission = FT_FIXED_TRANSMISSION;

    // Habilita o RSSI nas configurações do módulo RECEPTOR
    configuration.TRANSMISSION_MODE.enableRSSI = RSSI_ENABLED;

    // Salvar a configuração de forma permanente
    ResponseStatus rs = e22.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
    if (rs.code == E22_SUCCESS) {
      Serial.print("Módulo LoRa RECEPTOR configurado com Endereço ");
      Serial.print(MY_ADDH, HEX); Serial.print(MY_ADDL, HEX);
      Serial.println(" e Fixed Transmission e RSSI (Permanente).");
    } else {
      Serial.print("Erro ao configurar Fixed Transmission/RSSI no RECEPTOR: ");
      Serial.println(rs.getResponseDescription());
    }
    c.close();
  } else {
    Serial.print("Erro ao ler configuração inicial do módulo RECEPTOR: ");
    Serial.println(c.status.getResponseDescription());
  }

  //controle do numero de transmissoes
  controle.begin("controle", false); // Abre o namespace 'controle' em modo de escrita
  int numTransm = controle.getInt("prox", 0); // Lê o valor atual de 'prox'
  Serial.print("DEBUG Setup: numTransm lido ANTES do incremento: "); // DEBUG PRINT
  Serial.println(numTransm); // DEBUG PRINT
  nameSpc = "transmissao" + String(numTransm); // Define o nome do namespace atual (e.g., "transmissao2")
  controle.putInt("prox", numTransm + 1); // Incrementa e tenta salvar o novo valor de 'prox' (e.g., 3)
  delay(10); // Pequeno delay para garantir que putInt seja processado antes do end()
  controle.end(); // Fecha o namespace 'controle', gravando as mudanças

  Serial.print("Namespace atual: ");
  Serial.println(nameSpc);
  transmissao.begin(nameSpc.c_str(), false); // Abre o namespace 'transmissaoX' para salvar mensagens
}

void loop() {
  if (e22.available() > 1) { // Verifica se há dados disponíveis para leitura
    ResponseContainer rc = e22.receiveMessageRSSI();
    if (rc.status.code == E22_SUCCESS) {
      String msg = rc.data;

      String chave = "pacote" + String(contador);
      transmissao.putString(chave.c_str(), msg);

      Serial.print("Salvou: ");
      Serial.print(chave);
      Serial.print(" = ");
      Serial.print(msg);
      
      // --- CORREÇÃO: Imprime o valor do RSSI de forma mais robusta ---
      // Converte o byte RSSI para um signed char para obter valores negativos corretos.
      // A documentação sugere que RSSI é tipicamente um valor negativo em dBm.
      // O módulo pode retornar um valor de 0 a 255.
      // Se 0 dBm está sendo impresso, é provável que o byte 0x00 esteja sendo lido como RSSI.
      // Vamos tentar um cast para (signed char) ou (int8_t)

      Serial.print(" RSSI: ");
      // Alguns módulos Ebyte retornam RSSI como 256 - abs(dBm). Vamos verificar isso.
      // Ou pode ser um byte bruto que, quando interpretado como signed, é negativo.
      int rssi_val = (int)rc.rssi;
      if (rssi_val > 127) { // Se o byte for > 127, é provavelmente um número negativo em complemento de dois
          rssi_val = rssi_val - 256; // Converte para o valor negativo correto
      }
      Serial.print(rssi_val); // Imprime o byte RSSI como um inteiro
      Serial.println(" dBm"); 
      
      contador++;
    } else {
      Serial.print("Erro ao receber mensagem: ");
      Serial.println(rc.status.getResponseDescription());
    }
  }

  // comandos via Serial
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();

    if (comando == "listar") {
      list(); // Chama a função list() quando 'listar' é digitado
    } else if (comando.startsWith("transmissao")) {
      read(comando);
    } else {
      Serial.println("Comando inválido. Use 'listar' ou 'transmissaoX'.");
    }
  }
}

void read(const String& nome) {
  if (leitor.begin(nome.c_str(), true)) { // Abre o namespace em modo de leitura
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

    leitor.end(); // Fecha o namespace
  } else {
    Serial.println("Namespace não encontrado ou inválido.");
  }
}

void list() {
  controle.begin("controle", true); // Abre o namespace 'controle' em modo de leitura
  int totalTransmissoes = controle.getInt("prox", 0); // Lê o valor atual de 'prox'
  Serial.print("DEBUG List: totalTransmissoes lido na funcao list(): "); // DEBUG PRINT
  Serial.println(totalTransmissoes); // DEBUG PRINT
  controle.end(); // Fecha o namespace 'controle'

  Serial.println("Namespaces salvos:");
  for (int i = 0; i < totalTransmissoes; i++) {
    Serial.print("  transmissao");
    Serial.println(i);
  }
}