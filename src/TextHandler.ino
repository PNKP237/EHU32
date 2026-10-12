// below is data required to be included in every line - text formatting is based on those
const char DIS_leftadjusted[14]={0x00,0x1B,0x00,0x5B,0x00,0x66,0x00,0x53,0x00,0x5F,0x00,0x67,0x00,0x6D}, DIS_smallfont[14]={0x00,0x1B,0x00,0x5B,0x00,0x66,0x00,0x53,0x00,0x5F,0x00,0x64,0x00,0x6D}, DIS_centered[8]={0x00, 0x1B, 0x00, 0x5B, 0x00, 0x63, 0x00, 0x6D}, DIS_rightadjusted[8]={0x00, 0x1B, 0x00, 0x5B, 0x00, 0x72, 0x00, 0x6D};

static void transliterate_char(uint32_t charint, char *output, size_t *j) {
    switch (charint) {
        // Cyrillic uppercase (0xD0)
        case 0x0404: output[(*j)++] = 'E'; break; // Є
        case 0x0406: output[(*j)++] = 'I'; break; // І
        case 0x0407: output[(*j)++] = 'Y'; output[(*j)++] = 'i'; break; // Ї
        case 0x0410: output[(*j)++] = 'A'; break; // А
        case 0x0411: output[(*j)++] = 'B'; break; // Б
        case 0x0412: output[(*j)++] = 'V'; break; // В
        case 0x0413: output[(*j)++] = 'G'; break; // Г
        case 0x0414: output[(*j)++] = 'D'; break; // Д
        case 0x0415: output[(*j)++] = 'E'; break; // Е
        case 0x0401: output[(*j)++] = 'Y'; output[(*j)++] = 'o'; break; // Ё
        case 0x0416: output[(*j)++] = 'Z'; output[(*j)++] = 'h'; break; // Ж
        case 0x0417: output[(*j)++] = 'Z'; break; // З
        case 0x0418: output[(*j)++] = 'Y'; break; // И
        case 0x0419: output[(*j)++] = 'Y'; break; // Й
        case 0x041A: output[(*j)++] = 'K'; break; // К
        case 0x041B: output[(*j)++] = 'L'; break; // Л
        case 0x041C: output[(*j)++] = 'M'; break; // М
        case 0x041D: output[(*j)++] = 'N'; break; // Н
        case 0x041E: output[(*j)++] = 'O'; break; // О
        case 0x041F: output[(*j)++] = 'P'; break; // П
        case 0x0420: output[(*j)++] = 'R'; break; // Р
        case 0x0421: output[(*j)++] = 'S'; break; // С
        case 0x0422: output[(*j)++] = 'T'; break; // Т
        case 0x0423: output[(*j)++] = 'U'; break; // У
        case 0x0424: output[(*j)++] = 'F'; break; // Ф
        case 0x0425: output[(*j)++] = 'H'; break; // Х
        case 0x0426: output[(*j)++] = 'T'; output[(*j)++] = 's'; break; // Ц
        case 0x0427: output[(*j)++] = 'C'; output[(*j)++] = 'h'; break; // Ч
        case 0x0428: output[(*j)++] = 'S'; output[(*j)++] = 'h'; break; // Ш
        case 0x0429: output[(*j)++] = 'S'; output[(*j)++] = 'h'; output[(*j)++] = 'c'; output[(*j)++] = 'h'; break; // Щ
        case 0x042B: output[(*j)++] = 'Y'; break; // Ы
        case 0x042D: output[(*j)++] = 'E'; break; // Э
        case 0x042E: output[(*j)++] = 'Y'; output[(*j)++] = 'u'; break; // Ю
        case 0x042F: output[(*j)++] = 'Y'; output[(*j)++] = 'a'; break; // Я
        // Cyrillic lowercase (0xD0, 0xD1)
        case 0x0430: output[(*j)++] = 'a'; break; // а
        case 0x0431: output[(*j)++] = 'b'; break; // б
        case 0x0432: output[(*j)++] = 'v'; break; // в
        case 0x0433: output[(*j)++] = 'g'; break; // г
        case 0x0434: output[(*j)++] = 'd'; break; // д
        case 0x0435: output[(*j)++] = 'e'; break; // е
        case 0x0436: output[(*j)++] = 'z'; output[(*j)++] = 'h'; break; // ж
        case 0x0437: output[(*j)++] = 'z'; break; // з
        case 0x0438: output[(*j)++] = 'y'; break; // и
        case 0x0439: output[(*j)++] = 'y'; break; // й
        case 0x043A: output[(*j)++] = 'k'; break; // к
        case 0x043B: output[(*j)++] = 'l'; break; // л
        case 0x043C: output[(*j)++] = 'm'; break; // м
        case 0x043D: output[(*j)++] = 'n'; break; // н
        case 0x043E: output[(*j)++] = 'o'; break; // о
        case 0x043F: output[(*j)++] = 'p'; break; // п
        case 0x0440: output[(*j)++] = 'r'; break; // р
        case 0x0441: output[(*j)++] = 's'; break; // с
        case 0x0442: output[(*j)++] = 't'; break; // т
        case 0x0443: output[(*j)++] = 'u'; break; // у
        case 0x0444: output[(*j)++] = 'f'; break; // ф
        case 0x0445: output[(*j)++] = 'h'; break; // х
        case 0x0446: output[(*j)++] = 't'; output[(*j)++] = 's'; break; // ц
        case 0x0447: output[(*j)++] = 'c'; output[(*j)++] = 'h'; break; // ч
        case 0x0448: output[(*j)++] = 's'; output[(*j)++] = 'h'; break; // ш
        case 0x0449: output[(*j)++] = 's'; output[(*j)++] = 'h'; output[(*j)++] = 'c'; output[(*j)++] = 'h'; break; // щ
        case 0x044B: output[(*j)++] = 'y'; break; // ы
        case 0x044D: output[(*j)++] = 'e'; break; // э
        case 0x044E: output[(*j)++] = 'y'; output[(*j)++] = 'u'; break; // ю
        case 0x044F: output[(*j)++] = 'y'; output[(*j)++] = 'a'; break; // я
        case 0x0454: output[(*j)++] = 'y'; output[(*j)++] = 'e'; break; // є
        case 0x0456: output[(*j)++] = 'i'; break; // і
        case 0x0457: output[(*j)++] = 'y'; output[(*j)++] = 'i'; break; // ї
        // Ukrainian-specific (0xD2)
        case 0x0490: output[(*j)++] = 'G'; break; // Ґ
        case 0x0491: output[(*j)++] = 'g'; break; // ґ
    }
}

// converts an UTF-8 buffer to UTF-16, filters out unsupported chars, returns the amount of chars processed
unsigned int utf8_to_utf16(const char* utf8_buffer, char* utf16_buffer){
  unsigned int utf16_bytecount=0;
  char temp_buffer[8]; // Temporary buffer for transliterated ASCII characters

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
    } else if (charint >= 0x0400 && charint <= 0x04FF) {
        // Unsupported Cyrillic character, transliterate to ASCII
        size_t j = 0;
        transliterate_char(charint, temp_buffer, &j);
        temp_buffer[j] = '\0';

        // Convert transliterated ASCII to UTF-16
        for (size_t k = 0; k < j; k++) {
            utf16_buffer[utf16_bytecount++] = 0x00;
            utf16_buffer[utf16_bytecount++] = temp_buffer[k];
        }
    } // Else, skip unsupported characters
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
