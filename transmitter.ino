#include <SPI.h>
#include <RF24.h>

constexpr uint8_t CE_PIN = 4;
constexpr uint8_t CSN_PIN = 2;
constexpr uint8_t JOYSTICK_X_PIN = 34;
constexpr uint8_t JOYSTICK_Y_PIN = 35;


RF24 radio(CE_PIN, CSN_PIN);
const uint8_t radioAddress[6] = "Servo";

struct ServoCommand {
  uint8_t mouthAngle;
  uint8_t headAngle;

  
};

ServoCommand command;
int filteredXJoystick = 2048;
int filteredYJoystick = 2048;

constexpr int MOUTH_CLOSED = 80;
constexpr int MOUTH_OPEN = 120;

constexpr int HEAD_LEFT = 45;
constexpr int HEAD_RIGHT = 135;


void setup() {
  Serial.begin(115200);
  pinMode(JOYSTICK_X_PIN, INPUT);
  pinMode(JOYSTICK_Y_PIN, INPUT);

  analogReadResolution(12);

  SPI.begin();
  if(!radio.begin()){
    Serial.println("nRF24L01 was not detected.");
    while (true){
       delay(1000);
    }
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(100);
  radio.setRetries(5, 15);
  radio.openWritingPipe(radioAddress);
  radio.stopListening();

  Serial.println("Joystick transmitter ready.")

}
void loop(){
  int rawXJoystick = analogRead(JOYSTICK_X_PIN);
  int rawYJoystick = analogRead(JOYSTICK_Y_PIN);

  filteredXJoystick = (filteredXJoystick * 7 + rawXJoystick) / 8;
  filteredYJoystick = (filteredYJoystick * 7 + rawYJoystick) / 8;


  int headAngle = map(filteredXJoystick, 0, 4095, HEAD_LEFT, HEAD_RIGHT);
  int mouthAngle = map(filteredYJoystick, 0, 4095, MOUTH_CLOSED, MOUTH_OPEN);

  command.headAngle = static_cast<uint8_t>(
    constrain(headAngle, HEAD_LEFT, HEAD_RIGHT)
    );
    command.mouthAngle = static_cast<uint8_t>(
    constrain(mouthAngle, MOUTH_CLOSED, MOUTH_OPEN)
    );

  bool sent = radio.write(&command, sizeof(command));

  Serial.print("Joystick: ");
  Serial.print(filteredXJoystick);
  Serial.print("  Head: ");
  Serial.print(command.headAngle);

  Serial.print("Joystick: ");
  Serial.print(filteredYJoystick);
  Serial.print("  Mouth: ");
  Serial.print(command.mouthAngle);

  Serial.print("  Sent: ");
  Serial.println(sent ? "yes" : "no");

  delay(30);
}