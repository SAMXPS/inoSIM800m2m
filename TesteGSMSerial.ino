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
    
    //sim.setTCP_SSL(true);   // Make SIM800 use SSL auth when using TCP connections
}

bool auto_connect = true;

void SERIALcommand(String str) {
    if (str.startsWith("setup")) {
        sim.setupConfig();
    } else if (str.startsWith("reset")) {
        sim.reset();
    } else if (str.startsWith("disconnect")) {
        if (sim.tcp_connected) sim.TCPclose();
    } else if (str.startsWith("connect")) {
        sim.TCPconnect("srv.samxps.tk", 25565);
    } else if (str.startsWith("send ")) {
        sim.TCPsend(str.substring(5));
    } else if (str.startsWith("auto")) {
        auto_connect = !auto_connect;
    }
}


u8 ticks = 0;
u8 tries = 0;

void loop() {
    sim.loop();
    delay(1000);

    if (Serial.available()) {
        String str = Serial.readString();
        if (str.startsWith("AT")) {
            sim.sendCommand(str);
        } else if (str.startsWith(">")) {
            sim.sim_serial.println(escapeString(str.substring(1)));
        } else if (str.startsWith("/")) {
            SERIALcommand(str.substring(1));
        }
    }
    if (auto_connect) {
        if (sim.tcp_connected && sim.TCPsend(F("KEEP_ALIVE"))) {
            delay(250);
        } else {
            tries++;
            if (tries > 5) reset_ino();
            sim.tcp_connected = false;
            String status;
            if (sim.TCPstatus(&status)) {
                sprintln("TCP STATUS: " + status);
                sprintln("Connecting...");
                sim.TCPconnect("srv.samxps.tk", 25565);
                delay(5000);
            }
        }
    }

    ticks++;
}
