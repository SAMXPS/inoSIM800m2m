/**
 * This file is a tester/example for the SIM800 simple M2M library
*/
#include "SIM800m2m.h"
#define S_RX 2
#define S_TX 3
#define SRST 4
#define SBAU 4800

SIM800m2m sim = SIM800m2m(S_RX, S_TX, SRST, SBAU);

void on_receive_data(String data);
void on_error();

void setup() {
    Serial.begin(9600);
    Serial.println("initializing...");

    sim.set_apn_config("gprs.oi.com.br","guest","guest");   // APN configuration for the GPRS network
    sim.setup();                                // Initial setup for the module
    sim.tcp_sethost("srv.samxps.tk", 25565);    // Set the host to connect to
    sim.tcp_receiver(&on_receive_data);         // Set the handler function when receiving tcp data
    sim.tcp_auto_connect_error(&on_error);      // What to do when the module can't auto-connect to the TCP server
    sim.set_gprs_error_callback(&on_error);     // What to do when the module can't auto-connect to the GPRS network
    
    Serial.println("setup complete!");
}

void on_receive_data(String data) {
    Serial.println("{ RCV: \"" + String(data) + "\" }");
}

void on_error() {
    Serial.println("Cant auto-connect... So, reseting!");
    sim.setup();
}

void loop () {
    u32 start = millis(); // function start timer
    sim.loop();

    if (sim.tcp_send(String("Hello server ") + String(start))) {
        Serial.println("[INFO] Data sent!");
    } else {
        Serial.println("[WARN] can't send data to server");
    }

    while(Serial.available()) {
        sim.getSerial().write(Serial.read());
    }

    u32 diff = millis() - start;
    if (diff > 1000) {
        Serial.println("[WARN] tick took too long (" + String(diff) + "ms) ");
    } else delay(1000 - diff); /* wait until 1 second has passed */
}