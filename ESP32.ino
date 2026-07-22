#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <SPI.h>
#include <RF24.h>
#include <BluetoothA2DPSource.h>
#include <pgmspace.h>

#include "bert_audio_from_wav.h"



//--------------------- Wifi -----------------
//Creates ESP32 WiFi access point and web server
const char* ssid = "1881Institute";
const char* password = "FigureItOut!";
WebServer server(80);

//----------nRF24L01------------
//Radio pins and address
constexpr uint8_t CE_PIN = 4;
constexpr uint8_t CSN_PIN = 2;
const uint8_t radioAddress[6] = "Servo";
RF24 radio(CE_PIN, CSN_PIN);

//Data received from transmitter
struct ServoCommand {
  uint8_t mouthAngle;
  uint8_t headAngle;
};
ServoCommand command;


//-------------------------Servos---------------------------------
constexpr uint8_t MOUTH_SERVO_PIN = 14;
constexpr uint8_t HEAD_SERVO_PIN = 27;

Servo myServo;
Servo headServo;

//Mouth movement limits
constexpr int MOUTH_OPEN = 120;
constexpr int MOUTH_CLOSED = 80;

//Head movement limits
constexpr int HEAD_LEFT = 45;
constexpr int HEAD_CENTER = 90;
constexpr int HEAD_RIGHT = 135;

//Current servo positions
int currentHeadAngle = HEAD_CENTER;
int currentMouthAngle = MOUTH_CLOSED;

unsigned long lastRadioMessage = 0;


//--------Bluetooth Audio-----------------
//Bluetooth speaker connection
BluetoothA2DPSource a2dp_source;

//Tracks audio playback
volatile bool audioPlaying = false;
volatile size_t audioPosition = 0;

//Sends audio to Bluetooth speaker
int32_t getAudioData(uint8_t* data, int32_t byteCount){
  if (byteCount <= 0){
    return 0;
  }
  //Send silence when audio is not playing
  if (!audioPlaying){
    memset(data, 0, byteCount);
    return byteCount;
  }
  //Find remaining audio data
  size_t remaining = BERT_AUDIO_SIZE - audioPosition;
  size_t toCopy = (remaining < static_cast<size_t>(byteCount))
                    ? remaining
                    :static_cast<size_t>(byteCount);
  //Copy audio into Bluetooth buffer
  if (toCopy > 0) {
    memcpy_P(data, BERT_AUDIO + audioPosition, toCopy);
    audioPosition += toCopy;
  }

  //Stop playback when audio finishes
  if (toCopy < static_cast<size_t>(byteCount)){
    memset(data + toCopy, 0, byteCount - toCopy);
    audioPlaying = false;
    audioPosition = 0;
  }
  return byteCount;
}
//Starts audio from beginning
void playBertAudio(){
  //restart from begininng each time button is pressed
  audioPosition = 0;
  audioPlaying = true;
  Serial.println("Playing audio...");
}
//--------------------------Servo Functions----------------------------
//Moves mouth servo
void setMouthAngle(int angle){
  angle = constrain(angle, MOUTH_CLOSED, MOUTH_OPEN);
  //Keep mouth within limits
  if(abs(angle - currentMouthAngle) >= 2){
    currentMouthAngle = angle;
    myServo.write(currentMouthAngle);
  }
}
//Moves head servo
void setHeadAngle(int angle){
  //Keep head within limits
  angle = constrain(angle, HEAD_LEFT, HEAD_RIGHT);

  if(abs(angle - currentHeadAngle) >= 2){
    currentHeadAngle = angle;
    headServo.write(currentHeadAngle);
  }
}
//Opens mouth
void mouthOpen(){
    setMouthAngle(MOUTH_OPEN);
    delay(15);
  }
//Closes mouth
void mouthClosed(){
  setMouthAngle(MOUTH_CLOSED);
  delay(15);
}
//Tracks previous mouth position
bool mouthWasClosed = true;




//--------------------------Web Page-------------------------------------------------
//Control page displayed on phone/computer
const char PAGE[] PROGMEM = 
R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
  <meta name="viewport"
content="width=device-width, initial-scale=1">
  <title>Sock Puppet Control</title>
  <style>
  body {
    font-family: Arial, sans-serif;
    min-height: 100vh;
    background: black;
    display: flex;
    justify-content: center;
    align-items: flex-start;
    color:white;
    margin:0;
    text-align: center;

    }
  button {
    display: block;
    width: 70%;
    min-width: 260px;
    height: 120px;
    margin: 0 auto 40px;
    border: none;
    color: white;
    font-size: 38px;
    border-radius: 22px;
    cursor:pointer;
    }
  .controlPanel{
    width: 90%;
    max-width: 600px;
    min-height: 90vh;
    margin-top: 20px;
    padding: 30px 20px;
    background: #111;
    border: 2px solid #252525;
    border-radius: 35px;
    box-sizing: border-box;
    text-align: center;
  }
  h1{
    font-size: 54px;
    margin: 20px 0 70px;
  }
  .mouthOpen{
    background: #ADD8E6;
    color: white;
  }
  #status{
    margin-top: 80px;
    font-size: 36px;

  }
  button:active{
    transform: scale(0.97);
  }
  .mouthClosed{
    background: #ADD8E6;
    color: white;
  }

</style>

</head>
<body>
<div class="controlPanel">
  <h1> Sock Puppet Control</h1>
  <button class="mouthOpen" onclick="sendCommand('/open')">Open Mouth</button>
  <button class="mouthClosed" onclick="sendCommand('/close')">Close Mouth</button>
  <button class="talkButton" onclick="sendCommand('/talk')">Talk</button>

  <p id="status"> Ready</p>
  
  </div>
  <script>
    function sendCommand(route) {
      fetch(route)
      .then(response => response.text())
      .then(message => {
        document.getElementById("status").innerText = message;
      })
      .catch(error => {
        document.getElementById("status").innerText = "Connection error";
      });
    }
  </script>
</body>
</html>
)rawliteral";

//--------------ESP32 web handlers-------------

//Loads control webpage
void handleRoot() {
  server.send(200, "text/html", PAGE);
}

//Opens mouth from webpage
void handleOpen() {
  mouthOpen();
  server.send(200, "text/plain", "Mouth Open");
}

//Closes mouth from webpage
void handleClosed() {
  mouthClosed();
  server.send(200, "text/plain", "Close Mouth");
}

//Plays audio from webpage
void handleAudio() {
  playBertAudio();
  server.send(200, "text/plain", "playing audio");
}



//----------------------Setup----------------------------------------------
void setup() {
  //Start serial monitor
  Serial.begin(115200);

  //Allow the ESP32 to allocate PWM timers
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);

  //Set servo frequency
  myServo.setPeriodHertz(50);
  headServo.setPeriodHertz(50);


  //Attach servos
  myServo.attach(MOUTH_SERVO_PIN, 500, 2500);
  headServo.attach(HEAD_SERVO_PIN, 500, 2500);


  //Set starting positions
  myServo.write(MOUTH_CLOSED);
  headServo.write(HEAD_CENTER);

  delay(500);

  //----------------------- WiFi Setup--------------------------
  //Start ESP32 access point
  WiFi.softAP(ssid, password);

  //Start webpage routes
  server.on("/", handleRoot);
  server.on("/open", handleOpen);
  server.on("/close", handleClosed);
  server.on("/talk", handleAudio);

  //Start web server
  server.begin();

  Serial.print("Open: http://");
  Serial.println(WiFi.softAPIP());

  //------------------------------- Radio Setup ---------------------------
  //Start SPI and nRF24L01
  SPI.begin();
  if (!radio.begin()) {
    Serial.println("nRF24L01 was not detected.");
  } else {

  //Configure radio
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(100);

  //Listen for transmiter
  radio.openReadingPipe(1, radioAddress);
  radio.startListening();

  Serial.println("Servo receiver ready.");
}
lastRadioMessage = millis();

//----------Bluetooth A2DP-----------------------------------
//Automatically reconnect setup
a2dp_source.set_auto_reconnect(true);

//Set audio callback
a2dp_source.set_data_callback(getAudioData);

//Connect to Bluetooth Speaker
Serial.println("Looking for Bluetooth speaker: Soundcore Flare");
a2dp_source.start("Soundcore Flare");

Serial.println("Puppet receiver ready.");
}

//----------Main Loop---------------------------------------------
void loop() {
  //Handle webpage commands
  server.handleClient();
//-----------------------------Radio Commands----------------------
  if (radio.available()) {
    // Read the newest transmitter command
    while (radio.available()) {
      radio.read(&command, sizeof(command));
    }
    //Detect mouth opening
    bool mouthIsOpening = command.mouthAngle > (MOUTH_CLOSED + 8);

    //Start audio once when mouth first opens
    if (mouthIsOpening && mouthWasClosed && !audioPlaying){
      playBertAudio();
      Serial.println("Mouth opened - starting Bert audio");
    }

    //Remember mouth state 
    mouthWasClosed = !mouthIsOpening;

    //Joystick continues controlling the mouth
    setMouthAngle(command.mouthAngle);

    //Joystick controls head
    setHeadAngle(command.headAngle);

    lastRadioMessage = millis();
  }

  //Display servo positions 
  Serial.print("Mouth: ");
  Serial.println(currentMouthAngle);
  Serial.print("Head: ");
  Serial.println(currentHeadAngle);

  }
}







