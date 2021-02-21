/* Audio streamer with ESP32 and Adafruit elected microphone board. 
 * Created by Julian Schroeter.
*/
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include <driver/adc.h>

#define AUDIO_BUFFER_MAX 800

uint16_t audioBuffer[AUDIO_BUFFER_MAX];
uint16_t transmitBuffer[AUDIO_BUFFER_MAX];
uint32_t bufferPointer = 0;

const char* ssid     = "Peas For Our Time";
const char* password = "lovenotwar";
const char* host     = "192.168.1.255";
#define port 4444

bool transmitNow = false;


AsyncUDP udpSend; 
AsyncUDP udpRec; 

hw_timer_t * timer = NULL; // our timer
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED; 

void ReadMicInput()
{
  portENTER_CRITICAL_ISR(&timerMux); // says that we want to run critical code and don't want to be interrupted
  int adcVal = adc1_get_raw(ADC1_CHANNEL_0); // reads the ADC
  // uint8_t value = map(adcVal, 0 , 4096, 0, 255);  // converts the value to 0..255 (8bit)
  adcVal = map(adcVal, 0 , 4096, 0, 255);  // converts the value to 0..255 (8bit)

  audioBuffer[bufferPointer] = adcVal; // stores the value
  bufferPointer++;
 
  if (bufferPointer == AUDIO_BUFFER_MAX) { // when the buffer is full
    bufferPointer = 0;
    memcpy(transmitBuffer, audioBuffer, AUDIO_BUFFER_MAX); // copy buffer into a second buffer
    transmitNow = true; // sets the value true so we know that we can transmit now
  }
  portEXIT_CRITICAL_ISR(&timerMux); // says that we have run our critical code
}

void IRAM_ATTR onTimer() {
  ReadMicInput();
}

void Ping2()
{
    Serial.println("recvd!");
}

void setup() {
  
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.println("connecting to wifi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("MY IP address: ");
  Serial.println(WiFi.localIP());
  
  adc1_config_width(ADC_WIDTH_12Bit); // configure the analogue to digital converter
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_0db); // connects the ADC 1 with channel 0 (GPIO 36)

  udpSend.connect(IPAddress(192,168,1,255), port); // have to initialize to use

  udpRec.listen(port + 1);
  udpRec.onPacket([](AsyncUDPPacket packet) {
    Ping2();
  });

  timer = timerBegin(0, 80, true); // 80 Prescaler
  timerAttachInterrupt(timer, &onTimer, true); // binds the handling function to our timer 
  timerAlarmWrite(timer, 125, true);
  timerAlarmEnable(timer);
}

void loop() {
  if (transmitNow) { // checks if the buffer is full
    transmitNow = false;
    udpSend.broadcastTo((uint8_t *)audioBuffer, (size_t)sizeof(audioBuffer), port);
    // client.write((const uint8_t *)audioBuffer, sizeof(audioBuffer)); // sending the buffer to our server
  }
}
