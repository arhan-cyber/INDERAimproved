#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>

// ===========================
// Select camera model in board_config.h
// ===========================
#include "board_config.h"

void startCameraServer();
void setupLedFlash();

// ===========================
// WiFi variables
// ===========================
String selectedSSID = "";
String enteredPassword = "";

// WPA2-Enterprise fields
String eapIdentity = "";
String eapUsername = "";
String eapPassword = "";

// ===========================
// WiFi Interactive Setup
// ===========================
void connectToWiFiInteractive() {
  Serial.println("\nScanning WiFi networks...");

  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("No networks found!");
    return;
  }

  Serial.println("Available networks:");
  for (int i = 0; i < n; ++i) {
    Serial.printf("%d: %s (%ddBm)%s\n",
                  i,
                  WiFi.SSID(i).c_str(),
                  WiFi.RSSI(i),
                  (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " [OPEN]" : "");
  }

  Serial.println("\nEnter network number:");
  while (!Serial.available()) delay(10);

  int choice = Serial.parseInt();
  Serial.read(); // clear buffer

  if (choice < 0 || choice >= n) {
    Serial.println("Invalid choice!");
    return;
  }

  selectedSSID = WiFi.SSID(choice);
  wifi_auth_mode_t auth = WiFi.encryptionType(choice);

  Serial.println("\nSelected: " + selectedSSID);

  // =========================
  // OPEN NETWORK
  // =========================
  if (auth == WIFI_AUTH_OPEN) {
    Serial.println("Connecting to open network...");
    WiFi.begin(selectedSSID.c_str());
  }

  // =========================
  // WPA2-PSK / WPA3-PSK
  // =========================
  else if (auth == WIFI_AUTH_WPA2_PSK ||
           auth == WIFI_AUTH_WPA_WPA2_PSK ||
           auth == WIFI_AUTH_WPA3_PSK) {

    Serial.println("Enter WiFi password:");
    while (!Serial.available()) delay(10);
    enteredPassword = Serial.readStringUntil('\n');
    enteredPassword.trim();

    WiFi.begin(selectedSSID.c_str(), enteredPassword.c_str());
  }

  // =========================
  // WPA2 ENTERPRISE (PEAP)
  // =========================
  else if (auth == WIFI_AUTH_WPA2_ENTERPRISE ||
           auth == WIFI_AUTH_WPA3_ENT_192) {

    Serial.println("WPA2-Enterprise network detected (PEAP)");

    Serial.println("Enter EAP identity:");
    while (!Serial.available()) delay(10);
    eapIdentity = Serial.readStringUntil('\n');
    eapIdentity.trim();

    Serial.println("Enter username:");
    while (!Serial.available()) delay(10);
    eapUsername = Serial.readStringUntil('\n');
    eapUsername.trim();

    Serial.println("Enter password:");
    while (!Serial.available()) delay(10);
    eapPassword = Serial.readStringUntil('\n');
    eapPassword.trim();

    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);

    WiFi.begin(
      selectedSSID.c_str(),
      WPA2_AUTH_PEAP,
      eapIdentity.c_str(),
      eapUsername.c_str(),
      eapPassword.c_str()
    );
  }

  else {
    Serial.println("Unsupported WiFi security type.");
    return;
  }

  WiFi.setSleep(false);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 40) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect.");
  }
}

// ===========================
// Setup
// ===========================
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    config.frame_size = FRAMESIZE_240X240;
  }

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }

  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

#if defined(LED_GPIO_NUM)
  setupLedFlash();
#endif

  // ===========================
  // WIFI SETUP
  // ===========================
  connectToWiFiInteractive();

  if (WiFi.status() == WL_CONNECTED) {
    startCameraServer();

    Serial.print("Camera Ready! http://");
    Serial.println(WiFi.localIP());
  }
}

// ===========================
// Loop
// ===========================
void loop() {
  delay(10000);
}