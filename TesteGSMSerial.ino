#include <SoftwareSerial.h>
#include "GSM800L.cpp"

//Create software serial object to communicate with SIM800L
SoftwareSerial mySerial(3, 2); //SIM800L Tx & Rx is connected to Arduino #3 & #2

void setup()
{
  //Begin serial communication with Arduino and Arduino IDE (Serial Monitor)
  Serial.begin(19200);
  
  //Begin serial communication with Arduino and SIM800L
  mySerial.begin(19200);

  Serial.println("Initializing...");
  delay(1000);

 /* mySerial.println("AT"); //Once the handshake test is successful, it will back to OK
  updateSerial();
  mySerial.println("AT+CSQ"); //Signal quality test, value range is 0-31 , 31 is the best
  updateSerial();
  mySerial.println("AT+CCID"); //Read SIM information to confirm whether the SIM is plugged
  updateSerial();
  mySerial.println("AT+CREG?"); //Check whether it has registered in the network
  updateSerial();

  configuraGSM();*/
  delay(1000);
  configGPRS();
}

void configGPRS() {
  mySerial.print("AT+CSTT=\"gprs.oi.com.br\",\"guest\",\"guest\"\n");
   updateSerial();
  mySerial.print("AT+CIICR\n");
   updateSerial();
  mySerial.print("AT+CIFSR\n");
   updateSerial();
}

void configuraGSM() {
   mySerial.print("AT+CMGF=1\n;AT+CNMI=2,2,0,0,0\n;ATX4\n;AT+COLP=1\n"); 
}

int tlen(String* a, int s) {
  int i, b = 0;
  for(i=0;i<s;i++) {
    b+=a[i].length();  
    b+=1;
  }
  return b;
}

void httpGET(String host, int port, String path) {
   mySerial.print("AT+CIPSTART=\"TCP\",\"" + host + "\"," + port + "\n"); 
   updateSerial();
   String message[] = {
      "GET " + path + " HTTP/1.1",
      "Host: " + host,
   };
   mySerial.print("AT+CIPSEND=" + String(tlen(message,2)) + "\n");
   updateSerial();
   int e;
   for (e=0;e<2;e++){
    mySerial.print(message[e] + "\n");
    updateSerial();
   }
}

void enviaSMS(String telefone, String mensagem) {
  delay(500);
  mySerial.print("AT+CMGS=\"" + telefone + "\"");
  mySerial.print((char)13); 
  delay(500);
  mySerial.print(mensagem + "\n");
  delay(1000);
  mySerial.print((char)26); 
}

void requestLocation() {
  mySerial.print("AT+SAPBR=1,1\n");
  delay(500);
  mySerial.print("AT+CIPGSMLOC=1,1\n");
}

void loop()
{
  updateSerial();
  delay(500);
}

void updateSerial()
{
   delay(500);
  String line;
  if (Serial.available()) 
  {
    line = Serial.readString();
    if (line.startsWith("BT")){
        line = line.substring(2);
        if (line.startsWith("+SMS")) {
          enviaSMS("61981367528", "oii");
        }
        if (line.startsWith("+GPS")) {
          requestLocation();
        }
        if (line.startsWith("+HTTP")) {
          httpGET("ovh.bsbdistribuicao.tk", 80, "/gsm.php?message=1");
        }
    } else  mySerial.print(line + "\n");
  }
  while(mySerial.available()) 
  {
    Serial.write(mySerial.read());//Forward what Software Serial received to Serial Port
  }
}
