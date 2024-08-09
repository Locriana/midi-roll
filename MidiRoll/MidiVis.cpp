#include "MidiVis.h"

#define MIDI_STATUS_TIMING_CLOCK    0xF8
#define MIDI_STATUS_START           0xFA
#define MIDI_STATUS_CONTINUE        0xFB
#define MIDI_STATUS_STOP            0xFC
#define MIDI_STATUS_ACTIVE_SENSING  0xFE
#define MIDI_STATUS_SYSTEM_RESET    0xFF

#define MIDI_MSG_NOTE_OFF        0x80
#define MIDI_MSG_NOTE_ON         0x90
#define MIDI_MSG_POLY_AFTERTOUCH 0xA0
#define MIDI_MSG_CONTROL_CHANGE  0xB0
#define MIDI_MSG_PROGRAM_CHANGE  0xC0
#define MIDI_MSG_CH_AFTERTOUCH   0xD0
#define MIDI_MSG_PITCH_WHEEL     0xE0


MidiVis::~MidiVis(){
  return;
}

MidiVis::MidiVis(){
  return;
}

MidiVis::MidiVis(Stream *p_ser, T_NoteValue p_sync_every){
  this->serial_port = p_ser;
  this->sync_every = p_sync_every;
  return;
}


int MidiVis::decode_message(uint8_t rxb){
  static int state;
  int res = ATT_CODE_NONE;
  if(rxb >= 0xF0){
    //optionally: add support for system messages
    return 0;
  } //rxb >= 0xF0
  else{
    if(rxb & 0x80){
      this->midi_rx_info.ch = (rxb & 0x0F) + 1;
      this->midi_rx_info.status = rxb & 0xF0;
      this->midi_rx_info.data1 = 0;
      this->midi_rx_info.data2 = 0;
      switch(this->midi_rx_info.status){
        case MIDI_MSG_NOTE_OFF:
          this->midi_rx_info.onflag = 0;
          break;
        case MIDI_MSG_NOTE_ON:
          //may be temporary (if velocity @ the 2nd byte will be zero it will be cleared)
          this->midi_rx_info.onflag = 1;
          break;
      }
      state=1;
      res=ATT_CODE_NONE;
      return res;
    }
  } //else to rxb >= 0xF0

  //deal with the data bytes
  switch(state){
    default:
    case 0: //nothing happens
      res = 0;
      break;
    case 1: //data1 byte
      //if another byte with MSB set
      this->midi_rx_info.data1 = rxb;
      state = 2;  //advance fsm to state 2
      res = ATT_CODE_NONE;
      break;
    case 2: //velocity byte
      this->midi_rx_info.data2 = rxb;

      if(this->midi_rx_info.data2 == 0){  //turn off the note if the velocity bytes is zero
        this->midi_rx_info.onflag = 0;
      }
      if(this->midi_rx_info.status == MIDI_MSG_NOTE_OFF){
        this->midi_rx_info.onflag = 0;
      }

      uint8_t ch_idx = this->midi_rx_info.ch - 1;
      if(ch_idx>=MIDIVIS_NUMBER_OF_CHANNELS) {Serial.printf("channel index too large: %d\n", ch_idx); return -1;}
      uint8_t pitch_idx = this->midi_rx_info.data1;
      if(pitch_idx>=MIDIVIS_NUMBER_OF_NOTES) {Serial.printf("too many notes, i.e. pitch index out of range: %d\n", pitch_idx); return -1;}

      if(this->midi_rx_info.onflag){
        activeNotes[ch_idx][pitch_idx] = this->note_default_velocity;
        accNotes[ch_idx][pitch_idx] = this->note_default_velocity;
      }
      else{
        activeNotes[ch_idx][pitch_idx] = 0;
      }

      res = ATT_CODE_NOTE;  //notify that an additional action may be taken
      state = 1;  //go back to the state in which the data1 is rxed (if dealing with the running status case)
      break;
  }//switch(state)
  
  return res;
}//decode_message


//for future enhancement (maybe...)
void MidiVis::resetAccNotes(void){
  for(uint8_t ch=0;ch<16;ch++){
    for(uint8_t note=0;note<MIDIVIS_NUMBER_OF_NOTES;note++){
      if(activeNotes[ch][note]==0){
        accNotes[ch][note]=0;
      }
    }
  }
}


bool MidiVis::process(void){
  bool res = false;
  static uint8_t sixteenth_cntr;
  static uint8_t sync_div;
  static uint8_t rxb;

  while(this->serial_port->available()>0){
    rxb = this->serial_port->read();
    if(rxb == MIDI_STATUS_TIMING_CLOCK){
      if(sixteenth_cntr < 3){
        sixteenth_cntr++;
      }
      else{
        sixteenth_cntr = 0;
        if(sync_div < ( 16 / sync_every - 1 )){
          sync_div++;
        }
        else{
          sync_div = 0;
          res = true;
        }
      }
      //break;
    }//if(rxb == MIDI_STATUS_TIMING_CLOCK)
    else
    {
      int attention = decode_message(rxb);
      if(attention){
      }
    }
  }//available()>0

  return res;
}
