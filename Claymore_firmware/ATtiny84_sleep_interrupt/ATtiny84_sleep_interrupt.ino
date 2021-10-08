// ATMEL ATTINY 25/45/85


#include <avr/sleep.h>    // Sleep Modes
#include <avr/power.h>    // Power management

const byte LED = 5;
const byte SWITCH = 4;  //If you change this, you also have to change the 



void setup ()
{
  pinMode (LED, OUTPUT);
  pinMode (SWITCH, INPUT);
  digitalWrite (SWITCH, HIGH);  // internal pull-up

  
  //Enable pin change interrupt on selected pins
  /*
  Interrupts are separated in two groups. Depending on which you use you'll have to change the names of these registers:
  PCINT0...7  -> Use PCMSK0, PCIF0 and PCIE0
  PCINT8...10 -> Use PCMSK1, PCIF1 and PCIE1
  */
  bitClear(GIFR,PCIF0);   //Clear any outstanding interrupts (recommended)
  bitSet(GIMSK,PCIE0);    //Enable pin change interrupts
  bitSet(PCMSK0,PCINT4);  //Enable interrupt on a specific pin
}


void loop ()
{
  for(int i=0; i<5; i++)
  {
    digitalWrite (LED, HIGH);
    delay (1000); 
    digitalWrite (LED, LOW);
    delay (500);
  }
  goToSleep ();
}



void goToSleep ()
{
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  ADCSRA = 0;            // turn off ADC
  power_all_disable ();  // power off ADC, Timer 0 and 1, serial interface
  sleep_enable();
  sleep_cpu();
  //Sleep happens here
  //When the interrupt is triggered, wake up, execute ISR function and continue program execution from here                             
  sleep_disable();   
  power_all_enable();    // power everything back on
}


/*
 Interrupt service routine. Executes on wakeup from sleep.
 Do not use timer or delays here. The argument can be either of thes:
  PCINT0_vect: when using interrupts 0...7
  PCINT1_vect: when using interrupts 8...10
 */
ISR(PCINT0_vect) 
{
 // do something interesting here
}
