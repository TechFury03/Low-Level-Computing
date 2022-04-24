#include <avr/io.h>
#include <util/delay.h>
#include <util/atomic.h>

#define LATCH PB4
#define BMP PB3
#define CLK PB2
#define DO PB1
#define DI PB0

#define empty 10
#define celsius 11
#define fahrenheid 12
#define kPa 13

#define decimal true
#define noDecimal false

uint64_t symbols[14] {
  0x183c666666663c18, //0
  0x3c18181818181c18, //1
  0x3c3c0c1830303c1c, //2
  0x3c3c303c3c303c3c, //3
  0x3030307e7e363636, //4
  0x1c3c303c1c0c3c3c, //5
  0x3c66667e3e060c38, //6
  0x0c0c183030607e3e, //7
  0x3c7e667e7e667e3c, //8
  0x0c18307c6666663c, //9 
  0x00, //empty
  0x70d81818181bdb70, //celsius
  0x18181878781bfbf8, //fahrenheid
  0x00151515f395f500 //kPa
};
uint16_t T1;
int16_t T2, T3;
uint16_t P1;
int16_t P2, P3, P4, P5, P6, P7, P8, P9;
int32_t t_fine;

void init(){
  DDRB |= (1<<LATCH) | (1<<BMP) | (1<<CLK) | (1<<DO); //set PB1:PB4 as output
  DDRB &= ~(1<<DI); //set PB0 as input
  USICR = (1 << USIWM0) | (1 << USICS1) | (1 << USICLK); //select threewire mode, clocksource is software
  PORTB |= (1<<LATCH) | (1<<BMP); //set slave select high
}

uint8_t transfer(uint8_t data){
  USIDR = data;
  USISR &= ~(1<<USICNT3) | ~(1<<USICNT2) | ~(1<<USICNT1) | ~(1<<USICNT0); //set counter to 0
  USISR |= (1<<USIOIF); //clear flag

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
    while(!(USISR & (1<<USIOIF))){
      USICR |= (1<<USITC); //toggle clock
    }
  }
  return USIDR;
}

void cascade(int i){
  for(int j = 0; j < i; j++){
      transfer(0x00);
      transfer(0x00);  
  }
}

void writeMatrix(int index, int place, bool dot){
  int bitShiftAmount = 56;
  for(int i = 0; i < 8; i++){
    uint8_t row = (symbols[index]>>bitShiftAmount);

    //place decimal point if needed
    if((place == 1) && (i == 0) && (index != empty) && dot){
      row |= 0b10000000;
    } else if ((place == 2) && (i == 0) && (index != empty) && dot){
      row |= 0b0000001;
    } 

    PORTB &= ~(1<<LATCH);
    transfer(i+1); //register address
    transfer(row);
    cascade(place);
    PORTB |= (1<<LATCH);
    bitShiftAmount = bitShiftAmount - 8;
  }
}

void setupMatrix(){
  for(int i = 0; i < 4; i++){
    PORTB &= ~(1<<LATCH);
    transfer(0x0C); //shutdown register
    transfer(0x01); //normal operation
    cascade(i);
    PORTB |= (1<<LATCH);
    PORTB &= ~(1<<LATCH);
    transfer(0x09);//decode register
    transfer(0x00);//no decode
    cascade(i);
    PORTB |= (1<<LATCH);
    PORTB &= ~(1<<LATCH);
    transfer(0x0B); //scanlimit register
    transfer(0x07); //no limit
    cascade(i);
    PORTB |= (1<<LATCH);
    writeMatrix(2,0,noDecimal); //empty digit registers of whole matrix display
    writeMatrix(2,1,noDecimal);
    writeMatrix(2,2,noDecimal);
    writeMatrix(2,3,noDecimal);
  }
}

void sensorSetup(){
  PORTB &= ~(1<<BMP); //ss low
  transfer(0x74);
  transfer(0b01001111);
  PORTB |= (1<<BMP); //ss high
}

void calibrate(){
  PORTB &= ~(1<<BMP); //ss low
  transfer(0x88); //get calibration data

  uint16_t T1_low = transfer(0);
  uint16_t T1_high = transfer(0);
  T1 = (T1_high<<8) | (T1_low);

  int16_t T23_low = transfer(0);
  int16_t T23_high = transfer(0);
  T2 = (T23_high<<8) | (T23_low);

  T23_low = transfer(0);
  T23_high = transfer(0);
  T3 = (T23_high<<8) | (T23_low);

  PORTB |= (1<<BMP); //ss high 
}

void calibratePress(){
  PORTB &= ~(1<<BMP); //ss low
  transfer(0x8E);
  P1 = transfer(0);
  P1 |= (transfer(0)<<8);
  P2 = transfer(0);
  P2 |= (transfer(0)<<8);
  P3 = transfer(0);
  P3 |= (transfer(0)<<8);
  P4 = transfer(0);
  P4 |= (transfer(0)<<8);
  P5 = transfer(0);
  P5 |= (transfer(0)<<8);
  P6 = transfer(0);
  P6 |= (transfer(0)<<8);
  P7 = transfer(0);
  P7 |= (transfer(0)<<8);
  P8 = transfer(0);
  P8 |= (transfer(0)<<8);
  P9 = transfer(0);
  P9 |= (transfer(0)<<8);
  PORTB |= (1<<BMP);
}

float readTemp(){
  PORTB &= ~(1<<BMP); //ss low
  transfer(0xFA); //read temp
  int32_t temp_high = transfer(0);
  int32_t temp_middle = transfer(0);
  int32_t temp_low = transfer(0);

  int32_t adc_T = (temp_high<<12) | (temp_middle<<4) | (temp_low>>4);

  PORTB |= (1<<BMP); //ss high 
  long  var1 = (((( adc_T  >> 3) - ((long)T1 << 1))) * ((long)T2)) >> 11;
  long  var2 = ((((( adc_T  >> 4) - ((long)T1)) * ((adc_T  >> 4) - ((long)T1))) >> 12) * ((long)T3)) >> 14;
  t_fine = (int32_t)(var1 + var2);
  float  temp = (var1 + var2) / 5120.0;
  return temp;
}

float readPress(){
  PORTB &= ~(1<<BMP); //ss low
  transfer(0xF7);
  int32_t press_high = transfer(0);
  int32_t press_middle = transfer(0);
  int32_t press_low = transfer(0);

  int32_t adc_P = (press_high<<12) | (press_middle<<4) | (press_low>>4);

  PORTB |= (1<<BMP); //ss high 

  int64_t var1, var2, p;
  var1 = ((int64_t)t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)P6;
  var2 = var2 + ((var1 * (int64_t)P5) << 17);
  var2 = var2 + (((int64_t)P4) << 35);
  var1 = ((var1 * var1 * (int64_t)P3) >> 8) + ((var1 * (int64_t)P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)P1) >> 33;

  if (var1 == 0) {
    return 0; // avoid exception caused by division by zero
  }
  p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)P8) * p) >> 19;

  p = ((p + var1 + var2) >> 8) + (((int64_t)P7) << 4);
  float pressure = (float)p / 256 /1000; //convert to kPa
  return pressure;
}

void writeEmpty(){
  writeMatrix(empty,0,noDecimal);
  writeMatrix(empty,1,noDecimal);
  writeMatrix(empty,2,noDecimal);
  writeMatrix(empty,3,noDecimal);
}

void displayPress(float p){
  int hundreds = (int)(p / 100) % 10;
  int tens = (int)(p / 10)  % 10;
  int ones = (int)(p/1) % 10;

  writeMatrix(hundreds, 0, noDecimal);
  writeMatrix(tens, 1, noDecimal);
  writeMatrix(ones, 2, noDecimal);
  writeMatrix(kPa, 3, noDecimal);
  _delay_ms(2000);
}

void displayTempF(float temp){
  temp = temp * 1.8 + 32;

  if (temp > 99.9){
    int hundreds = temp / 100;
    int tens = (((int)temp) / 10) % 10;
    int ones = ((int)temp) % 10;

    writeEmpty();
    _delay_ms(100);

    writeMatrix(hundreds, 0, noDecimal);
    writeMatrix(tens, 1, noDecimal);
    writeMatrix(ones, 2, noDecimal);
    writeMatrix(fahrenheid, 3, noDecimal);
    _delay_ms(2000);
  } else {
    int tens = temp / 10;
    int ones = (int)temp % 10;
    int oneTens = temp * 10; 
    oneTens = oneTens % 10;

    writeEmpty();
    _delay_ms(100);

    writeMatrix(tens, 0, decimal);
    writeMatrix(ones, 1, decimal);
    writeMatrix(oneTens, 2, decimal);
    writeMatrix(fahrenheid, 3, decimal);
    _delay_ms(2000);
  }
}

void displayTemp(float temp){
  int tens = temp / 10;
  int ones = (int)temp % 10;
  int oneTens = temp * 10; 
  oneTens = oneTens % 10;
  int oneHundreds = temp * 100;
  oneHundreds = oneHundreds % 10;

  writeMatrix(tens, 0, decimal);
  writeMatrix(ones, 1, decimal);
  writeMatrix(oneTens, 2, decimal);
  writeMatrix(celsius, 3, decimal);
  _delay_ms(2000);
  displayTempF(temp);
}

int main(){
  init();
  setupMatrix();
  calibrate();
  calibratePress();
  sensorSetup();
  while(true){
    displayTemp(readTemp());
    writeEmpty();
    _delay_ms(100);
    displayPress(readPress());
    writeEmpty();
    _delay_ms(100);
  }
  return 0;
}