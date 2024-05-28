// EN2853 - Embedded Systems
// Medi Box - Programming Assignment 1
// 210682D - Wathudura T.R. - ENTC
// 07 / 03 / 2024

// Press OK to enter the Menu
// Enter UTC Offset to set time

// Red LED turns ON when the temperature or humidity are not within acceptable ranges
// Blue LED turns on with the alarm

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Libraries  
#include <Wire.h>              // For I2C communication
#include <Adafruit_GFX.h>      // For graphics functions
#include <Adafruit_SSD1306.h>  // For OLED display
#include <DHTesp.h>            // For interfacing with DHT temperature and humidity sensor
#include <WiFi.h>              // For connecting to the internet
#include <time.h>              // For time/date manipulation and formatting
#include <NTPClient.h>         // To connect to a NTP server
#include <WiFiUdp.h>           // To send and receive UDP messages


// Definitions

// NTP server
#define NTP_SERVER "pool.ntp.org"   // NTP server used for obtaining time
#define UTC_OFFSET_DST 0            // UTC offset in hours during daylight saving time

// OLED display
#define SCREEN_WIDTH 128          // OLED display width, in pixels
#define SCREEN_HEIGHT 64          // OLED display height, in pixels
#define OLED_RESET -1             // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C       // From datasheet

// Pins
#define BUZZER 18
#define LED_1 15
#define LED_2 2
#define CANCEL 34
#define UP 35
#define DOWN 32
#define OK 33
#define DHT 12


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
int UTC_OFFSET = 0;            // UTC offset in hours (without daylight saving time)

// For time
int hours = 0;
int minutes = 0;
int seconds = 0;


// For alarm
bool alarm_enabled = true;
int num_alarms = 3;
int alarm_hours[] = {-1,-1,-1}; // Stop Alarm from ringing at start 
int alarm_minutes[] = {0,0,0};
bool alarm_triggered[] = {false, false, false};

// For DHT sensor
int max_temp = 32;
int min_temp = 26;
int max_humidity = 80;
int min_humidity = 60;

// For menu
int current_mode = 0;
int max_modes = 5;
String options[] = {"1 - Set Time", "2 - Set Alarm 1", "3 - Set Alarm 2", "4 - Set Alarm 3", "5 - Remove Alarms"};


// Object declarations

// OLED display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);  
// DHT22 sensor object
DHTesp dhtSensor;   
// instance of the WiFi UDP class
WiFiUDP ntpUDP;
//  instance of the NTPClient class
NTPClient timeClient(ntpUDP, NTP_SERVER, UTC_OFFSET);        

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

void setup() {

  //Start serial communication
  Serial.begin(9600);

  // Set up pins
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  pinMode(CANCEL, INPUT);
  pinMode(UP, INPUT);
  pinMode(DOWN, INPUT);
  pinMode(OK, INPUT);

  // Set up DHT sensor
  dhtSensor.setup(DHT, DHTesp::DHT22);

  // Set up OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed!"));
    for (;;);  //Don't proceed, loop forever
  }

  // Initialize OLED display
  display.display();
  delay(2000);

  // Clear the buffer 
  display.clearDisplay();
  // Display welcome message
  print_line("Welcome to Medibox!" , 2, 0, 0);
  delay(3000);

  // Connect to WiFi
  WiFi.begin("Wokwi-GUEST", "", 6);
  while(WiFi.status() != WL_CONNECTED){
    delay(250);
    display.clearDisplay();
    print_line("Connecting to WiFi", 2, 0, 0);
  }
  
  display.clearDisplay();
  print_line("Connected to WiFi", 2, 0, 0);

  // Configure NTP time
  timeClient.begin();

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

void loop() {

  delay(500);
  
  timeClient.update(); // Update NTP time
  update_time_with_check_alarm();
  if(digitalRead(OK) == LOW){
    delay(250);
    go_to_menu();
  }
  check_temp();

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to print a line of text in a given text size and the given position
void print_line(String message, int text_size, int row, int column){

  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(message);
  display.display();

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to set the UTC offset from the user
void setTimeZone() {

  float new_UTC_OFFSET = 0;

  while (true) {


    display.clearDisplay();
    print_line("Enter UTC offset", 0, 0, 1);
    print_line(String(new_UTC_OFFSET), 0, 10, 1);

    int pressed = wait_for_button_press();
    if (pressed == UP) {
      delay(100);
      new_UTC_OFFSET += 0.5;                  // Increase by half an hour
    } 
    else if (pressed == DOWN) {
      delay(100);
      new_UTC_OFFSET -= -0.5;                 // Decrease by half an hour
    }
    else if (pressed == OK) {
      delay(100);
      UTC_OFFSET = new_UTC_OFFSET * 3600;     // Convert to seconds
      timeClient.setTimeOffset(UTC_OFFSET);
      display.clearDisplay();
      print_line("Time Zone Set", 0, 0, 2);
      delay(1000);
      break;
    }
    else if (pressed == CANCEL) {
      delay(100);
      break;
    }
  }

} 

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to automatically update the current time
void update_time() {

  // Update time based on NTP server
  hours = timeClient.getHours();
  minutes = timeClient.getMinutes();
  seconds = timeClient.getSeconds();

  display.clearDisplay();

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to display the current time 
void print_time_now(){

  display.clearDisplay();
  print_line(String(hours), 2, 10, 0);
  print_line(":", 2, 10, 20);
  print_line(String(minutes), 2, 10, 30);
  print_line(":", 2, 10, 50);
  print_line(String(seconds), 2, 10, 60);
  display.display();

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to automatically update the current time while checking for alarms
void update_time_with_check_alarm(){

  display.clearDisplay();
  update_time();
  print_time_now();

  // Check for alarms
  if(alarm_enabled){
    for (int i = 0; i < num_alarms; i++){
      if(alarm_triggered[i] == false && hours == alarm_hours[i] && minutes == alarm_minutes[i]){
        ring_alarm(); // Call the ringing function
        alarm_triggered[i] = true;
      }
    }
  }

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to ring an alarm until cancelled by the user
void ring_alarm(){
  
  // Show message on display
  display.clearDisplay();
  print_line("Medicine  Time", 2, 0, 0); // Print the title on the display

  // Light up LED 1
  digitalWrite(LED_1, HIGH); 

  // Loop until alarm is cancelled
  while(digitalRead(CANCEL) == HIGH){
    for( int i = 0; i < n_notes; i++){
      if(digitalRead(CANCEL) == LOW){
        delay(100);
        break;
      }
      tone(BUZZER, notes[i]);  // Play the tones
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }

  delay(200);
  // Switch off LED 1
  digitalWrite(LED_1,  LOW);

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to wait for button press in the menu. Returns the pressed button
int wait_for_button_press(){

  while(true){
    if (digitalRead(UP) == LOW){
      delay(100);
      return UP;
    }

    else if (digitalRead(DOWN) == LOW){
      delay(100);
      return DOWN;
    }

    else if (digitalRead(CANCEL) == LOW){
      delay(100);
      return CANCEL;
    }

    else if (digitalRead(OK) == LOW){
      delay(100);
      return OK;
    }

    update_time();
  }

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to navigate through the menu
void go_to_menu(void) { 

  while (digitalRead(CANCEL) == HIGH) {
    display.clearDisplay(); 
    print_line(options [current_mode], 1.5, 0, 0);

    int pressed = wait_for_button_press();

    if (pressed == UP) {
      current_mode += 1;
      current_mode %= max_modes; 
      delay(200);
    }

    else if (pressed == DOWN) { 
      current_mode -= 1;
      if (current_mode < 0) { 
        current_mode = max_modes - 1;
      } 
      delay(200);
    } 

    else if (pressed == OK) {
      delay(200);
      run_mode(current_mode);
    }
  }

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to perform the task in the selected mode. The user can set time, set an alarm or cancel an alarm.
void run_mode(int mode){

  // Set time in mode 0
  if (mode == 0){
    setTimeZone();
    update_time(); 
  }

  // Set an alarm in mode 1 or mode 2 or mode 3
  else if (mode == 1 || mode == 2 || mode == 3){
    set_alarm(mode-1);
  }

  // Disable all alarms in mode 4
  else if (mode == 4){
    alarm_enabled = false;
    display.clearDisplay();
    print_line("All alarms disabled!",0, 0, 2);
    delay(1000);
  }

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to set an alarm
void set_alarm(int alarm){

  int temp_hour = alarm_hours[alarm];
  // Loop until the the alarm hour is set or the operation is cancelled
  while (true){
    display.clearDisplay();
    // Display current hour
    print_line("Enter hour: " + String(temp_hour) , 0, 0, 2);

    // Wait for user to press a button
    int pressed = wait_for_button_press();

    // Increase hour by 1 if UP is pressed
    if (pressed == UP){
      delay(200);
      temp_hour += 1;
      temp_hour = temp_hour % 24;
    }

    // Decrease hour by 1 if DOWN is pressed
    else if (pressed == DOWN){
      delay(200);
      temp_hour -= 1;
      if (temp_hour < 0){
        temp_hour = 23;
      }
    }

    // Update the alarm hour and break the loop if the OK is pressed 
    else if (pressed == OK){
      delay(200);
      alarm_hours[alarm]  = temp_hour;
      break;
    }

    // Break the loop and cancel the operation if the CANCEL is pressed 
    else if (pressed == CANCEL){
      delay(200);
      break;
    }
  }

  
  int temp_minute = alarm_minutes[alarm];
  // Loop until the the alarm minute is set or the operation is cancelled
  while (true){
    display.clearDisplay();
    // Display current minute
    print_line("Enter minute: " + String(temp_minute) , 0, 0, 2);

    // Wait for user to press a button
    int pressed = wait_for_button_press();

    // Increase minute by 1 if UP is pressed
    if (pressed == UP){
      delay(200);
      temp_minute += 1;
      temp_minute = temp_minute % 60;
    }

    // Decrease minute by 1 if DOWN is pressed
    else if (pressed == DOWN){
      delay(200);
      temp_minute -= 1;
      if (temp_minute < 0){
        temp_minute = 59;
      }
    }

    // Update the alarm minute and break the loop if the OK is pressed 
    else if (pressed == OK){
      delay(200);
      alarm_minutes[alarm]  = temp_minute;
      break;
    }

    // Break the loop and cancel the operation if the CANCEL is pressed 
    else if (pressed == CANCEL){
      delay(200);
      break;
    }
  }

  display.clearDisplay();
  //Show confirmation message
  print_line("Alarm is set!", 0, 0, 2);
  delay(1000);

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//

// Function to check if temperature and humidity are within acceptable ranges
void check_temp(){

  // Read temperature and humidity data
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  bool all_good = true;

  // If temperature is higher than max_temp(32), turn on LED 2 and display a message
  if (data.temperature > max_temp){
    all_good = false;
    digitalWrite(LED_2, HIGH);
    print_line("Temperature is High!", 1, 40, 0);
  }
  
  // If temperature is less than min_temp(26), turn on LED 2 and display a message
  else if (data.temperature < min_temp){
    all_good = false;
    digitalWrite(LED_2, HIGH);
    print_line("Temperature is Low!", 1, 40, 0);
  }

  // If humidity is greater than max_humidity(80%), turn on LED 2 and display a message
  if (data.humidity > max_humidity){
    all_good = false;
    digitalWrite(LED_2, HIGH);
    print_line("Humidity is High!", 1, 50, 0);
  }
  // If humidity is less than min_humidity(60%), turn on LED 2 and display a message
  else if (data.humidity < min_humidity){
    all_good = false;
    digitalWrite(LED_2, HIGH);
    print_line("Humidity is Low!", 1, 50, 0);
  }

  // If both temperature and humidity are within acceptable ranges, turn off LED 2
  if (all_good){
    digitalWrite(LED_2, LOW);
  }

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//