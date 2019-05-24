//
// Created by user on 12.05.2019.
//

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <Wire.h>

// Software SPI (slower updates, more flexible pin options):
// pin 7 - Serial clock out (SCLK)
// pin 6 - Serial data out (DIN)
// pin 5 - Data/Command select (D/C)
// pin 4 - LCD chip select (CS)
// pin 3 - LCD reset (RST)
//Adafruit_PCD8544 display = Adafruit_PCD8544(7, 6, 5, 4, 3);
Adafruit_PCD8544 display = Adafruit_PCD8544(3, 4, 5, 6, 7);

#define measurementPeriod 60000             // Период измерения

// Программирвоание шкалы цвета
#define TH1 15              // фиолетовый
#define TH2 20              // синий
#define TH3 30              // голубой
#define TH4 60              // зелёный
#define TH5 100             // жёлтый
#define TH6 200             // красный

// Коэффициент для трубки J305ß - CPM to uSV/h
#define CONV_FACTOR 0.00812 // это как раз для трубки J305ß

const byte rainbow[6][3] = {
  {1, 0, 1}, // фиолетовый
  {0, 0, 1}, // синий
  {0, 1, 1}, // голубой
  {0, 1, 0}, // зелёный
  {1, 1, 0}, // жёлтый
  {1, 0, 0}, // красный
};

int rgbPins [3] = {10, 12, 11}; // выводы светодиода красн, зел, син
int geiger_input = 2;   // цифровой вход, на который поступают импульсы со счетчика. На него повешено прерывание 0

long count = 0;
long countPerMinute = 0;
long timePrevious = 0;
long timePreviousс = 0;

long countPrevious = 0;
float radiationValue = 0.0;

boolean symbol_izm = false; // Признак изменения при передаче данных (служебная)

static unsigned long G;     // для цифрового фильтра

long D_rs = 0;              // В этой переменной накапливаем счётчик для циф. обработки
float D_rsf = 0.0;          // переменная для расчётной средней

unsigned long counts;       //variable for GM Tube events
float cpm;                  //variable for CPM

unsigned long allCounts = 0;       // количество импульсов всего
unsigned long kolMeassure = 1;     // количество измерерний

void setup() {

  Serial.begin(9600);

  // инициализация и очистка дисплея
  display.begin();
  display.cp437(true);
  display.clearDisplay();
  display.setContrast(50);      // установка контраста
  display.setTextSize(1);       // установка размера шрифта
  display.setTextColor(BLACK);  // установка цвета текста
  display.setCursor(0, 0);      // установка позиции курсора
  display.println(utf8rus("Подготовка..."));
  display.display();

  for ( byte i = 0; i < 3; i++ ) {pinMode( rgbPins[i], OUTPUT );};

  counts = 0;
  cpm = 0;

  pinMode(geiger_input, INPUT);
  //digitalWrite(geiger_input, HIGH); // из-за него не работал счетчик импульсов !!!

  attachInterrupt(0, countPulse, FALLING);      // Подключение прерывания
}

void loop() {

  if (millis() - timePreviousMeassure >= measurementPeriod) {

    countPerMinute = count;                             // количество импульсов за период
    radiationValue = countPerMinute * CONV_FACTOR;      // переводим значение в уровень радиации
    timePreviousMeassure = millis();
    count = 0;                                          // счетчик обнулим тут, для правильности счёта, вывод на экран ведь время занимает

    kolMeassure++;

    // непонятный алгоритм. Переделываю
    // вычисляем среднее с предыдущим средним )))
    /*
    if (D_rsf > 0) {
    D_rsf = (D_rsf + radiationValue) / 2;
    } else
    {
    D_rsf = radiationValue;
    }
    */

    // его замена
    D_rsf = allCounts / kolMeassure;

    // и выводим в порт и на дисплей  
    Serial.print("cpm = ");
    Serial.print(countPerMinute, DEC);
    Serial.print(" - ");
    Serial.print("uSv/h = ");
    Serial.println(radiationValue, 4);

    display.clearDisplay();
    display.print("cpm = ");
    display.println(countPerMinute, DEC);
    display.print("uSv/h = ");
    display.println(radiationValue, 4);
    display.print("mR/h = ");
    display.println(D_rsf * 100, 4);
    display.display();

    //led var setting 
    if (countPerMinute <= TH1) ledVar(0);
    if ((countPerMinute <= TH2) && (countPerMinute > TH1)) ledVar(1);
    if ((countPerMinute <= TH3) && (countPerMinute > TH2)) ledVar(2);
    if ((countPerMinute <= TH4) && (countPerMinute > TH3)) ledVar(3);
    if ((countPerMinute <= TH5) && (countPerMinute > TH4)) ledVar(4);
    if (countPerMinute > TH5) ledVar(5);
  }
}

void tube_impulse() { //subprocedure for capturing events from Geiger Kit
  counts++;
  allCounts++;
}

void countPulse() {
  Serial.println(symbol_izm);
  display.setCursor(40, 40);
  if (symbol_izm) {
    display.print("+");
  } else {
    display.print("-");
  }
  display.display();
  symbol_izm = !symbol_izm;

  detachInterrupt(0);
  count++;
  while (digitalRead(geiger_input) == 0) {
  }
  attachInterrupt(0, countPulse, FALLING);
}

void ledVar(int value) {
  // перебираем три компоненты каждого из шести цветов
  for ( int k = 0; k < 3; k++ ) {digitalWrite( rgbPins[k], rainbow[value][k] );};
}

// Recode russian fonts from UTF-8 to Windows-1251
String utf8rus(String source)
{
  int i, k;
  String target;
  unsigned char n;
  char m[2] = { '0', '\0' };

  k = source.length(); i = 0;

  while (i < k) {
    n = source[i]; i++;

    if (n >= 0xC0) {
      switch (n) {
        case 0xD0: {
            n = source[i]; i++;
            if (n == 0x81) {
              n = 0xA8;
              break;
            }
            if (n >= 0x90 && n <= 0xBF) n = n + 0x30;
            break;
          }
        case 0xD1: {
            n = source[i]; i++;
            if (n == 0x91) {
              n = 0xB8;
              break;
            }
            if (n >= 0x80 && n <= 0x8F) n = n + 0x70;
            break;
          }
      }
    }
    m[0] = n; target = target + String(m);
  }
  return target;
}
