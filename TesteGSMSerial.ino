#define VERBOSE 1
#include "GSM800L.cpp"

SIM800L sim(2,3,4,5,4800);
#define INO_RST 12 /* Digital output connected into the arduino reset */

void reset_ino() {
    pinMode(INO_RST, OUTPUT);
    digitalWrite(INO_RST, LOW);
}

void setup() {
    // Configuring reset pins
    pinMode(INO_RST, INPUT);
    digitalWrite(INO_RST, LOW);

    // Begin serial communication with Arduino and Arduino IDE (Serial Monitor)
    Serial.begin(19200);
    Serial.println("Iniciando...");

    // Seting up SIM800
    sim.setup();
    sim.reset();
    sim.setupConfig();
    
    sim.setTCP_SSL(true);   // Make SIM800 use SSL auth when using TCP connections
}

void SERIALcommand(String str) {
    if (str.startsWith("setup")) {
        sim.setupConfig();
    } else if (str.startsWith("reset")) {
        sim.reset();
    } else if (str.startsWith("disconnect")) {
        if (sim.tcp_connected) sim.TCPclose();
    } else if (str.startsWith("connect")) {
        if (sim.tcp_connected) sim.TCPconnect("srv.samxps.tk", 25565);
    } else if (str.startsWith("send ")) {
        sim.TCPsend(str.substring(5));
    }
}


u8 ticks = 0;
void loop() {
    sim.loop();
    delay(1000);

    if (Serial.available()) {
        String str = Serial.readString();
        if (str.startsWith("AT")) {
            Serial.println("[CMD OUT] " + sim.sendCommand(str));
        } else if (str.startsWith(">")) {
            sim.sim_serial.println(str.substring(1));
        } else if (str.startsWith("/")) {
            SERIALcommand(str.substring(1));
        }
    }

    ticks++;
}
