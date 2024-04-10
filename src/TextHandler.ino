// below is data required to be included in every line - text formatting is based on those
const char DIS_leftadjusted[14]={0x00,0x1B,0x00,0x5B,0x00,0x66,0x00,0x53,0x00,0x5F,0x00,0x67,0x00,0x6D}, DIS_smallfont[14]={0x00,0x1B,0x00,0x5B,0x00,0x66,0x00,0x53,0x00,0x5F,0x00,0x64,0x00,0x6D};

// clears chosen buffer
void clear_buffer(char* buf_to_clear){
  for(int i=0;i<sizeof(buf_to_clear);i++){
    buf_to_clear[i]=0;
  }
}

// process an UTF-8 buffer, returns the amount of chars processed
unsigned int utf8_to_utf16(const char* utf8_buffer, char* utf16_buffer){
  unsigned int utf16_bytecount=0;
  while (*utf8_buffer!='\0'){
    uint32_t charint=0;
    if ((*utf8_buffer&0x80)==0x00){
      charint=*utf8_buffer&0x7F;
      utf8_buffer++;
    }
    else if ((*utf8_buffer&0xE0)==0xC0){
      charint=(*utf8_buffer & 0x1F)<<6;
      charint|=(*(utf8_buffer+1)&0x3F);
      utf8_buffer+=2;
    }
    else if ((*utf8_buffer&0xF0)==0xE0){
      charint=(*utf8_buffer&0x0F)<<12;
      charint|=(*(utf8_buffer+1)&0x3F)<<6;
      charint|=(*(utf8_buffer+2)&0x3F);
      utf8_buffer += 3;
    }
    else if ((*utf8_buffer&0xF8)==0xF0){
      charint=(*utf8_buffer&0x07)<<18;
      charint|=(*(utf8_buffer+1)&0x3F)<<12;
      charint|=(*(utf8_buffer+2)&0x3F)<<6;
      charint|=(*(utf8_buffer+3)&0x3F);
      utf8_buffer+=4;
    }
    else {
      return utf16_bytecount/2;
    }
    // only process supported chars, latin and extended latin works, cyrillic does not
    if ((charint>=0x0000&&charint<=0x024F) || (charint>=0x1E00 && charint<=0x2C6F)){
      if (charint>=0x10000) {
        charint-=0x10000;
        utf16_buffer[utf16_bytecount++]=static_cast<char>((charint>>10)+0xD8);
        utf16_buffer[utf16_bytecount++]=static_cast<char>((charint>>2)&0xFF);
        utf16_buffer[utf16_bytecount++]=static_cast<char>(0xDC|((charint>>10)&0x03));
        utf16_buffer[utf16_bytecount++]=static_cast<char>((charint&0x03)<<6);
      }
      else {
        utf16_buffer[utf16_bytecount++]=static_cast<char>((charint>>8)&0xFF);
        utf16_buffer[utf16_bytecount++]=static_cast<char>(charint&0xFF);
      }
    }
  }
  return utf16_bytecount/2;  // amount of chars processed
}

// converts UTF-8 strings from arguments to real UTF-16, then compiles a full display message with formatting; returns total bytes written as part of message payload
int utf8_conversion(char* upper_line_buffer, char* middle_line_buffer, char* lower_line_buffer){
  int upper_line_buffer_length=0, middle_line_buffer_length=0, lower_line_buffer_length=0;
  if(upper_line_buffer!=nullptr){                                           // calculating string lengths to keep track of processed data
    upper_line_buffer_length=utf8_to_utf16(upper_line_buffer, utf16_album);
  }
  if(middle_line_buffer!=nullptr){
    middle_line_buffer_length=utf8_to_utf16(middle_line_buffer, utf16_title);
  }
  if(lower_line_buffer!=nullptr){
    lower_line_buffer_length=utf8_to_utf16(lower_line_buffer, utf16_artist);
  }

  if(DEBUGGING_ON){               // debug stuff
    Serial.printf("\nTitle length: %d", middle_line_buffer_length);
    Serial.printf("\nAlbum length: %d", upper_line_buffer_length);
    Serial.printf("\nArtist length: %d", lower_line_buffer_length);
    Serial.println("\nTitle buffer in UTF-8:");
    for(int i=0;i<middle_line_buffer_length;i++){
      Serial.printf(" %02X", middle_line_buffer[i]);
    }
    Serial.println("\nTitle buffer in UTF-16:");
    for(int i=0;i<(middle_line_buffer_length*2);i++){
      Serial.printf(" %02X", utf16_title[i]);
    }
    Serial.println("\nAlbum buffer in UTF-8:");
    for(int i=0;i<upper_line_buffer_length;i++){
      Serial.printf(" %02X", upper_line_buffer[i]);
    }
    Serial.println("\nAlbum buffer in UTF-16:");
    for(int i=0;i<(upper_line_buffer_length*2);i++){
      Serial.printf(" %02X", utf16_album[i]);
    }
    Serial.println("\nArtist buffer in UTF-8:");
    for(int i=0;i<lower_line_buffer_length;i++){
      Serial.printf(" %02X", lower_line_buffer[i]);
    }
    Serial.println("\nArtist buffer in UTF-16:");
    for(int i=0;i<(lower_line_buffer_length*2);i++){
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
  if(middle_line_buffer_length>1){  // if the upper line data is just a space, don't apply formatting - saves 2 frames of data
    for(int i=1;i<=sizeof(DIS_leftadjusted);i++){                               // write left-justified formatting string
      utf16buffer[last_byte_written+i]=DIS_leftadjusted[i-1];
    }
    last_byte_written+=sizeof(DIS_leftadjusted);
    utf16buffer[6]=sizeof(DIS_leftadjusted)/2;
  }
  for(int i=1;i<=(middle_line_buffer_length*2);i++){
    utf16buffer[last_byte_written+i]=utf16_title[i-1];
  }
  last_byte_written+=(middle_line_buffer_length*2);
  utf16buffer[6]+=middle_line_buffer_length;  // this is static, char count = title+(formatting/2)

  int album_count_pos=10;
  // ALBUM FIELD
  last_byte_written++;
  utf16buffer[last_byte_written]=0x11;
  last_byte_written++;
  album_count_pos=last_byte_written;
  if(upper_line_buffer_length>=1){  // if the upper line data is just a space, don't apply formatting - saves 2 frames of data
    for(int i=1;i<=sizeof(DIS_smallfont);i++){                               // formatting - small text
      utf16buffer[last_byte_written+i]=DIS_smallfont[i-1];
    }
    last_byte_written+=sizeof(DIS_smallfont);
    utf16buffer[album_count_pos]=sizeof(DIS_smallfont)/2;
  }
  for(int i=1;i<=(upper_line_buffer_length*2);i++){
    utf16buffer[last_byte_written+i]=utf16_album[i-1];
  }
  last_byte_written+=(upper_line_buffer_length*2);
  utf16buffer[album_count_pos]+=upper_line_buffer_length;

  int artist_count_pos=album_count_pos;
    // ARTIST FIELD
  last_byte_written++;
  utf16buffer[last_byte_written]=0x12;
  last_byte_written++;
  artist_count_pos=last_byte_written;
  if(lower_line_buffer_length>=1){  // if the upper line data is just a space, don't apply formatting - saves 2 frames of data
    for(int i=1;i<=sizeof(DIS_smallfont);i++){                               // formatting - small text
      utf16buffer[last_byte_written+i]=DIS_smallfont[i-1];
    }
    last_byte_written+=sizeof(DIS_smallfont);
    utf16buffer[artist_count_pos]=sizeof(DIS_smallfont)/2;
  }
  for(int i=1;i<=(lower_line_buffer_length*2);i++){
    utf16buffer[last_byte_written+i]=utf16_artist[i-1];
  }
  last_byte_written+=(lower_line_buffer_length*2);
  utf16buffer[artist_count_pos]+=lower_line_buffer_length;

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

