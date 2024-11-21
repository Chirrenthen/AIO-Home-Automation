#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"
#include <IRremote.h>     // https://github.com/Arduino-IRremote/Arduino-IRremote (3.6.1)
#include <DHT.h>          // https://github.com/adafruit/DHT-sensor-library (1.4.3)
#include <SimpleTimer.h>  // https://github.com/kiryanenko/SimpleTimer (1.0.0)
#include <AceButton.h>    // https://github.com/bxparks/AceButton (1.9.2)
#include <Preferences.h>
Preferences pref;

// Relay State
bool toggleState_1 = LOW; //Define integer to remember the toggle state for relay 1

using namespace ace_button;

// BLE Credentials
const char *service_name = "PROV_chirrenthen"; //name of node in BLE
const char *pop = "Chirrenthen123"; //password

// for Turning On and Off the Serial Monitor
#define DEBUG_SW 1

// By Default all the Relays will be in OFF State
#define DEFAULT_RELAY_STATE false

// define the Node Name
char nodeName[] = "All in One Home Automation";

// GPIO for Relay (Appliance Control)
static uint8_t relay1 = 27;

// GPIO for switch
static uint8_t switch1 = 26;

static uint8_t gpio_reset = 0;   // Reset Pin
static uint8_t IR_SENS    = 25;  // IR Receiver Pin
static uint8_t DHTPIN     = 24;  // DHT11 Pin

// Name of the device shown in the App
char Device1[] = "light1";

// IR Remote Code for Lights
#define IR_Relay1           0x1FE50AF
#define IR_Relay_All_Off    0x1FE48B7
#define IR_Relay_All_On     0x1FE7887

// Relay State
bool switch_state_ch1 = HIGH;

static Switch my_switch1("light1", &relay1);

static TemperatureSensor temperature("Temperature");
static TemperatureSensor humidity("Humidity");

ButtonConfig config1;
AceButton button1(&config1);

void handleEvent1(AceButton*, uint8_t, uint8_t);

DHT dht(DHTPIN, DHT11);
IRrecv irrecv(IR_SENS);
decode_results results;
SimpleTimer Timer;

void sysProvEvent(arduino_event_t *sys_event)
{
  switch (sys_event->event_id)
  {
    case ARDUINO_EVENT_PROV_START:
      if (DEBUG_SW)if (DEBUG_SW)Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on BLE\n", service_name, pop);
      printQR(service_name, pop, "ble");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      if (DEBUG_SW)if (DEBUG_SW)Serial.print("\nConnected IP address : ");
      if (DEBUG_SW)if (DEBUG_SW)Serial.println(IPAddress(sys_event->event_info.got_ip.ip_info.ip.addr));
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (DEBUG_SW)if (DEBUG_SW)Serial.println("\nDisconnected. Connecting to the AP again... ");
      break;
    case ARDUINO_EVENT_PROV_CRED_RECV:
      if (DEBUG_SW)if (DEBUG_SW)Serial.println("\nReceived Wi-Fi credentials");
      if (DEBUG_SW)if (DEBUG_SW)Serial.print("\tSSID : ");
      if (DEBUG_SW)if (DEBUG_SW)Serial.println((const char *) sys_event->event_info.prov_cred_recv.ssid);
      if (DEBUG_SW)if (DEBUG_SW)Serial.print("\tPassword : ");
      if (DEBUG_SW)if (DEBUG_SW)Serial.println((char const *) sys_event->event_info.prov_cred_recv.password);
      break;
    case ARDUINO_EVENT_PROV_INIT:
      network_prov_mgr_disable_auto_stop(10000);
      break;
    case ARDUINO_EVENT_PROV_CRED_SUCCESS:
      if (DEBUG_SW)if (DEBUG_SW)Serial.println("Stopping Provisioning!!!");
      network_prov_mgr_stop_provisioning();
      break;
  }
}

void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx)
{
  const char *device_name = device->getDeviceName();
  const char *param_name = param->getParamName();

  if (strcmp(device_name, Device1) == 0)
  {
    if (DEBUG_SW)if (DEBUG_SW)Serial.printf("Switch value_1 = %s\n", val.val.b ? "true" : "false");

    if (strcmp(param_name, "Power") == 0)
    {
      if (DEBUG_SW)if (DEBUG_SW)Serial.printf("Received value = %s for %s - %s\n", val.val.b ? "true" : "false", device_name, param_name);
      switch_state_ch1 = val.val.b;
      (switch_state_ch1 == false) ? digitalWrite(relay1, LOW) : digitalWrite(relay1, HIGH);
      pref.putBool("Relay1", switch_state_ch1);
      param->updateAndReport(val);
    }
}

void readSensor() {

  float h = dht.readHumidity();
  float t = dht.readTemperature(); // or dht.readTemperature(true) for Fahrenheit

  if (isnan(h) || isnan(t)) {
    if (DEBUG_SW)Serial.println("Failed to read from DHT sensor!");
    return;
  }
  else
  {
    humidity_value = h;
    temperature_value = t;
    if (DEBUG_SW)Serial.print("Temperature - "); if (DEBUG_SW)Serial.println(t);
    if (DEBUG_SW)Serial.print("Humidity - "); if (DEBUG_SW)Serial.println(h);
  }
}

void sendSensor()
{
  readSensor();
  temperature.updateAndReportParam("Temperature", temperature_value);
  humidity.updateAndReportParam("Temperature", humidity_value);
}


// This function will recall the last state
void getRelayState()
{
  toggleState_1 = pref.getBool("Relay1", 0);
  Serial.print("Last State Relay1 - "); Serial.println(toggleState_1);
  digitalWrite(relay1, toggleState_1);
  my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, toggleState_1);
  delay(200);
}

void ir_remote() {
  if (DEBUG_SW)Serial.println("Inside IR REMOTE");
  if (irrecv.decode(&results)) {
    if (DEBUG_SW)Serial.println(results.value, HEX); //print the HEX code
    switch (results.value) {
      case IR_Relay1:
        switch_state_ch1 = !switch_state_ch1;
        digitalWrite(relay1, switch_state_ch1);
        if (DEBUG_SW)Serial.println("RELAY1 ON");
        my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, switch_state_ch1);
        pref.putBool("Relay1", switch_state_ch1);
        delay(100);
        break;
      case IR_Relay_All_Off:
        All_Lights_Off();
        break;
      case IR_Relay_All_On:
        All_Lights_On();
        break;
      default : break;
    }
    irrecv.resume();
  }
}

void All_Lights_Off()
{
  switch_state_ch1 = 0;
  digitalWrite(relay1, HIGH);
  my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, switch_state_ch1);
  pref.putBool("Relay1", switch_state_ch1);
}

void All_Lights_On()
{
  switch_state_ch1 = 1;
  digitalWrite(relay1, LOW);
  my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, switch_state_ch1);
  pref.putBool("Relay1", switch_state_ch1);
}

void setup()
{

  if (DEBUG_SW)Serial.begin(115200);
  pref.begin("Relay_State", false);
   
  // Set the Relays GPIOs as output mode
  pinMode(relay1, OUTPUT);

  //Turning All Relays Off by default
  digitalWrite(relay1, HIGH);

  // Configure the input GPIOs
  pinMode(switch1, INPUT);
  pinMode(gpio_reset, INPUT);

  config1.setEventHandler(button1Handler);

  button1.init(switch1);

  irrecv.enableIRIn(); // Enabling IR sensor
  dht.begin();    // Enabling DHT sensor

  Node my_node;
  my_node = RMaker.initNode(nodeName);

  my_switch1.addCb(write_callback);
  my_node.addDevice(my_switch1);
  my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, switch_state_ch1);
  delay(500);

  my_node.addDevice(temperature);
  my_node.addDevice(humidity);


  Timer.setInterval(30000);

  //This is optional
  RMaker.enableOTA(OTA_USING_PARAMS);
  //If you want to enable scheduling, set time zone for your region using setTimeZone().
  //The list of available values are provided here https://rainmaker.espressif.com/docs/time-service.html
   RMaker.setTimeZone("Asia/Kolkata");
  // Alternatively, enable the Timezone service and let the phone apps set the appropriate timezone
  RMaker.enableTZService();
  RMaker.enableSchedule();

  if (DEBUG_SW)Serial.printf("\nStarting ESP-RainMaker\n");
  RMaker.start();

  WiFi.onEvent(sysProvEvent);
#if CONFIG_IDF_TARGET_ESP32
  WiFiProv.beginProvision(NETWORK_PROV_SCHEME_BLE, NETWORK_PROV_SCHEME_HANDLER_FREE_BTDM, NETWORK_PROV_SECURITY_1, pop, service_name);
#else
  WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP, WIFI_PROV_SCHEME_HANDLER_NONE, WIFI_PROV_SECURITY_1, pop, service_name);
#endif


  getRelayState(); // Get the last state of Relays

  my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, false);
}

void loop()
{
  button1.check();

  // Read GPIO0 (external button to gpio_reset device
  if (digitalRead(gpio_reset) == LOW) {
    //Push button pressed
    if (DEBUG_SW)Serial.printf("reset Button Pressed!\n");
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(gpio_reset) == LOW) delay(50);
    int endTime = millis();

    if ((endTime - startTime) > 5000) {
      // If key pressed for more than 5secs, reset all
      if (DEBUG_SW)Serial.printf("reset to factory.\n");
      RMakerFactoryReset(2);
    }
  }
  delay(100);

  if (WiFi.status() != WL_CONNECTED)
  {
    if (DEBUG_SW)Serial.println("WiFi Not Connected");

  }
  else
  {
    if (DEBUG_SW)Serial.println("WiFi Connected");
    if (Timer.isReady()) {
      if (DEBUG_SW)Serial.println("Sending Sensor Data");
      sendSensor();
      Timer.reset();      // Reset a second timer
    }
  }

  ir_remote(); //IR remote Control
}

//functions for defineing manual switch

void button1Handler(AceButton* button, uint8_t eventType, uint8_t buttonState) {
  if (DEBUG_SW)Serial.println("EVENT1");
  //EspalexaDevice* d1 = espalexa.getDevice(0);
  switch (eventType) {
    case AceButton::kEventPressed:
      if (DEBUG_SW)Serial.println("kEventPressed");
      switch_state_ch1 = true;
      my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, switch_state_ch1);
      digitalWrite(relay1, LOW);
      pref.putBool("Relay1", switch_state_ch1);
      break;
    case AceButton::kEventReleased:
      if (DEBUG_SW)Serial.println("kEventReleased");
      switch_state_ch1 = false;
      my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, switch_state_ch1);
      digitalWrite(relay1, HIGH);
      pref.putBool("Relay1", switch_state_ch1);
      break;
  }
}
