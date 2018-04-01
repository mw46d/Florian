/*
 * Pololu A-Star handling:
 *   Adafruit AMG88xx breakout board - IR sensor 8x8 pixles I2C
 *   SSD1306 display 128x64 SPI
 *   3 * RC servos (2 for horizontal/vertical, 1 to open/close the CO2 inflator)
 *   RGB led
 *   Yellow binking leds
 *
 * Connected via USB to ROS/RasPi or Up-Board
 */

#include <SPI.h>
#include <Servo.h>
#include <Wire.h>
#include <Adafruit_AMG88xx.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define HORI_PIN 9
#define VERT_PIN 10
#define VALVE_PIN 11

#define HORI_NORMAL_VALUE 90
#define HORI_MIN_VALUE 20
#define HORI_MAX_VALUE 160

#define VERT_NORMAL_VALUE 90
#define VERT_MIN_VALUE 70
#define VERT_MAX_VALUE 110

#define VALVE_NORMAL_VALUE 90
#define VALVE_OPEN_VALUE 170

#define LED_RED_PIN 5
#define LED_BLUE_PIN 6
#define LED_GREEN_PIN 13

#define YELLOW_LED1 7
#define YELLOW_LED2 8

#define OLED_MOSI  A1  //  9
#define OLED_CLK   A0  // 10
#define OLED_DC    A3  // 11
#define OLED_CS    A4  // 12
#define OLED_RESET A2  // 13
Adafruit_SSD1306 display1(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

Servo horiServo;
Servo vertServo;
Servo valveServo;

Adafruit_AMG88xx amg;

unsigned long yellow_time = 0;
unsigned long blink_time = 0;
unsigned long display_time = 0;

bool yellow_state = false;
bool blink_state = false;
bool track_state = false;

int horiAngle = HORI_NORMAL_VALUE;
int vertAngle = VERT_NORMAL_VALUE;
int leds[] = { 0, LED_RED_PIN, LED_BLUE_PIN };
char display_buffer[4][12];
char input_buffer[32];
unsigned char led_color[3];
unsigned char input_p;

void setup() {
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);

  pinMode(YELLOW_LED1, OUTPUT);
  pinMode(YELLOW_LED2, OUTPUT);

  // little green light;-)
  analogWrite(LED_GREEN_PIN, 1);

  Serial.begin(115200);
  Serial.println("AMG88xx servo");

  display1.begin(SSD1306_SWITCHCAPVCC);
  display1.display();
  delay(2000);
  display1.clearDisplay();
  display1.display();
  display1.setTextSize(2);
  display1.setTextColor(WHITE);
  
  // default settings
  bool status = amg.begin();
  if (!status) {
    Serial.println("Could not find a valid AMG88xx sensor, check wiring!");
    while (1);
  }

  delay(100); // let sensor boot up

  horiServo.attach(HORI_PIN);
  vertServo.attach(VERT_PIN);
  valveServo.attach(VALVE_PIN);

  horiServo.write(HORI_NORMAL_VALUE);
  vertServo.write(VERT_NORMAL_VALUE);
  valveServo.write(VALVE_NORMAL_VALUE);

  delay(1000);

  // remove the strain from the servo
  valveServo.detach();

  for (int i = 0; i < 4; i++) {
    for (int k = 0; k < 12; k++) {
      display_buffer[i][k] = '\0';
    }
  }
  
  for (int i = 0; i < 32; i++) {
    input_buffer[i] = 0;
  }
  input_p = 0;
}

void send_pixels(float *pixels) {
  Serial.print("XXXX");

  for (int i = 0; i < AMG88xx_PIXEL_ARRAY_SIZE; i++) {
    union {
      float f;
      byte b[4];
    } u;

    u.f = pixels[i];
    Serial.write(u.b, 4);
  }
}

void handle_yellow() {
  unsigned long t = millis();

  if (t > yellow_time + 1000) {
    yellow_time = t;
    yellow_state = !yellow_state;

    digitalWrite(YELLOW_LED1, yellow_state);
    digitalWrite(YELLOW_LED2, !yellow_state);
  }
}

void handle_blink() {
  unsigned long t = millis();

  if (t > blink_time + 50) {
    blink_time = t;

    if (blink_state) { 
      analogWrite(LED_RED_PIN, 0);
      analogWrite(LED_BLUE_PIN, 0);
      analogWrite(LED_GREEN_PIN, 0);

      int i = random(0, 3);

      if (i > 0) {
        int k = random(5, 150);

        analogWrite(leds[i], k);
      }
    }
    else {
      analogWrite(LED_RED_PIN, led_color[0]);
      analogWrite(LED_GREEN_PIN, led_color[1]);
      analogWrite(LED_BLUE_PIN, led_color[2]);
    }
  }
}

void handle_display() {
  unsigned long t = millis();

  if (t > display_time + 1000) {
    display_time = t;

    display1.clearDisplay();
    display1.setCursor(0, 0);
    display1.setTextSize(2);
    display1.setTextColor(WHITE);

    for (int l = 0; l < 4; l++) {
      /*
      if (l % 2 == 0) {
        display1.setTextColor(WHITE);
      }
      else {
        display1.setTextColor(BLACK, WHITE);
      }
      */
      display1.println(display_buffer[l]);
    }
    
    display1.display();
  }
}

void clear_line(int l) {
  if (l < 0 || l > 3) {
    return;
  }

  for (int i = 0; i < 12; i++) {
    display_buffer[l][i] = '\0';
  }
}

void send_reply(char c, float temp, int hori_angle) {
  int a;
  int t = (int)(temp * 10);
  int i;

  if (hori_angle > -1000) {
    a = (hori_angle + 270) % 360;
  }
  else {
    a = hori_angle;
  }
  
  Serial.print(c);
  Serial.print(' ');
  Serial.print(a);
  Serial.print(' ');
  Serial.println(temp, 1);

  i = (t / 100);
  display_buffer[3][0] = (i > 0 ? (i + '0') : ' ');
  i = (t / 10) % 10;
  display_buffer[3][1] = i + '0';
  display_buffer[3][2] = '.';
  i = t % 10;
  display_buffer[3][3] = i + '0';

  if (a > -1000) {
    display_buffer[3][4] = ' ';
    display_buffer[3][5] = ' ';
    display_buffer[3][6] = ' ';
    i = (a / 100);
    display_buffer[3][7] = (i > 0 ? (i + '0') : ' ');
    i = (a / 10) % 10;
    display_buffer[3][8] = i + '0';
    i = a % 10;
    display_buffer[3][9] = i + '0';
    display_buffer[3][10] = '\0';
  }
  else {
    display_buffer[3][4] = '\0';
  }
}

void my_delay(int d) {
  if (d <= 0) {
    return;
  }

  unsigned long t = millis();
  unsigned long target_time = t + d;

  while (t < target_time - 60) {
    handle_yellow();
    handle_blink();
    handle_display();

    t = millis();
    if (t < target_time - 20) {
      delay(10);
    }
    
    t = millis();
  }

  t = millis();

  if (t < target_time - 5) {
    delay(target_time - t - 1);
  }
}

void co2_open() {
  valveServo.attach(VALVE_PIN);
  valveServo.write(VALVE_OPEN_VALUE);
  my_delay(10000);
  valveServo.write(VALVE_NORMAL_VALUE);
  my_delay(1000);
  valveServo.detach();
}

int search_max() {
  float pixels[AMG88xx_PIXEL_ARRAY_SIZE];
  float windowMax = -1.0;
  float windowMin = 100.0;
  float maxValue = -1.0;
  int windowIndex = -1;
  int maxAngle;
  float sum = 0;
  int inc = (HORI_MAX_VALUE - HORI_MIN_VALUE) / 4; // about 35 deg


  for (int h = HORI_MIN_VALUE; h <= HORI_MAX_VALUE; h += inc) {
    float sum = 0.0;
    
    windowMax = -1.0;
    windowMin = 100.0;
    windowIndex = -1;
    
    horiServo.write(h);
    vertServo.write(VERT_NORMAL_VALUE);
    // Serial.print(h); Serial.print(": ");

    my_delay(1000);  // wait for at least one AMG read

    // read all the pixels
    amg.readPixels(pixels);

    for (int i = 0; i < AMG88xx_PIXEL_ARRAY_SIZE; i++) {
      sum += pixels[i];

      if (pixels[i] < windowMin) {
        windowMin = pixels[i];
      }
    }

    for (int i = 0; i < AMG88xx_PIXEL_ARRAY_SIZE; i++) {
      // Serial.print(pixels[i], 2); Serial.print(", ");

      if (pixels[i] > windowMax && pixels[i] > windowMin + 3.0) {
        windowMax = pixels[i];
        windowIndex = i / 8;
      }
    }

    // Serial.println("");
    
    if (windowIndex >= 0) {
      int hh = windowIndex - 4;

      if (windowMax > sum / AMG88xx_PIXEL_ARRAY_SIZE + 10.0) {
        int a = h + hh * 7;
        
        send_reply('S', windowMax, a);
        
        // Serial.print("Early exit Max= "); Serial.print(windowMax); Serial.print(", Angle= "); Serial.println(a);
        // early return
        return a;  // about 60 deg FoV for 8 columns
      }

      if (windowMax > maxValue) {
        // Serial.print("windowMax= "); Serial.print(windowMax); Serial.print(", Max= "); Serial.print(maxValue);
        // Serial.print(", h= "); Serial.print(h); Serial.print(", hh= "); Serial.print(hh); Serial.print(", Angle= "); Serial.println(h + hh * 7);
        maxValue = windowMax;
        maxAngle = h + hh * 7;
      }
    }
  }

  // Serial.print("Max= "); Serial.print(maxValue); Serial.print(", Angle= "); Serial.println(maxAngle);
  int a = maxAngle;
  if (a < HORI_MIN_VALUE) {
    a = HORI_MIN_VALUE;
  }
  else if (a > HORI_MAX_VALUE) {
    a = HORI_MAX_VALUE;
  }

  if (maxValue > 45.0) {
    // Serial.println("found");
    horiServo.write(a);
    send_reply('S', maxValue, maxAngle);
    return maxAngle;
  }

  horiServo.write(HORI_NORMAL_VALUE);
  send_reply('S', maxValue, -1000);
  return -1000;
}

int track_max(int &h, int &v) {
  float pixels[AMG88xx_PIXEL_ARRAY_SIZE];
  float windowMax = -1.0;
  float windowMin = 100.0;
  int windowIndex = -1;

  if (h == -1000) {
    return h;
  }
  
  vertServo.write(v);
  horiServo.write(h);

  my_delay(120);
  
  amg.readPixels(pixels);

  for (int i = 0; i < AMG88xx_PIXEL_ARRAY_SIZE; i++) {
    if (pixels[i] < windowMin) {
      windowMin = pixels[i];
    }
  }
  
  for (int i = 0; i < AMG88xx_PIXEL_ARRAY_SIZE; i++) {
    if (pixels[i] > windowMax && pixels[i] > windowMin + 3.0) {
      windowMax = pixels[i];
      windowIndex = i;
    }
  }

  if (windowIndex >= 0) {
    int hh = 4 - (windowIndex / 8);

    h += hh * 2;

    if (h < HORI_MIN_VALUE) {
      h = HORI_MIN_VALUE;
    }
    else if (h > HORI_MAX_VALUE) {
      h = HORI_MAX_VALUE;
    }

    int vv = 4 - (windowIndex % 8);

    v += vv * 2;

    if (v < VERT_MIN_VALUE) {
      v = VERT_MIN_VALUE;
    }
    else if (v > VERT_MAX_VALUE) {
      v = VERT_MAX_VALUE;
    }

    vertServo.write(v);
    horiServo.write(h);

    send_reply('T', windowMax, h);
    return h;
  }

  send_reply('T', 0.0, -1000);
  return -1000;
}

void buffer_line() {
  int l;
  
  if (input_p < 3) {
    return;
  }

  l = input_buffer[1] - '0';

  if (l > 3) {
    return;
  }

  for (int i = 0; i < 12; i++) {
    display_buffer[l][i] = 0;
  }

  for (int i = 0; i < 10 && input_buffer[i + 3] != 0; i++) {
    display_buffer[l][i] = input_buffer[i + 3];
  }
}

void loop() {
  char cmd = '\0';
  static int vertAngle = VERT_NORMAL_VALUE;
  static int horiAndle = HORI_NORMAL_VALUE;
  
  if (Serial.available() > 0) {
    while (Serial.available() > 0) {
      int c = Serial.read();
      // Serial.print(c);

      if (input_p > 0) {
        // handle display string
        if (c == 10 || c == 13) {
          input_buffer[input_p++] = 0;
          buffer_line();
          for (int i = 0; i < 32; i++) {
            input_buffer[i] = 0;
          }

          input_p = 0;
        }
        else {
          if (input_p < 32) {
            input_buffer[input_p++] = c;
          }
        }
      }
      else {
        switch (c) {
          case 'B':
          case 'b':
            blink_state = true;
            Serial.println("OK");
            break;

          case 'D':
          case 'd':
            input_buffer[input_p++] = c;
            break;
            
          case 'G':
          case 'g':
            blink_state = false;
            led_color[0] = led_color[2] = 0;
            led_color[1] = 2;
            Serial.println("OK");
            break;
      
          case 'S':
          case 's':
            cmd = 'S';
            track_state = false;
            Serial.println("OK");
            break;

          case 'T':
          case 't':
            track_state = true;
            Serial.println("OK");
            break;
      
          case 'U':
          case 'u':
            track_state = false;
            horiServo.write(HORI_NORMAL_VALUE);
            vertServo.write(VERT_NORMAL_VALUE);
            clear_line(3);
            Serial.println("OK");
            break;
      
          case 'X':
          case 'x':
            cmd = 'X';
            Serial.println("OK");
            break;

          case 'Y':
          case 'y':
            blink_state = false;
            led_color[0] = 1;
            led_color[1] = 2;
            led_color[2] = 1;
            Serial.println("OK");
            break;
        }
      }
    }
  }

  if (track_state) {
    int h = track_max(horiAngle, vertAngle);

    if (h == -1000) {
      track_state = false;
    }
  }
  else {
    switch (cmd) {
      case 'S':
          vertAngle = VERT_NORMAL_VALUE;
          horiAngle = search_max();
          break;
      case 'X':
          co2_open();
          break;
    }
    
    my_delay(100);
  }
}
