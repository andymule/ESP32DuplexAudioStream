/* Duplex ESP32 audio streamer 
*/

// TODO would be great to read and fill mic buffer from the ULP just bc its sitting there and should be fast enough

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include <driver/adc.h>
#include <driver/dac.h>

#define AUDIO_BUFFER_MAX 8000 // maximum allowable audio playback buffer
#define AUDIO_BUFFER_TRANSMIT_PACKET_SIZE 800 // how many samples to capture before transmitting 
#define AUDIO_BUFFER_SIZE_WAIT_TO_PLAY 800 // how many packets to buffer before playback starts

uint8_t audioMicCollectBuffer[AUDIO_BUFFER_TRANSMIT_PACKET_SIZE];
uint8_t audioMicTransmitBuffer[AUDIO_BUFFER_TRANSMIT_PACKET_SIZE];
int audioMicBufferIndex = 0;
int debugpacketcounter = 0;

const char *ssid = "Peas For Our Time";
const char *password = "lovenotwar";
const char *host = "192.168.1.255"; //broadcast to subnet audio data to 
#define portsend 4444
#define portrecv 4445

bool audioMicTransmitNow = false;

uint8_t audioOutputNetworkBuffer[AUDIO_BUFFER_TRANSMIT_PACKET_SIZE];
uint8_t audioOutputPlaybackBuffer[AUDIO_BUFFER_MAX];
bool recieveBufferFull = false;
int audioOutputReadIndex = 0, audioOutputWriteIndex = 1;
int audioDataInPlaybackBuffer = 0;
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
  uint16_t adcVal = adc1_get_raw(ADC1_CHANNEL_0); // reads the ADC, LOLIN32 pinout marked VP on board, GPIO36
  uint8_t value = map(adcVal, 0, 4096, 0, 255);   // converts the value to 0..255 (8bit)
  // TODO transmit all 12bits for great great audio

  audioMicCollectBuffer[audioMicBufferIndex] = value; // stores the value
  audioMicBufferIndex++;

  if (audioMicBufferIndex == AUDIO_BUFFER_TRANSMIT_PACKET_SIZE)
  { // when the buffer is full
    portENTER_CRITICAL_ISR(&timerMux);              // says that we want to run critical code and don't want to be interrupted
    audioMicBufferIndex = 0;
    memcpy(audioMicTransmitBuffer, audioMicCollectBuffer, AUDIO_BUFFER_TRANSMIT_PACKET_SIZE); // copy buffer into a second buffer
    audioMicTransmitNow = true;                                                               // sets the value true so we know that we can transmit now
    portEXIT_CRITICAL_ISR(&timerMux); // says that we have run our critical code
  }
}
void CopyToOutputBuffer(int length) // copies from network to speaker buffer
{
  if (audioDataInPlaybackBuffer + length < AUDIO_BUFFER_MAX) // if our buffer isn't already full TODO drop oldest data instead
  {
    for (int i = 0; i < length; i++)
    {
      audioOutputPlaybackBuffer[audioOutputWriteIndex] = audioOutputNetworkBuffer[i];
      audioOutputWriteIndex++;
      if (audioOutputWriteIndex == AUDIO_BUFFER_MAX)
        audioOutputWriteIndex = 0;
    }
    audioDataInPlaybackBuffer += length;
  }
  else
  {
    Serial.println("Buffer overflow? TOO MUCH DATA WOW");
  }

  if (audioDataInPlaybackBuffer > AUDIO_BUFFER_SIZE_WAIT_TO_PLAY)
    play = true;
}
void PlaybackAudio()
{
  if (recieveBufferFull)
  {
    portENTER_CRITICAL_ISR(&timerMux); // says that we want to run critical code and don't want to be interrupted
    CopyToOutputBuffer(AUDIO_BUFFER_TRANSMIT_PACKET_SIZE);
    recieveBufferFull = false;
    portEXIT_CRITICAL_ISR(&timerMux); // says that we have run our critical code
  }
  if (play)
  {
    dac_output_voltage(DAC_CHANNEL_1, audioOutputPlaybackBuffer[audioOutputReadIndex]); // DAC 1 is GPIO 25 on Lolin32

    audioOutputReadIndex++;
    if (audioOutputReadIndex == AUDIO_BUFFER_MAX)
    {
      audioOutputReadIndex = 0;
    }

    audioDataInPlaybackBuffer -= 1;
    if (audioDataInPlaybackBuffer == 0)
    {
      // TODO this still happens rarely, should switch to a TCP protocol? Or bad parallel work?
      Serial.print("Buffer underrun!!! writeP,readp,packets:");
      Serial.print(audioOutputWriteIndex);
      Serial.print(" ");
      Serial.print(audioOutputReadIndex);
      Serial.print(" ");
      Serial.println(debugpacketcounter);
      debugpacketcounter = 0;
      play = false;
    }
  }
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
  mictimer = timerBegin(0, 80, true);                           // 80 Prescaler, hw timer 1. makes timer have 1us scale
  playbacktimer = timerBegin(1, 80, true);                      // 80 Prescaler, hw timer 0. makes timer have 1us scale?
  timerAttachInterrupt(mictimer, &MicInterupt, true);           // binds the handling function to our timer
  timerAttachInterrupt(playbacktimer, &PlaybackInterupt, true); // binds the handling function to our timer
  timerAlarmWrite(mictimer, 125, true);       // wake every 125us AKA 8khz sampling rate
  timerAlarmWrite(playbacktimer, 125, true);  // same playback rate as capture -- could be independent though
  timerAlarmEnable(mictimer);
  timerAlarmEnable(playbacktimer);

  while (true)
  {
    delay(4900); // keeps watchdog from triggering but essentially useless since we're all on interupts on the audio core
  }
}

// int DEBUGCOUNTER = 0;
/////////// END Audio Code ///////////
void setup()
{

  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  dac_output_enable(DAC_CHANNEL_1);
  dac_output_enable(DAC_CHANNEL_2);

  Serial.println("connecting to wifi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("MY IP address: ");
  Serial.println(WiFi.localIP());

  adc1_config_width(ADC_WIDTH_12Bit);                       // configure the analogue to digital converter
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_0db); // connects the ADC 1 with channel 0 (GPIO 36, VP pin LOLIN32)

  udpSend.connect(IPAddress(192, 168, 1, 255), portsend); // have to initialize to use

  udpRec.listen(portrecv);

  // long gross recvieve function

  udpRec.onPacket([](AsyncUDPPacket packet) { // CANNOT insert a function here that references packet directly, will cause dereft bug/crash
    size_t length = packet.length();
    uint8_t *data = packet.data();
    ++debugpacketcounter;
    // portENTER_CRITICAL_ISR(&timerMux); // says that we want to run critical code and don't want to be interrupted
    if (recieveBufferFull)
      Serial.println("Network backup detected -- packet dropped");
    else
    {
      memcpy(audioOutputNetworkBuffer, data, length);
      recieveBufferFull = true;
    }
    // portEXIT_CRITICAL_ISR(&timerMux); // says that we have run our critical code
    // Serial.println(DEBUGCOUNTER);
    // ++DEBUGCOUNTER;
    // if (DEBUGCOUNTER % 10 == 0)
    // {
    //   // DEBUGCOUNTER = 0;
    //   Serial.println("second");
    // }
    // CopyToOutputBuffer(length);
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
    dac_output_voltage(DAC_CHANNEL_2, 0); // DAC 2 is GPIO 26 on Lolin32
  else
    dac_output_voltage(DAC_CHANNEL_2, 255); // DAC 2 is GPIO 26 on Lolin32, 255 is 8 bit max signal
}

void loop()
{
  if (audioMicTransmitNow)
  { // checks if the buffer is full and sends if so
    udpSend.broadcastTo(audioMicTransmitBuffer, AUDIO_BUFFER_TRANSMIT_PACKET_SIZE, portsend);
    audioMicTransmitNow = false;
    // Serial.print("Sent. Buffsize:");
    // Serial.println(audioDataInPlaybackBuffer);
  }
  GenNoiseDAC2(); // test tool for closed loop noise test
}
