#define BLYNK_TEMPLATE_ID "TMPL2POjxEM79"
#define BLYNK_TEMPLATE_NAME "Estufa inteligente"
#define BLYNK_AUTH_TOKEN "CZMxxf6gMncDfxlJSCz2esLFx-yxdyhC" 

//==============================================================================================
// Bibliotecas
//==============================================================================================
#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

//==============================================================================================
// Definições de Pinos
//==============================================================================================
// Saídas de sinalização (LEDs, relés, etc.)
#define PIN_GAS_OUT 18
#define PIN_TEMP_OUT 19
#define PIN_UMISOLO_OUT 21
#define PIN_UMIAR_OUT 22

// Pinos de entrada dos sensores
#define PIN_HIGROMETRO 34
#define PIN_DHT22 4
#define PIN_MQ135 12

#define VIRTUAL_PIN_LED_TEMP V9       // Definição do pino virtual para blynk
#define VIRTUAL_PIN_LED_UMIAR V10     // Definição do pino virtual para blynk
#define VIRTUAL_PIN_LED_GAS V11       // Definição do pino virtual para blynk
#define VIRTUAL_PIN_LED_UMISOLO V12   // Definição do pino virtual para blynk

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Wokwi-GUEST"; // Seu SSID do Wi-Fi
char pass[] = "";           // Sua senha do Wi-Fi

BlynkTimer timer;

//==============================================================================================
// Configurações e Limites dos Sensores
//==============================================================================================
// Configuração do DHT22
#define DHTTYPE DHT22 // Define o tipo de sensor DHT (DHT22 ou DHT11)

// Parâmetros de calibração do MQ135 (para CO2)
#define MQ135_PARAM_A 116.6020682
#define MQ135_PARAM_B 2.769034857
#define MQ135_RL 10.0 // Resistência de carga em KΩ

// Limites para acionamento das saídas
const float LIMIT_GAS_PPM = 750.0;
const float LIMIT_TEMP_CELSIUS = 30.0;
const float LIMIT_UMIAR_PERCENT = 55.0;
const float LIMIT_UMISOLO_MIN_PERCENT = 55.0;
const float LIMIT_UMISOLO_MAX_PERCENT = 65.0;

//==============================================================================================
// Variáveis Globais
//==============================================================================================
// Instâncias de sensores
DHT dhtSensor(PIN_DHT22, DHTTYPE);

// Variáveis de estado dos sensores
float temperaturaCelsius = 0.0;
float umidadeArPercent = 0.0;
float umidadeSoloPercent = 0.0;
float co2PPM = 0.0;

// Variáveis de controle
bool estadoValvula = false;
float mq135_R0 = 0.0; // R0 calibrado para o MQ135

// Variáveis de calibração do MQ135
float r0CalibracaoAtual = 0.0;
float r0CalibracaoAnterior = 0.0;
float r0MediaCalibracao = 0.0;
int contadorCalibracao = 0;

//==============================================================================================
// Protótipos de Funções
//==============================================================================================
void inicializarSensoresEPinos();
void lerDadosSensores();
void atualizarSaidas();
void imprimirDadosSerial();
float calibrarMQ135();
void limparSerial();

//==============================================================================================
// Configuração Inicial (setup)
//==============================================================================================
void setup() {
  Serial.begin(115200);
  Blynk.begin(auth, ssid, pass);
  inicializarSensoresEPinos();
  mq135_R0 = calibrarMQ135(); // Calibra o sensor MQ135 uma vez na inicialização
}

//==============================================================================================
// Loop Principal (loop)
//==============================================================================================
void loop() {
  lerDadosSensores();
  atualizarSaidas();
  imprimirDadosSerial(); // Imprime dados apenas quando há mudança significativa
  delay(500); // Pequeno atraso para estabilidade e leitura
}

//==============================================================================================
// Implementação das Funções
//==============================================================================================

void inicializarSensoresEPinos() {
  dhtSensor.begin();
  pinMode(PIN_GAS_OUT, OUTPUT);
  pinMode(PIN_TEMP_OUT, OUTPUT);
  pinMode(PIN_UMISOLO_OUT, OUTPUT);
  pinMode(PIN_UMIAR_OUT, OUTPUT);
  // Não é necessário configurar HIGROMETRO e MQ135 como INPUT, são lidos com analogRead
}

void limparSerial() {
  for (int i = 0; i < 50; i++) {
    Serial.println();
  }
}

float calibrarMQ135() {
  const int NUM_LEITURAS_CALIBRACAO = 30; // Reduzido para agilizar o processo inicial
  Serial.println("Iniciando calibracao do MQ135...");

  for (int i = 0; i < NUM_LEITURAS_CALIBRACAO; i++) {
    float sensorValue = analogRead(PIN_MQ135);
    float Vout = (sensorValue / 4095.0) * 3.3; // Converte para tensão (considerando 3.3V AREF)
    float Rs = MQ135_RL * (3.3 - Vout) / Vout;

    // Fórmula para calcular R0 a partir de Rs em ar limpo (considerando 420ppm CO2 no ar ambiente)
    // Usando a equação Rs/R0 = A * (PPM)^-B => R0 = Rs / (A * (PPM)^-B)
    r0CalibracaoAtual = Rs / pow(420.0 / MQ135_PARAM_A, -1.0 / MQ135_PARAM_B);

    // Média móvel simples para estabilizar R0
    r0MediaCalibracao = (r0MediaCalibracao * i + r0CalibracaoAtual) / (i + 1);

    Serial.print("CALIBRANDO MQ135 - Leitura ");
    Serial.print(i + 1);
    Serial.print("/");
    Serial.print(NUM_LEITURAS_CALIBRACAO);
    Serial.print(" - R0 provisorio: ");
    Serial.println(r0MediaCalibracao, 2); // Exibe com 2 casas decimais

    delay(500);
  }
  Serial.print("Calibracao MQ135 finalizada. R0 = ");
  Serial.println(r0MediaCalibracao, 2);
  return r0MediaCalibracao;
}

void lerDadosSensores() {
  // Leitura do Sensor de Umidade do Solo
  int valorMedidoSolo = analogRead(PIN_HIGROMETRO);
  // Mapeia o valor do sensor (0-4095) para porcentagem de umidade (0-100)
  // Ajuste os limites de '1600' e '4095' conforme a calibração do seu sensor de solo
  umidadeSoloPercent = (valorMedidoSolo < 1600) ? 100.0 : map(valorMedidoSolo, 1600, 4095, 100, 0);

  // Leitura do Sensor DHT22
  float novaTemperatura = dhtSensor.readTemperature();
  float novaUmidadeAr = dhtSensor.readHumidity();

  // Verifica se a leitura do DHT22 foi bem sucedida
  if (isnan(novaTemperatura) || isnan(novaUmidadeAr)) {
    Serial.println("Erro ao ler do sensor DHT!");
  } else {
    temperaturaCelsius = novaTemperatura;
    umidadeArPercent = novaUmidadeAr;
  }

  // Leitura do Sensor MQ135 (CO2)
  int sensorValueMQ135 = analogRead(PIN_MQ135);
  float VoutMQ135 = (sensorValueMQ135 / 4095.0) * 3.3; // Converte para tensão
  float RsMQ135 = MQ135_RL * (3.3 - VoutMQ135) / VoutMQ135; // Calcula Rs
  float ratioMQ135 = RsMQ135 / mq135_R0; // Calcula a relação Rs/R0
  co2PPM = (MQ135_PARAM_A * pow(ratioMQ135, -MQ135_PARAM_B)); // Fórmula para CO2

  // Enviar dados do sensor para o Blynk
  Blynk.virtualWrite(V0, temperaturaCelsius); // Temperatura
  Blynk.virtualWrite(V1, umidadeArPercent); // Umidade do ar
  Blynk.virtualWrite(V3, co2PPM); // Concentração de CO2
  Blynk.virtualWrite(V2, umidadeSoloPercent); // Umidade do solo
}

void atualizarSaidas() {
  // Saída de Gás (CO2)
  digitalWrite(PIN_GAS_OUT, co2PPM >= LIMIT_GAS_PPM ? HIGH : LOW);
  Blynk.virtualWrite(PIN_GAS_OUT, co2PPM >= LIMIT_GAS_PPM ? 255 : 0);

  // Saída de Temperatura
  digitalWrite(PIN_TEMP_OUT, temperaturaCelsius >= LIMIT_TEMP_CELSIUS ? HIGH : LOW);
  Blynk.virtualWrite(PIN_TEMP_OUT, temperaturaCelsius >= LIMIT_TEMP_CELSIUS ? 255 : 0);

  // Saída de Umidade do Ar
  digitalWrite(PIN_UMIAR_OUT, umidadeArPercent >= LIMIT_UMIAR_PERCENT ? HIGH : LOW);
  Blynk.virtualWrite(PIN_UMIAR_OUT, umidadeArPercent >= LIMIT_UMIAR_PERCENT ? 255 : 0);

  // Controle da Válvula de Umidade do Solo (Lógica de Histerese)
  if (!estadoValvula && umidadeSoloPercent <= LIMIT_UMISOLO_MIN_PERCENT) {
    estadoValvula = true; // Liga a válvula se a umidade estiver abaixo do mínimo
  } else if (estadoValvula && umidadeSoloPercent >= LIMIT_UMISOLO_MAX_PERCENT) {
    estadoValvula = false; // Desliga a válvula se a umidade atingir o máximo
  }
  digitalWrite(PIN_UMISOLO_OUT, estadoValvula ? HIGH : LOW);
  Blynk.virtualWrite(PIN_UMISOLO_OUT, estadoValvula ? 255 : 0);

}

void imprimirDadosSerial() {
  static float lastTemperatura = 0.0;
  static float lastUmidadeAr = 0.0;
  static float lastUmidadeSolo = 0.0;
  static float lastCo2 = 0.0;

  // Verifica se houve mudança significativa para evitar spam na serial
  if (abs(temperaturaCelsius - lastTemperatura) > 0.1 ||
      abs(umidadeArPercent - lastUmidadeAr) > 0.1 ||
      abs(umidadeSoloPercent - lastUmidadeSolo) > 0.1 ||
      abs(co2PPM - lastCo2) > 50) { // Tolerância maior para CO2 devido a flutuações

    limparSerial(); // Limpa a serial para uma nova impressão

    Serial.print("Umidade do Solo: ");
    Serial.print(umidadeSoloPercent);
    Serial.println("%");

    Serial.print("Umidade do Ar: ");
    Serial.print(umidadeArPercent);
    Serial.println("%");

    Serial.print("Temperatura: ");
    Serial.print(temperaturaCelsius);
    Serial.println(" °C");

    Serial.print("CO2: ");
    Serial.print(co2PPM);
    Serial.println(" ppm");

    // Atualiza os valores anteriores
    lastTemperatura = temperaturaCelsius;
    lastUmidadeAr = umidadeArPercent;
    lastUmidadeSolo = umidadeSoloPercent;
    lastCo2 = co2PPM;
  }
}
