Change:
* seting brightnes from 0 to 100 like seting level of % not 0.3f etc
* rgb from 0x000000 to 0-255 for r,g,b individual

Add:
* Pmod a pin
* map gpio to those conectors like pmod1/2/3, usubA/B (i2c), iled...

Final:
- I want it to be accesible and downloadable library inside arduino ide

Examples:
/*
#include <Saturn> //include basic dependenties as pin mapping, and functions
#include <Saturn.Button> //include buttons functions (if user wants to use buttons)
#inclide <Saturn.display> //include display -,,- (there could be expantion for every module, buttons, joystick, piezo, rtc, etc... more complex libraries would be made just by including existing officials libraries like for piezo tone or rfid modul where it would just like the rc522 library but renamed or simplified funcrtions for Saturn user)

Saturn saturn;

Saturn.display display;

Saturn.Button button(Pmod1.Pin1)

void setup() {
  display.begin();		 
}

void loop() {
  button.on("click") {
    display.clear();
    display.drawText(16,20,"Hi!");
    display.show();
    delay(1000);
    display.clear();	
  }
}*/
