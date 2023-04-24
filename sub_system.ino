#include <FlexiTimer2.h>
#include <SoftwareSerial.h>
#include "DHT.h"
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Set the LCD address to 0x3f for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x26, 16, 2);

#define DHTPIN 4                               // Digital pin connected to the DHT sensor  //온습도 센서 연결 핀
#define DHTTYPE DHT11                          // DHT 11 모델(온습도 센서)
#define countof(a) (sizeof(a) / sizeof(a[0]))  //sizeof 연산자를 사용해서 a의 변수형 크기를 배열로 나눈 개수
#define button 2                               // 비상작동 버튼
#define relay 9                                // 전원 차단 릴레이 모듈
#define piezo 8

byte relay_state = 0;  // 0부터 255를 저장하는 relay_state 변수 선언(릴레이가 켜지면 1, 꺼지면 0인 값을 가짐)
byte piezo_state = 0;  // 0부터 255를 저장하는 piezo_state 변수 선언(부저가 켜지면 1, 꺼지면 0인 값을 가짐)
unsigned long timeVal = 0;

DHT dht(DHTPIN, DHTTYPE);
ThreeWire myWire(6, 5, 7);  // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);
SoftwareSerial P_Serial(A2, A1);  //3,4
int tones[] = { 330 };            //소리낼 음 설정
char datestring[20];              //20문자 배열 datestring 생성
RtcDateTime now;
String timedata;
char timestring[20];

void flash() {
  interrupts();
  now = Rtc.GetDateTime();
  snprintf_P(datestring,              // char* datestring (포인터 datestring)
             countof(datestring),     //size_t datestring (datestring을 출력할 문자 개수)
             PSTR("%04u/%02u/%02u"),  //const char* pstr 출력할 서식
             now.Year(),              //현재 년도
             now.Month(),             //현재 월
             now.Day());              //현재 요일
  //lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print(datestring);
  snprintf_P(datestring,              // char* datestring (포인터 datestring)
             countof(datestring),     //size_t datestring (datestring을 출력할 문자 개수)
             PSTR("%02u:%02u:%02u"),  //const char* pstr 출력할 서식
             now.Hour(),              //현재 시간
             now.Minute(),            //현재 분
             now.Second());           //현재 초
  lcd.setCursor(4, 1);
  lcd.print(datestring);
}

void setup() {
  Serial.begin(9600);             //0, 1
  P_Serial.begin(9600);           // 3, 4
  dht.begin();                    //dht 모듈 작동
  Rtc.Begin();                    //rtc 모듈 작동
  pinMode(A2, INPUT);             // 시리얼 통신 수신 핀
  pinMode(A1, OUTPUT);            // 시리얼 통신 송신 핀
  pinMode(button, INPUT_PULLUP);  // 아두이노 자체 풀업 방식으로 버튼에 입력
  pinMode(relay, OUTPUT);         // 출력하는 릴레이 모듈
  pinMode(piezo, OUTPUT);         // 피에조 부저
  digitalWrite(relay, LOW);       // 전원 꺼져있는 상태
  relay_state = 0;                // 릴레이의 상태를 0로 저장
  digitalWrite(piezo, LOW);       // 부저 꺼져있는 상태
  piezo_state = 0;                // 부저의 상태를 0으로 저장
  attachInterrupt(digitalPinToInterrupt(button), abcd, FALLING);  //디지털핀에 연결된 버튼을 눌러 인터럽트 발생, abcd함수 실행, FALLING방식(눌렀을 때 인터럽트 발생)
  lcd.begin();
  lcd.backlight();
  // Turn on the blacklight and print a message.
  lcd.print("    WELCOME!    ");

  FlexiTimer2::set(500, flash);   //0.5 간격으로 실행되게 설정
  FlexiTimer2::start();
  //RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);      //현재 컴퓨터 시간 저장
  //printDateTime(compiled);

  // RtcDateTime now = Rtc.GetDateTime();
  // if (now < compiled)
  // {
  //     Serial.println("RTC is older than compile time!  (Updating DateTime)");
  //     Rtc.SetDateTime(compiled);
  // }
}

void loop() {

  now = Rtc.GetDateTime();         //현재 시간 받아오기
  snprintf_P(timestring,           // char* datestring (포인터 datestring)
             countof(timestring),  //size_t datestring (datestring을 출력할 문자 개수)
             PSTR("%02u:%02u"),    //const char* pstr 출력할 서식
             now.Hour(),           //현재 시간
             now.Minute());

  if ((String)timestring == (String)timedata) {    //timedata가 timestring과 문자가 같으면
    digitalWrite(relay, HIGH);                //릴레이가 켜진다

    timeVal = millis();               //프로그램이 실행되고 난 시간을 timeVal에 저장
  }

  if (millis() - timeVal >= 30000) {     //프로그램이 실행되고 난 시간 - 릴레이가 켜진 시간

    timeVal++;                  
    digitalWrite(relay, LOW);     //릴레이가 꺼진다
  }


  while (P_Serial.available()) {         //P_Serial이 연결되었을 때
    String str = P_Serial.readString();  //수신 받은 문자를 str에 저장
    Serial.println(str);

    if (str.equals("tp"))  //메인에서 보낸 tp의 문자가 "tp"일 경우 (요청)
    {
      float t = dht.readTemperature();                         //온도 값을 float형 변수 t에 저장
      if (isnan(t)) {                                          //습도 또는 온도 또는 조도의 값을 못읽을 경우 (is not a number)
        Serial.println(F("Failed to read from DHT sensor!"));  //"Failed to read from DHT sensor!" 출력
        return;
      }
      Serial.println(str);                      //온도 값 나한테 출력하고
      P_Serial.print((String)t + " C" + "\r");  //메인 서버에 "T: (온도 값 문자)" 송신
      Serial.print(F("Temperature: "));         //"% Temperature: " 출력
      Serial.println(t);                        //온도 값 출력
    }

    else if (str.equals("hum"))  //메인에서 보낸 hum의 문자가 "hum"일 경우 (요청)
    {
      float h = dht.readHumidity();                            //습도 값을 float형 변수 h에 저장
      if (isnan(h)) {                                          //습도 또는 온도 또는 조도의 값을 못읽을 경우 (is not a number)
        Serial.println(F("Failed to read from DHT sensor!"));  //"Failed to read from DHT sensor!" 출력
        return;
      }
      Serial.println(str);                      //습도 값 나한테 출력하고
      P_Serial.print((String)h + " %" + "\r");  //메인 서버에 "H: (습도 값 문자)" 송신
      Serial.print(F("Humidity: "));            //"Humidity: " 출력
      Serial.println(h);                        //습도 값 출력
    }

    else if (str.equals("jodo"))  //메인에서 보낸 jodo의 문자가 "jodo"일 경우 (요청)
    {
      int analogPin = A0;                                      //아날로그핀 A0연결 선언
      int i = 0;                                               //변수 i 선언
      i = analogRead(analogPin);                               //받은 조도 값을 변수 i에 저장
      if (isnan(i)) {                                          //습도 또는 온도 또는 조도의 값을 못읽을 경우 (is not a number)
        Serial.println(F("Failed to read from DHT sensor!"));  //"Failed to read from DHT sensor!" 출력
        return;
      }
      Serial.println(str);                        //조도 값 나한테 출력하고
      P_Serial.print((String)i + " lux" + "\r");  //메인 서버에 "I: (조도 값 문자)" 송신
      Serial.print(F("illuminance: "));           //"illuminance: " 출력
      Serial.println(i);                          //조도 값 출력
    }

    else if (str.equals("date"))  //메인에서 보낸 date의 문자가 "date"일 경우 (요청)
    {
      now = Rtc.GetDateTime();  //현재 시간을 불러옴

      if (!now.IsValid()) {  //now로 받은 값이 유효하지않을 때
        // Common Causes:
        //    1) the battery on the device is low or even missing and the power line was disconnected //배터리 확인
        Serial.println("RTC lost confidence in the DateTime!");  //"RTC lost confidence in the DateTime!" 출력
      }
      snprintf_P(datestring,                          // char* datestring (포인터 datestring)
                 countof(datestring),                 //size_t datestring (datestring을 출력할 문자 개수)
                 PSTR("%04u/%02u/%02u %02u:%02u\r"),  //const char* pstr 출력할 서식
                 now.Year(),                          //현재 년도
                 now.Month(),                         //현재 월
                 now.Day(),                           //현재 요일
                 now.Hour(),                          //현재 시간
                 now.Minute());                       //현재 분

      Serial.println(str);         //날짜와 시간 나한테 출력하고
      P_Serial.print(datestring);  //메인 서버에 "datestring" 문자 송신
      Serial.println(datestring);  //날짜와 시간 출력
    }

    else if (str.equals("rel")) {  //메인에서 "rel"이 넘어오면
      if (relay_state == LOW) {    //relay가 꺼져있으면
        Serial.println(str);
        P_Serial.print("RELAY OFF\r");  //메인에게 RELAY OFF 송신
        Serial.println(relay_state);
      } else {  //relay가 켜져있으면
        Serial.println(str);
        P_Serial.print("RELAY ON\r");  //메인에게 RELAY ON 송신
        Serial.println(relay_state);
      }
    }

    else if (str.equals("Relay ON")) {  //메인에서 "Relay ON"이 넘어오면
      if (relay_state == LOW) {         //relay가 꺼져있으면
        digitalWrite(relay, HIGH);      //relay 켜기
        relay_state = 1;
        digitalWrite(piezo, LOW);      //piezo 소리 안남
        P_Serial.print("RELAY ON\r");  //메인에게 "RELAY ON" 송신
        Serial.println(relay_state);

      } else {                           //relay가 켜져있으면
        P_Serial.print("ALREADY ON\r");  //메인에게 "ALREADY ON" 송신
        Serial.println(relay_state);
      }
    }

    else if (str.equals("Relay OFF")) {  //메인에서 "Relay OFF"가 넘어오면
      if (relay_state == HIGH) {         //릴레이가 켜져있으면
        digitalWrite(relay, LOW);        //릴레이 끔
        relay_state = 0;
        digitalWrite(piezo, HIGH);      //부저 켜기
        P_Serial.print("RELAY OFF\r");  //메인에게 "RELAY OFF" 송신
        Serial.println(relay_state);

      } else {                            //릴레이가 꺼져있으면
        P_Serial.print("ALREADY OFF\r");  //메인에게 "ALREADY OFF" 송신
        Serial.println(relay_state);
      }
    }

    else if (str.equals("buzzer")) {  //메인에서 buzzer이 넘어오면
      if (piezo_state == LOW) {       //부저가 꺼져있으면
        Serial.println(str);
        P_Serial.print("buzzer on!!\r");  //메인에게 "buzzer on!!" 송신
        digitalWrite(piezo, HIGH);        //부저 켜기
        piezo_state = !piezo_state;

      } else {  //부저가 켜져있으면
        Serial.println(str);
        P_Serial.print("buzzer off!!\r");  //메인에게 "buzzer off!!" 송신
        digitalWrite(piezo, LOW);
        piezo_state = !piezo_state;
      }
    }

    else if (str.length() == 5) {
      timedata = str;
    }
  }
}

void abcd() {
  digitalWrite(relay, LOW);  //릴레이 끄기
  relay_state = 0;
  digitalWrite(piezo, HIGH);  //부저 켜기
  piezo_state = 1;
  Serial.println("Interrupt btn");
}
