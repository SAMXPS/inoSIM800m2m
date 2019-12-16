#include <Arduino.h>
#include <SoftwareSerial.h>
#include "SIM800m2m.h"

    class CommandResolver {
        public:
        String match;
        bool returner;

        CommandResolver(String match, bool returner) {
            this->match = match;
            this->returner = returner;
        };
    };

    #define R(x) (r_code)(1 << x)

    const CommandResolver _resolvers[] = {
        CommandResolver("OK", true), 
        #define _OK R(0x0)

        CommandResolver("\n", true),   
        #define _NEWLINE R(0x1) 

        CommandResolver("", false), 
        #define _EMPTY R(0x2)
        
        CommandResolver("+CME ERROR", false),
        #define _CME_ERROR R(0x3) 

        CommandResolver("CONNECT OK", true),
        #define _CONNECT_OK R(0x4)  

        CommandResolver("ALREADY CONNECT", false),
        #define _ALREADY_CONNECT R(0x5)  

        CommandResolver("CONNECT FAIL", false),
        #define _CONNECT_FAIL R(0x6)  
        
        CommandResolver("DATA ACCEPT", true),
        #define _DATA_ACCEPT R(0x7)  

        CommandResolver("SEND OK", true),
        #define _SEND_OK R(0x8)  

        CommandResolver("SEND FAIL", false),
        #define _SEND_FAIL R(0x9)  

        CommandResolver("ERROR", false),
        #define _ERROR R(0xA)  

        CommandResolver(">", true),
        #define _DATA_BEGIN R(0xB)  

        CommandResolver("STATE", true),
        #define _STATE R(0xC)

        CommandResolver("+IPD,", true),
        #define _DATA_RECEIVED R(0xD)
    };


    int _2bas(r_code x) {
        int i = 0;
        while (x>=1) {
            x >> 1;
        }
        return i;
    }

    SIM800m2m::SIM800m2m (u8 RX, u8 TX, u8 RST, unsigned int bprate) : sim_serial(RX, TX){
        this->RX = RX;
        this->TX = TX;
        this->RST = RST;
        this->bprate = bprate;
    }

    SoftwareSerial SIM800m2m::getSerial() {
        return this->sim_serial;
    }

    bool SIM800m2m::set_apn_config(String apn, String user, String password) {
        this->apn = apn;
        this->user = user;
        this->password = password;
    }

    bool SIM800m2m::tcp_auto_connect(bool auto_connect) {
        tcp_auto = auto_connect;
    }

    bool SIM800m2m::tcp_sethost(String host, unsigned int port) {
        this->tcp_host = host;
        this->tcp_port = port;
        return true;
    }

    bool SIM800m2m::tcp_receiver(void (*callback)(String data_rcv)) {
        this->tcp_callback = callback;
    }

    bool SIM800m2m::tcp_auto_connect_error(void (*callback)()) {
        this->tcp_error_callback = callback;
    }

    bool SIM800m2m::set_gprs_error_callback(void (*callback)()) {
        this->gprs_error_callback = callback;
    }

    /**
     * Setup function: sets up the serial communication and used pins
     * for the SIM800L to work properly.
    */
    bool SIM800m2m::setup() {
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
    bool SIM800m2m::reset() {
        Serial.println(F("Resetings..."));
        digitalWrite(RST, LOW);
        delay(2000);
        digitalWrite(RST, HIGH);
        delay(7000);

        /* reseting private state variables */
        tries = 0;
        tcp_status = false;
        tcp_ssl  = false;
        gprs_connected = false;
        _last_resolver = -1;
        last_line = "";

        Serial.println(F("Testing..."));
        return sendCommand(F("AT"));
    }

    
    bool SIM800m2m::config() {
        if (!sendCommand("AT")) return false; // Testa o handshake
        sendCommand(F("AT+CSQ"));         // Testa o nível do sinal da rede
        sendCommand(F("AT+CCID"));        // Verifica as informações do cartão SIM (chip)
        sendCommand(F("AT+CREG?"));       // Verifica se está registrado na REDE
        sendCommand(F("AT&D2"));          // Sets the funcion of DTR pin
        sendCommand(F("AT+IFC=0,0"));     // Sets no flow control
        config_gprs();

        return true;
    }

    bool SIM800m2m::config_gprs() {
        /* +CSTT is used to configure the APN used in the GPRS context */
        sendCommand("AT+CSTT=\"" + apn + "\",\"" + user + "\",\"" + password + "\"");
        Serial.println("Wait...");
        soft_wait(3000);
        sendCommand(F("AT+CIICR"));        // Turn on GPRS circuit
        sendCommand(F("AT+CIFSR"));        // Check received IP address
        /* +CIPQSEND=1: Quick send mode – when the data is sent to module, it will
        respond DATA ACCEPT:<n>,<length>, not responding SEND OK.*/
        sendCommand(F("AT+CIPQSEND=1"));        
        //sendCommand(F("AT+CIPSRIP=1"));  // Show Remote IP Address and Port When Received Data 
        sendCommand(F("AT+CIPHEAD=1"));

        gprs_connected = true;
        tcp_check_status();
        return true;
    }

    bool SIM800m2m::sendCommand(String command) {
        return sendCommand(command, _OK | _CME_ERROR | _ERROR);
    }

    bool SIM800m2m::tcp_connected() {
        return tcp_status;
    }

    int SIM800m2m::match(String line, r_code code) {
        for (u8 i = 0; i < 16; i++) {
            if ((code >> i) % 2){
                if (line.indexOf(_resolvers[i].match) >= 0) {
                    return i;
                }
            }
        }
        return -1; // No match
    }

    bool SIM800m2m::processResolvers(r_code code) {
        u32 start = millis();
        while (millis() - start < 5000) {
            while (sim_serial.available()) {
                _last_resolver = serial_process_line(serial_read_line(), code);
                if (_last_resolver >= 0) {
                    return _resolvers[_last_resolver].returner;
                }
            }
            delay(100);
        }
        return false;
    }

    bool SIM800m2m::sendData(String data, r_code resolvers) {
        sim_serial.print(data + "\r\n");
        delay(200L);
        bool r = processResolvers(resolvers);
        return r;
    }

    bool SIM800m2m::sendCommand(String command, r_code resolvers) {
        sim_serial.print(command + "\r\n");
        delay(100L);
        return processResolvers(resolvers);
    }

    bool SIM800m2m::tcp_set_ssl(bool active) {
        if (sendCommand("AT+CIPSSL=" + String((u8) active))) {
            this->tcp_ssl = active;
            return true;
        }
        return false;
    }

    void SIM800m2m::tcp_check_status() {
        tcp_last_check = millis();

        if (sendCommand(F("AT+CIPSTATUS"), _OK | _ERROR | _CME_ERROR) && processResolvers(_STATE)) {
            if (last_line.indexOf(F("CONNECT OK")) != -1) {
                gprs_connected = true;
                tcp_status = true;
                Serial.println(F("[INFO] TCP IS CONNECTED"));
                return;
            } else if (last_line.indexOf(F("PDP DEACT")) != -1) {
                // No connection to the GPRS network
                gprs_connected = false;
                tcp_status = false;
                Serial.println(F("[INFO] GPRS IS NOT CONNECTED"));
            }
            return;
        }
        
        tcp_status = false;
        Serial.println(F("[INFO] TCP IS DISCONNECTED"));
    }

    bool SIM800m2m::tcp_connect() {
        bool r = sendCommand("AT+CIPSTART=\"TCP\",\"" + tcp_host + "\"," + String(tcp_port), 
            _CONNECT_FAIL | _CONNECT_OK | _CME_ERROR | _ALREADY_CONNECT | _ERROR
        );
        return (this->tcp_status = (r || _last_resolver == _2bas(_ALREADY_CONNECT))); 
    }

    bool SIM800m2m::tcp_send(String data) {
        if (!tcp_connected()) return false;

        if (!sendCommand("AT+CIPSEND=" + String(data.length()), 
            _DATA_BEGIN | _ERROR | _CME_ERROR
        )) return false;

        delay(100);

        return sendData(data, 
            _DATA_ACCEPT | _SEND_OK | _SEND_FAIL | _CME_ERROR | _ERROR
        );
    }

    int SIM800m2m::serial_process_line(String line, r_code resolvers) {
        Serial.println("<-- "  + line);
        last_line = line;
        if (match(line, _DATA_RECEIVED) >= 0) {
            if (tcp_callback) {
                int start = line.indexOf(':') + 1;
                int data_len = String(line.substring(line.indexOf(',') + 1, start - 1)).toInt();
                String data_received = line.substring(start, start + data_len);
                tcp_callback(data_received);
                if (!line.endsWith(data_received))
                    return serial_process_line(line.substring(start + data_len + 1));
            }
        } else if (resolvers)
            return match(line, resolvers);
        return -1;
    }

    /**
     * Reads a line from the SIM800 serial connection.
    */
    String SIM800m2m::serial_read_line() {
        String s = ""; char c;
        while(sim_serial.available()) {
            c = sim_serial.read();
            if (c == '\r') continue;
            if (c == '\n') break;
            s += c;
        }
        return s;
    }

    void SIM800m2m::soft_wait(u32 t) {
        u32 start = millis();
        while (millis() - start < t) {
            while (sim_serial.available()) {
                serial_process_line(serial_read_line());
            }
            delay(100);
        }
    }

    void SIM800m2m::loop() {
        while (sim_serial.available()) {
            serial_process_line(serial_read_line());
        }

        // Checking the TCP connection
        if (millis() - tcp_last_check > 10000) {
            tcp_check_status();
        }
        Serial.println("a");
        // Auto connect to the GPRS network
        while (!gprs_connected) {
            tries++;
            config_gprs();
            delay(100);
            if (tries > 15) {
                tries = 0;
                Serial.println("Failed to connect to the gprs_connected");
                if (gprs_error_callback) 
                    (*gprs_error_callback)();
                return;
            }
        }

        // Auto connect to the TCP server
        if (!tcp_status) {
            if (tcp_auto) {
                while (tries < 15) {
                    tries++;
                    Serial.println(F("[INFO] Trying to connect.."));
                    if (tcp_connect()) break;
                    delay(100);
                }
                Serial.println(F("[INFO] yay..")); 
                if (!tcp_status && tcp_error_callback) 
                    (*tcp_error_callback)();
            }
        } else {
            tries = 0;
        }
    }