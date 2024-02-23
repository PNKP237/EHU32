// below is data required to be included in every line - text formatting is based on those
const char DIS_leftadjusted[14]={0x00,0x1B,0x00,0x5B,0x00,0x66,0x00,0x53,0x00,0x5F,0x00,0x67,0x00,0x6D}, DIS_smallfont[14]={0x00,0x1B,0x00,0x5B,0x00,0x66,0x00,0x53,0x00,0x5F,0x00,0x64,0x00,0x6D};

// clears chosen buffer
void clear_buffer(char* buf_to_clear){
  for(int i=0;i<sizeof(buf_to_clear);i++){
    buf_to_clear[i]=0;
  }
}

// converts EASCII data from arguments to fake UTF-16, then compiles a full display message with formatting; returns total bytes written as part of message payload
int utf8_conversion(char* upper_line_buffer, char* middle_line_buffer, char* lower_line_buffer){            // go through title, artist, album     faking UTF-16 since 16-02-24
  int upper_line_buffer_lenght=0, middle_line_buffer_lenght=0, lower_line_buffer_lenght=0;
  if(upper_line_buffer!=nullptr){                                           // calculating string lenghts to keep track of processed data
    upper_line_buffer_lenght=snprintf(nullptr, 0, upper_line_buffer);
  }
  if(middle_line_buffer!=nullptr){
    middle_line_buffer_lenght=snprintf(nullptr, 0, middle_line_buffer);
  }
  if(lower_line_buffer!=nullptr){
    lower_line_buffer_lenght=snprintf(nullptr, 0, lower_line_buffer);
  }

  for(int i=0; i<middle_line_buffer_lenght && middle_line_buffer!=nullptr; i++){          // janky conversion to UTF-16
    utf16_title[i*2]=0x0;
    utf16_title[(i*2)+1]=middle_line_buffer[i];
  }
  for(int i=0; i<upper_line_buffer_lenght && upper_line_buffer!=nullptr; i++){
    utf16_album[i*2]=0x0;
    utf16_album[(i*2)+1]=upper_line_buffer[i];
  }
  for(int i=0; i<lower_line_buffer_lenght && lower_line_buffer!=nullptr; i++){
    utf16_artist[i*2]=0x0;
    utf16_artist[(i*2)+1]=lower_line_buffer[i];
  }

  if(DEBUGGING_ON){               // debug stuff
    Serial.printf("\nTitle lenght: %d", middle_line_buffer_lenght);
    Serial.printf("\nAlbum lenght: %d", upper_line_buffer_lenght);
    Serial.printf("\nArtist lenght: %d", lower_line_buffer_lenght);
    Serial.println("\nTitle buffer in UTF-8:");
    for(int i=0;i<middle_line_buffer_lenght;i++){
      Serial.printf(" %02X", middle_line_buffer[i]);
    }
    Serial.println("\nTitle buffer in UTF-16:");
    for(int i=0;i<(middle_line_buffer_lenght*2);i++){
      Serial.printf(" %02X", utf16_title[i]);
    }
    Serial.println("\nAlbum buffer in UTF-8:");
    for(int i=0;i<upper_line_buffer_lenght;i++){
      Serial.printf(" %02X", upper_line_buffer[i]);
    }
    Serial.println("\nAlbum buffer in UTF-16:");
    for(int i=0;i<(upper_line_buffer_lenght*2);i++){
      Serial.printf(" %02X", utf16_album[i]);
    }
    Serial.println("\nArtist buffer in UTF-8:");
    for(int i=0;i<lower_line_buffer_lenght;i++){
      Serial.printf(" %02X", lower_line_buffer[i]);
    }
    Serial.println("\nArtist buffer in UTF-16:");
    for(int i=0;i<(lower_line_buffer_lenght*2);i++){
      Serial.printf(" %02X", utf16_artist[i]);
    }
  }

  for(int i=0;i<sizeof(utf16buffer);i++){  // utf16buffer HAS to be cleared! otherwise stuff breaks due to locations of certain data not being static
    utf16buffer[i]=0;
  }

  //utf16buffer[0]= message size in bytes
  utf16buffer[1]=0x40; // COMMAND
  //utf16buffer[2]=0x00 always
  //utf16buffer[3]= message size -3
  utf16buffer[4]=0x03; //type

  int last_byte_written=4;      // this tracks the current position in buffer
  // SONG TITLE FIELD
  last_byte_written++;
  utf16buffer[last_byte_written]=0x10;
  last_byte_written++;                                                        // we skip utf16buffer[6], its filled in the end (char count for id 0x10)
  if(middle_line_buffer_lenght>1){  // if the upper line data is just a space, don't apply formatting - saves 2 frames of data
    for(int i=1;i<=sizeof(DIS_leftadjusted);i++){                               // write left-justified formatting string
      utf16buffer[last_byte_written+i]=DIS_leftadjusted[i-1];
    }
    last_byte_written+=sizeof(DIS_leftadjusted);
    utf16buffer[6]=sizeof(DIS_leftadjusted)/2;
  }
  for(int i=1;i<=(middle_line_buffer_lenght*2);i++){
    utf16buffer[last_byte_written+i]=utf16_title[i-1];
  }
  last_byte_written+=(middle_line_buffer_lenght*2);
  utf16buffer[6]+=middle_line_buffer_lenght;  // this is static, char count = title+(formatting/2)

  int album_count_pos=10;
  // ALBUM FIELD
  last_byte_written++;
  utf16buffer[last_byte_written]=0x11;
  last_byte_written++;
  album_count_pos=last_byte_written;
  if(upper_line_buffer_lenght>=1){  // if the upper line data is just a space, don't apply formatting - saves 2 frames of data
    for(int i=1;i<=sizeof(DIS_smallfont);i++){                               // formatting - small text
      utf16buffer[last_byte_written+i]=DIS_smallfont[i-1];
    }
    last_byte_written+=sizeof(DIS_smallfont);
    utf16buffer[album_count_pos]=sizeof(DIS_smallfont)/2;
  }
  for(int i=1;i<=(upper_line_buffer_lenght*2);i++){
    utf16buffer[last_byte_written+i]=utf16_album[i-1];
  }
  last_byte_written+=(upper_line_buffer_lenght*2);
  utf16buffer[album_count_pos]+=upper_line_buffer_lenght;

  int artist_count_pos=album_count_pos;
    // ARTIST FIELD
  last_byte_written++;
  utf16buffer[last_byte_written]=0x12;
  last_byte_written++;
  artist_count_pos=last_byte_written;
  if(lower_line_buffer_lenght>=1){  // if the upper line data is just a space, don't apply formatting - saves 2 frames of data
    for(int i=1;i<=sizeof(DIS_smallfont);i++){                               // formatting - small text
      utf16buffer[last_byte_written+i]=DIS_smallfont[i-1];
    }
    last_byte_written+=sizeof(DIS_smallfont);
    utf16buffer[artist_count_pos]=sizeof(DIS_smallfont)/2;
  }
  for(int i=1;i<=(lower_line_buffer_lenght*2);i++){
    utf16buffer[last_byte_written+i]=utf16_artist[i-1];
  }
  last_byte_written+=(lower_line_buffer_lenght*2);
  utf16buffer[artist_count_pos]+=lower_line_buffer_lenght;

  if((last_byte_written+1)%7==0){                   // if the amount of bytes were to result in a full packet (ie no unused bytes), add a char to overflow into the next packet
    utf16buffer[artist_count_pos]+=1;          // workaround because if the packets are full the display would ignore the message
    utf16buffer[last_byte_written+1]=0x00; utf16buffer[last_byte_written+2]=0x20;
    last_byte_written+=2;
  }
  if(last_byte_written>254){                      // message size can't be larger than 255 bytes, as the character specifying total payload is an 8 bit value
    last_byte_written=254;                        // we can send that data though, it will just be ignored, no damage is done
  }
  utf16buffer[0]=(last_byte_written+1);         // TOTAL PAYLOAD SIZE based on how many bytes have been written
  utf16buffer[3]=utf16buffer[0]-3;
  return last_byte_written+1;                   // return the total message size
}