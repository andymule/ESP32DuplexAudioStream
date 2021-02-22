/* Duplex ESP32 audio streamer 
 * Started by Julian Schroeter, modified by Andy Muehlhausen 
*/

// TODO would be great to read and fill mic buffer from the ULP just bc its sitting there and should be fast enough

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include <driver/adc.h>
#include <driver/dac.h>

#define AUDIO_BUFFER_MAX 800

uint16_t audioBuffer[AUDIO_BUFFER_MAX];
uint16_t transmitBuffer[AUDIO_BUFFER_MAX];
uint32_t bufferPointer = 0;

const char *ssid = "Peas For Our Time";
const char *password = "lovenotwar";
const char *host = "192.168.1.255"; //broadcast to subnet
#define portsend 4444
#define portrecv 4445

// const int SpeakerPin = GPIO_NUM_32;

bool transmitNow = false;

#define PLAYBACKBUFFFERMAX 8000
uint8_t dataBuffer[PLAYBACKBUFFFERMAX];
int readPointer = 0, writePointer = 1;
int dataInPlaybackBuffer=0;
bool play = false;

AsyncUDP udpSend;
AsyncUDP udpRec;

hw_timer_t *mictimer = NULL;      // our microphone timer
hw_timer_t *playbacktimer = NULL; // our playback timer
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

/////////// Audio Code -- runs on seperate core ///////////
TaskHandle_t AudioTask;

void ReadMicInput()
{
  portENTER_CRITICAL_ISR(&timerMux);         // says that we want to run critical code and don't want to be interrupted
  int adcVal = adc1_get_raw(ADC1_CHANNEL_0); // reads the ADC, LOLIN32 pinout marked VP on board
  // uint8_t value = map(adcVal, 0 , 4096, 0, 255);  // converts the value to 0..255 (8bit)
  // TODO transmit all 12bits for great great audio
  adcVal = map(adcVal, 0, 4096, 0, 255); // converts the value to 0..255 (8bit)

  audioBuffer[bufferPointer] = adcVal; // stores the value
  bufferPointer++;

  if (bufferPointer == AUDIO_BUFFER_MAX)
  { // when the buffer is full
    bufferPointer = 0;
    memcpy(transmitBuffer, audioBuffer, AUDIO_BUFFER_MAX); // copy buffer into a second buffer
    transmitNow = true;                                    // sets the value true so we know that we can transmit now
  }
  portEXIT_CRITICAL_ISR(&timerMux); // says that we have run our critical code
}
void PlaybackAudio()
{
  // Serial.println("playback");
  portENTER_CRITICAL_ISR(&timerMux); // says that we want to run critical code and don't want to be interrupted
  if (play)
  {
    dac_output_voltage(DAC_CHANNEL_1, dataBuffer[readPointer]); // DAC 1 is GPIO 25 on Lolin32

    readPointer++;
    if (readPointer == PLAYBACKBUFFFERMAX)
    {
      readPointer = 0;
    }

    if (getAbstand() == 0)
    {
      Serial.println("Buffer underrun!!!");
      play = false;
    }
  }
  portEXIT_CRITICAL_ISR(&timerMux); // says that we have run our critical code
}
void IRAM_ATTR MicInterupt()
{
  ReadMicInput();
}
void IRAM_ATTR PlaybackInterupt()
{
  PlaybackAudio();
}
void AudioCore(void *pvParameters)
{
  mictimer = timerBegin(0, 80, true);                           // 80 Prescaler, hw timer 0
  playbacktimer = timerBegin(1, 80, true);                      // 80 Prescaler, hw timer 1
  timerAttachInterrupt(mictimer, &MicInterupt, true);           // binds the handling function to our timer
  timerAttachInterrupt(playbacktimer, &PlaybackInterupt, true); // binds the handling function to our timer
  timerAlarmWrite(mictimer, 125, true);
  timerAlarmWrite(playbacktimer, 125, true);
  timerAlarmEnable(mictimer);
  timerAlarmEnable(playbacktimer);

  while (true)
  {
    delay(1000); // keeps watchdog from triggering but essentially useless since we're all on interupts on the audio core
  }
}
/////////// END Audio Code ///////////

void setup()
{

  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  pinMode(LED_BUILTIN, OUTPUT);
  // pinMode(SpeakerPin, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  dac_output_enable(DAC_CHANNEL_1);
  dac_output_enable(DAC_CHANNEL_2);

  pinMode(33, INPUT_PULLUP);
  pinMode(32, INPUT_PULLUP);

  Serial.println("connecting to wifi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("MY IP address: ");
  Serial.println(WiFi.localIP());

  adc1_config_width(ADC_WIDTH_12Bit);                       // configure the analogue to digital converter
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_0db); // connects the ADC 1 with channel 0 (GPIO 36)

  udpSend.connect(IPAddress(192, 168, 1, 255), portsend); // have to initialize to use

  udpRec.listen(portrecv);

  // long gross recvieve function
  udpRec.onPacket([](AsyncUDPPacket packet) { // CANNOT insert a function here that references packet directly, will cause dereft bug/crash
    size_t length = packet.length();
    uint8_t *data = packet.data();

    // TODO should I not do this here? Am I blocking network?
    for (size_t i = 0; i < length; i++)
    {
      dataBuffer[writePointer] = data[i];
      writePointer++;
      if (writePointer == PLAYBACKBUFFFERMAX)
        writePointer = 0;
    }

    dataInPlaybackBuffer += length;
    if (dataInPlaybackBuffer > 2000)
      play = true;
  });
  /// end recv function

  xTaskCreatePinnedToCore(
      AudioCore,       /* Task function. */
      "AudioCoreTask", /* name of task. */
      10000,           /* Stack size of task */
      NULL,            /* parameter of the task */
      1,               /* priority of the task */
      &AudioTask,      /* Task handle to keep track of created task */
      0);              /* pin task to core 0 */
}


// stupid little function to let us test the ADC as a "real audio signal" to send out
bool noiseflip = true;
void GenNoiseDAC2()
{
  noiseflip = !noiseflip;
  if (noiseflip)
    dac_output_voltage(DAC_CHANNEL_2, 0); // DAC 2 is GPIO 25 on Lolin32
  else
    dac_output_voltage(DAC_CHANNEL_2, 255); // DAC 2 is GPIO 25 on Lolin32, 255 is 8 bit max signal
}

void loop()
{
  if (transmitNow)
  { // checks if the buffer is full and sends if so
    transmitNow = false;
    udpSend.broadcastTo((uint8_t *)audioBuffer, (size_t)sizeof(audioBuffer), portsend);
  }
  GenNoiseDAC2(); // test tool for closed loop noise test
}
