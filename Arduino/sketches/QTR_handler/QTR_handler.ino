/* Pololu A-Star handling
 *   2 * QTR sensors (left & right)
 *   1 * 8-sensor line (middle)
 *
 * Connected via USB
 */
#include <QTRSensors.h>

#define NUM_SENSORS   10     // number of sensors used
#define TIMEOUT       2500  // waits for 2500 microseconds for sensor outputs to go low
#define EMITTER_PIN   0     // emitter is controlled by digital pin 2

QTRSensorsRC qtrrc((unsigned char[]) { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
  NUM_SENSORS, TIMEOUT, EMITTER_PIN); 
unsigned int sensorValues[NUM_SENSORS];

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Calibration");
  
  delay(2000);
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);    // turn on Arduino's LED to indicate we are in calibration mode
  for (int i = 0; i < 800; i++)  // make the calibration take about 10 seconds
  {
    qtrrc.calibrate();       // reads all sensors 10 times at 2500 us per read (i.e. ~25 ms per call)
  }
  digitalWrite(13, LOW);     // turn off Arduino's LED to indicate we are through with calibration

  // print the calibration minimum values measured when emitters were on
  Serial.print("Min   ");
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    Serial.print(qtrrc.calibratedMinimumOn[i]);
    Serial.print('\t');
  }
  Serial.println();
  
  // print the calibration maximum values measured when emitters were on
  Serial.print("Max   ");
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    Serial.print(qtrrc.calibratedMaximumOn[i]);
    Serial.print('\t');
  }
  Serial.println();
  Serial.println();
  delay(20000);
}


void loop() {
  // read calibrated sensor values and obtain a measure of the line position from 0 to 5000
  // To get raw sensor values, call:
  qtrrc.read(sensorValues); // instead of unsigned int position = qtrrc.readLine(sensorValues);
  // unsigned int position = qtrrc.readLine(sensorValues);

  // print the sensor values as numbers from 0 to 1000, where 0 means maximum reflectance and
  // 1000 means minimum reflectance, followed by the line position
  for (unsigned char i = 0; i < NUM_SENSORS; i++)
  {
    Serial.print(sensorValues[i]);
    Serial.print('\t');
  }
  Serial.println();

  unsigned int position = qtrrc.readLine(sensorValues);
  for (unsigned char i = 0; i < NUM_SENSORS; i++)
  {
    Serial.print(sensorValues[i]);
    Serial.print('\t');
  }
  Serial.println(position);

  
  delay(250);
}
