#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Wire.h>
#include <SoftwareSerial.h>

// define 설정
#define sw_serial_rx 4
#define sw_serial_tx 5
#define arr_size 40

// software Serial 통신 연결
SoftwareSerial sw_serial(sw_serial_rx, sw_serial_tx);

// 통신 데이터 저장변수 크기 
char receivedData[arr_size] = { 0 };
// 문자열을 저장하기 위한 변수
String subData;

// lcd 생성 , i2c 통신, 주소 0x3F, 2행 16열
LiquidCrystal_I2C lcd(0x3F, 16, 2);  

// 키패드 입력값 저장 변수
char key;
// 키패드 배열
char keyStr[] = "0123456789*#ABCD";
// 키패드 버튼에 문자열 맵핑
const byte ROWS = 4;  //four rows
const byte COLS = 4;  //three columns
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
byte rowPins[ROWS] = { A3, 2, A1, A0 };  //connect to the row pinouts of the kpd
byte colPins[COLS] = { 9, 8, 7, 6 };     //connect to the column pinouts of the kpd
//키패드 생성
Keypad kpd = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// tone 출력에 사용되는 주파수 리스트
int note_array[] = { 196, 220, 247, 262, 294, 330, 392, 415, 440, 494, 523, 587, 622, 659, 698, 784 };

// 입력하는 비밀번호 저장 변수
String password = "";
// 비밀번호 저장 변수
String PW = "1234";
// 릴레이 타이머 변수
String msg = "24:00";
// 비밀번호 틀린 횟수 저장 변수
byte cntPw = 1;
// 데이터를 받을 때 timeout를 사용하기 위한 변수
unsigned long pre_time;
// 메뉴 페이지를 제어하는 변수
byte menu_page1 = 0;
// 시스템 잠금 변수 0: 열림, 1:잠금
byte lockFlag = 1;

void setup() {
  // 컴퓨터 시리얼 통신
  Serial.begin(9600);  
  // 아두이노 간 통신
  sw_serial.begin(9600);
  // lcd 초기화 및 출력
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Starting System");
  delay(1000);
  lcd.clear();
  // 시스템 도어락
  lockFlag = enterSystem();
}

void loop() {
  if (lockFlag) {
    lockFlag = enterSystem();
  } else {
    // 메뉴 페이지
    if (menu_page1 == 0) {
      lcdPrint(String("1.") + F("Temperature   "), String("2.") + F("Humidity      "), 0, 0, 0);
    } else if (menu_page1 == 1) {
      lcdPrint(String("3.") + F("Illuminance   "), String("4.") + F("Date & Clock  "), 0, 0, 0);
    } else if (menu_page1 == 2) {
      lcdPrint(String("5.") + F("Relay state?  "), String("6.") + F("Relay ON      "), 0, 0, 0);
    } else if (menu_page1 == 3) {
      lcdPrint(String("7.") + F("Relay OFF     "), String("8.") + F("Relay Timer?  "), 0, 0, 0);
    } else if (menu_page1 == 4) {
      lcdPrint(String("9.") + F("Relay time set"), String("0.") + F("Buzzer        "), 0, 0, 0);
    } else if (menu_page1 == 5) {
      lcdPrint(String("C.") + F("LOCK system   "), String("D.") + F("change pw     "), 0, 0, 0);
    }
    // 키패드 입력시 해당 기능 수행
    key = kpd.getKey();
    if (key) {
      toneKey(&key);
      switch (key) {
        case '0':
          subCommand((String)F("buzzer"), String(F("BUZZER!!!!")), 10000);
          break;
        case '1':
          subCommand((String)F("tp"), String(F("Temperature")), 10000);
          break;
        case '2':
          subCommand((String)F("hum"), String(F("Humidity")), 10000);
          break;
        case '3':
          subCommand((String)F("jodo"), String(F("Illuminance")), 10000);
          break;
        case '4':
          subCommand((String)F("date"), String(F("")), 10000);
          break;
        case '5':
          subCommand((String) "rel", String(F("Relay State?")), 10000);
          break;
        case '6':
          subCommand((String)F("Relay ON"), String(F("Relay is")), 10000);
          break;
        case '7':
          subCommand((String)F("Relay OFF"), String(F("Relay is")), 10000);
          break;
        case '8':
          lcdPrint(String("Relay Timer?"), msg, 0, 0, 1);
          delay(2000);

          break;
        case '9':
          relayTimer();
          break;
        case 'A':
          menu_page1--;
          Serial.println(menu_page1);
          if (menu_page1 == 255) {
            menu_page1 = 5;
          }
          lcd.clear();
          break;
        case 'B':

          menu_page1++;
          Serial.println(menu_page1);
          if (menu_page1 == 6) {
            menu_page1 = 0;
          }
          lcd.clear();
          break;
        case 'C':
          lcdPrint(F("Lock?"), F("1. yes  2. no"), 3, 0, 1);
          while (1) {
            key = kpd.getKey();
            if (key == '1') {
              lockFlag = 1;
              break;
            } else if (key == '2') {
              break;
            }
          }

          break;
        case 'D':
          changePW();
          break;
        default:
          break;
      }
    }
  }
}

// 시스템 잠금 해제 함수
// 4자리 값 입력 받고 PW와 같으면 시스템 사용 가능하다 틀리면 1시간 기다려야 한다.
// return 0 - 잠금 해제, 1, 계속 잠금
byte enterSystem() {
  lcdPrint(F("Password?"), F(">>"), 0, 0, 1);
  while (1) {
    key = kpd.getKey();
    int i = 0;
    password = "";
    while (1) {
      key = kpd.getKey();
      if (key) {
        password += key;
        //i++;
        Serial.println(key);
        lcd.setCursor(3 + (i++) * 2, 1);
        lcd.print("*");
        delay(100);
        if (i == 4) {
          i = 0;
          break;
        }
      }
    }
    if (password == PW) {
      lcdPrint(F("Correct!!"), F(""), 3, 0, 1);
      return 0;
      delay(2000);
      break;

    } else {
      if (cntPw == 4) {
        lcdPrint(F("4 times"), F("wait 1 hour"), 1, 1, 1);
        cntPw = 1;
        lcd.noBacklight();
        delay(3600000);
        return 1;
      } else {
        lcdPrint(F("Wrong!!"), String(cntPw), 3, 0, 1);
      }
      cntPw++;
    }
  }
  delay(10000);
  lcd.backlight();
  lcd.clear();
}

// 데이터 받는 함수
// char* ary - 데이터 받을 공간
// unsigned long time - timeout 설정값
// timeout 전까지 데이터를 받거나, \r이 들어올 때까지 데이터를 받는다.
void receiveData(char* ary, unsigned long time) {
  int index = 0;
  byte flag = 0;
  char d;
  
  pre_time = millis();
  while (millis() - pre_time < time) {
    if (sw_serial.available()) {
      d = sw_serial.read();
      if (d == '\r') {
        flag = 1;
        break;
      }
      ary[index++] = d;
    }
  }
  if (flag) {
    ary[index] = '\0';
    flag = 0;
  }
}

// 데이터 보내는 함수
// String data - 보내고 싶은 문자열
// unsigned long time - receiveData()함수의 timeout에 사용될 함수
// return String - 데이터를 보냈을 때 받은 데이터를 반환
String sendData(String data, unsigned long time) {
  sw_serial.print(data);
  Serial.println(data);
  String str;
  byte flag = 1;
  init_ary(receivedData, arr_size);
  receiveData(receivedData, time);
  str = String(receivedData);
  if (str.equals(""))
    return (String) "null";
  return str;
}

// 배열 초기화 함수
void init_ary(char* ary, int size){
  for( int i = 0; i < size; i++){
    ary[i] = 0;
  }
}

// 명령어 보내는 함수
// 받은 명령어를 lcd로 출력한다.
void subCommand(String data, String row, unsigned long time) {
  lcdPrint(String("Wait...."), String(""), 3, 0, 1);
  subData = sendData(data, time);

  if (subData.equals("null")) {
    if (row.equals(F("OK!"))) {
      lcdPrint(String(F("OK!!")), data, 2, 3, 1);
      delay(1000);
    } else {
      lcdPrint(String(F("TIME OUT!!")), subData, 2, 3, 1);
      Serial.println(subData);
      delay(1000);
    }

  } else {
    if (row.equals("")) {
      int split_index = subData.indexOf(String(" "));
      String date_temp = subData.substring(0, split_index);
      String time_temp = subData.substring(split_index, subData.length());
      lcdPrint(date_temp, time_temp, 1, 1, 1);
    } else {
      lcdPrint(row, subData, 2, 3, 1);
    }
    Serial.println(subData);
    while (1) {
      key = kpd.getKey();
      if (key) {
        toneKey(key);
        break;
      }
    }
  }
  lcd.clear();
  subData = "";
}

// 입력받은 시간을 보조 시스템으로 전송하는 함수
// 시간에서 시는 24단위로 한다
void relayTimer() {
  lcdPrint(String("auto relay timer"), String("select 1, 2, 3"), 0, 1, 1);
  char timeFlag = 0;

  while (1) {
    key = kpd.getKey();
    msg = "";
    if (key) {
      switch (key) {
        case '1':
          lcdPrint(String("1 timer"), String(">>"), 0, 0, 1);
          int i = 0;
          char cntFlag = 0;
          char fourLimitFlag = 0;
          while (1) {
            key = kpd.getKey();
            if (key) {


              delay(100);

              lcd.setCursor(4 + i, 1);
              if (i == 0) {
                if (key >= '0' && key <= '2') {
                  lcd.print(key);
                  msg += key;
                  cntFlag = 1;
                  Serial.println(i);
                  if (key == 2) {
                    fourLimitFlag = 1;
                  }
                } else {
                  continue;
                }
              } else if (i == 1) {
                if (fourLimitFlag == 1) {
                  if (key >= '0' && key <= '3') {

                    lcd.print(key);
                    msg += key;
                    cntFlag = 1;
                    i++;
                    lcd.setCursor(4 + i, 1);
                    lcd.print(":");
                    msg += ":";

                    Serial.println(i);
                  } else {
                    continue;
                  }
                } else {
                  if (key >= '0' && key <= '9') {

                    lcd.print(key);
                    msg += key;
                    cntFlag = 1;
                    i++;
                    lcd.setCursor(4 + i, 1);
                    lcd.print(":");
                    msg += ":";

                    Serial.println(i);
                  } else {
                    continue;
                  }
                }

              } else if (i == 3) {
                if (key >= '0' && key <= '5') {
                  lcd.print(key);
                  cntFlag = 1;
                  msg += key;

                  Serial.println(i);
                } else {
                  continue;
                }
              } else if (i == 4) {
                if (key >= '0' && key <= '9') {
                  lcd.print(key);
                  cntFlag = 1;
                  msg += key;

                  Serial.println(i);
                } else {
                  continue;
                }
              }
              if (cntFlag) {
                cntFlag = 0;
                i++;
              }

              if (i == 5) {
                key = 0;
                break;
              }
            }
          }

          if (i == 5) {
            lcdPrint(msg, F("1. yes  2. no"), 0, 0, 1);
            while (1) {

              key = kpd.getKey();
              if (key == '1') {
                subCommand(msg, String(F("OK!")), 3000);
                timeFlag = 1;
                break;
              }
              if (key == '2') {
                timeFlag = 1;
                break;
              }
            }
          }

          break;

        default:
          break;
      }
    }
    if (timeFlag) {
      break;
    }
  }

}

// 비밀번호 변경 함수
// 기존 비밀번호를 입력받고 맞으면 새로운 비밀번호를 수정할 수 있다
void changePW() {
  lcdPrint(F("Change Password?"), F("1. yes, 2 no"), 0, 0, 1);

  while (1) {
    key = kpd.getKey();
    if (key == '1') {
      lcdPrint(F("Now password"), F(">>"), 0, 0, 1);
      int i = 0;
      password = "";
      while (1) {
        key = kpd.getKey();
        if (key) {
          password += key;
          //i++;
          Serial.println(key);
          lcd.setCursor(3 + (i++) * 2, 1);
          lcd.print("*");
          delay(100);
          if (i == 4) {
            i = 0;
            break;
          }
        }
      }
      if (password == PW) {
        lcdPrint(F("Correct!!"), F(""), 3, 0, 1);
        delay(1000);
        lcdPrint(F("New Password!!"), F(">>"), 3, 0, 1);
        password = "";
        while (1) {
          key = kpd.getKey();
          if (key) {
            password += key;
            Serial.println(key);
            lcd.setCursor(3 + (i++) * 2, 1);
            lcd.print("*");
            delay(100);
            if (i == 4) {
              PW = password;
              Serial.println(PW);
              i = 0;
              break;
            }
          }
        }
      } else {
        lcdPrint(F("Wrong!!"), password, 0, 0, 1);
        delay(5000);
      }
      lcd.clear();
      break;
    } else if (key == '2') {
      lcd.clear();
      break;
    }
  }
}

// lcd를 출력하는 함수
// String row1 - 첫 번째 줄에 출력할 변수
// String row2 - 두 번째 줄에 출력할 변수
// int num1 - 첫 번째 줄의 오프셋
// int num2 - 두 번째 줄에 오프셋
// byte cl - clear() 실행 여부 1 : 실행 0 : 미실행

void lcdPrint(String row1, String row2, int num1, int num2, byte cl) {
  if (cl) {
    lcd.clear();
  }

  lcd.setCursor(num1, 0);
  lcd.print(row1);
  lcd.setCursor(num2, 1);
  lcd.print(row2);
}

// 눌러진 키에 해당하는 주파수 출력 함수
void toneKey(char* key) {
  tone(A2, note_array[index_arr(keyStr, 16, *key)], 100);
}

// 문자열에 특정 문자의 index 출력 함수
int index_arr(char* arr, int size, char val) {
  // 배열 원소를 차례대로 순회하면서 val 값과 일치 여부 검사
  for (int i = 0; i < size; i++) {
    if (arr[i] == val) return i;  // 있으면 인덱스 위치 반환
  }
  return -1;  // 배열 원소들을 전부 확인 후 없으면 -1 반환
}
