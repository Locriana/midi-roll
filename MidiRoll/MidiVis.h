#ifndef MIDIVIS_H
#define MIDIVIS_H

#include <Arduino.h>

#define MIDIVIS_NUMBER_OF_NOTES   128
#define MIDIVIS_NUMBER_OF_CHANNELS  16
typedef enum {
  NOTE_VALUE_NONE = 0,
  NOTE_VALUE_WHOLE = 1,
  NOTE_VALUE_HALF = 2,
  NOTE_VALUE_QUARTER = 4,
  NOTE_VALUE_EIGHT = 8,
  NOTE_VALUE_SIXTEENTH = 16
}T_NoteValue;

enum {
  ATT_CODE_NONE = 0,
  ATT_CODE_NOTE = 1,
  ATT_CODE_SYNC = 2
};

typedef struct {
  uint8_t status;
  uint8_t ch;
  uint8_t data1;
  uint8_t data2;
  uint8_t onflag; //note on flag (temporary helper)
}T_midi_info;

class MidiVis{
public:
  MidiVis();
  MidiVis(Stream *p_ser, T_NoteValue p_sync_every);
  ~MidiVis();
  uint8_t activeNotes[MIDIVIS_NUMBER_OF_CHANNELS][MIDIVIS_NUMBER_OF_NOTES];
  uint8_t accNotes[MIDIVIS_NUMBER_OF_CHANNELS][MIDIVIS_NUMBER_OF_NOTES];  //accumulated notes, not used yet
  bool process();  //returns sync info
  void resetAccNotes();
private:
  T_NoteValue sync_every=NOTE_VALUE_SIXTEENTH;  //not used in this version
  Stream* serial_port;
  T_midi_info midi_rx_info;
  const uint8_t note_default_velocity = 127;
  int decode_message(uint8_t rxb);
};



#endif
