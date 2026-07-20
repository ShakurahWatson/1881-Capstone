#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <SPI.h>
#include <RF24.h>



//--------------------- Wifi -----------------
const char* ssid = "1881Institute";
const char* password = "FigureItOut!";
WebServer server(80);

//----------nRF24L01------------
constexpr uint8_t CE_PIN = 4;
constexpr uint8_t CSN_PIN = 2;
const uint8_t radioAddress[6] = "Servo";
RF24 radio(CE_PIN, CSN_PIN);

struct ServoCommand {
  uint8_t mouthAngle;
  uint8_t headAngle;
};
ServoCommand command;


//----------Servo Function------------
constexpr uint8_t MOUTH_SERVO_PIN = 14;
constexpr uint8_t HEAD_SERVO_PIN = 27;

Servo myServo;
Servo headServo;

constexpr int MOUTH_OPEN = 120;
constexpr int MOUTH_CLOSED = 80;

constexpr int HEAD_LEFT = 45;
constexpr int HEAD_CENTER = 90;
constexpr int HEAD_RIGHT = 135;





int currentHeadAngle = HEAD_CENTER;
int currentMouthAngle = MOUTH_CLOSED;

unsigned long lastRadioMessage = 0;

void setMouthAngle(int angle){
  angle = constrain(angle, MOUTH_CLOSED, MOUTH_OPEN);

  if(abs(angle - currentMouthAngle) >= 2){
    currentMouthAngle = angle;
    myServo.write(currentMouthAngle);
  }
}

void setHeadAngle(int angle){
  angle = constrain(angle, Head_LEFT, HEAD_RIGHT);

  if(abs(angle - currentHeadhAngle) >= 2){
    currentHeadAngle = angle;
    headServo.write(currentHeadAngle);
  }
}

void mouthOpen(){
    setMouthAngle(MOUTH_OPEN);
    delay(15);
  }

void mouthClosed(){
  setMouthAngle(MOUTH_CLOSED);
  delay(15);
}
void talk(){
  for (int i = 0; i < 10; i++){
    myServo.write(MOUTH_OPEN);
    delay(100);

    myServo.write(MOUTH_CLOSED);
    delay(100);
  }
  myServo.write(MOUTH_CLOSED);
}



//-----------Web Page-------------
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
<div class=controlPanel">
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
void handleRoot() {
  server.send(200, "text/html", PAGE);
}

void handleOpen() {
  mouthOpen();
  server.send(200, "text/plain", "Mouth Open");
}

void handleClosed() {
  mouthClosed();
  server.send(200, "text/plain", "Close Mouth");
}

void handletalk() {
  server.send(200, "text/html", "talking");
  talk();
}

//-----------------
void setup() {
  Serial.begin(115200);

  //Allow the ESP32 to allocate PWM timers
  ESP32PWM:allocateTimer(0);
  ESP32PWM:allocateTimer(1);


  //Standard servo pulse range
  myServo.setPeriodHertz(50);
  myServo.attach(SERVO_PIN, 500, 2500);
  headServo.attach(SERVO_PIN, 500, 2500);


  //Start centered
  myServo.write(MOUTH_CLOSED);
  myServo.write(HEAD_CENTER);

  delay(500);

  //Wifi and server setup
  Wifi.softAP(ssid, password);
  server.on("/", handleRoot);
  server.on("/open", handleOpen);
  server.on("/closed", handleClose);
  server.on("/talk", handleTalk);

  server.begin();

  Serial.print("Open: http://");
  Serial.println(wiFi.softAPIP());

  SPI.begin();
  if (!radio.begin()) {
    Serial.println("nRF24L01 was not detected.");
    while (true) {
      delay(1000);
    }
  }


  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(100);


  radio.openReadingPipe(1, radioAddress);
  radio.startListening();


  Serial.println("Servo receiver ready.");
}


void loop() {
  server.handleClient();

  if (radio.available()) {
    // Read the newest packet if several packets are waiting.
    while (radio.available()) {
      radio.read(&command, sizeof(command));
    }
  
  //
  int mouthAngle = map(command.angle, 0, 180, MOUTH_CLOSED, MOUTH_OPEN);
  setMouthAngle(mouthAngle);
  setHeadAngle(headAngle);

  lastRadioMessage = millis();

  
  Serial.print("Joystick command: ");
  Serial.print(command.angle);
  Serial.print("Mouth angle: ");
  Serial.println(currentAngle);
  Serial.print("Head: ");
  Serial.println(currentHeadAngle);

  }
}







