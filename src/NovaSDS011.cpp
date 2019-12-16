// NovaSDS011( sensor of PM2.5 and PM10 )
// ---------------------------------
//
// By R. Orecki
// December 2019
//
// Documentation:
//		- The iNovaFitness NovaSDS(datasheet)
//

#include "NovaSDS011.h"
#include "Commands.h"

//#define PRINT_DEBUG
#define MIN_QUERY_INTERVAL 3000

// --------------------------------------------------------
// Debug output         
// --------------------------------------------------------
void DebugOut(const String &text, bool linebreak = true)
{
#ifdef PRINT_DEBUG
  if (linebreak)
  {
    Serial.println(text);
  }
  else
  {
    Serial.print(text);
  }
#endif
}

NovaSDS011::NovaSDS011(void) {
}

// --------------------------------------------------------
// NovaSDS011:clearSerial
// --------------------------------------------------------
void NovaSDS011::clearSerial()
{
  while (_sdsSerial->available() > 0) {
		_sdsSerial->read();
	}
}

// --------------------------------------------------------
// NovaSDS011:calculateCheckSum
// --------------------------------------------------------
uint8_t NovaSDS011::calculateCommandCheckSum(CommandType cmd){
  uint16_t checksum = 0;
  for (int i = 2; i <= 16; i++)
  {
    checksum += cmd[i];
  }
  checksum &= 0xFF;
  return checksum;
}

// --------------------------------------------------------
// NovaSDS011:calculateCheckSum
// --------------------------------------------------------
uint8_t NovaSDS011::calculateReplyCheckSum(ReplyType reply){
  uint16_t checksum = 0;
  for (int i = 2; i <= 7; i++)
  {
    checksum += reply[i];
  }

  checksum &= 0xFF;
  return checksum;
}

// --------------------------------------------------------
// NovaSDS011:readReply
// --------------------------------------------------------
void  NovaSDS011::readReply(ReplyType &reply)
{
  uint64_t start = millis();
  while(_sdsSerial->available() == 0)
  {
     if(millis() > (start + _waitWriteRead))
     {
       break;
     }

     delay(1);
  }

  uint32_t duration = (millis() - start);
  DebugOut("readReply - Wait for " + String(duration) + "ms");

  for (int i=0; (_sdsSerial->available() > 0) && (i < sizeof(ReplyType)); i++)
  {
    reply[i] = _sdsSerial->read(); 
  }

  clearSerial();
}

// --------------------------------------------------------
// NovaSDS011:begin
// --------------------------------------------------------
void NovaSDS011::begin(uint8_t pin_rx, uint8_t pin_tx, uint16_t wait_write_read) {
  _waitWriteRead = wait_write_read;
  SoftwareSerial *softSerial = new SoftwareSerial(pin_rx, pin_tx);

  // Initialize soft serial bus
  softSerial->begin(9600);
  _sdsSerial = softSerial;
  
  clearSerial();
}

// --------------------------------------------------------
// NovaSDS011:setDataReportingMode
// --------------------------------------------------------
bool NovaSDS011::setDataReportingMode(DataReportingMode mode, uint16_t device_id)
{
  ReplyType reply;

  REPORTTYPECMD[3] = 0x01; //Set reporting mode
  REPORTTYPECMD[4] = uint8_t(mode & 0xFF); 
  REPORTTYPECMD[17] = calculateCommandCheckSum(REPORTTYPECMD);

	for (uint8_t i = 0; i < 19; i++) {
		_sdsSerial->write(REPORTTYPECMD[i]);
	}
	_sdsSerial->flush();

  readReply(reply);

  REPORTTYPEREPLY[3] = REPORTTYPECMD[3]; //Set reporting mode
  REPORTTYPEREPLY[4] = REPORTTYPECMD[4]; //Reporting mode
  if(device_id != 0xFFFF)
  {
    REPORTTYPEREPLY[6] = REPORTTYPECMD[15]; //Device ID byte 1
    REPORTTYPEREPLY[7] = REPORTTYPECMD[16]; //Device ID byte 2
  }
  else
  {      
    REPORTTYPEREPLY[6] = reply[6]; //Device ID byte 1
    REPORTTYPEREPLY[7] = reply[7]; //Device ID byte 2  
  }  
  REPORTTYPEREPLY[8] = calculateReplyCheckSum(reply);

  for (int i=0; i < sizeof(ReplyType); i++)
  {
    if(REPORTTYPEREPLY[i] != reply[i])
    {
      DebugOut("setDataReportingMode - Error on byte " + String(i) + " Recived byte=" +  String(reply[i]) + 
      " Expected byte=" + String(REPORTTYPEREPLY[i]));
      return false;
    }
  }
  return true;
}

// --------------------------------------------------------
// NovaSDS011:getDataReportingMode
// --------------------------------------------------------
DataReportingMode NovaSDS011::getDataReportingMode(uint16_t device_id)
{
  ReplyType reply;

  REPORTTYPECMD[3] = 0x00; //Get reporting mode
  REPORTTYPECMD[15] = device_id & 0xFF;
  REPORTTYPECMD[16] = (device_id>>8) & 0xFF;
  REPORTTYPECMD[17] = calculateCommandCheckSum(REPORTTYPECMD);

	for (uint8_t i = 0; i < 19; i++) {
		_sdsSerial->write(REPORTTYPECMD[i]);
	}
	_sdsSerial->flush();
 
  readReply(reply);

  REPORTTYPEREPLY[3] = REPORTTYPECMD[3]; //Get reporting mode
  REPORTTYPEREPLY[4] = reply[4]; //Reporting mode
  if(device_id != 0xFFFF)
  {
    REPORTTYPEREPLY[6] = REPORTTYPECMD[15]; //Device ID byte 1
    REPORTTYPEREPLY[7] = REPORTTYPECMD[16]; //Device ID byte 2
  }
  else
  {      
    REPORTTYPEREPLY[6] = reply[6]; //Device ID byte 1
    REPORTTYPEREPLY[7] = reply[7]; //Device ID byte 2  
  }  
  REPORTTYPEREPLY[8] = calculateReplyCheckSum(reply);

  for (int i=0; i < sizeof(ReplyType); i++)
  {
    if(REPORTTYPEREPLY[i] != reply[i])
    {
      DebugOut("getDataReportingMode - Error on byte " + String(i) + " Recived byte=" +  String(reply[i]) + 
      " Expected byte=" + String(REPORTTYPEREPLY[i]));
      return DataReportingMode::error;
    }
  }

  if(reply[4] == DataReportingMode::active) 
  {
    return DataReportingMode::active;
  }
  else if(reply[4] == DataReportingMode::query)
  {
    return DataReportingMode::query;
  }
  else
  {    
    return DataReportingMode::error;    
  }  
}

// --------------------------------------------------------
// NovaSDS011:queryData
// --------------------------------------------------------
QuerryError NovaSDS011::queryData(float &PM25, float &PM10, uint16_t device_id)
{
  static uint64_t lastCall = 0;
  static uint16_t lastPM25 = 0;
  static uint16_t lastPM10 = 0;

  ReplyType reply;  
	uint16_t pm25Serial = 0;
  uint16_t pm10Serial = 0;

  if(millis() < (lastCall + MIN_QUERY_INTERVAL))
  {
    return QuerryError::call_to_often;
  }
  lastCall = millis();

  QUERYCMD[15] = device_id & 0xFF;
  QUERYCMD[16] = (device_id>>8) & 0xFF;
  QUERYCMD[17] = calculateCommandCheckSum(QUERYCMD);

	for (uint8_t i = 0; i < 19; i++) {
		_sdsSerial->write(QUERYCMD[i]);
	}
	_sdsSerial->flush();

  readReply(reply);

  QUERYREPLY[2] = reply[2]; //data byte 1 (PM2.5 low byte)
  QUERYREPLY[3] = reply[3]; //data byte 2 (PM2.5 high byte)
  QUERYREPLY[4] = reply[4]; //data byte 3 (PM10 low byte)
  QUERYREPLY[5] = reply[5]; //data byte 4 (PM10 high byte)

  if(device_id != 0xFFFF)
  {
    QUERYREPLY[6] = REPORTTYPECMD[15]; //Device ID byte 1
    QUERYREPLY[7] = REPORTTYPECMD[16]; //Device ID byte 2
  }
  else
  {      
    QUERYREPLY[6] = reply[6]; //Device ID byte 1
    QUERYREPLY[7] = reply[7]; //Device ID byte 2  
  }  
  QUERYREPLY[8] = calculateReplyCheckSum(reply);

  for (int i=0; i < sizeof(ReplyType); i++)
  {
    if(QUERYREPLY[i] != reply[i])
    {
      DebugOut("queryData - Error on byte " + String(i) + " Recived byte=" +  String(reply[i]) + 
      " Expected byte=" + String(REPORTTYPEREPLY[i]));
      return QuerryError::response_error;
    }
  }

  pm25Serial = reply[2]; 
	pm25Serial += (reply[3] << 8); 
	pm10Serial = reply[4]; 
	pm10Serial += (reply[5] << 8); 

  if((lastPM25 == pm25Serial) && (lastPM10 == pm10Serial))
  {
    return QuerryError::no_new_data;
  }

  lastPM25 = pm25Serial; 
  lastPM10 = pm10Serial;

	PM25 = (float)pm25Serial/10.0;
	PM10 = (float)pm10Serial/10.0;

  return QuerryError::no_error;
}










// --------------------------------------------------------
// NovaSDS011:sleep
// --------------------------------------------------------
void NovaSDS011::sleep() {
	for (uint8_t i = 0; i < 19; i++) {
		_sdsSerial->write(SLEEPCMD[i]);
	}
	_sdsSerial->flush();
	while (_sdsSerial->available() > 0) {
		_sdsSerial->read();
	}
}

// --------------------------------------------------------
// NovaSDS011:wakeup
// --------------------------------------------------------
void NovaSDS011::wakeup() {
	_sdsSerial->write(0x01);
	_sdsSerial->flush();
}

// --------------------------------------------------------
// NovaSDS011:setDutyCycle
// --------------------------------------------------------
void NovaSDS011::setDutyCycle(uint8_t duty_cycle) {

	DUTTYCMD[4] = duty_cycle;
	DUTTYCMD[17] = calculateCommandCheckSum(DUTTYCMD);

	_sdsSerial->write(DUTTYCMD, sizeof(DUTTYCMD));
	_sdsSerial->flush();

	while (_sdsSerial->available() > 0) 
	{
		_sdsSerial->read();
	}
}



 /*****************************************************************
  /* read NovaSDS011 sensor values                                     *
  /*****************************************************************/
String NovaSDS011::SDS_version_date()
{ 
  String s = "";
  String value_hex;
  byte buffer;
  int value;
  int len = 0;
  String version_date = "";
  String device_id = "";
  int checksum_is;
  int checksum_ok = 0;
  int position = 0;


  _sdsSerial->write(VERSIONCMD, sizeof(VERSIONCMD));
  _sdsSerial->flush();
  delay(100);

  while (_sdsSerial->available() > 0)
  {
    buffer = _sdsSerial->read();
    // debug_out(String(len) + " - " + String(buffer, HEX) + " - " + int(buffer) + " .", 1);
    //    "aa" = 170, "ab" = 171, "c0" = 192
    value = int(buffer);
    switch (len)
    {
    case (0):
      if (value != 170)
      {
        len = -1;
      };
      break;
    case (1):
      if (value != 197)
      {
        len = -1;
      };
      break;
    case (2):
      if (value != 7)
      {
        len = -1;
      };
      break;
    case (3):
      version_date = String(value);
      checksum_is = 7 + value;
      break;
    case (4):
      version_date += "-" + String(value);
      checksum_is += value;
      break;
    case (5):
      version_date += "-" + String(value);
      checksum_is += value;
      break;
    case (6):
      if (value < 0x10)
      {
        device_id = "0" + String(value, HEX);
      }
      else
      {
        device_id = String(value, HEX);
      };
      checksum_is += value;
      break;
    case (7):
      if (value < 0x10)
      {
        device_id += "0";
      };
      device_id += String(value, HEX);
      checksum_is += value;
      break;
    case (8):
      if (value == (checksum_is % 256))
      {
        checksum_ok = 1;
      }
      else
      {
        len = -1;
      };
      break;
    case (9):
      if (value != 171)
      {
        len = -1;
      };
      break;
    }
    len++;
    if (len == 10 && checksum_ok == 1)
    {
      s = version_date + "(" + device_id + ")";
      len = 0;
      checksum_ok = 0;
      version_date = "";
      device_id = "";
      checksum_is = 0;
    }
    yield();
  }

  return s;
}


/*****************************************************************
  /* start NovaSDS011 sensor                                           *
  /*****************************************************************/
void NovaSDS011::start_SDS()
{
  const uint8_t start_SDS_cmd[] =
      {
          0xAA, 0xB4, 0x06, 0x01, 0x01,
          0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00,
          0xFF, 0xFF, 0x05, 0xAB};
  _sdsSerial->write(start_SDS_cmd, sizeof(start_SDS_cmd));
  _sdsSerial->flush();
  
  _isSDSRunning = true;
  
  while (_sdsSerial->available() > 0) {
    _sdsSerial->read();
  }
}

/*****************************************************************
  /* stop NovaSDS011 sensor                                            *
  /*****************************************************************/
void NovaSDS011::stop_SDS()
{
  const uint8_t stop_SDS_cmd[] =
      {
          0xAA, 0xB4, 0x06, 0x01, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00,
          0xFF, 0xFF, 0x05, 0xAB};
  _sdsSerial->write(stop_SDS_cmd, sizeof(stop_SDS_cmd));
  _isSDSRunning = false;
  _sdsSerial->flush();
  
  while (_sdsSerial->available() > 0) {
    _sdsSerial->read();
  }
}

//set initiative mode
void NovaSDS011::set_initiative_SDS()
{
  //aa, 0xb4, 0x08, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x0a, 0xab
  const uint8_t stop_SDS_cmd[] =
      {
          0xAA, 0xB4, 0x08, 0x01, 0x03,
          0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00,
          0xFF, 0xFF, 0x0A, 0xAB};
  _sdsSerial->write(stop_SDS_cmd, sizeof(stop_SDS_cmd));
  _isSDSRunning = false;
  _sdsSerial->flush();
  
  while (_sdsSerial->available() > 0) {
    _sdsSerial->read();
  }
}

