#include <Wire.h>

// Создаем софтовый com порт, т.к. аппаратный мешает заливке программы в Ардуино
#include <SoftwareSerial.h>
SoftwareSerial mySerial(10, 11); // RX1, TX1
#define SERIAL_BAUD 9600
#define TRANSFER_PAUSE 50       // Пауза при передачи символов что бы все успело передаться

/*********************** DHT11 датчик температуры и влажности **************/
#include "stDHT.h"
DHT sens(DHT11); // Указать датчик DHT11, DHT21, DHT22
/***************************************************************************/


/********************** MG811 датчик CO2 **********************************/
#define MG_PIN (0)      // определяем, какой аналоговый вход будем использовать
#define BOOL_PIN (2)    // номер цифрового входа ???
#define DC_GAIN (8.5)   // определить коэффициент усиления усилителя постоянного тока

#define READ_SAMPLE_INTERVAL (1000)  //определяет, сколько замеров будем делать в нормальном режиме
#define READ_SAMPLE_TIMES (15)       // (было 5) определение интервала времени (в миллисекундах) между каждой выборкой в нормальном режиме

// Эти два значения отличаются от датчика к датчику. пользователь должен определить это значение.
#define ZERO_POINT_VOLTAGE (1.850)  //(0.220) определите выходной сигнал датчика в вольтах, когда концентрация СО2 400PPM (чистый воздух в парке, например)
// 1.850 в. в саду
#define REACTION_VOLTGAE (0.750)    //(0.020) определите падение напряжения датчика при перемещении датчика из воздуха в 10000 ppm CO2
// 1.1 в. у выхлопушки машины

#define CORRECTION_FACTOR 5.6420    // коэффициент корректировки результата. Придуман мной для приведения результата к реальности. (Исходные формулы от другого обвеса датчика.)

// две точки взяты из кривой. с этими двумя точками образуется линия, которая "приблизительно эквивалентно" исходной кривой.
float CO2Curve[3] = {2.602, ZERO_POINT_VOLTAGE, (REACTION_VOLTGAE / (2.602 - 3))};
//data format:{ x, y, slope}; point1: (lg400, 0.324), point2: (lg4000, 0.280)
//slope = ( reaction voltage ) / (log400 –log1000)

unsigned int onTimerMG811 = 90000;           // время паузы перед включением измерения 1,5 мин., до этого уровень СО2=0
/***************************************************************************/


/*************** BME280 датчик давления, влажности, температуры по I2C******/
#include <BME280I2C.h>
// Default : forced mode, standby time = 1000 ms
// Oversampling = pressure ×1, temperature ×1, humidity ×1, filter off,
BME280I2C bme(0x76);
#define SEALEVELPRESSURE_HPA (1013.25)  // давление на уровне моря
#define ALTITUDE 237.0    // это высота над уровнем моря указана в метрах 221 + 2,5*7 (мой дом + мой этаж)
boolean noBME280 = false;
/***************************************************************************/


/*********************** Обшие глобальные переменные ***********************/
/* Массив результатов измерений
        measurementResult[0] = признак приема данных оставлен для совместимости по индексам
        measurementResult[1] = температура с DHT11 С
        measurementResult[2] = влажность с DHT11 %
        measurementResult[3] = давление с BME280 мм. рт. ст.
        measurementResult[4] = концентрация co2 ppm MG811
        measurementResult[5] = температура с BME280 С
        measurementResult[6] = влажность с BME280 %
        measurementResult[7] = напряжение с MG811
*/
int measurementResult[8] = {0, 0, 0, 0, 0, 0, 0, 0};
boolean symbol_izm = false; // Признак изменения при передаче данных (служебная)
/***************************************************************************/

void setup()
{
  mySerial.begin(9600);                   //UART setup, baudrate = 9600bps
  Serial.begin(9600);                     //UART setup, baudrate = 9600bps
  pinMode(13, OUTPUT);                    // иницируем светодиод
  digitalWrite(13, LOW);                  // гасим светодиод
  Wire.begin();

  // MG811 датчик CO2
  pinMode(BOOL_PIN, INPUT);               // Объявляем пин как вход
  digitalWrite(BOOL_PIN, HIGH);           // Включить подтягивающие резисторы
  Serial.println("MG-811 Demonstration");

  // инициация датчика DHT11
  pinMode(2, INPUT);
  digitalWrite(2, HIGH);

  while (!mySerial) {} // Wait ??? не понятно зачем

  // инициация датчика BME280
  noBME280 = initSensorBME280();
}

void loop()
{
  digitalWrite(13, HIGH);         // Включаем светодиод

  measurementsFromDHT11Sensor();  // Замер с датчика DHT11

  measurementsFromBME280Sensor(); // Замер с датчика BME280

  measurementsFromMG811Sensor();  // Замер с датчика MG811

  outputResultsToBluetooth();     // Вывод результатов в блютуз

  digitalWrite(13, LOW);          // Выключаем светодиод

  delay(3000);
}


/******************* Вывод результатов в блютуз *****************************/
void outputResultsToBluetooth()
{
  int MesChecksum = 0;      // контрльная сумма
  char MessageBuffer[100];  // массив буфера строки
  String stringOut = "";    // строка для вывода в бюлтуз

  delay(TRANSFER_PAUSE);
  if (symbol_izm) {
    stringOut += "+";
  } else {
    stringOut += "-";
  }
  symbol_izm = !symbol_izm;

  for (int i = 1; i < 8; i++) {
    stringOut += ":" + String(measurementResult[i]);
  }

  stringOut.toCharArray(MessageBuffer, stringOut.length()+1); //Arduino String Class

  for (int x = 0; x < stringOut.length(); x++)
  {
    MesChecksum ^= MessageBuffer[x]; //XOR the Message data...
  }
  stringOut += ":*";
  stringOut += String(MesChecksum, HEX);

  Serial.println(stringOut);
  mySerial.println(stringOut);

}


/******************* Запуск замера с датчика DHT11 *****************************/
void measurementsFromDHT11Sensor() {
  measurementResult[1] = sens.readTemperature(2);  // чтение датчика на пине 2
  delay(2000);
  measurementResult[2] = sens.readHumidity(2);     // чтение датчика на пине 2
  delay(500);
}


/******************* Запуск замера с датчика BME280 *****************************/
void measurementsFromBME280Sensor() {
  float t2 = 0, h2 = 0, p0 = 0;
  // читаем данные с датчика BME280, если он доступен
  if (!noBME280) {
    BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
    BME280::PresUnit presUnit(BME280::PresUnit_Pa);
    bme.read(p0, t2, h2, tempUnit, presUnit);
    p0 = (p0 * 0.750063755419211 / 100) + (ALTITUDE / 10.5); // Корректировка давления на уровень моря и приведение к мм
  }
  measurementResult[3] = int(p0);
  measurementResult[5] = int(t2);
  measurementResult[6] = int(h2);
}


/******************* Запуск замера с датчика MG811 CO2 *****************************/
void measurementsFromMG811Sensor() {

  // Если не прошло 1,5 мин., то датчик не прогрелся и измерять не стоит
  if (millis() < onTimerMG811) {
    measurementResult[4] = -1;
    measurementResult[7] = -1;
    return;
  }

  float percentage;
  float volts;

  volts = MGRead(MG_PIN);

  percentage = MGGetPercentage(volts);

  measurementResult[4] = percentage;

  measurementResult[7] = volts * 100;
}


/*****************************  MGRead ********************************************
  Input:   mg_pin - analog channel
  Output:  output of SEN-000007
  Remarks: This function reads the output of SEN-000007
*********************************************************************************/
float MGRead(int mg_pin)
{
  int i;
  float v = 0;

  for (i = 0; i < READ_SAMPLE_TIMES; i++) {
    v += analogRead(mg_pin);
    delay(READ_SAMPLE_INTERVAL);
  }
  v = (v / READ_SAMPLE_TIMES) * 5. / 1024. ;
  return v;
}


/*****************************  MQGetPercentage **********************************
  Input:   volts   - SEN-000007 output measured in volts
           pcurve  - pointer to the curve of the target gas
  Output:  ppm of the target gas
  Remarks: By using the slope and a point of the line. The x(logarithmic value of ppm)
           of the line could be derived if y(MG-811 output) is provided. As it is a
           logarithmic coordinate, power of 10 is used to convert the result to non-logarithmic
           value.

  curve-указатель на кривую целевого газа
  Выход: ppm целевого газа
  Примечания: используя наклон и точку линии. X (логарифмическое значение ppm)
           линии могут быть получены, если y(мг-811 выходной) предоставляется. Как это
           логарифмическая координата, основанию (мощность) 10 используется для преобразования результата в не логарифмическую
           значение.

*********************************************************************************/
double MGGetPercentage(float volts)
{
  return (-12800 * volts + 24080);  // подобрал формулу в экселе
}


/******************* Инициация датчика BME280 *****************************/
boolean initSensorBME280() {

  if (!bme.begin()) {
    mySerial.println("Could not find BME280 sensor!");
    return true;
  }

  switch (bme.chipModel()) {
    case BME280::ChipModel_BME280:
      mySerial.println("Found BME280 sensor! Success.");
      return false;
      break;
    case BME280::ChipModel_BMP280:
      mySerial.println("Found BMP280 sensor! No Humidity available.");
      return false;
      break;
    default:
      mySerial.println("Found UNKNOWN sensor! Error!");
      return true;
  }
}
