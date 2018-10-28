#include <PinChangeInterrupt.h>
#include <PinChangeInterruptBoards.h>
#include <PinChangeInterruptPins.h>
#include <PinChangeInterruptSettings.h>

#include <Adafruit_NeoPixel.h>

#define FRONT 5           // front pixel data pin
#define REAR 6          // rear pixel data pin
#define PIXEL 16        // number of pixels
#define LEFT_BUTTON 2
#define RIGHT_BUTTON 3
#define STOP_BUTTON 7

#define IDLE_DELAY 25
#define TURN_DELAY 40
#define TIMEOUT 20000   // signal timeout


// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)

Adafruit_NeoPixel frontStrip = Adafruit_NeoPixel(PIXEL, FRONT, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel rearStrip = Adafruit_NeoPixel(PIXEL, REAR, NEO_GRB + NEO_KHZ800);

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.

volatile unsigned long timer = 0;
volatile unsigned long timerStart = 0;

volatile bool leftFlag = false;
volatile bool rightFlag = false;
volatile bool stopFlag = false;

const int LED_L[] = {0,7};
const int LED_R[] = {8,15};
const int LED_F[] = {0,15};

const uint32_t red = strip.Color(255,0,0);
const uint32_t halfRed = strip.Color(127,0,0);
const uint32_t white = strip.Color(127,127,127);

int idleCount = 0;
int idleSlope = 1;
volatile int turnCount = 0;
int turnSlope = 1;

void setup() {
    // put your setup code here, to run once:
    strip.begin();
    strip.show(); // Initialize all pixels to 'off'
    //Serial.begin(9600);
    
    pinMode(LEFT_BUTTON, INPUT_PULLUP);
    pinMode(RIGHT_BUTTON, INPUT_PULLUP);
    pinMode(STOP_BUTTON, INPUT_PULLUP);   // digital input
    //pinMode(STOP_BUTTON, INPUT);            // using internal analog comparator
    //ACSR = B01011000;                       // see pg 299 of manual. Set to toggle change
    attachInterrupt(digitalPinToInterrupt(LEFT_BUTTON), leftFlagISR, RISING);
    attachInterrupt(digitalPinToInterrupt(RIGHT_BUTTON), rightFlagISR, RISING);
    attachPCINT(digitalPinToPCINT(STOP_BUTTON), stopFlagISR, CHANGE);

    leftFlag = false;
    rightFlag = false;
    stopFlag = false;


}

void loop() {
    // put your main code here, to run repeatedly:
    timer = millis();
    //STATE MACHINE
//    Serial.print("leftFlag=");
//    Serial.print(leftFlag,BIN);
//    Serial.print("...rightFlag=");
//    Serial.print(rightFlag,BIN);
//    Serial.print("...stopFlag=");
//    Serial.println(stopFlag,BIN);
//    Serial.print("Timer...");
//    Serial.print(timer, DEC);
//    Serial.print("...timerStart...");
//    Serial.println(timerStart, DEC);
    //Serial.print(digitalRead(STOP_BUTTON),BIN);

    if(leftFlag == 1 && rightFlag == 0 && stopFlag == 0)
    {
        //left turn, full
        leftConfig(LED_F, red);
        strip.show();
        timerCheck(timerStart);
        delay(TURN_DELAY);
    }
    else if(leftFlag == 0 && rightFlag == 1 && stopFlag == 0)
    {
        //right turn, full
        rightConfig(LED_F, red);
        strip.show();
        timerCheck(timerStart);
        delay(TURN_DELAY);
    }
    else if(leftFlag == 0 && rightFlag == 0 && stopFlag == 1)
    {
        //stop, full
        stopConfig(LED_F, red);
        strip.show();
    }
    else if(leftFlag == 1 && rightFlag == 0 && stopFlag == 1)
    {
        //stop and left
        leftConfig(LED_L, red);
        stopConfig(LED_R, red);
        strip.show();
        timerCheck(timerStart);
        delay(TURN_DELAY);
    }
    else if(leftFlag == 0 && rightFlag == 1 && stopFlag == 1)
    {
        //stop and right
        rightConfig(LED_R, red);
        stopConfig(LED_L, red);
        strip.show();
        timerCheck(timerStart);
        delay(TURN_DELAY);
    }
    else
    {
        //State: Idle
        idleConfig(LED_F, red);
        strip.show();
        delay(IDLE_DELAY);        // need a delay or else it's too fast.. 50ms is approx correct.
    }


}

void leftFlagISR()
{
    //INTERRUPT RESPONSE FUNCTION
    //toggle left flag
    leftFlag = !leftFlag;
    rightFlag = false;
    turnCount = 0;
    timerStart = timer;
}

void rightFlagISR()
{
    //INTERRUPT RESPONSE FUNCTION
    //toggle right flag
    rightFlag = !rightFlag;
    leftFlag = false;
    turnCount = 0;    
    timerStart = timer;
}

void stopFlagISR()
{
    //INTERRUPT RESPONSE FUNCTION
    //detects change in stop flag
    stopFlag = !stopFlag;
    turnCount = 0;
    //Serial.print(stopFlag, BIN);
}

//ISR(ANALOG_COMP_vect)
//{
//    //INTERRUPT RESPONSE FUNCTION
//    //detects change in stop flag
//    stopFlag = !stopFlag;
//    turnCount = 0;
//    //Serial.print(stopFlag, BIN);
//}

void timerCheck(unsigned long t)      
{
    if( timer - t > TIMEOUT)
    {
        rightFlag = leftFlag = false;
    }
}

void idleConfig(int ledArray[], uint32_t color)
{
    // idle state
    // passes in an array with 2 values, beginning and end of LED strip to config
    // changes brightness slowly, triangular profile, linear 0 to brightness
    
    // idleSlope determines speed 
    uint8_t rVal = color >> 16;
    uint8_t gVal = color >> 8;
    uint8_t bVal = color;

    int maxVal = max(max(rVal,gVal),bVal);

    if(idleCount <= 0)
    {
        idleSlope = 5;
    }
    else if(idleCount >= maxVal)             //FIX THIS FOR GENERAL COLOUR
    {
        idleSlope = -5;
    }
    
    color = (uint32_t(constrain(rVal-idleCount,0,rVal)) << 16) 
        | (uint32_t(constrain(gVal-idleCount,0,gVal)) << 8) 
        | (uint32_t(constrain(bVal-idleCount,0,bVal)));

    for(int i = ledArray[0]; i <= ledArray[1]; i++) 
    {
        strip.setPixelColor(i,color);  // strip.setPixelColor(n, red, green, blue) or strip.setPixelColor(n, color)
    }
    
    idleCount = constrain(idleCount+idleSlope,0,maxVal);     //without constrain, it can flash high intensity.. kinda neat


}


void rightConfig(int ledArray[], uint32_t color)
{
    // right turn state
    if(ledArray[0]+turnCount == ledArray[1])
    {
        turnSlope = -1;
    } 
    else if (ledArray[0]+turnCount == ledArray[0])
    {
        turnSlope = 1;
    }
  
    for(int i = ledArray[0]; i <= ledArray[0]+turnCount; i++)
    {
        //lights up the correct LEDs
        strip.setPixelColor(i, color);
    }
    for(int j = ledArray[0]+turnCount+1; j <= ledArray[1]; j++)
    {
        //blanks the remaining ones
        strip.setPixelColor(j,0);
    }
    turnCount += turnSlope;
}

void leftConfig(int ledArray[], uint32_t color)
{
    // left turn state
    if(ledArray[0]+turnCount == ledArray[1])
    {
        turnSlope = -1;
    } 
    else if (ledArray[0]+turnCount == ledArray[0])
    {
        turnSlope = +1;
    }
    
    for(int i = ledArray[1]; i >= ledArray[1]-turnCount; i--)
    {
        //lights up the correct LEDs
        strip.setPixelColor(i, color);
    }
    for(int j = ledArray[1]-turnCount-1; j >= ledArray[0]; j--)
    {
        //blanks the remaining ones
        strip.setPixelColor(j,0);
    }
    turnCount += turnSlope;
}

void stopConfig(int ledArray[], uint32_t color)
{
    // stop state, sets all pixels to full red (255)
    for(int i = ledArray[0]; i <= ledArray[1]; i++) 
    {
        strip.setPixelColor(i,color);  // strip.setPixelColor(n, red, green, blue) or strip.setPixelColor(n, color)
    }
}

