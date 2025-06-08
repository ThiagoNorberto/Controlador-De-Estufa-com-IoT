//==============================================================================================
#include <Arduino.h>
#include <DHT.h>

//=============================================================================================

#define gas_out 18
#define temp_out 19
#define umiSolo_out 21
#define umiAr_out 22

//=============================================================================================

#define HIGROMETRO 34
bool estado_valvula = false;
float umidadeSolo = 0.0;

//=============================================================================================

#define DHT22_PIN 4
#define DHTTYPE DHT22
DHT DHT22_Sensor(DHT22_PIN, DHTTYPE);

float temperatura = 0.0;
float umidadeAr = 0.0;

//=============================================================================================

#define MQ135 12
#define PARAMETROA 116.6020682
#define PARAMETROB 2.769034857

float CO2 = 0.0;
float RL = 10;  // Resistência de carga em KΩ
float R0 = 170; // Resistência de carga em KΩ

//=============================================================================================

float gas_limit = 750.0;
float temp_limit = 30.0;
float umidadeAr_limit = 55;
float umi_limit_max = 65;
float umi_limit_min = 55;

//=============================================================================================

float R0_calibracao = 0.0;
float R0_calibracao_antigo = 0.0;
float R0_media = 0.0;

void clearSerial()
{
  for (int i = 0; i < 50; i++)
  {
    Serial.println();
  }
}

void setup()
{
  Serial.begin(115200);
  DHT22_Sensor.begin();

  pinMode(gas_out, OUTPUT);
  pinMode(temp_out, OUTPUT);
  pinMode(umiSolo_out, OUTPUT);
  pinMode(umiAr_out, OUTPUT);
}

void loop()
{

  //=============Configuração e Leitura Sensor de Umidade do Solo==================================================================================

  float umidadeSolo_feedback = umidadeSolo;
  float valor_medido = analogRead(HIGROMETRO);

  umidadeSolo = (valor_medido < 1600) ? 100 : map(valor_medido, 1600, 4095, 100, 0);

  //===========================Leitura do Sensor DHT22=====================================================================================

  float temperatura_feedback = temperatura;
  temperatura = DHT22_Sensor.readTemperature();

  float umidadeAr_feedback = umidadeAr;
  umidadeAr = DHT22_Sensor.readHumidity();

  //===========================Leitura do Sensor MQ135=====================================================================================

  float CO2_feedback = CO2;
  float sensorValue = analogRead(MQ135);

  float Vout = (sensorValue / 4095) * 3.3; // Converte para tensão
  float Rs = RL * (3.3 - Vout) / Vout;     // Calcula Rs
  // R0 = Rs / pow(420.0 / PARAMETROA, -1.0 / PARAMETROB);

  float ratio = Rs / R0;                        // Calcula a relação Rs/R0
  CO2 = (PARAMETROA * pow(ratio, -PARAMETROB)); // Fórmula para CO2

  //===============================================================================================================

if (umidadeSolo_feedback != umidadeSolo || temperatura_feedback != temperatura || umidadeAr_feedback != umidadeAr || abs(CO2 - CO2_feedback) > 150)
{
  clearSerial();

  Serial.print("Umidade:");
  Serial.print(umidadeSolo);
  Serial.print("%");
  Serial.print("\n");

  Serial.print("Umidade do ar: ");
  Serial.print(umidadeAr);
  Serial.print(" %\n");

  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.print(" °C\n");

  Serial.print("Gas:");
  Serial.print(CO2);

  Serial.print("ppm\n");
}

  //===============================================================================================================

  digitalWrite(gas_out, CO2 >= gas_limit ? HIGH : LOW);
  digitalWrite(temp_out, temperatura >= temp_limit ? HIGH : LOW);
  digitalWrite(umiAr_out, umidadeAr >= umidadeAr_limit ? HIGH : LOW);

  if (!estado_valvula && umidadeSolo <= umi_limit_min)
  {
    estado_valvula = true;
  }
  else if (estado_valvula && umidadeSolo >= umi_limit_max)
  {
    estado_valvula = false;
  }
  digitalWrite(umiSolo_out, estado_valvula ? HIGH : LOW);

  // R0_calibracao_antigo = R0_calibracao;

  // R0_calibracao = Rs / pow(420.0 / PARAMETROA, -1.0 / PARAMETROB);

  // R0_media = ((R0_calibracao + R0_calibracao_antigo) / 2 + (R0_media)) / 2;

  // Serial.print("R0 média:");
  // Serial.print(R0_media);
  // Serial.print("\n");

  delay(800);
}
