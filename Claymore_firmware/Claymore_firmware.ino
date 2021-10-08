#include <avr/sleep.h>
#include <avr/power.h>    // Power management



///////////////////////////////////////////////////////////
////CONSTANTS//////////////////////////////////////////////
///////////////////////////////////////////////////////////

////BUTTONS
const int BUTTONpin = 4;  //Will be used for an interrupt
const int SHORTPRESSTIME = 800;  //Max milliseconds to consider press as short
const int LONGPRESSTIME = 2000;  //Min milliseconds to consider press as long

///PIR SENSOR
const int PIRpin = 7;
const int CALIBRTIME = 10;  //Seconds to allow the PIR sensor to calibrate after setting the mine (10+ recommended)

////SIGNAL LEDs AND PIEZO
const int LEDpin = 5;
const int PIEZOpin = 8;

////SERVO
const int SERVOpin = 10;
const int SERVOPOWERpin = 9;
const int angleOpen = 140;
const int angleClosed = 30;

////RF RECEIVER
const int RF_A_pin = 1;   //Receiver pin D2
const int RF_B_pin = 3;   //Receiver pin D0
const int RF_C_pin = 0;   //Receiver pin D3
const int RF_D_pin = 2;   //Receiver pin D1


                        
                        
///////////////////////////////////////////////////////////
////GLOBAL VARIABLES///////////////////////////////////////
///////////////////////////////////////////////////////////

////SERVO
boolean servoIsOpen;  //Tracks servo position. Starts closed


////State of the finite state machine
enum state_type{IDLE_SERVO_CLOSED, IDLE_SERVO_OPEN, CALIBRATING, ARMED};
state_type state = IDLE_SERVO_CLOSED;




///////////////////////////////////////////////////////////
////SETUP//////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void setup()
{
  ////BUTTON
  pinMode(BUTTONpin, INPUT_PULLUP);  // Configure the pin as an input, and turn on the pullup resistor.
  
  //PIR Sensor
  pinMode(PIRpin, INPUT);
  
  //SERVO
  pinMode(SERVOpin,OUTPUT);
  pinMode(SERVOPOWERpin,OUTPUT);
  
  ////LEDS & PIEZO
  pinMode(LEDpin,OUTPUT);
  pinMode(PIEZOpin,OUTPUT);

  ////RF RECIEVER
  pinMode(RF_B_pin,INPUT);
  pinMode(RF_D_pin,INPUT);
  pinMode(RF_C_pin,INPUT);
  pinMode(RF_A_pin,INPUT);
  
  //Serial
  //Serial.begin(9600);

  PowerOnSignal();
  delay(100);
}



///////////////////////////////////////////////////////////
///////LOOP////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void loop()
{
  switch (state)
  {

    case IDLE_SERVO_CLOSED:
    { 
      //Serial.println("----------------------------------\nState: IDLE_SERVO_CLOSED");
  
      GoToSleep(); //Goes to sleep until input is detected (button or remote)
  
      delay(5);
      
      //The uC wakes up. Check if user made a short or long button press
      if (digitalRead(BUTTONpin)==LOW)
      {
        //Serial.print("Button pressed: ");
        long pressLength = GetPressLength();  //The button has been pressed. Check how long it's been pressed
       
        if (pressLength < SHORTPRESSTIME)
        {
          //Serial.println("Short");
          openServo();
          state = IDLE_SERVO_OPEN;
        }
        else
        {
          //Serial.println("Long");
          CalibratingSignal();
          state = CALIBRATING;
        }
      }

      else if (digitalRead(RF_A_pin)==HIGH)
      {
        CalibratingSignal();
        state = ARMED;        
      }
      else if (digitalRead(RF_B_pin)==HIGH)
      {
        //Serial.println("Short");
        openServo();
        state = IDLE_SERVO_OPEN;
      }
      else if (digitalRead(RF_C_pin)==HIGH)
      {
        CheckRange();
      }
      
      break;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////

    case IDLE_SERVO_OPEN:
    {
      //Serial.println("----------------------------------\nState: IDLE_SERVO_OPEN");
      GoToSleep(); //Goes to sleep until input is detected (button or remote)

      //The uC wakes up. Check if user made a short or long button press
      if (digitalRead(BUTTONpin)==LOW)
      {  
        //Serial.print("Button pressed: ");
        long pressLength = GetPressLength();  //The button has been pressed. Check how long it's been pressed
        
        if (pressLength < SHORTPRESSTIME)
        {
          //Serial.println("Short");
          closeServo();
          state = IDLE_SERVO_CLOSED;
        }
        else
        {
          //Serial.println("Long");
          closeServo();
          CalibratingSignal();
          state = CALIBRATING;
        }
      }

      else if (digitalRead(RF_B_pin)==HIGH)
      {
        closeServo();
        state = IDLE_SERVO_CLOSED;
      }
      else if (digitalRead(RF_C_pin)==HIGH) 
      {
        CheckRange();
      }
      
      break;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////

    case CALIBRATING:
    {
      delay(1000);  //Give time to release button

      //Wait some time while the PIR is calibrating, but still check for any user input
      //Additionally, don't finish callibration until the PIR has gone low (or else there's a few seconds 
      //after entering ARMED mode when it isn't really able to detect enemies
      boolean deactivated = false;
      long start = millis();
      while((millis()-start < CALIBRTIME*1000) && !(digitalRead(PIRpin)) && !deactivated)
      {
        if(digitalRead(BUTTONpin)==LOW) //Button pressed: Disarm explosive
        {
          deactivated = true;
          DisarmedSignal();
          state = IDLE_SERVO_CLOSED;          
        }
        else if(digitalRead(RF_D_pin)==HIGH)  //Remote D (Deactivate) pressed
        {
          deactivated = true;
          DisarmedSignal();
          state = IDLE_SERVO_CLOSED;
        }
        else if(digitalRead(RF_B_pin)==HIGH)  //Remote B (Boooom!) pressed
        {
          Explode();
          state = IDLE_SERVO_OPEN;
        }

        delay(5);
      }

      if(!deactivated)
      {
        //Calibration finished. Claymore now live and dangerous!
        EndOfCalibrationSignal();
        state = ARMED;
      }
      
      break;
    }


    /////////////////////////////////////////////////////////////////////////////////////////////////////////

    case ARMED:
    {
      //Serial.println("\n----------------------------------\nState: ARMED");
      GoToSleep();

      //Detected PIR --> Explode
      if(digitalRead(PIRpin)==HIGH)
      {
        Explode();
        state = IDLE_SERVO_OPEN;
      }

      //Detected button --> Deactivate
      else if(digitalRead(BUTTONpin)==LOW)
      { 
        //Serial.println("\nButton pressed: Disarming explosive");
        DisarmedSignal();
        state = IDLE_SERVO_CLOSED;
      }

      
      //Detected remote
      else if(digitalRead(RF_B_pin)==HIGH)
      {
        Explode();
        state = IDLE_SERVO_OPEN;
      }
      else if(digitalRead(RF_C_pin)==HIGH)
      {
        CheckRange();
      }
      else if(digitalRead(RF_D_pin)==HIGH)
      {
        //Serial.println("\nRemote 'D' button pressed: Disarming explosive");
        DisarmedSignal();
        state = IDLE_SERVO_CLOSED;
      }
      break;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    default:
      break;

  }

}




///////////////////////////////////////////////////////////
////FUNCTIONS//////////////////////////////////////////////
///////////////////////////////////////////////////////////

//The Tone library is incompatible with ATtiny microcontrollers, so I use this function to play notes on the piezo
void PlayNotePiezo(int freq, int duration)
{
  int halfT = 0.5*1000000/freq;

  long start = millis();
  while(millis()-start < duration)
  {
    digitalWrite(PIEZOpin,HIGH);
    delayMicroseconds(halfT);
    digitalWrite(PIEZOpin,LOW);
    delayMicroseconds(halfT);
  }
}


//Makes a signal when the device is powered on and starts servo at closed position
void PowerOnSignal()
{
  
  digitalWrite(LEDpin,HIGH);
  PlayNotePiezo(300,20);
  delay(500);
  digitalWrite(LEDpin,LOW); 

  closeServo();
  //Serial.println("Program start");
}



//Put the IC to sleep until it receives an external signal from the button or the remote. Also from the PIR when Claymore is armed.
void GoToSleep()
{
  delay(50);
  //Serial.println("Preparing sleep");
  delay(50);

  //Enable pin change interrupt on selected pins
  bitClear(GIFR,PCIF0);   //Clear any outstanding interrupts (recommended)
  bitSet(GIMSK,PCIE0);    //Enable pin change interrupts
  bitSet(PCMSK0,PCINT4);  //Enable interrupt on Pushbutton pin
  if(state==ARMED) bitSet(PCMSK0,PCINT7);  //Enable interrupt on PIR pin
  bitSet(PCMSK0,PCINT0);
  bitSet(PCMSK0,PCINT1);
  bitSet(PCMSK0,PCINT2);
  bitSet(PCMSK0,PCINT3);
  
  //Disable ADC to save energy
  ADCSRA = 0;
 
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  power_all_disable();  // power off ADC, Timer 0 and 1, serial interface
  sleep_enable();

 /*
  //Do not interrupt before we go to sleep, or the ISR will detach interrupts and we won't wake up.
  noInterrupts();
 
  //Attach interrupt to a pin  
  attachPinChangeInterrupt(BUTTONpin, Wake, FALLING);
  attachPinChangeInterrupt(RF_VTpin, Wake, RISING);
  if (state == ARMED) attachPinChangeInterrupt(PIRpin, Wake, RISING);
  
  //Turn off brown-out enable in software
  //BODS must be set to one and BODSE must be set to zero within four clock cycles
  MCUCR = bit(BODS) | bit(BODSE);
  //The BODS bit is automatically cleared after three clock cycles
  MCUCR = bit(BODS);
  */

  sleep_cpu();
  //Sleep happens here
  //When the interrupt is triggered, wake up, execute ISR function and continue program execution from here                             
  sleep_disable();   
  power_all_enable();    // power everything back on
}


ISR(PCINT0_vect) 
{
 //Do nothing
}



//Tracks how long the button has been pressed. If it has been pressed long enough for a long press, automatically returns
long GetPressLength()
{  
  long pressLength = 0;
  long previousTime = millis();
  while((digitalRead(BUTTONpin)==LOW)&&(pressLength<LONGPRESSTIME)){
    pressLength += millis() - previousTime;
    previousTime = millis();
    delay(10);
  }
  return(pressLength);
}



void openServo()
{
  int pulseDuration = map(angleOpen, 0, 180, 500, 2400);

  digitalWrite(SERVOPOWERpin,HIGH);
  delay(10);
  long now = millis();
  while(millis()-now < 500)
  {
    digitalWrite(SERVOpin,HIGH);
    delayMicroseconds(pulseDuration);
    digitalWrite(SERVOpin,LOW);
    delay(20);
  }
  //Serial.println("Opening servo");
  servoIsOpen = true;
  digitalWrite(SERVOPOWERpin,LOW);
}


void closeServo()
{
  int pulseDuration = map(angleClosed, 0, 180, 500, 2400);

  digitalWrite(SERVOPOWERpin,HIGH);
  delay(10);
  long now = millis();
  while(millis()-now < 500)
  {
    digitalWrite(SERVOpin,HIGH);
    delayMicroseconds(pulseDuration);
    digitalWrite(SERVOpin,LOW);
    delay(20);
  }
  //Serial.println("Closing servo");
  servoIsOpen = false;
  digitalWrite(SERVOPOWERpin,LOW);
}



void EndOfCalibrationSignal()
{
  digitalWrite(LEDpin,HIGH);
  PlayNotePiezo(1000,10);
  delay(300);
  digitalWrite(LEDpin,LOW);
}



//Closes the servo and makes a signal to indicate the Claymore will soon be armed and dangerous
void CalibratingSignal()
{
  PlayNotePiezo(500,20);
  delay(50);
  PlayNotePiezo(1200,15);

  for(int i=0;i<20;i++)
  {
    digitalWrite(LEDpin,HIGH);
    delay(50);
    digitalWrite(LEDpin,LOW);
    delay(50);
  }
}



//Makes a signal to indicate the Claymore is now loaded but idle
void DisarmedSignal()
{
  PlayNotePiezo(1200,20);
  delay(50);
  PlayNotePiezo(500,15);

  digitalWrite(LEDpin,HIGH);
  delay(500);
  digitalWrite(LEDpin,LOW);
}



void Explode()
{
  openServo();
  
  for(int i=0;i<4;i++)
  {
    digitalWrite(LEDpin,HIGH);
    PlayNotePiezo(1500,300);
    digitalWrite(LEDpin,LOW);
    delay(150);
    
    digitalWrite(LEDpin,HIGH);
    PlayNotePiezo(500,300);
    digitalWrite(LEDpin,LOW);
    delay(150);
  }
}




//Used when the user wants to check if the remote is in or out of range
void CheckRange()
{
    const int TIME = 4000;
    const int PERIOD = 1000;
    
    boolean audioFeedback = false;
    
    long start = millis();
    while(millis()-start < TIME)
    {
      //Update the LED's PWM value to make a heartbeat pattern
      analogWrite(LEDpin,128+127*cos(2*PI/PERIOD*millis()));  //Sets the LED's brightness following a heartbeat pattern

      //If the remote is held down for some seconds, also use the buzzer   
      if((millis()-start > TIME/2) && (digitalRead(RF_C_pin)==HIGH) && !audioFeedback)
      {
        for(int i=0;i<2;i++)
        {
           PlayNotePiezo(1000,10);
           delay(50);
        }
        audioFeedback = true;
      }
    }

  digitalWrite(LEDpin,LOW);
}
