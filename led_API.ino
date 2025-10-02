#include <ArduinoHttpClient.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <Client.h>
#include <assert.h>
#include <SoftwareSerial.h>
//#include <SPI.h>
#include <DMD2.h>
#include <fonts/Arial_Black_16.h>
#include <fonts/SystemFont5x7.h>
#include <fonts/Arial14.h>
#include <DS3231.h>

// Init the DS3231 using the hardware interface
DS3231  rtc(SDA, SCL);  

SoftDMD dmd(2,1);  // DMD controls the entire display   -test

uint8_t i=0, j=0;
char receiveVal[200];

int Plan, process, actual;
//int strplan_qty;
//String plan, act; 

int time_break_1 = 610;   //--> day shift break
int time_break_2 = 750;
int time_break_3 = 910;

int time_break_4;  //--> night shift break
int time_break_5;
int time_break_6;

//byte mac[]= {0x14,0xDD, 0xA9, 0xD4, 0x47, 0xAF};
 byte mac[]= {0xDE,0xAD, 0xBE, 0xEF, 0xFE, 0xED};
//Arduini IP, Subnetmask, Getway
byte ip[]= {10,254,115,250}; 
static byte Gateway_IPAddr[] = {10,254,115,1 };
static byte Subnet_Mask[] = {255,255,255,0 }; 
//Server IP
byte server[]= {10,0,108,10}; 
EthernetClient client; 

//String test="";
//String test2="";

//-----------------------------------------Define for ip address---------------------
// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield


void setup() {
  
    delay(3000);
    dmd.clearScreen();  
    pinMode(2, INPUT);
    dmd.setBrightness(230); 
    dmd.selectFont(SystemFont5x7);  
    Serial.begin(9600);
    
    dmd.begin();   
    rtc.begin();
  
    int k = 0;

}

void GetData(String API){

   // start the Ethernet connection:
    if (Ethernet.begin(mac) == 0) {
      Serial.println("Failed to configure Ethernet using DHCP");
      // try to congifure using IP address instead of DHCP:
      Ethernet.begin(mac, ip);
    }
    // give the Ethernet shield a second to initialize:
    delay(1000);
   // Serial.println("connecting...");
  
   // if you get a connection, report back via serial:
    if (client.connect(server, 80)) {
      Serial.println("connected");
     
      
      //MakeHttpRequest();
      client.print("GET " + API);
      client.println(" HTTP/1.1");
      client.println("Host: 10.0.108.10");

       //test ="1040";
       //test2 ="100";
      //client.println("Connection: close");
      client.println();
  
    } else {
      // if you didn't get a connection to the server:
      Serial.println("connection failed");
    }
}


int progress_plan (int t){
      
      int progressplan = 0;
      int currentsecond = t*60;
      
      if ( 480<= t && t < 1030){                        //--> dayshift
          int cycletime_day = round(28800/Plan);
          int extra_time = 0;
          if ( t >= time_break_1 && t< time_break_2){        //--> 8h
            extra_time = 600;
          }
          else if ( t >= time_break_2 && t < time_break_3){
            extra_time = 3600;
          }
          else if (t >= 910){
            extra_time = 4200;
          }
          //Serial.println(totalPlan);
         // Serial.println(cycletime_day);
          int delta_time = currentsecond - 8*3600 - extra_time;
           //Serial.println(delta_time);
          progressplan = round(delta_time/cycletime_day);
           
      }

      else if ( 1030<= t && t < 1200){                  //--> ot day shift
          progressplan = Plan;
      }
      
      else if ( 1260<= t || t < 370){                   //--> nightshift
          int cycletime_night = round(28800/Plan);
          int extra_time = 0;
          if ( t < time_break_5 || t > time_break_4){
            extra_time = 600;
          }
          else if ( t >= time_break_5 && t < time_break_6){
            extra_time = 3600;
          }
          else if (t >= time_break_6){
            extra_time = 4200;
          }
          if (t<=480){
               currentsecond = currentsecond + 24*3600;
            }
          
          int delta_time = currentsecond - 21*3600 - extra_time;
          progressplan = round(delta_time/cycletime_night);
      }

      else if ( 370<= t && t < 430){                  //--> ot night shift
          progressplan = Plan;   
      }
      else{
            progressplan = 0;
      }
      return progressplan;
  }


String signal_start;
String start_mode = "online";        //---> switch mode online = 00000 / offline = 11111
String data;

void loop() {

 
//Serial.flush(); 
//Serial.begin(9600);
//Serial.print("1");

  
  /*
  totalPlan = 1440;
  int t = 500;
  Serial.println(progress_plan(t)); //--> 60
  Serial.println("------------------------------------");
  Serial.println(progress_plan(630)); //--> 420
  Serial.println("------------------------------------");
  Serial.println(progress_plan(780));
  Serial.println("------------------------------------");
  Serial.println(progress_plan(920));
  */

 
  if(Serial.available() > 0){
          delay(10);
          data = "";
             int k=0; 
             data = "";
             
             while(Serial.available())
             {
              
               receiveVal[k]=Serial.read();
               //Serial.println(receiveVal[k]);
               k++;
               delay(10);
             }
             
             data = "";
              for (int l=0;l<=k;l++)
              {
                if(l==0){
                  data = receiveVal[l];
              }
              else{
                data= data + receiveVal[l];                                                              
              }
               delay(10);
              //Serial.println(data);
            }

            signal_start = "1";
    
           int po =  data.indexOf(":");
           String key = data.substring(0, po);

           if (key == "mode"){
              String get_str = data.substring(po+1, data.length()-2);
              Serial.print(F("strget "));
              Serial.println(get_str);
              if (get_str.toInt()>0){
                  start_mode = "offline";
              }
              else{
                  start_mode = "online";
              }
              Serial.print(F("mode: "));
              Serial.println(start_mode);
           }
}

  if(start_mode =="offline"){
    //Serial.print(F("Offline mode!"));
    //dmd.begin();
        //----------------------------offline mode------------------------------------
        /*
        if(Serial.available() > 0)          
        {
          delay(10);
          data = "";
          //dataMMC1= "";
          //Serial.println("2");
             int k=0; 
             data = "";
             
             while(Serial.available())
             {
              
               receiveVal[k]=Serial.read();
               //Serial.println(receiveVal[k]);
               k++;
               delay(10);
             }
             
             data = "";
              for (int l=0;l<=k;l++)
              {
                if(l==0){
                  data = receiveVal[l];
              }
              else{
                data= data + receiveVal[l];                                       //--> MMC1:1480%0%-1480%
                                                                                  //--> D1:610#750#910#
                                                                                  //--> N1:1390@100@240@   
                                                                                  //--> mode:online                                                            
              }
               delay(10);
              //Serial.println(data);
            }
           //Serial.println(data);
            */
           //-------------------------------------get actual-----------------------------------------
           //
           //dataMMC1 = data;
           int pos_define =  data.indexOf(":");
           String var_name = data.substring(0, pos_define);

           //if (var_name == "mode"){
             // start_mode = data.substring(pos_define+1, data.length());
           //}
           
           if (var_name == "MMC1" && signal_start == "1"){
     
                String timeget1 = rtc.getTimeStr();         // --> 11:33:51
               // Serial.println(timeget1);
                int pos_dev = timeget1.indexOf(":");  //=> 2
                
                String hour1 = timeget1.substring(0, pos_dev);
                String time_cut = timeget1.substring(pos_dev+1 , timeget1.length()-1);
                int pos_dev1 = timeget1.indexOf(":");  //=> 2
                String minu = time_cut.substring(0, pos_dev1);
    
                int t1 = hour1.toInt()*60 + minu.toInt();
    
               actual =0;
               String test = data.substring(pos_define+1, data.length());
               //Serial.println(test);  //=> 1440%650%-120%4
    
               int temp1 = test.indexOf("%");
              
               String strPlan = test.substring(0,temp1);
               Plan = strPlan.toInt();
               String test2= test.substring(temp1+1,test.length());//=> 650%-120%4
              // Serial.println(test2);  //=> 1440
               int temp2 = test2.indexOf("%");
               String strActual = test2.substring(0,temp2);
               actual = strActual.toInt();
              // String test3= test2.substring(temp2+1,test2.length());//=> -120%4
               //Serial.println(test3);  //=> 1440
               //int temp3 = test3.indexOf("%");
               //String strPrgress = test3.substring(0,temp3);
              
              String strPrgress = String(actual - progress_plan(t1));
              // String strPrgress = String(actual - progress_plan(510));
              //Serial.println(strPlan);  //=> 1440
              //Serial.println(strActual);  //=> 650
              //Serial.println(actual);  //=> 650
              //Serial.println(strPrgress);  //=> -120
    
              dmd.drawFilledBox(0,0,64,16);  //=> Clear background
    
             dmd.drawString(32,0,"MMC1",GRAPHICS_OFF);  //=>  Cell name
      
             dmd.drawString(64-strPlan.length()*6,8,strPlan,GRAPHICS_OFF);  //=>  Plan
      
             dmd.drawString(32-strActual.length()*6,0,strActual, GRAPHICS_OFF); //=>  Actual
    
             dmd.drawString(32-strPrgress.length()*6,8, strPrgress, GRAPHICS_OFF); //=>  Progress

             signal_start = "0";
              
           }
    
           if (var_name == "D1"){
    
                String value_prog = data.substring(pos_define + 1, data.length());          
                //----------------------------------- GET PROGRESS ---------------------------------------------
                // ------------------------------------- DAYSHIFT ----------------------------------------
            
                
               int pos_prog_1 = value_prog.indexOf("#");
               time_break_1 = value_prog.substring(0, pos_prog_1 ).toInt();
               String data_pro_cut_1 = value_prog.substring(pos_prog_1 + 1, value_prog.length());
        
               //Serial.println(value_prog);
               //Serial.println(pos_prog_1); 
               //Serial.println(time_break_1); 
              // Serial.println(data_pro_cut_1); 
              // Serial.println("--------------------------"); 
              
               int pos_prog_2 = data_pro_cut_1.indexOf("#");
               time_break_2 = data_pro_cut_1.substring(0, pos_prog_2 ).toInt();
               String data_pro_cut_2 = data_pro_cut_1.substring(pos_prog_2 + 1, data_pro_cut_1.length());
    
               //Serial.println(time_break_2); 
               //Serial.println(data_pro_cut_2); 
               //Serial.println("--------------------------");
        
               int pos_prog_3 = data_pro_cut_2.indexOf("#");
               time_break_3 = data_pro_cut_2.substring(0, pos_prog_3 ).toInt();
               String data_pro_cut_3 = data_pro_cut_2.substring(pos_prog_3 + 1, data_pro_cut_2.length());
    
               //Serial.println(time_break_3); 
              // Serial.println(data_pro_cut_3); 
               //Serial.println("--------------------------");
        
               
    
           }
        
           if (var_name == "N1"){
    
               String value_prog = data.substring(pos_define + 1, data.length()); 
               
               int pos_prog_6 = value_prog.indexOf("@");
               time_break_4 = value_prog.substring(0, pos_prog_6).toInt();
               String data_pro_cut_6 = value_prog.substring(pos_prog_6 + 1, value_prog.length());
    
               //Serial.println(time_break_4); 
               //Serial.println(data_pro_cut_6); 
               //Serial.println("--------------------------");
        
               int pos_prog_7 = data_pro_cut_6.indexOf("@");
               time_break_5 = data_pro_cut_6.substring(0, pos_prog_7 ).toInt();
               String data_pro_cut_7 = data_pro_cut_6.substring(pos_prog_7 + 1, data_pro_cut_6.length());
    
               //Serial.println(time_break_5); 
               //Serial.println(data_pro_cut_7); 
               //Serial.println("--------------------------");
         
               int pos_prog_8 = data_pro_cut_7.indexOf("@");
               time_break_6 = data_pro_cut_7.substring(0, pos_prog_8 ).toInt();
               String data_pro_cut_8 = data_pro_cut_7.substring(pos_prog_8 + 1, data_pro_cut_7.length());
    
               //Serial.println(time_break_6); 
               //Serial.println(data_pro_cut_8); 
               //Serial.println("--------------------------");
      
              
           }
           
    
        
        
        
        //Neu Bam Button
        
         if (digitalRead(2) == 1 ) {
          //Serial.println("ABC");
          //Serial.println(dataMMC1);
            if (data!=""){
              delay(500);          
              if (digitalRead(2) == 0 ) {
    
                //Serial.println("up");
                String timeget = rtc.getTimeStr();         // --> 11:33:51
                //Serial.println(timeget);
                int pos_device1 = timeget.indexOf(":");  //=> 2
                
                String h = timeget.substring(0, pos_device1);
                String timeget_cut = timeget.substring(pos_device1+1 , timeget.length()-1);
                int pos_device2 = timeget.indexOf(":");  //=> 2
                String m = timeget_cut.substring(0, pos_device2);
    
                int t = h.toInt()*60 + m.toInt();
    
                actual = actual +1;
                process = actual - progress_plan(t);
    
                //Serial.println(progress_plan(t));
                //Serial.println(process);
                //Serial.println("--------------------");
    
                //dmd.drawFilledBox(32,0,64,16);  //=> Clear background
                dmd.drawFilledBox(0,0,32,16);  //=> Clear background
                dmd.drawString(32-String(actual).length()*6,0,String(actual), GRAPHICS_OFF); //=>  Actual
                dmd.drawString(32-String(process).length()*6,8, String(process), GRAPHICS_OFF); //=>  Progress
                
              }
               
                
            }
            
         }
        
    
    } //end offline mode


   
    if (start_mode == "online"){

          Serial.print(F("online mode"));
           //---------------------------ONLINE MODE--------------------------------------

            /*
            String API1 ="/MMC/MMC/Get_Plan_2?cellname=MMC1";
            GetData(API1);

            String line = "api1" + client.readStringUntil('[');     //remove HTTP HEADER 
            String line1 = client.readString();                     //PAYLOAD json data
            String line_s = line1.substring(0, line1.length()-1);   //--> remove "]" at the end of json data
            Serial.println(line_s);                        // ---> {"cellname":"MMC1","shift":"D","plan_qty":"1440","f_break1":"10h00","t_break1":"10h10","f_break2":"11h40","t_break2":"12h30","f_break3":"15h00","t_break3":"15h10"}
            
            StaticJsonDocument<192> doc;
            DeserializationError error = deserializeJson(doc, line_s);     //--> reference at https://arduinojson.org/v6/assistant/#/step2
            
            if (error) {
             Serial.print(F("deserializeJson() failed: "));
             Serial.println(error.f_str());
              return;
            } 
            
           // const char* cellname = doc["cellname"]; // "MMC1"
            //const char* shift = doc["shift"]; // "D"
            String plan_get = doc["plan_qty"]; // "1440"
            time_break_1 = doc["t_break1"]; // "10h10"
            time_break_2 = doc["t_break2"]; // "12h30"
            time_break_3 = doc["t_break3"]; // "15h10"

            String brk1 = String(time_break_1);
            Serial.println(time_break_3);  

          //----------------------------API get current time + total part --------------------------------
            
             String API2 ="/MMC/MMC/Get_QtyIn_ByCellname_FixPN?cellname=MMC1";
             GetData(API2);
             String line2 = "api2" + client.readStringUntil('[');       //remove HTTP HEADER 
             String line12 = client.readString();                       //PAYLOAD json data
             String line2_s = line12.substring(0, line12.length()-1);   //--> remove "]" at the end of json data
             Serial.println(line2_s);                                   // --->{"time_current":"16:17:26","qty_in":0}
              
             StaticJsonDocument<64> doc1;
             DeserializationError error1 = deserializeJson(doc1, line2_s);
            
            if (error1) {
              Serial.print(F("deserializeJson() failed: "));
              Serial.println(error1.f_str());
              return;
            } 
            
            const char* time_current = doc1["time_current"]; // "time_current"
            actual = doc1["qty_in"];             // "qty_in"
            String act = String(actual);
        
           Serial.println(time_current);
           Serial.println(actual);
           String Plan_take = String(Plan);            
          */                                                                                        
          
         String API ="/MMC/MMC/Get_Plan_Actual_Process?cellname=MMC1";       
         GetData(API);
         delay(1000);
         String line2 = "api" + client.readStringUntil('[');       //remove HTTP HEADER 
         String line12 = client.readString();                       //PAYLOAD json data
         String line2_s = line12.substring(0, line12.length()-1);   //--> remove "]" at the end of json data
         Serial.println(line2_s);                                   // --->1440,645,-114

          //----------------------------------------------Display on led board-------------------------------------------------------------

          int pos = line2_s.indexOf(',');
          String plan = line2_s.substring(0,pos);
          Plan = plan.toInt();
          String other = line2_s.substring(pos+1, line2_s.length()); //-->645,-114

          pos = other.indexOf(',');
          String act = other.substring(0, pos);
          actual = act.toInt();
          String pro = other.substring(pos+1, other.length());

          Serial.print(F("Plan: "));
          Serial.println(plan);
          Serial.print(F("act: "));
          Serial.println(act);
          Serial.print(F("process: "));
          Serial.println(pro);
          client.stop();

          dmd.drawFilledBox(0,0,64,16);  //=> Clear background
          //dmd.drawString(0,0,"MMC1",GRAPHICS_OFF);  //=>  Cell name
          //dmd.drawString(32-plan.length()*6,8,plan,GRAPHICS_OFF);  //=>  Plan
          //dmd.drawString(64-act.length()*6,0, act, GRAPHICS_OFF); //=>  Actual
          //dmd.drawString(64-pro.length()*6,8, pro, GRAPHICS_OFF); //=>  Progress
          
          dmd.drawString(32,0, "MMC1",GRAPHICS_OFF);  //=>  Cell name
          dmd.drawString(64-plan.length()*6,8,plan,GRAPHICS_OFF);  //=>  Plan
          dmd.drawString(32-act.length()*6,0, act, GRAPHICS_OFF); //=>  Actual
          dmd.drawString(32-pro.length()*6,8, pro, GRAPHICS_OFF); //=>  Progress

           
            //----------------------------if the server's disconnected, stop the client--------------------------------------
          if (!client.connected()) {
            
            Serial.println();
            Serial.println("disconnecting.");
            delay(10000);
       
          } 
          
         
      }
    
   
    

  delay(100);
  

}
