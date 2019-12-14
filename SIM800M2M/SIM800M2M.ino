/**
 * This file is a tester/example for the SIM800L simple M2M library
*/

#include "SIM800L.cpp"
#define S_RX 3
#define S_TX 2
#define SRST 4
#define SBAU 4800

SIM800L sim = SIM800L(S_RX, S_TX, SRST, SBAU);

void on_receive_data(byte data[]);

void setup() {
    Serial.begin(9600);
    Serial.println("initializing...");

    sim.setup();                                // Initial setup for the module
    sim.tcp_sethost("srv.samxps.tk", 25565);    // Set the host to connect to
    sim.tcp_receiver(&on_receive_data);         // Set the handler function when receiving tcp data
    
    Serial.println("setup complete!");
}

void on_receive_data(String data) {
    Serial.println("{ RCV: \"" + String(data) + "\" }");
}


void loop () {
    u32 start = millis(); // function start timer
    sim.loop();

    if (!sim.tcp_connected()) {
        sim.tcp_connect();
    } else {
        if (sim.tcp_send(String("Hello server"))) {
            Serial.println("[INFO] Data sent!");
        } else {
            Serial.println("[WARN] can't send data to server");
        }
    }

    while(Serial.available()) {
        sim.getSerial().write(Serial.read());
    }

    u32 diff = millis() - start;
    if (diff > 1000) {
        Serial.println("[WARN] tick took too long (" + String(diff) + "ms) ");
    } else delay(1000 - diff); /* wait until 1 second has passed */
}