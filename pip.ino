// Keep only necessary includes and definitions
#include <Wire.h>
#include <INA219_WE.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "Fonts\monofonto_rg12pt7b.h"
#include "Fonts\Open_Sans_Condensed_Light_18.h"
#include <BlynkSimpleEsp32.h>
#define FONT1 &monofonto_rg12pt7b
#define FONT2 &Open_Sans_Condensed_Light_18 

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite img = TFT_eSprite(&tft);
#define currentThreshold 2

// Simplified menu states
enum MenuState {
  MAIN_SCREEN,
  MENU_SCREEN,
  WIFI_SCREEN,
  EDIT_BRIGHTNESS
};

char auth[] = "ozogc-FyTEeTsd_1wsgPs5rkFazy6L79";
WidgetTerminal terminal(V10);
BlynkTimer timer;
// Core variables
INA219_WE INA1 = INA219_WE(0x40);
MenuState currentState = MAIN_SCREEN;
int selectedMenuItem = 0;
bool connected = false;
unsigned long accumulatedTestTime = 0;
unsigned long lastTestTime = 0;
bool wasTestingPreviously = false;
double ina1_mWh = 0.0;
double ina1_mAh = 0.0;
unsigned long lastSampleTime = 0;
String lastButtonState = "None";
unsigned long lastButtonPress = 0;
float voltage;
float current;
int brightness = 255;
unsigned long buttonHoldStart = 0;
const int HOLD_DELAY = 500;     // Time before acceleration starts (ms)
const int ACCELERATION_RATE = 5; // How many steps to increment when holding

const int btnUp    = 2;
const int btnDown  = 20;     
const int btnLeft  = 10;     
const int btnRight = 21;     
const int btnCenter= 5;
const int LEDPin = 3;
int wifiX = 148;
int wifiY = 76;
void myTimer() 
{
  Blynk.virtualWrite(V21, voltage);
  Blynk.virtualWrite(V22, current);
  Blynk.virtualWrite(V23, ina1_mAh);
  Blynk.virtualWrite(V24, ina1_mWh);
}

String getRawButtonPressed() {
  if (digitalRead(btnUp) == LOW)    return "UP";
  if (digitalRead(btnDown) == LOW)  return "DOWN";
  if (digitalRead(btnLeft) == LOW)  return "LEFT";
  if (digitalRead(btnRight) == LOW) return "RIGHT";
  if (digitalRead(btnCenter) == LOW)return "CENTER";
  return "None";
}

String getDebouncedButton() {
  String currentButton = getRawButtonPressed();
  unsigned long currentTime = millis();
  
  if (currentButton != lastButtonState) {
      // Button state changed
      lastButtonState = currentButton;
      if (currentButton != "None") {
          lastButtonPress = currentTime;
          buttonHoldStart = currentTime;
          return currentButton;  // Only return on initial press
      }
  } else if (currentButton != "None" && 
             (currentTime - lastButtonPress >= 50)) {
      // Button being held
      lastButtonPress = currentTime;
      
      // Only allow continuous presses for UP/DOWN in brightness edit mode
      if (currentState == EDIT_BRIGHTNESS && 
          (currentButton == "UP" || currentButton == "DOWN")) {
          return currentButton;
      }
  }
  
  return "None";
}

#define LIGHTBLUE 0x07FF
#define LIGHTCYAN 0x87FF
// Colors
#define COLOR_VOLTAGE LIGHTBLUE
#define COLOR_CURRENT TFT_GREEN
#define COLOR_MAH TFT_YELLOW
#define COLOR_MWH TFT_RED
#define COLOR_TIME TFT_DARKGREY


const char* ssid = "mikesnet";
const char* password = "springchicken";

#define every(interval) \
  static uint32_t __every__##interval = millis(); \
  if (millis() - __every__##interval >= interval && (__every__##interval = millis()))

void drawWiFiSignalStrength(int32_t x, int32_t y, int32_t radius) { //chatGPT-generated function to draw a wifi icon
    uint32_t color;
    int numArcs;
    color = TFT_CYAN;
    img.fillCircle(x, y+1, 1, color);
    img.drawArc(x, y, radius / 3, radius / 3 - 1, 135, 225, color, TFT_BLACK);  // Arc 1
    img.drawArc(x, y, 2 * radius / 3, 2 * radius / 3 - 1, 135, 225, color, TFT_BLACK);  // Arc 2
    img.drawArc(x, y, radius, radius - 1, 135, 225, color, TFT_BLACK);  // Arc 3
}

void drawMain() {
    img.fillSprite(TFT_BLACK);

    if (WiFi.status() == WL_CONNECTED) {
            drawWiFiSignalStrength(wifiX, wifiY, 9);
    }
    
    // Draw quadrant dividers with shifted centerline (71 instead of 80)
    img.drawFastVLine(71, 0, 60, TFT_DARKGREY);
    img.drawFastHLine(0, 30, 160, TFT_DARKGREY);
    img.drawFastHLine(0, 60, 160, TFT_DARKGREY);

    // Get measurements and format as Strings
    voltage = INA1.getBusVoltage_V();
    current = INA1.getCurrent_mA();
    if (current < 0) {current = 0;}
    
        // Format all numbers to fixed width with padding
        char voltageStr[8], currentStr[8], mahStr[8], mwhStr[8];
        
        snprintf(voltageStr, sizeof(voltageStr), "%1.2f", voltage);
        if (current < 10) {
                snprintf(currentStr, sizeof(currentStr), "  %3.1f", current);
        } else if (current < 100) {
                snprintf(currentStr, sizeof(currentStr), " %4.1f", current);
        } else {
                snprintf(currentStr, sizeof(currentStr), "%5.1f", current);
        }

        if (ina1_mAh <= 0) {
            snprintf(mahStr, sizeof(mahStr), "0.0");
        } else if (ina1_mAh < 10) {
            snprintf(mahStr, sizeof(mahStr), "%1.1f", ina1_mAh);
        } else if (ina1_mAh < 100) {
            snprintf(mahStr, sizeof(mahStr), "%02.0f", ina1_mAh);
        } else {
            snprintf(mahStr, sizeof(mahStr), "%3.0f", ina1_mAh);
        }

        // Format mWh with conditional decimal place and leading zeros
        if (ina1_mWh <= 0) {
            snprintf(mwhStr, sizeof(mwhStr), "00.0");  // Changed format to force 3 leading zeros
        } else if (ina1_mWh < 10) {
            snprintf(mwhStr, sizeof(mwhStr), "0%1.1f", ina1_mWh);  // Changed to force 3 leading zeros
        } else if (ina1_mWh < 100) {
            snprintf(mwhStr, sizeof(mwhStr), "%2.1f", ina1_mWh);  // Changed to force 3 leading zeros
        } else if (ina1_mWh < 1000) {
            snprintf(mwhStr, sizeof(mwhStr), "0%3.0f", ina1_mWh);
        } else {
            snprintf(mwhStr, sizeof(mwhStr), "%4.0f", ina1_mWh);
        }
    
        // Format time with leading zeros
        char timeStr[9];
        int hours = accumulatedTestTime / 3600;
        int minutes = (accumulatedTestTime % 3600) / 60;
        int seconds = accumulatedTestTime % 60;
        sprintf(timeStr, "%02d:%02d:%02d", hours, minutes, seconds);
        
        // Top left - Voltage (moved 9px left)
        img.setFreeFont(FONT1);
        img.setTextColor(COLOR_VOLTAGE);
        img.setTextDatum(ML_DATUM);
        img.drawString(voltageStr, 11, 15);  // Was 20
        img.setFreeFont(FONT2);
        img.setTextDatum(BR_DATUM);
        img.drawString("V", 69, 30);  // Was 78

        // Top right - Current
        img.setFreeFont(FONT1);
        img.setTextColor(COLOR_CURRENT);
        img.setTextDatum(ML_DATUM);
        img.drawString(currentStr, 80, 15);  // Was 80
        img.setFreeFont(FONT2);
        img.setTextDatum(BR_DATUM);
        img.drawString("mA", 160, 30);
        
        // Bottom left - mAh (left aligned in cell)
        img.setFreeFont(FONT1);
        img.setTextColor(COLOR_MAH);
        img.setTextDatum(ML_DATUM);  // Changed to ML_DATUM
        img.drawString(mahStr, 2, 45);  // Align with voltage position
        img.setFreeFont(FONT2);
        img.setTextDatum(BR_DATUM);
        img.drawString("mAh", 69, 60);
        
        // Bottom right - mWh (left aligned in cell)
        img.setFreeFont(FONT1);
        img.setTextColor(COLOR_MWH);
        img.setTextDatum(ML_DATUM);  // Changed to ML_DATUM
        img.drawString(mwhStr, 73, 45);  // Align with current position
        img.setFreeFont(FONT2);
        img.setTextDatum(BR_DATUM);
        img.drawString("mWh", 160, 60);
    
    // Bottom row - Time (centered)
    img.setFreeFont(FONT1);
    img.setTextColor(COLOR_TIME);
    img.setTextDatum(BC_DATUM);
    img.drawString(timeStr, 80, 85);

    img.setTextFont(1);
    img.setTextDatum(BL_DATUM);
    img.setTextColor(TFT_WHITE);

    img.pushSprite(0, 0);
}

void drawMenu() {
  img.fillSprite(TFT_BLACK);
  img.setTextDatum(TL_DATUM);
  img.setTextFont(1);
  img.setTextSize(1);
  if (WiFi.status() == WL_CONNECTED) {
    drawWiFiSignalStrength(wifiX, wifiY, 9);
  }
  
  const char* menuItems[] = {"Connect WiFi", "Brightness", "Main Screen"};
  for(int i = 0; i < 3; i++) {
      if(i == selectedMenuItem && currentState != EDIT_BRIGHTNESS) {
          img.setTextColor(TFT_BLACK, TFT_WHITE);
          img.fillRect(0, 20 + (i * 20), 160, 18, TFT_WHITE);
      } else {
          img.setTextColor(TFT_WHITE, TFT_BLACK);
      }
      img.drawString(menuItems[i], 10, 20 + (i * 20), 2);
      
      // Draw brightness value
      if(i == 1) {  // Brightness menu item
          img.setTextDatum(TR_DATUM);
          if(currentState == EDIT_BRIGHTNESS) {
              img.setTextColor(TFT_BLACK, TFT_WHITE);
              img.fillRect(120, 20 + (i * 20), 35, 18, TFT_WHITE);
          }
          img.drawString(String(brightness), 155, 20 + (i * 20), 2);
          img.setTextDatum(TL_DATUM);
      }
  }
  
  img.pushSprite(0, 0);
}

void drawWiFiScreen() {
    img.fillSprite(TFT_BLACK); 
    img.setTextColor(TFT_WHITE, TFT_BLACK);
    img.setTextDatum(TL_DATUM);
    img.setTextFont(1);
    img.setTextSize(1);
    if (!connected){
      img.drawString("Connecting to WiFi...", 10, 10, 2);
      img.pushSprite(0, 0);
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      WiFi.setTxPower(WIFI_POWER_8_5dBm);
      int attempts = 0;
      
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
          delay(500);
          img.drawString(".", 10 + (attempts * 6), 10, 2);
          attempts++;
      }
      if (WiFi.status() == WL_CONNECTED) {
        ArduinoOTA.setHostname("Pip");
        ArduinoOTA.begin();
        Blynk.config(auth, IPAddress(192, 168, 50, 197), 8080);
        Blynk.connect();
        timer.setInterval(30000L, myTimer); 
        terminal.println("Pip Started.");
        terminal.print("IP address: ");
        terminal.println(WiFi.localIP());
        terminal.flush();
        connected = true;
      } else {
          img.drawString("Connection failed", 10, 10, 2);
      }
    }
    else {
    img.fillSprite(TFT_BLACK); 
    img.drawString("Connected!", 10, 10, 2);
    img.drawString("IP: " + WiFi.localIP().toString(), 10, 30, 2);
    img.drawString("Press any key", 10, 50, 2);
    }
    img.pushSprite(0, 0);
}

void setup() {
  Wire.begin();
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  // Create sprite once
  img.createSprite(160, 80);

  
  pinMode(btnUp, INPUT_PULLUP);
  pinMode(btnDown, INPUT_PULLUP);
  pinMode(btnLeft, INPUT_PULLUP);
  pinMode(btnRight, INPUT_PULLUP);
  pinMode(btnCenter, INPUT_PULLUP);
  pinMode(LEDPin, OUTPUT);
  analogWrite(LEDPin, brightness);
  
  if (!INA1.init()) {
      img.fillSprite(TFT_BLACK);
      img.setTextColor(TFT_WHITE);
      img.drawString("INA219 Init Failed!", 10, 10, 2);
      img.pushSprite(0, 0);
      delay(2000);
  }
  
  INA1.setBusRange(BRNG_16);
}

void loop() {
  // Power measurements with timestamp
  unsigned long currentMillis = millis();
  double deltaHours = (currentMillis - lastSampleTime) / 3600000.0;
  
  if (deltaHours > 0) {
      double power = (double)INA1.getBusPower();
      double current = (double)INA1.getCurrent_mA();
      voltage = INA1.getBusVoltage_V();
      
      ina1_mWh += power * deltaHours;
      ina1_mAh += current * deltaHours;
      
      if (current > currentThreshold) {
          if (!wasTestingPreviously) {
              lastTestTime = currentMillis / 1000;
              wasTestingPreviously = true;
          }
          accumulatedTestTime = accumulatedTestTime + ((currentMillis / 1000) - lastTestTime);
          lastTestTime = currentMillis / 1000;
      } else {
          wasTestingPreviously = false;
      }
      
      lastSampleTime = currentMillis;
  }
  
  // WiFi and remote updates
  if (WiFi.status() == WL_CONNECTED) {
      ArduinoOTA.handle();
      Blynk.run();
      timer.run(); 
  }
  
  // Handle button presses with debouncing
  String button = getDebouncedButton();
  if (button != "None") {
      if (currentState == MAIN_SCREEN) {
          if (button == "CENTER") {
              currentState = MENU_SCREEN;
              selectedMenuItem = 0;
          }
      } 
      else if (currentState == MENU_SCREEN || currentState == EDIT_BRIGHTNESS) {
          if (button == "UP" || button == "DOWN") {
              if (currentState == EDIT_BRIGHTNESS) {
                  // Start tracking hold time on first press
                  if (buttonHoldStart == 0) {
                      buttonHoldStart = millis();
                  }
                  
                  // Calculate how long button has been held
                  unsigned long holdTime = millis() - buttonHoldStart;
                  
                  // Apply change based on direction with acceleration
                  int increment = (holdTime > HOLD_DELAY) ? ACCELERATION_RATE : 1;
                  if (button == "UP") {
                      brightness = min(255, brightness + increment);
                  } else {
                      brightness = max(1, brightness - increment);
                  }
                  analogWrite(LEDPin, brightness);
              } else {
                  // Regular menu navigation - only on initial press
                  if (button == "UP") {
                      selectedMenuItem = (selectedMenuItem > 0) ? selectedMenuItem - 1 : 2;
                  } else {
                      selectedMenuItem = (selectedMenuItem < 2) ? selectedMenuItem + 1 : 0;
                  }
              }
          }
          else if (button == "CENTER") {
              buttonHoldStart = 0;  // Reset hold timer
              if (currentState == EDIT_BRIGHTNESS) {
                  currentState = MENU_SCREEN;
              } else {
                  switch (selectedMenuItem) {
                      case 0:  // WiFi
                          currentState = WIFI_SCREEN;
                          drawWiFiScreen();
                          break;
                      case 1:  // Brightness
                          currentState = EDIT_BRIGHTNESS;
                          break;
                      case 2:  // Main Screen
                          currentState = MAIN_SCREEN;
                          break;
                  }
              }
          }
      }
      else if (currentState == WIFI_SCREEN) {
          if (button != "None") {
              currentState = MAIN_SCREEN;
          }
      }
  }
  
  // Update display based on state

      switch (currentState) {
          case MAIN_SCREEN:
              drawMain();
              break;
          case MENU_SCREEN:
          case EDIT_BRIGHTNESS:
              drawMenu();
              break;
          case WIFI_SCREEN:
              drawWiFiScreen();
              break;
      }

}
