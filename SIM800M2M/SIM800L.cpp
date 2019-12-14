#include <Arduino.h>
#include <SoftwareSerial.h>
//#include "SIM800L.h"

typedef struct {
    String  match;
    bool returner;
} CommandResolver;

#define resolver(_match, _ret) CommandResolver{.match = _match, .returner = _ret}
#define def_resolver(name, _match, _ret)\
    const CommandResolver name = resolver(_match, _ret)

def_resolver(default_resolver, "OK", true);
def_resolver(line_resolver, "\n", true);
def_resolver(empty_resolver, "", true);
def_resolver(cme_err, "+CME ERROR" , false);
def_resolver(timeout_resolver, "_!RESERVED--TIMEOUT", false);

class SIM800L {
private:
    SoftwareSerial sim_serial;
    u8 RX,TX,RST;
    unsigned int bprate;
    String       tcp_host;
    unsigned int tcp_port;
    void (*tcp_callback)(String data_rcv) = NULL;
    bool tcp_status = false;
    CommandResolver last_found;

public:
    SIM800L (u8 RX, u8 TX, u8 RST, unsigned int bprate) : sim_serial(RX, TX){
        this->RX = RX;
        this->TX = TX;
        this->RST = RST;
        this->bprate = bprate;
    }

    SoftwareSerial getSerial() {
        return this->sim_serial;
    }

    bool tcp_sethost(String host, unsigned int port) {
        this->tcp_host = host;
        this->tcp_port = port;
        return true;
    }

    bool tcp_receiver(void (*callback)(String data_rcv)) {
        this->tcp_callback = callback;
    }

    /**
     * Setup function: sets up the serial communication and used pins
     * for the SIM800L to work properly.
    */
    bool setup() {
        // Setup serial connection to the module
        sim_serial.begin(bprate);
        sim_serial.setTimeout(1500);

        // Config reset and dtr pins
        pinMode(RST, OUTPUT);

        // Pull up the pins from ground (default mode)
        digitalWrite(RST, HIGH);

        // Reset the module
        if (!reset()) return false;

        // Configure the module
        if (!config()) return false;

        return true;
    }

    /**
     * Reset function: sets the RST PIN into low mode for 2 seconds,
     * wait for the module to reset and sends AT test command.
    */
    bool reset() {
        Serial.println(F("Reseting..."));
        digitalWrite(RST, LOW);
        delay(2000);
        digitalWrite(RST, HIGH);
        delay(7000);
        Serial.println(F("Testing..."));
        return sendCommand("AT");
    }

   
    bool config() {
        if (!sendCommand("AT")) return false; // Testa o handshake
        sendCommand("AT+CSQ");         // Testa o nível do sinal da rede
        sendCommand("AT+CCID");        // Verifica as informações do cartão SIM (chip)
        sendCommand("AT+CREG?");       // Verifica se está registrado na REDE
        sendCommand("AT&D2");          // Sets the funcion of DTR pin
        sendCommand("AT+IFC=0,0");     // Sets no flow control
       
        // Configurando APN
        sendCommand(F("AT+CSTT=\"gprs.oi.com.br\",\"guest\",\"guest\""));
        Serial.println("Waiit.....");
        delay(5000);
        sendCommand(F("AT+CIICR"));        // Habilitando o circuito do GPRS

        sendCommand(F("AT+CIFSR"));             // Verifica o IP recebido
        sendCommand(F("AT+CIPQSEND=1"));        
        /* Quick send mode – when the data is sent to module, it will
        respond DATA ACCEPT:<n>,<length>, while not responding SEND OK.*/
        //sendCommand(F("AT+CIPSRIP=1"));         // Show Remote IP Address and Port When Received Data 
        sendCommand(F("AT+CIPHEAD=1"));
    }

    bool sendCommand(String command) {
        CommandResolver resolvers[] = {default_resolver, cme_err}; 
        return sendCommand(command, resolvers, 2);
    }

    bool tcp_connected() {
        return tcp_status;
    }

    bool processResolvers(CommandResolver resolvers[], u8 len) {
        u32 start = millis();
        while (millis() - start < 5000) {
            while (sim_serial.available()) {
                String line = serial_read_line();
                serial_process_line(line);
                for (u8 i = 0; i < len; i++) {
                    Serial.println("testing resolver " + resolvers[i].match);
                    if (line.indexOf(resolvers[i].match) >= 0) {
                        last_found = resolvers[i];
                        return resolvers[i].returner;
                    }
                }
            }
            delay(100);
        }
        return false;
    }

    bool dataCommand(String command, String data, CommandResolver resolvers[], u8 len) {
        delay(500L);
        sim_serial.print(command + "\r\n");
        delay(200L);
        sim_serial.print(data + "\r\n");
        delay(200L);
        bool r = processResolvers(resolvers, len);
        Serial.println("resolver: " + last_found.match);
        return r;
    }

    bool sendCommand(String command, CommandResolver resolvers[], u8 len) {
        sim_serial.print(command + "\r\n");
        delay(100L);
        return processResolvers(resolvers, len);
    }

    bool tcp_connect() {
        CommandResolver resolvers[] = {
            resolver("CONNECT OK", true),
            cme_err,
            resolver("ALREADY CONNECT", false), //this should never happen, our code must support tcp_connected() function
            resolver("CONNECT FAIL", false)
        }; 

        bool r = sendCommand("AT+CIPSTART=\"TCP\",\"" + tcp_host + "\"," + String(tcp_port), resolvers, sizeof(resolvers));
        this->tcp_status = (r || last_found.match.indexOf("ALREADY") != -1); 
        return r;
    }

    bool tcp_send(String data) {
        if (!tcp_connected()) return false;

        CommandResolver resolvers[] = {
            resolver("DATA ACCEPT", true),
            resolver("SEND OK", true),
            resolver("SEND FAIL", false),
            cme_err
        };

        return dataCommand("AT+CIPSEND=" + String(data.length()), data, resolvers, sizeof(resolvers));
    }

    void serial_process_line(String line) {
        Serial.println("<-- "  + line);
    }

    /**
     * Reads a line from the SIM800 serial connection.
    */
    String serial_read_line() {
        String s = ""; char c;
        while(sim_serial.available()) {
            c = sim_serial.read();
            if (c == '\r') continue;
            if (c == '\n') break;
            s += c;
        }
        return s;
    }

    void loop() {
        while (sim_serial.available()) {
            serial_process_line(serial_read_line());
        }
    }

};