// EN2853 - Embedded Systems
// Medi Box - Programming Assignment 2
// 210682D - Wathudura T.R. - ENTC
// 12 / 05 / 2024

// Press OK to enter the Menu

// Red LED turns ON when the temperature or humidity are not within acceptable ranges
// Blue LED turns on with the alarm

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Libraries
#include <Wire.h>                // For I2C communication
#include <Adafruit_GFX.h>        // For graphics functions
#include <Adafruit_SSD1306.h>    // For OLED display
#include <DHTesp.h>              // For interfacing with DHT temperature and humidity sensor
#include <WiFi.h>                // For connecting to the internet
#include <PubSubClient.h>        // To send and receive MQTT messages (Assignment 2)
#include <ESP32Servo.h>          //To control servo motor (Assignment 2)
#include <NTPClient.h>           // To connect to a NTP server
#include <WiFiUdp.h>             // To send and receive UDP messages


// OLED display
#define SCREEN_WIDTH 128        // OLED display width, in pixels
#define SCREEN_HEIGHT 64        // OLED display height, in pixels
#define OLED_RESET -1           // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0X3C     // From datasheet


// Pins
#define BUZZER 5
#define LED_1 15
#define PB_CANCEL 34
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 35
#define DHTPIN 12
#define LDR1 A0
#define LDR2 A3
#define SERVMO 18

// NTP server
#define NTP_SERVER "pool.ntp.org"
#define UTC_OFFSET_DST 0


// Global Variables

// For Buzzer
int n_notes = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};

// For NTP server
int UTC_OFFSET = 5*60*60 + 30*60;;   // UTC offset in hours (without daylight saving time)

// For time
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;
unsigned long timeNow = 0;
unsigned long timeLast = 0;

// For alarm
bool alarm_enabled = true;
int n_alarms = 3;
int alarm_hours[] = {0, 1, 2};
int alarm_minutes[] = {1, 10, 25};
bool alarm_triggered[] = {false, false, false};

// For menu
int current_mode = 0;
int max_modes = 5;
String modes[] = {"1 - Set Time", "2 - Set Alarm 1", "3 - Set Alarm 2", "4 - Set Alarm 3","5 - Disable Alarms"};

// For schedule (Assignment 2)
bool isScheduledON = false;
unsigned long scheduledOnTime;

// For Light intensity (Assignment 2)
float GAMMA = 0.75;
const float RL10 = 50;
float MIN_ANGLE = 30;
float lux = 0;
String luxName;

// For servo moter (Assignment 2)
int pos = 0;

// For storing data to send through MQTT (Assignment 2)
char tempAr[10];      // Temperature data
char lightAr[10];     // Light intensity data


// Object declarations

// OLED display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// DHT22 sensor object
DHTesp dhtSensor;
// Instance of the WiFi UDP class
WiFiUDP ntpUDP;
//  Instance of the NTPClient class
NTPClient timeClient(ntpUDP);
// WiFi Client (Assignment 2)
WiFiClient espClient;    
// MQTT Client (Assignment 2)
PubSubClient mqttClient(espClient); 
// Instance of the Servo class (Assignment 2)
Servo servo;  

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

void setup() {

  // Set up pins
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);
  pinMode(LDR1, INPUT);
  pinMode(LDR2, INPUT);

  //Start serial communication
  Serial.begin(115200);

  // Set up OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  // Initialize OLED display
  display.display();
  delay(2000);

  // Set up DHT sensor
  dhtSensor.setup(DHTPIN, DHTesp::DHT22);
  // Set up servo moter
  servo.attach(SERVMO, 500, 2400);
  // Set up WiFi
  setupWifi();
  // Set up MQTT (Assignment)
  setupMqtt();

  // Configure NTP time
  timeClient.begin();
  timeClient.setTimeOffset(5.5*3600);
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  // Clear the buffer 
  display.clearDisplay();

  // Display welcome message
  print_line("Welcome", 25, 0, 2);
  print_line("to", 50, 20, 2);
  print_line("MediBox!", 20, 40, 2);

  display.clearDisplay();
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

void loop() {

  // Connecting to MQTT broker (Assignment 2)
  if(!mqttClient.connected()){
    connectToBroker();
  }
  mqttClient.loop();

  // Publish topics with data
  mqttClient.publish("Light_Intensity", lightAr);  //Light Intensity - Highest OF LDR1 and LDR2
  mqttClient.publish("Temperature", tempAr);    // Temperature 

  update_time_with_check_alarm();

  if (digitalRead(PB_OK) == LOW) {
    delay(200);
    go_to_menu();
  }

  check_temp();
  read_ldr() ;
  serv_mo();
  check_schedule();
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to set up MQTT connection (Assignment 2)
void setupMqtt() {

  mqttClient.setServer("test.mosquitto.org", 1883);
  mqttClient.setCallback(recieveCallback);

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to receive the message from the MQTT broker (Assignment 2)
void recieveCallback(char* topic, byte* payload, unsigned int length) {

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char payloadCharAr[length];
  for (int i = 0; i < length; i++){
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }
  Serial.print("\n");
 
  // Receive the minimum angle of the Servo motor
  if (strcmp(topic, "Minimum_Angle") == 0){
    MIN_ANGLE = atoi(payloadCharAr); //ASCII to integer
  }
  // Receive control factor of the Servo motor
  if (strcmp(topic, "Servo_Controller_Factor") == 0) {
    GAMMA = atof(payloadCharAr); //ASCII to float
  }
  // Receive the status of the main switch
  if (strcmp(topic, "MainSwitch_Status") == 0) {
    buzzerOn(payloadCharAr[0] == 't');
  }
  // Receive the scheduled time
  if(strcmp(topic, "Schedule_Time_RX") == 0){
    if(payloadCharAr[0] =='N'){
      isScheduledON = false;
    }else{
      isScheduledON = true;
      scheduledOnTime = atol(payloadCharAr);
    }
  }

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to connect with the broker (Assignment 2)
void connectToBroker() {

  while(!mqttClient.connected()){
    Serial.println("Attempting MQTT connection");
    if(mqttClient.connect("ESP32-12345645454")){
      Serial.println("connected");
      mqttClient.subscribe("Minimum_Angle");
      mqttClient.subscribe("Servo_Controller_Factor");
      mqttClient.subscribe("MainSwitch_Status");
      mqttClient.subscribe("Schedule_Time_RX");
    }else{
      Serial.println("failed");
      Serial.println(mqttClient.state());
      delay(5000);
    }
  }

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to connect to WiFi (Assignment 2)
void setupWifi() {

  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting to WiFi", 0, 0, 2);
  }

  display.clearDisplay();
  print_line("Connected to WiFi", 0, 0, 2);
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to turn On the Buzzer (Assignment 2)
void buzzerOn(bool on){

  if(on){
    tone(BUZZER, C);
  }
  else{
    noTone(BUZZER);

  }
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to get and update time (Assignment 2)
unsigned long getTime(){

  timeClient.update();
  return timeClient.getEpochTime();

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to calculate the Light intensities from LDRs and get the maximum (Assignment 2)
void read_ldr() {

  int analogValue1 = analogRead(LDR1); //INDOOR LDR
  int analogValue2 = analogRead(LDR2); //OUTDOOR LDR

  // Calculate light intensity for LDR1
  float voltage1 = analogValue1 / 1024.0 * 5.0;
  float resistance1 = 2000.0 * voltage1 / (1.0 - voltage1 / 5.0);
  float maxlux1 = pow(RL10 * 1e3 * pow(10, GAMMA) / 322.58, (1 / GAMMA));
  float lux1 = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance1, (1 / GAMMA)) / maxlux1;

  // Calculate light intensity for LDR2
  float voltage2 = analogValue2 / 1024.0 * 5.0;
  float resistance2 = 2000.0 * voltage2 / (1.0 - voltage2 / 5.0);
  float maxlux2 = pow(RL10 * 1e3 * pow(10, GAMMA) / 322.58, (1 / GAMMA));
  float lux2 = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance2, (1 / GAMMA)) / maxlux2;

  // Determine the highest intensity
  float lux = max(lux1, lux2);
  String(lux).toCharArray(lightAr,6);

  Serial.println(lux); 
  // Output the highest intensity
  String highestLDR;
  if (lux1 > lux2) {
    highestLDR = "Left"; // LDR1 has higher intensity
    lux = lux1;
  } else {
    highestLDR = "Right"; // LDR2 has higher intensity
    lux = lux2;
  }
  mqttClient.publish("Highest_LI", highestLDR.c_str());

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

//Function to set the position of the servo motor (Assignment 2)
void serv_mo(){
  //calculating position of servo motor
  pos = MIN_ANGLE + (180 - MIN_ANGLE) * lux * GAMMA;
  servo.write(pos);
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to turn On the buzzer at scheduled time (Assignment 2)
void check_schedule() {
  
  if (isScheduledON){
    unsigned long currentTime = getTime();
    if (currentTime > scheduledOnTime) {
      buzzerOn(true);
      isScheduledON = false;
      mqttClient.publish("Controller", "1");
      mqttClient.publish("Scheduler", "0");
      Serial.println("Scheduled ON");
      Serial.println(currentTime);
      Serial.println(scheduledOnTime);
    }
  }
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to print a line of text in a given text size and the given position
void print_line(String text, int column, int row, int text_size) {

  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text);
  display.display();

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to display the current time 
void print_time_now(void) {
  display.clearDisplay();
  print_line(String(days), 0, 0, 2);
  print_line(":", 20, 0, 2);
  print_line(String(hours), 30, 0, 2);
  print_line(":", 50, 0, 2);
  print_line(String(minutes), 60, 0, 2);
  print_line(":", 80, 0, 2);
  print_line(String(seconds), 90, 0, 2);

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to automatically update the current time
void update_time() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char timeHour[3];
  strftime(timeHour, 3, "%H", &timeinfo);
  hours = atoi(timeHour);

  char timeMinute[3];
  strftime(timeMinute, 3, "%M", &timeinfo);
  minutes = atoi(timeMinute);

  char timeSecond[3];
  strftime(timeSecond, 3, "%S", &timeinfo);
  seconds = atoi(timeSecond);

  char timeDay[3];
  strftime(timeDay, 3, "%d", &timeinfo);
  days = atoi(timeDay);

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to ring an alarm until cancelled by the user
void ring_alarm() {
  display.clearDisplay();
  print_line("MEDICINE TIME!", 0, 0, 2);
  digitalWrite(LED_1, HIGH);

  bool break_happened = false;
  while (break_happened == false && digitalRead(PB_CANCEL) == HIGH) {
    for (int i = 0; i < n_notes; i++) {
      if (digitalRead(PB_CANCEL) == LOW) {
        delay(200);
        break_happened = true;
        break;
      }
      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }

  digitalWrite(LED_1, LOW);
  display.clearDisplay();
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to automatically update the current time while checking for alarms
void update_time_with_check_alarm() {
  update_time();
  print_time_now();

  if (alarm_enabled == true) {
    for (int i = 0; i < n_alarms; i++) {
      if (alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes) {
        ring_alarm();
        alarm_triggered[i] = true;
      }
    }
  }
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to wait for button press in the menu. Returns the pressed button
int wait_for_button_press() {
  while (true) {
    if (digitalRead(PB_UP) == LOW) {
      delay(200);
      return PB_UP;
    }
    else if (digitalRead(PB_DOWN) == LOW) {
      delay(200);
      return PB_DOWN;
    }
    else if (digitalRead(PB_OK) == LOW) {
      delay(200);
      return PB_OK;
    }
    else if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      return PB_CANCEL;
    }
    update_time();
  }
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to navigate through the menu
void go_to_menu() {
  // display.clearDisplay();
  // print_line("MENU",0,0,2);
  // delay(1000);
  while (digitalRead(PB_CANCEL) == HIGH) {
    display.clearDisplay();
    print_line(modes[current_mode], 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      current_mode += 1;
      current_mode = current_mode % max_modes;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      current_mode -= 1;
      if (current_mode < 0) {
        current_mode = max_modes - 1;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      Serial.println(current_mode);
      run_mode(current_mode);
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to set the time from the user
void set_time() {
  int temp_hour = 5;

  while (true) {
    display.clearDisplay();
    print_line("Hour offset:" + String(temp_hour), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_hour += 1;
      if (temp_hour > 14) {
        temp_hour = -12;
      }
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour -= 1;
      if (temp_hour < -12) {
        temp_hour = 14;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      hours = temp_hour;
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  int temp_minute = 30;

  while (true) {
    display.clearDisplay();
    print_line("Minute offset:" + String(temp_minute), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_minute += 15;
      temp_minute = temp_minute % 60;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_minute -= 15;
      if (temp_minute < 0) {
        temp_minute = 45;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      minutes = temp_minute;
      if (temp_hour >= 0){
        UTC_OFFSET = temp_hour*60*60 + temp_minute*60;
      }
      else{
        UTC_OFFSET = -(-temp_hour*60*60 + temp_minute*60);
      }
      configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
      update_time();  
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  display.clearDisplay();
  print_line("Time is  set", 0, 0, 2);
  delay(1000);

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to set an alarm
void set_alarm(int alarm) {
  int temp_hour = alarm_hours[alarm];

  while (true) {
    display.clearDisplay();
    print_line("Enter hour:" + String(temp_hour), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_hour += 1;
      temp_hour = temp_hour % 24;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour -= 1;
      if (temp_hour < 0) {
        temp_hour = 23;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      alarm_hours[alarm] = temp_hour;
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  int temp_minute = alarm_minutes[alarm];

  while (true) {
    display.clearDisplay();
    print_line("Enter minute:" + String(temp_minute), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_minute += 1;
      temp_minute = temp_minute % 60;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_minute -= 1;
      if (temp_minute < 0) {
        temp_minute = 59;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      alarm_minutes[alarm] = temp_minute;
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  display.clearDisplay();
  print_line("Alarm is  set", 0, 0, 2);
  delay(1000);

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to perform the task in the selected mode. The user can set time, set an alarm or cancel an alarm.
void run_mode(int mode) {
  if (mode == 0) {
    set_time();
  }

  else if (mode == 1 || mode == 2 || mode == 3) {
    set_alarm(mode - 1);
  }

  else if (mode == 4) {
    alarm_enabled = false;
    print_line("All alarms are disabled", 0, 0, 2);
    delay(1000);
  }
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to check if temperature and humidity are within acceptable ranges
void check_temp() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  if (data.temperature > 35) {
    display.clearDisplay();
    print_line("Temp is high", 0, 40, 1);
  }
  else if (data.temperature < 25) {
    display.clearDisplay();
    print_line("Temp is low", 0, 40, 1);
  }

  if (data.humidity > 40) {
    display.clearDisplay();
    print_line("Humidity high", 0, 50, 1);
  }
  else if (data.humidity < 20) {
    display.clearDisplay();
    print_line("Humidity low", 0, 50, 1);
  }

    String(data.temperature,2).toCharArray(tempAr, 10);
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//
