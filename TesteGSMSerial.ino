#include "GSM800L.cpp"

SIM800L sim(2,3,4,5,4800);
#define INO_RST 12 /* Digital output connected into the arduino reset */

void reset_ino() {
  pinMode(INO_RST, OUTPUT);
  digitalWrite(INO_RST, LOW);
}

void setup() {
  pinMode(INO_RST, INPUT);
  digitalWrite(INO_RST, LOW);

  //Begin serial communication with Arduino and Arduino IDE (Serial Monitor)
  Serial.begin(19200);
  Serial.println("Iniciando...");

  sim.setup();
  sim.reset();
  sim.setupConfig();

}

u8 ticks = 0;
void loop() {
  sim.loop();
  delay(1000);

  
  /*if (!sim.tcp_connected) {
      sim.TCPconnect("sam.bsbdistribuicao.tk", 25565);
  } else if (ticks % 8 == 0) {
      if (!sim.TCPsend(sim.requestLocation())) sim.tcp_connected = false;
  }*/
  

  ticks++;
}
