#include "Arduino.h"
#include "EscapeUtils.h"
#include "Assert.h"
#include "SerialLogger.h"
#include <SoftwareSerial.h>

class SIM800L {        
  public:
    SoftwareSerial  sim_serial;     // Emulated Serial connection using digital pins
    unsigned int    bprate;         // 
    u8 RX, TX, RST, DTR;            // PINs used for the SIM800L
    bool            tcp_connected;  // false: no connection, true: connected
    bool            tcp_ssl;
    const String    _ok = "OK";

    /**
     * RX       -> Serial PIN for receiving data.
     * TX       -> Serial PIN for sending data.
     * RST      -> PIN used to RESET the module.
     * DTR      -> PIN used to SLEEP mode and switching between data/command modes. 
     * bprate   -> Baud rate of the serial connection.
    */
    SIM800L (u8 RX, u8 TX, u8 RST, u8 DTR, unsigned int bprate) : sim_serial(TX,RX) {
        this->bprate = bprate;
        this->RX = RX;
        this->TX = TX;
        this->RST = RST;
        this->DTR = DTR;
        this->tcp_connected = false;
        this->tcp_ssl       = false;
    }
    
    /**
     * Setup function: sets up the serial communication and used pins
     * for the SIM800L to work properly.
    */
    void setup() {
        // Setup serial connection to the module
        sim_serial.begin(bprate);
        sim_serial.setTimeout(1500);

        // Config reset and dtr pins
        pinMode(RST, OUTPUT);
        pinMode(DTR, OUTPUT);

        // Pull up the pins from ground (default mode)
        digitalWrite(RST, HIGH);
        digitalWrite(DTR, HIGH);
    }

    /**
     * Reset function: sets the RST PIN into low mode for 2 seconds,
     * wait for the module to reset and sends AT test command.
    */
    void reset() {
        sprintln(F("Reseting..."));
        digitalWrite(RST, LOW);
        delay(2000);
        digitalWrite(RST, HIGH);
        delay(7000);
        sprintln(F("Testing..."));
        assert(sendCommand("AT", _ok));
    }

   
    void setupConfig() {
        assert(sendCommand("AT", _ok));     // Testa o handshake
        sendCommand("AT+CSQ", _ok);         // Testa o nível do sinal da rede
        sendCommand("AT+CCID", _ok);        // Verifica as informações do cartão SIM (chip)
        sendCommand("AT+CREG?", _ok);       // Verifica se está registrado na REDE
        sendCommand("AT&D2", _ok);          // Sets the funcion of DTR pin
        sendCommand("AT+IFC=0,0", _ok);     // Sets no flow control
        //setupGSM();
        setupGPRS();
    }

    void setupGSM() {
        sendCommand("AT+CMGF=1", _ok);         //
        sendCommand("AT+CNMI=2,2,0,0,0", _ok); //
        sendCommand("ATX4", _ok);              //
        sendCommand("AT+COLP=1", _ok);         //
    }

    void setupGPRS() {
        // Configurando APN
        sendCommand(F("AT+CSTT=\"gprs.oi.com.br\",\"guest\",\"guest\""), _ok);
        sendCommand("AT+CIICR", _ok);       // Habilitando o circuito do GPRS
        sendCommand("AT+CIFSR");            // Verifica o IP recebido
        sendCommand("AT+SAPBR=1,1", _ok);   // 
        sendCommand("AT+SAPBR=2,1", _ok);   //
    }

    bool TCPconnect(String host, int port) {
        if(sendCommand("AT+CIPSTART=\"TCP\",\"" + host + "\"," + String(port), "CONNECT OK")) {
            this->tcp_connected = true;
            return true;
        }
        if (!this->tcp_connected) 
            TCPclose();
        return false;
    }

    bool setTCP_SSL(unsigned char active) {
        if (sendCommand("AT+CIPSSL=" + String(active))) {
            this->tcp_ssl = active;
            return true;
        }
        return false;
    }

    bool TCPclose() {
        if (!sendCommand("AT+CIPCLOSE", _ok)) 
            return false;
        this->tcp_connected = false;
        return true;
    }

    bool TCPsend(String data) {
        if (this->tcp_connected && sendCommand("AT+CIPSEND=" + String(data.length()), ">")) {
            delay(200);
            sim_serial.print(data.c_str());
            while(sim_serial.available()) {
                String s = serialReadLine();
                onReceive(s);
                if (s.startsWith("SEND OK")) return true;
            }
        }
        return false;
    }

    bool requestLocation(String* resp) {
        return sendCommand("AT+CIPGSMLOC=1,1", _ok, resp);
    }

    void onReceive(String str) {
        sprintln("<?-- " + str);
        if (str.startsWith("CLOSED")){
            this->tcp_connected = false;
        }
    }

    void loop() {
        while (sim_serial.available()) {
            onReceive(serialReadLine());
        }
    }
    
    bool checkCommand(String rcv, String expect) {
        u8 l1, l2, i;
        rcv.replace("\r\n", "\n");
        expect.replace("\r\n", "\n");
        if (abs((l1=rcv.length()) - (l2=expect.length())) <= 2) {
            for (i=0;i<min(l1,l2);i++) {
                if (rcv.charAt(i) != expect.charAt(i)){
                    sprintln("[!] " + rcv + " != " + expect);
                    sprintln("Diff:" + String((int) rcv.charAt(i)) + " and " + String((int) expect.charAt(i)));
                    return false;
                }
            }
            return true;
        }
        sprintln("[!] " + rcv + " !+ " + expect);
        return false;
    }

    /**
     * Sends the command via serial to the SIM800L
     * and waits <timeout> seconds.
    */
    bool sendCommand(String cmd, String expect="", String* rsp = NULL, u8 timeout=15) {
        sim_serial.println(cmd);
        sprintln("--> " + cmd);
        String rcv, a;

        bool j = false;
        for(u8 i = 0; i<timeout*10; i++) {
            delay(100);
            if (sim_serial.available()) {
                if (!j) {
                    rcv = serialReadLine();
                    if (j = checkCommand(rcv, cmd)) continue;
                    onReceive(rcv);
                } else {
                    return readLinesUntil(expect, rsp);
                }
            }
        }
        delay(100);
        return false;
    }

    bool readLinesUntil(String expect, String* change, u8 timeout=15) {
        String build = "";
        String line;
        bool sucess = false;
        u32 time = millis();

        // Waits up to timeout seconds before exiting
        while(time + timeout * 1000 > millis()){
            line = serialReadLine();
            if (line.length()) {
                build += line + "\n";

                sprintln("<-- " + line);

                if (!expect.length() || line.indexOf(expect) != -1){
                    sucess = true;
                    break;
                }
            }
            delay(100);
        }

        /* Removes the last line break */
        if (build.length())
            build.substring(0, build.length() - 1);
        
        /* Updates the String pointer */
        if (change) *change = build;

        return sucess;
    }

    /**
     * Reads a line from the SIM800 serial connection.
    */
    String serialReadLine() {
        String s = ""; char c;
        while(sim_serial.available()) {
            c = sim_serial.read();
            if (c == '\r') continue;
            if (c == '\n') break;
            s += c;
        }
        return s;
    }

    /**
     *  Clears the input buffer from the SIM800L serial connection.
     */
    void serialFlush(){
        while(sim_serial.available() > 0) {
            char t = sim_serial.read();
        }
    }   

};
