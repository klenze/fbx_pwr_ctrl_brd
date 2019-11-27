//#include <HardwareSerial.h>
#include <Arduino.h> //delay
#include "uart.h" // uart for grown-ups (incl printf)
#include "commit.h"
#include <stdio.h>
#include <string.h>
#define PIN_VIN  A3
#define PIN_VBUF A1
#define PIN_VOUT A2
#define LED 13
#define FET_ON 9

//config section
#define SHOWVERSION 1
#define SHOWRAW 0
#define SHOWCAL 1
#define SHOWUSAGE 1
#define SHOWSTATUS 1
#define SHOW_TOGGLE 0

#define MAX9611 1
#define ENABLE_VOUT2 0

// end of config

#define SHOWANY (SHOWVERSION || SHOWRAW || SHOWCAL || SHOWUSAGE || SHOWSTATUS )
#if MAX9611
#include <Wire.h>
#define I2C_MAX9611 (0xe0)>>1 // MAX9611
#endif

uint32_t cal[]={10, 179, 179, 179, 125, 145};
// voltages in 1e-4 V per adc unit
// current  in 1e-4 A per adc unit


enum sensor_t
  {
    time=0,
    vin=1,
    vbuf=2,
    vout=3,
    vout2=4,
    curout=5
  };

    
enum dev_t {dev_time, adc, max9611};
static struct
{
  dev_t dev;
  uint8_t ch_id;
  const char* name;
}
  data_src[]={
    {dev_time, -1, "Time_us"},
    {adc, PIN_VIN, "Vin"}, {adc, PIN_VBUF, "Vbuf"}, {adc, PIN_VOUT, "Vout"},
    {max9611, 0x00, "Iout"}
#if ENABLE_VOUT2
    {max9611, 0x02, "Vout2"}
#endif
    // , {max9611, 0x08, "Temp"}
  };
const int num_sensors=sizeof(data_src)/sizeof(data_src[0]);


inline uint32_t read_sensor(sensor_t id)
{
  uint32_t res;
  switch (data_src[id].dev)
    {
    case dev_time:
      res=micros();
      break;
    case adc:
      res=analogRead(data_src[id].ch_id);
      break;
    case max9611:
      res=0;
#if MAX9611
      Wire.beginTransmission(I2C_MAX9611);
      Wire.write(data_src[id].ch_id);
      Wire.endTransmission();
      //Wire.beginTransmission(I2C_MAX9611);
      uint8_t avail=Wire.requestFrom(I2C_MAX9611, 2);
      int j=0;
      while (avail=Wire.available())
	{
	  res=(res<<8)+Wire.read();
	}
      res>>=4;
#endif
      break;
    }
  return res;
}

int main()
{
  init();
  uart_init();
  uart_connect_stdio();
  //Serial.begin(9600);
  pinMode(13, OUTPUT);
  pinMode(9, OUTPUT);
  analogReference(EXTERNAL);
#if MAX9611
  Wire.begin();
  Wire.beginTransmission(I2C_MAX9611);
  Wire.write(0x0a);
  Wire.write(0x02); // Ch A: gain 8x
  Wire.endTransmission();
  
#if ENABLE_VOUT2
  Wire.beginTransmission(I2C_MAX9611);
  Wire.write(0x0a);
  Wire.write(0x07); //read all channels, last gain setting
  Wire.endTransmission();
#endif
#endif
  

  

  uint32_t data_raw[num_sensors];
  uint32_t data_cal[num_sensors]; // in mV,mA

  
  float data_sum[num_sensors];
  float data_ssum[num_sensors];

  memset(data_sum, 0, sizeof(data_sum));
  memset(data_ssum, 0, sizeof(data_ssum));
  
  int32_t n=SHOWRAW?-50:0;
  printf_P(PSTR("       Startup finished.\n\r"));
  bool desired_power=1; //switch on if cap is full
  bool is_on=0;
  while(1)
    {
  
      n++;
      delay(50);

      int16_t input=uart_getc_try();
      switch(input)
	{
	case '0':
	  desired_power=0;
	  break;
	case '1':
	  desired_power=1;
	  break;
	case 'x':
	  desired_power^=1;
	  break;
	}
      
      if (n<1 && SHOWRAW)
	{
      printf_P(PSTR("\x1b[H %d"), (int)n);
        continue;
        }  
      for (int i=0; i<num_sensors; i++)
	{
	  //	  printf_P(PSTR("step %d %d\r\n"), n, i);

	  data_raw[i]=read_sensor(sensor_t(i));
	  
	  data_sum[i]+=data_raw[i];
	  data_ssum[i]+=data_raw[i]*data_raw[i];

	  data_cal[i]=cal[i]*data_raw[i]/10;

	}
      // basically a two stop hysteresis:
      // turn FET on if cap is full, turn it off if Vin fails.
      
      // bool have_vin=data_cal[vin]> 9000;
      bool have_vin=data_cal[vin]> 11000;
      bool cap_full=data_cal[vbuf]>11500;
      //      bool cap_full=1;
      
      bool is_on_new=desired_power && have_vin && (cap_full || is_on );
#if DEBUG_TOGGLE
      if (is_on_new && !is_on)
	{
	  //	  printf_P(PSTR("Turning FET On.\n\r"));
	  // start sampling as fast as possible, turn on before s==10
	  const int num_samples=30;
	  const int start=3;
	  uint32_t data[num_samples][num_sensors];
	  memset(data, 0, sizeof(data));
	  for (int s=0; s<num_samples; s++)
	    {
	      if (s==start)
		digitalWrite(FET_ON, is_on_new);
	      
	      for (int i=0; i<num_sensors; i++)
		  data[s][i]=read_sensor(sensor_t(i));
	      putchar('.');
	    }
	  //now write out the data as csv
	  printf_P(PSTR("\n\r#"));
	  for (int i=0; i<num_sensors; i++)
	    printf_P(PSTR("%20s,  "), data_src[i].name);
	  printf_P(PSTR("\n\r"));
	  for (int s=0; s<num_samples; s++)
	    for (int i=0; i<num_sensors; i++)
	      printf_P(PSTR("%20ld%s"), i?(cal[i]*data[s][i]/10):(data[s][i]-data[start][i]), i!=num_sensors-1?",   ":"\n\r");
	}
      else
	{
	  if (is_on && ! is_on_new)
	    printf_P(PSTR("Turning FET Off.\n\r"));

	    digitalWrite(FET_ON, is_on_new);
	}
#else
      is_on=is_on_new;
      digitalWrite(FET_ON, is_on_new);
#endif
      
      digitalWrite(LED, (n>>3)%2);

      if (!(n%5))
	{
#if SHOWANY
	  printf_P(PSTR("\x1b[H"));
#endif
#if SHOWVERSION
	  printf_P(PSTR("Febex power delay board firmware, git " COMMIT "\n"));
#endif
#if SHOWRAW //print raw
	  printf_P(PSTR("%10s   %10s   %10s   %10s   n=%lu\r\n"),  "_Name_", "_Latest_", "_Mean_", "_RMS_", n); //
	  for (int i=1; i<num_sensors; i++)
	    printf_P(PSTR("%10s   %10ld   %10lu   %10lu\r\n"), data_src[i].name, data_raw[i], uint32_t(data_sum[i]/n), 
		     uint32_t(sqrt(data_ssum[i]/n-(data_sum[i]*data_sum[i]/(n*n)) )));
#endif
#if SHOWCAL
	  printf_P(PSTR("%10s   %10s    \r\n"), "_Name_", "_Latest (mV/mA)_"); //
	  for (int i=1; i<num_sensors; i++)
	    printf_P(PSTR("%10s   %10ld\r\n"), data_src[i].name, data_cal[i]);
	    //	    printf_P(PSTR("%d  %10s   %10d.%03d\r\n"), i, data_src[i].name, data_cal[i]/1000, data_cal[i]%1000);
#endif
#if SHOWSTATUS
	  const char* label[]={"Off", "On "};
	  //	  printf_P(PSTR("n=12%ld, time=%20ldus, time per loop: %8ldus"), n, micros(), micros()/n);
	  printf_P(PSTR("\n\rStatus: desired state: %s, Vin ok: %d, cap full: %d => FET: %s\n\r"), label[desired_power], have_vin, cap_full, label[is_on]);
#endif
#if SHOWUSAGE
	  printf_P(PSTR("Input Menu: '0': turn off    '1': turn on         'x': toggle\n\r"));
#endif
	}
      
	    
	
    }
  return 0;
}
