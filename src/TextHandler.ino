// below is data required to be included in every line - text formatting is based on those
const char DIS_leftadjusted[14]={0x00,0x1B,0x00,0x5B,0x00,0x66,0x00,0x53,0x00,0x5F,0x00,0x67,0x00,0x6D}, DIS_smallfont[14]={0x00,0x1B,0x00,0x5B,0x00,0x66,0x00,0x53,0x00,0x5F,0x00,0x64,0x00,0x6D}, DIS_centered[8]={0x00, 0x1B, 0x00, 0x5B, 0x00, 0x63, 0x00, 0x6D}, DIS_rightadjusted[8]={0x00, 0x1B, 0x00, 0x5B, 0x00, 0x72, 0x00, 0x6D};

// converts an UTF-8 buffer to UTF-16, filters out unsupported chars, returns the amount of chars processed
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
int processDisplayMessage(char* upper_line_buffer, char* middle_line_buffer, char* lower_line_buffer){
  static char utf16_middle_line[256], utf16_lower_line[256], utf16_upper_line[256];
  int upper_line_buffer_length=0, middle_line_buffer_length=0, lower_line_buffer_length=0;
  if(upper_line_buffer!=nullptr){                                           // converting UTF-8 strings to UTF-16 and calculating string lengths to keep track of processed data
    upper_line_buffer_length=utf8_to_utf16(upper_line_buffer, utf16_upper_line);
  }
  if(middle_line_buffer!=nullptr){
    middle_line_buffer_length=utf8_to_utf16(middle_line_buffer, utf16_middle_line);
    if(middle_line_buffer_length==0 || (middle_line_buffer_length==1 && utf16_middle_line[1]==0x20)){ // -> empty line (or unsupported chars)
      snprintf(middle_line_buffer, 8, "Playing");    // if the middle line was to be blank you can at least tell that there's audio being played
      middle_line_buffer_length=utf8_to_utf16(middle_line_buffer, utf16_middle_line);   // do this once again for the new string
    }
  }
  if(lower_line_buffer!=nullptr){
    lower_line_buffer_length=utf8_to_utf16(lower_line_buffer, utf16_lower_line);
  }

  #ifdef DEBUG_STRINGS               // debug stuff
    Serial.printf("\nTitle length: %d", middle_line_buffer_length);
    Serial.printf("\nAlbum length: %d", upper_line_buffer_length);
    Serial.printf("\nArtist length: %d", lower_line_buffer_length);
    Serial.println("\nTitle buffer in UTF-8:");
    for(int i=0;i<middle_line_buffer_length;i++){
      Serial.printf(" %02X", middle_line_buffer[i]);
    }
    Serial.println("\nTitle buffer in UTF-16:");
    for(int i=0;i<(middle_line_buffer_length*2);i++){
      Serial.printf(" %02X", utf16_middle_line[i]);
    }
    Serial.println("\nAlbum buffer in UTF-8:");
    for(int i=0;i<upper_line_buffer_length;i++){
      Serial.printf(" %02X", upper_line_buffer[i]);
    }
    Serial.println("\nAlbum buffer in UTF-16:");
    for(int i=0;i<(upper_line_buffer_length*2);i++){
      Serial.printf(" %02X", utf16_upper_line[i]);
    }
    Serial.println("\nArtist buffer in UTF-8:");
    for(int i=0;i<lower_line_buffer_length;i++){
      Serial.printf(" %02X", lower_line_buffer[i]);
    }
    Serial.println("\nArtist buffer in UTF-16:");
    for(int i=0;i<(lower_line_buffer_length*2);i++){
      Serial.printf(" %02X", utf16_lower_line[i]);
    }
  #endif

  memset(DisplayMsg, 0, sizeof(DisplayMsg));  // clearing the buffer

  //DisplayMsg[0]= message size in bytes
  DisplayMsg[1]=0x40; // COMMAND
  //DisplayMsg[2]=0x00 always
  //DisplayMsg[3]= message size -3
  DisplayMsg[4]=0x03; //type

  int last_byte_written=4;      // this tracks the current position in buffer, 4 because of the first four bytes which go as follows: [size] 40 00 [size-3]
  // SONG TITLE FIELD
  last_byte_written++;
  DisplayMsg[last_byte_written]=0x10;                 // specifying "title" field (middle line, or the only line of text in case of displays such as 1-line GID/BID/TID)
  last_byte_written++;                                                        // we skip DisplayMsg[6], its filled in the end (char count for id 0x10)
  if(middle_line_buffer_length>1){  // if the middle line data is just a space, don't apply formatting - saves 2 frames of data
    memcpy(DisplayMsg+last_byte_written+1, DIS_leftadjusted, sizeof(DIS_leftadjusted));
    last_byte_written+=sizeof(DIS_leftadjusted);
    DisplayMsg[6]=sizeof(DIS_leftadjusted)/2;
  }
  memcpy(DisplayMsg+last_byte_written+1, utf16_middle_line, middle_line_buffer_length*2);
  last_byte_written+=(middle_line_buffer_length*2);

  DisplayMsg[6]+=middle_line_buffer_length;  // this is static, char count = title+(formatting/2)

  int album_count_pos=10;
  // ALBUM FIELD
  last_byte_written++;
  DisplayMsg[last_byte_written]=0x11;             // specifying "album" field (upper line)
  last_byte_written++;
  album_count_pos=last_byte_written;
  if(upper_line_buffer_length>=1){  // if the upper line data is just a space, don't apply formatting - saves 2 frames of data
    memcpy(DisplayMsg+last_byte_written+1, DIS_smallfont, sizeof(DIS_smallfont));
    last_byte_written+=sizeof(DIS_smallfont);
    DisplayMsg[album_count_pos]=sizeof(DIS_smallfont)/2;
  }
  memcpy(DisplayMsg+last_byte_written+1, utf16_upper_line, upper_line_buffer_length*2);
  last_byte_written+=(upper_line_buffer_length*2);
  DisplayMsg[album_count_pos]+=upper_line_buffer_length;

  int artist_count_pos=album_count_pos;
    // ARTIST FIELD
  last_byte_written++;
  DisplayMsg[last_byte_written]=0x12;                             // specifying "artist" field (lower line)
  last_byte_written++;
  artist_count_pos=last_byte_written;
  if(lower_line_buffer_length>=1){  // if the lower line data is just a space, don't apply formatting - saves 2 frames of data
    memcpy(DisplayMsg+last_byte_written+1, DIS_smallfont, sizeof(DIS_smallfont));
    last_byte_written+=sizeof(DIS_smallfont);
    DisplayMsg[artist_count_pos]=sizeof(DIS_smallfont)/2;
  }
  memcpy(DisplayMsg+last_byte_written+1, utf16_lower_line, lower_line_buffer_length*2);
  last_byte_written+=(lower_line_buffer_length*2);
  DisplayMsg[artist_count_pos]+=lower_line_buffer_length;

  if((last_byte_written+1)%7==0){                   // if the amount of bytes were to result in a full packet (ie no unused bytes), add a char to overflow into the next packet
    DisplayMsg[artist_count_pos]+=1;          // workaround because if the packets are full the display would ignore the message. This is explained on the EHU32 wiki
    DisplayMsg[last_byte_written+1]=0x00; DisplayMsg[last_byte_written+2]=0x20;
    last_byte_written+=2;
  }
  if(last_byte_written>254){                      // message size can't be larger than 255 bytes, as the character specifying total payload is an 8 bit value
    last_byte_written=254;                        // we can send that data though, it will just be ignored, no damage is done
  }
  DisplayMsg[0]=last_byte_written+1;         // TOTAL PAYLOAD SIZE based on how many bytes have been written
  DisplayMsg[3]=DisplayMsg[0]-3;               // payload size written as part of the 4000 command
  return last_byte_written+1;                   // return the total message size
}