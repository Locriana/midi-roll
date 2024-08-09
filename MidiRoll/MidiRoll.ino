#include <freertos/FreeRTOS.h>

#include <Adafruit_GFX.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "MidiVis.h"

//these are not the standard/optimal pins
//and they may not work @ full speed
//however... they work okay for MIDI data rate of 31250 bit/s
#define MIDI_RX_PIN     32
#define MIDI_TX_PIN     33

//here we define the display interface pins
//please remember about the E pin for larger HUB75 displays
//(A-E are for selecting scan areas)
enum HubPins {
  R1_PIN = 25,
  G1_PIN = 26,
  B1_PIN = 27,
  R2_PIN = 14,
  G2_PIN = 12,
  B2_PIN = 13,
  A_PIN = 21,
  B_PIN = 19,
  C_PIN = 5,
  D_PIN = 17,
  E_PIN = 18,
  LAT_PIN = 4,
  OE_PIN = 15,
  CLK_PIN = 16
};

//effective number of notes to display = the width of the roll
//88 is the standard piano keyboard size
//you can set it to more than 88 if your disp is wide enough
//here I use the full range -1
#define ROLL_WIDTH              127

//the lowest note for a standard piano is A0 (MIDI 20 dec)
//here I set it to the lowest possible value to get the full range
#define LOWEST_NOTE_DEFAULT    ((int8_t)0)

#define HIGHEST_NOTE_DEFAULT   (LOWEST_NOTE_DEFAULT + ROLL_WIDTH - 1)

#define DISP_RES_X 128    // width in pixels of each panel module. 
#define DISP_RES_Y 64     // height in pixels of each panel module.
#define DISP_CHAIN 1      // number of HUB75 panels chained one to another

//rather for future implementations - now it should be kept equal
#define ROLL_Y_SIZE   DISP_RES_Y

//this one can be modified if you want the rolt to start more to the right
#define ROLL_X_START  0
#define ROLL_X_END    (ROLL_X_START + ROLL_WIDTH)

//we can select one channel that would have
//different colors for different notes
//the ch number is expressed from 0
//in my tracks I usually use 0 for a drums channel
#define MULTICOLOR_CHANNEL    0

//the whole array which holds colors of pixels
//it will be shifted by default on each 1/16 note
uint16_t roll_array[ROLL_WIDTH][ROLL_Y_SIZE];

//the lowest line which will be "moved up" in the roll
uint16_t roll_line[ROLL_WIDTH];


MatrixPanel_I2S_DMA *dma_display = nullptr;
HardwareSerial MidiSerial(2);
MidiVis vis = MidiVis(&MidiSerial, NOTE_VALUE_SIXTEENTH);


//shifts the MIDI roll up by one pixel
void roll_shift(){
  for(int t=0;t<ROLL_WIDTH;t++){
    for(int y=0;y<(ROLL_Y_SIZE-1);y++){
      roll_array[t][y]=roll_array[t][y+1];
    }
  }
}

//adds a fresh row of pixels at the bottom line
void roll_add(){
  //clear the lower line
  for(int t=0;t<ROLL_WIDTH;t++){
    roll_array[t][ROLL_Y_SIZE-1]=roll_line[t];
  }
}

//copies the roll content to the display mem buffer
//(i.e. it just draws the roll contents)
void roll_update(){
  /*rx is the roll x relative to the left side of the roll
    (so 0 is the first column of _the roll_, not the display)*/
  for(uint8_t rx=0;rx<ROLL_WIDTH;rx++){
    for(int y=0;y<ROLL_Y_SIZE;y++){
      dma_display->drawPixel(ROLL_X_START+rx,y,roll_array[rx][y]);
    }
  }
}

//Convert RGB888 to RGB565 which is used by the library
//It gets converted anyway inside the libs,
//but this is just to keep thing compatible
inline uint16_t disp_color(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

//an "unspecified" color for anything unassigned
#define DEFAULT_COLOR   disp_color(0x50,0x20,0x20)

//here we can define colors for each midi track
//except the one track (selected with MULTICOLOR_CHANNEL)
//which has colors overriden in get_note_color
const uint16_t channel_color_lut[MIDIVIS_NUMBER_OF_CHANNELS] = {
  //ch 1-8
  disp_color(0xF0,0x00,0x00),  //ch 1 (1) here MC 707 drums, overriden
  disp_color(0,0x80,0), //ch 2 (2) bass 707
  disp_color(0x20,0x00,0xC0), //ch 3 (3) pad
  disp_color(0x80,0,0x80),//ch 4 (4) lead
  disp_color(0xFF,0xFF,0x0), //ch 5 fx
  disp_color(0,0xA0,0xA0), //6 arps
  disp_color(0x50,0x30,0x0), //7
  disp_color(0x80,0x80,0), //8
  disp_color(0xFF,0xFF,0xFF),  //ch 9 (1) drums 101
  disp_color(0,0x80,0), //ch 10 (2) bass
  disp_color(0,0x80,0x80), //ch 11 (3) pads
  disp_color(0x80,0,0x80),//ch 12 (4) lead
  DEFAULT_COLOR, //13
  DEFAULT_COLOR, //14
  DEFAULT_COLOR, //15
  DEFAULT_COLOR, //16
};


//channel - start from zero
uint16_t get_note_color(uint8_t channel, uint8_t note){
  //the multicolor channel overwrites the the LUT color for the selected channel
  //set MULTICOLOR_CHANNEL to something > 15 if you dont't need the multicolor feature on any channel
  if(channel == MULTICOLOR_CHANNEL){
    //this mapping here is for Roland MC-101 or 707 drum pads to MIDI notes
    //the parameters are configured for the specific track
    //change accordingly to your sequencer and/or track
    //I used return instead of break to shorten the code
    switch(note){
      case 36: return disp_color(0xFF,0x00,0x00); //kick
      case 38: return disp_color(0xA0,0xA0,0x40); //snare main
      case 37: return disp_color(0x80,0x80,0x40); //snare 2nd
      case 39: return disp_color(0x80,0x80,0x20); //clap
      case 48: return disp_color(0xF0,0xA0,0x00); //tom h
      case 45: return disp_color(0xF0,0xA0,0x00); //tom m
      case 41: return disp_color(0xF0,0xA0,0x00); //tom l
      case 51: return disp_color(0xA0,0x50,0x50); //ride cymbal
      case 56: return disp_color(0xFF,0x40,0x40); //huge concert drum
      default: //other notes/pads of the multicolor channel
        return disp_color(0x60,0x20,0x20); 
    }
  }
  else{ //for other channels than the multicolor one, use the regular LUT
    return channel_color_lut[channel];
  }

  //just in case anything was left out unhandled
  return DEFAULT_COLOR;
}

//works like "transpose"
//you can define shifts + or - by a number of semitones
//12 is one octave
//the parameters are configured for each channel/track
const int8_t note_x_offset[MIDIVIS_NUMBER_OF_CHANNELS] = {
  (int8_t)(-2.5*12), //ch 1
  0,    //ch 2
  -12,  //ch 3
  2,    //ch 4
  12*4, //ch 5
  12,   //ch 6
  0,    //ch 7
  0,    //ch 8
  0, //ch 9
  0, //ch 10
  0, //ch 11
  0, //ch 12
  0, //ch 13
  0, //ch 14
  0, //ch 15
  0  //ch 16
};




void line_from_note_map(){
  memset(roll_line,0,ROLL_WIDTH*sizeof(uint16_t));
  
  for(uint8_t ch=0; ch<MIDIVIS_NUMBER_OF_CHANNELS;ch++){
    int16_t check_start = LOWEST_NOTE_DEFAULT - note_x_offset[ch];
    if(check_start > MIDIVIS_NUMBER_OF_NOTES) {Serial.printf("ch%d, check start %02d out of range\n",ch, check_start); continue;}
    if(check_start < 0){
      check_start = 0;
    }
    int16_t check_end = check_start + ROLL_WIDTH;
    if(check_end < 0) {Serial.printf("ch%d, check end %02d, out of range\n",ch,check_end); continue;}
    if(check_end>MIDIVIS_NUMBER_OF_NOTES) {
      check_end=MIDIVIS_NUMBER_OF_NOTES-1;
    }

    for(uint8_t note = check_start; note<check_end; note++){
      if(vis.activeNotes[ch][note]>0){
        uint8_t roll_idx = note + note_x_offset[ch] - LOWEST_NOTE_DEFAULT;
        roll_line[roll_idx] |= get_note_color(ch,note);
      }  //if(MidiVis.activeNotes[ch][note]>0)
      
    } //for(int8_t note = check_start; note<check_end; note++)
  }   //for(uint8_t ch=0; ch<MIDIVIS_NUMBER_OF_CHANNELS;ch++)



}

void roll_process(){
  line_from_note_map();
  roll_shift();
  roll_add();
  roll_update();
}

//modify to suit your taste :)
void show_splash_screen(void){
  dma_display->clearScreen();
  dma_display->setFont(&FreeSerif9pt7b);
  dma_display->setCursor(4,20);
  dma_display->setTextColor(disp_color(0xFF, 0x00, 0xFF));
  dma_display->print("Locriana");
  dma_display->setCursor(4,40);
  dma_display->setTextColor(disp_color(0x80, 0x40, 0xFF));
  dma_display->print("MIDI Roll");
  vTaskDelay(2000);
}


//a general-purpose hex-dump function for simple serial port debugging
void dump(void* mem, uint16_t len){
  Serial.printf("dump of address %08X, len=%d, contents:\n",(unsigned int)mem, len);
  uint8_t* membyteptr = (uint8_t*)mem;
  for(int i=0; i<len; i++){
    Serial.printf(" %02X",membyteptr[i]);
  }
  Serial.printf("\ndump in text: ");
  for(int i=0; i<len; i++){
    char c = membyteptr[i];
    if(isprint(c)){
      Serial.printf("%c",c);
    }
  }
  Serial.printf("\n");
}



void setup(){
  MidiSerial.begin(31250,SERIAL_8N1,MIDI_RX_PIN,MIDI_TX_PIN,false,10);
  Serial.begin(115200);

  //fill the display array with zeros
  memset(roll_array,0,sizeof(roll_array));

  HUB75_I2S_CFG::i2s_pins _pins={R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
  HUB75_I2S_CFG mxconfig(
    DISP_RES_X,   // module width
    DISP_RES_Y,   // module height
    DISP_CHAIN,    // chain length (how many modules are connected in chain)
    _pins // pin mapping
  );
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(120); //0-255
  dma_display->clearScreen();
  Serial.printf("splash screen...\n");
  show_splash_screen();
  Serial.printf("setup exit\n");
}


void loop(){
  bool sync = vis.process();

  if(sync == true){
    roll_process();
  }

  vTaskDelay(1);
}
