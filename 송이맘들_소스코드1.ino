#include <math.h>
#include <ESP8266WiFi.h>
#include "DHT.h"

const char* ssid = "sj";
const char* password = "01051841442";

#define DHTPIN D4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

float h=0;
float t=0;
float f=0;

int relay = D3;
int rgb_red = D5;
int rgb_green = D6;
int rgb_blue = D7;
int dustPin = D8;

int led_flag = 1;
int fan_status = 0; //0일 때 auto, 1일 때 manual
float fan_value = 80.5; //automode모터작동기준의 미세먼지 수치
int dustCompare_flag=1; //1이면 바깥이 더 좋은 상태

unsigned long duration;
unsigned long starttime;
unsigned long sampletime_ms = 30000;
unsigned long lowpulseoccupancy = 0;
float pcsPerCF = 0;
float ugm3 = 0;
float ratio = 0;
float concentration = 0;


WiFiServer server(80);


void setup() {
  Serial.begin(115200);
  delay(10);
  pinMode(rgb_red, OUTPUT);
  pinMode(rgb_green, OUTPUT);
  pinMode(rgb_blue, OUTPUT);
  digitalWrite(rgb_red, HIGH);
  digitalWrite(rgb_green, HIGH);
  digitalWrite(rgb_blue, HIGH);

  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);
  
  pinMode(dustPin, INPUT);
  starttime = millis();
  
  // Connect to WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
 
  WiFi.begin(ssid, password);
  dht.begin(); 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
 
  // Start the server
  server.begin();
  Serial.println("Server started");
  // Print the IP address
  Serial.print("Use this URL : ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
}

void loop() {
  //client 접속 확인
  WiFiClient client = server.available();
  if (!client) {
    return;
  }
 
  // Wait until the client sends some data
  Serial.println("new client");
  while(!client.available()){
    delay(1);
  }
 
  // Read the first line of the request
  String request = client.readStringUntil('\r');
  Serial.println(request);
  client.flush();


//외부의 미세먼지 수치가 더 높을 시에는 모터가 작동하지 않도록 flag 이용
 if (request.indexOf("/OutGood") > 0){
  dustCompare_flag=1;
 }
 if (request.indexOf("/OutBad") > 0){
  dustCompare_flag=0;
 }

 //요청 url에 따라 LED를 ON/Off
 if (request.indexOf("/ledOn") > 0){
  led_flag = 1;
  digitalWrite(rgb_red, HIGH);
  digitalWrite(rgb_green, HIGH);
  digitalWrite(rgb_blue, HIGH);
 }
 if (request.indexOf("/ledOff") > 0){
  led_flag = 0;
  digitalWrite(rgb_red, LOW);
  digitalWrite(rgb_green, LOW);
  digitalWrite(rgb_blue, LOW);
 }
 
 //요청 url에 따라 fan mode
 if (request.indexOf("/fanAuto") > 0){
  fan_status = 0;
  if(ugm3 < fan_value || dustCompare_flag==0){
    digitalWrite(relay, LOW);
  }
  else{
    digitalWrite(relay, HIGH);
  }
 }
 if (request.indexOf("/fanManual") > 0){
  fan_status = 1;
  digitalWrite(relay, LOW);
 }

 //요청 url에 따라 수동 fan ON/Off
 if (request.indexOf("/fanOn") > 0){
  digitalWrite(relay, HIGH);
 }
 if (request.indexOf("/fanOff") > 0){
  digitalWrite(relay, LOW);
 }


 // Return the response 웹브라우저에 출력할 웹페이지
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println(""); //  do not forget this one
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.println("<br />"); 
  client.println(""); //  do not forget this one

  h = dht.readHumidity();
  t = dht.readTemperature();
  f = dht.readTemperature(true);
 
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }


  duration = pulseIn(dustPin, LOW);
  lowpulseoccupancy = lowpulseoccupancy + duration;

  if ((millis() - starttime) > sampletime_ms){
    ratio = lowpulseoccupancy / (sampletime_ms * 10.0); // Integer percentage 0=>100
    concentration = 1.1 * pow(ratio, 3) - 3.8 * pow(ratio, 2) + 520 * ratio + 0.62; // using spec sheet curve
    pcsPerCF = concentration * 100;
    ugm3 = pcsPerCF / 7000;
    Serial.println(ugm3);
    lowpulseoccupancy = 0;
    starttime = millis();

    if(led_flag == 1){
      if(ugm3 <= 30.4){                           // 대기 중 미세먼지가 좋음 일 때 파란색 출력
        analogWrite(rgb_red, 0);
        analogWrite(rgb_green, 0);
        analogWrite(rgb_blue, 255);
      }else if(30.4 < ugm3 && ugm3 <= 80.4){      // 대기 중 미세먼지가 보통 일 때 녹색 출력
        analogWrite(rgb_red, 0);
        analogWrite(rgb_green, 255);
        analogWrite(rgb_blue, 0);
      }else if (80.4 < ugm3 && ugm3 <= 150.4){    // 대기 중 미세먼지가 나쁨 일 때 노란색 출력
        analogWrite(rgb_red, 255);
        analogWrite(rgb_green, 155);
        analogWrite(rgb_blue, 0);
      }else{                                      // 대기 중 미세먼지가 매우 나쁨 일 때 빨간색 출력
        analogWrite(rgb_red, 255);
        analogWrite(rgb_green, 0);
        analogWrite(rgb_blue, 0);
      }
    }

    if(fan_status == 0){
      if(ugm3 >= fan_value && dustCompare_flag==1){
        digitalWrite(relay, HIGH);
      }
      else{
        digitalWrite(relay, LOW);
      }
    }
  }

  
  client.print("<dust>");
  client.print(ugm3, 0);
  client.println("</dust><br/>");
  client.print("<temperature>");
  client.print(t, 1);
  client.println("</temperature><br/>");
  client.print("<humidity>");
  client.print(h, 0);
  client.println("</humidity><br/>");
  
  client.println("<a href=\"/ledOn\"><button>LED ON</button></a>");
  client.println("<a href=\"/ledOff\"><button>LED OFF</button></a><br />"); 
  client.println("<a href=\"/fanAuto\"><button>FAN AUTO MODE</button></a>");
  client.println("<a href=\"/fanManual\"><button>FAN MANUAL MODE</button></a><br />");
  client.println("<a href=\"/fanOn\"><button>FAN ON</button></a>");
  client.println("<a href=\"/fanOff\"><button>FAN OFF</button></a><br /> ");
  client.println("<a href=\"/OutGood\"><button>OUT GOOD</button></a>");
  client.println("<a href=\"/OutBad\"><button>OUT BAD</button></a><br /> ");
  client.println("</html>"); 
  delay(1);
}
