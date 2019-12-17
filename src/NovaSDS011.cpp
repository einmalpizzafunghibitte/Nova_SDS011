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

#define PRINT_DEBUG
#define MIN_QUERY_INTERVAL 3000

// --------------------------------------------------------
// Debug output
// --------------------------------------------------------
void NovaSDS011::DebugOut(const String &text, bool linebreak)
{
  if (_enableDebug)
  {
    if (linebreak)
    {
      Serial.println(text);
    }
    else
    {
      Serial.print(text);
    }
  }
}

NovaSDS011::NovaSDS011(void)
{
}

// --------------------------------------------------------
// NovaSDS011:clearSerial
// --------------------------------------------------------
void NovaSDS011::clearSerial()
{
  while (_sdsSerial->available() > 0)
  {
    _sdsSerial->read();
  }
}

// --------------------------------------------------------
// NovaSDS011:calculateCheckSum
// --------------------------------------------------------
uint8_t NovaSDS011::calculateCommandCheckSum(CommandType cmd)
{
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
uint8_t NovaSDS011::calculateReplyCheckSum(ReplyType reply)
{
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
bool NovaSDS011::readReply(ReplyType &reply)
{
  bool timeout = true;

  uint64_t start = millis();
  while (_sdsSerial->available() == 0)
  {
    if (millis() > (start + _waitWriteRead))
    {
      break;
    }

    delay(1);
  }

  uint32_t duration = (millis() - start);
  DebugOut("readReply - Wait for " + String(duration) + "ms");

  for (int i = 0; (_sdsSerial->available() > 0) && (i < sizeof(ReplyType)); i++)
  {
    reply[i] = _sdsSerial->read();
    if (i == (sizeof(ReplyType) - 1))
    {
      timeout = false;
    }
  }

  clearSerial();
  return timeout;
}

// --------------------------------------------------------
// NovaSDS011:begin
// --------------------------------------------------------
void NovaSDS011::begin(uint8_t pin_rx, uint8_t pin_tx, uint16_t wait_write_read)
{
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

  REPORT_TYPE_CMD[3] = 0x01; //Set reporting mode
  REPORT_TYPE_CMD[4] = uint8_t(mode & 0xFF);
  REPORT_TYPE_CMD[15] = device_id & 0xFF;
  REPORT_TYPE_CMD[16] = (device_id >> 8) & 0xFF;
  REPORT_TYPE_CMD[17] = calculateCommandCheckSum(REPORT_TYPE_CMD);

  for (uint8_t i = 0; i < 19; i++)
  {
    _sdsSerial->write(REPORT_TYPE_CMD[i]);
  }
  _sdsSerial->flush();

  if (readReply(reply))
  {
    DebugOut("setDataReportingMode - Error read reply timeout");
    return false;
  }
  readReply(reply);

  REPORT_TYPE_REPLY[3] = REPORT_TYPE_CMD[3]; //Set reporting mode
  REPORT_TYPE_REPLY[4] = REPORT_TYPE_CMD[4]; //Reporting mode
  if (device_id != 0xFFFF)
  {
    REPORT_TYPE_REPLY[6] = REPORT_TYPE_CMD[15]; //Device ID byte 1
    REPORT_TYPE_REPLY[7] = REPORT_TYPE_CMD[16]; //Device ID byte 2
  }
  else
  {
    REPORT_TYPE_REPLY[6] = reply[6]; //Device ID byte 1
    REPORT_TYPE_REPLY[7] = reply[7]; //Device ID byte 2
  }
  REPORT_TYPE_REPLY[8] = calculateReplyCheckSum(reply);

  for (int i = 0; i < sizeof(ReplyType); i++)
  {
    if (REPORT_TYPE_REPLY[i] != reply[i])
    {
      DebugOut("setDataReportingMode - Error on byte " + String(i) + " Recived byte=" + String(reply[i]) +
               " Expected byte=" + String(REPORT_TYPE_REPLY[i]));
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

  REPORT_TYPE_CMD[3] = 0x00; //Get reporting mode
  REPORT_TYPE_CMD[15] = device_id & 0xFF;
  REPORT_TYPE_CMD[16] = (device_id >> 8) & 0xFF;
  REPORT_TYPE_CMD[17] = calculateCommandCheckSum(REPORT_TYPE_CMD);

  for (uint8_t i = 0; i < 19; i++)
  {
    _sdsSerial->write(REPORT_TYPE_CMD[i]);
  }
  _sdsSerial->flush();

  if (readReply(reply))
  {
    DebugOut("getDataReportingMode - Error read reply timeout");
    return DataReportingMode::report_error;
  }

  REPORT_TYPE_REPLY[3] = REPORT_TYPE_CMD[3]; //Get reporting mode
  REPORT_TYPE_REPLY[4] = reply[4];           //Reporting mode
  if (device_id != 0xFFFF)
  {
    REPORT_TYPE_REPLY[6] = REPORT_TYPE_CMD[15]; //Device ID byte 1
    REPORT_TYPE_REPLY[7] = REPORT_TYPE_CMD[16]; //Device ID byte 2
  }
  else
  {
    REPORT_TYPE_REPLY[6] = reply[6]; //Device ID byte 1
    REPORT_TYPE_REPLY[7] = reply[7]; //Device ID byte 2
  }
  REPORT_TYPE_REPLY[8] = calculateReplyCheckSum(reply);

  for (int i = 0; i < sizeof(ReplyType); i++)
  {
    if (REPORT_TYPE_REPLY[i] != reply[i])
    {
      DebugOut("getDataReportingMode - Error on byte " + String(i) + " Recived byte=" + String(reply[i]) +
               " Expected byte=" + String(REPORT_TYPE_REPLY[i]));
      return DataReportingMode::report_error;
    }
  }

  if (reply[4] == DataReportingMode::active)
  {
    return DataReportingMode::active;
  }
  else if (reply[4] == DataReportingMode::query)
  {
    return DataReportingMode::query;
  }
  else
  {
    return DataReportingMode::report_error;
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

  if (millis() < (lastCall + MIN_QUERY_INTERVAL))
  {
    return QuerryError::call_to_often;
  }
  lastCall = millis();

  QUERY_CMD[15] = device_id & 0xFF;
  QUERY_CMD[16] = (device_id >> 8) & 0xFF;
  QUERY_CMD[17] = calculateCommandCheckSum(QUERY_CMD);

  for (uint8_t i = 0; i < 19; i++)
  {
    _sdsSerial->write(QUERY_CMD[i]);
  }
  _sdsSerial->flush();

  if (readReply(reply))
  {
    DebugOut("queryData - Error read reply timeout");
    return QuerryError::response_error;
  }

  QUERY_REPLY[2] = reply[2]; //data byte 1 (PM2.5 low byte)
  QUERY_REPLY[3] = reply[3]; //data byte 2 (PM2.5 high byte)
  QUERY_REPLY[4] = reply[4]; //data byte 3 (PM10 low byte)
  QUERY_REPLY[5] = reply[5]; //data byte 4 (PM10 high byte)

  if (device_id != 0xFFFF)
  {
    QUERY_REPLY[6] = QUERY_CMD[15]; //Device ID byte 1
    QUERY_REPLY[7] = QUERY_CMD[16]; //Device ID byte 2
  }
  else
  {
    QUERY_REPLY[6] = reply[6]; //Device ID byte 1
    QUERY_REPLY[7] = reply[7]; //Device ID byte 2
  }
  QUERY_REPLY[8] = calculateReplyCheckSum(reply);

  for (int i = 0; i < sizeof(ReplyType); i++)
  {
    if (QUERY_REPLY[i] != reply[i])
    {
      DebugOut("queryData - Error on byte " + String(i) + " Recived byte=" + String(reply[i]) +
               " Expected byte=" + String(REPORT_TYPE_REPLY[i]));
      return QuerryError::response_error;
    }
  }

  pm25Serial = reply[2];
  pm25Serial += (reply[3] << 8);
  pm10Serial = reply[4];
  pm10Serial += (reply[5] << 8);

  if ((lastPM25 == pm25Serial) && (lastPM10 == pm10Serial))
  {
    return QuerryError::no_new_data;
  }

  lastPM25 = pm25Serial;
  lastPM10 = pm10Serial;

  PM25 = (float)pm25Serial / 10.0;
  PM10 = (float)pm10Serial / 10.0;

  return QuerryError::no_error;
}

// --------------------------------------------------------
// NovaSDS011:setDeviceID
// --------------------------------------------------------
bool NovaSDS011::setDeviceID(uint16_t new_device_id, uint16_t device_id)
{
  ReplyType reply;

  SET_ID_CMD[13] = new_device_id & 0xFF;
  SET_ID_CMD[14] = (new_device_id >> 8) & 0xFF;
  SET_ID_CMD[15] = device_id & 0xFF;
  SET_ID_CMD[16] = (device_id >> 8) & 0xFF;
  SET_ID_CMD[17] = calculateCommandCheckSum(SET_ID_CMD);

  for (uint8_t i = 0; i < 19; i++)
  {
    _sdsSerial->write(SET_ID_CMD[i]);
  }
  _sdsSerial->flush();

  if (readReply(reply))
  {
    DebugOut("setDeviceID - Error read reply timeout");
    return false;
  }

  SET_ID_REPLY[6] = SET_ID_CMD[13]; //Device ID byte 1
  SET_ID_REPLY[7] = SET_ID_CMD[14]; //Device ID byte 2

  SET_ID_REPLY[8] = calculateReplyCheckSum(reply);

  for (int i = 0; i < sizeof(ReplyType); i++)
  {
    if (SET_ID_REPLY[i] != reply[i])
    {
      DebugOut("setDeviceID - Error on byte " + String(i) + " Recived byte=" + String(reply[i]) +
               " Expected byte=" + String(REPORT_TYPE_REPLY[i]));
      return false;
    }
  }
  return true;
}

// --------------------------------------------------------
// NovaSDS011:setWorkingMode
// --------------------------------------------------------
bool NovaSDS011::setWorkingMode(WorkingMode mode, uint16_t device_id)
{
  ReplyType reply;
  bool timeout;

  WORKING_MODE_CMD[3] = 0x01; //Set reporting mode
  WORKING_MODE_CMD[4] = uint8_t(mode & 0xFF);
  WORKING_MODE_CMD[15] = device_id & 0xFF;
  WORKING_MODE_CMD[16] = (device_id >> 8) & 0xFF;
  WORKING_MODE_CMD[17] = calculateCommandCheckSum(WORKING_MODE_CMD);

  for (uint8_t i = 0; i < 19; i++)
  {
    _sdsSerial->write(WORKING_MODE_CMD[i]);
  }
  _sdsSerial->flush();

  timeout = readReply(reply);

  if ((mode == WorkingMode::sleep) && (timeout))
  {
    DebugOut("setWorkingMode - Read timeout");
    return true;
  }

  WORKING_MODE_REPLY[3] = WORKING_MODE_CMD[3]; //Set reporting mode
  WORKING_MODE_REPLY[4] = WORKING_MODE_CMD[4]; //Reporting mode
  if (device_id != 0xFFFF)
  {
    WORKING_MODE_REPLY[6] = WORKING_MODE_REPLY[15]; //Device ID byte 1
    WORKING_MODE_REPLY[7] = WORKING_MODE_REPLY[16]; //Device ID byte 2
  }
  else
  {
    WORKING_MODE_REPLY[6] = reply[6]; //Device ID byte 1
    WORKING_MODE_REPLY[7] = reply[7]; //Device ID byte 2
  }
  WORKING_MODE_REPLY[8] = calculateReplyCheckSum(reply);

  for (int i = 0; i < sizeof(ReplyType); i++)
  {
    if (WORKING_MODE_REPLY[i] != reply[i])
    {
      DebugOut("setWorkingMode - Error on byte " + String(i) + " Recived byte=" + String(reply[i]) +
               " Expected byte=" + String(WORKING_MODE_REPLY[i]));
      return false;
    }
  }
  return true;
}

// --------------------------------------------------------
// NovaSDS011:getWorkingMode
// --------------------------------------------------------
WorkingMode NovaSDS011::getWorkingMode(uint16_t device_id)
{
  ReplyType reply;

  WORKING_MODE_CMD[3] = 0x00; //Get reporting mode
  WORKING_MODE_CMD[15] = device_id & 0xFF;
  WORKING_MODE_CMD[16] = (device_id >> 8) & 0xFF;
  WORKING_MODE_CMD[17] = calculateCommandCheckSum(WORKING_MODE_CMD);

  for (uint8_t i = 0; i < 19; i++)
  {
    _sdsSerial->write(WORKING_MODE_CMD[i]);
  }
  _sdsSerial->flush();

  if (readReply(reply))
  {
    DebugOut("getWorkingMode - Error read reply timeout");
    return WorkingMode::working_error;
  }

  WORKING_MODE_REPLY[3] = WORKING_MODE_CMD[3]; //Get reporting mode
  WORKING_MODE_REPLY[4] = reply[4];              //Reporting mode
  if (device_id != 0xFFFF)
  {
    WORKING_MODE_REPLY[6] = WORKING_MODE_CMD[15]; //Device ID byte 1
    WORKING_MODE_REPLY[7] = WORKING_MODE_CMD[16]; //Device ID byte 2
  }
  else
  {
    WORKING_MODE_REPLY[6] = reply[6]; //Device ID byte 1
    WORKING_MODE_REPLY[7] = reply[7]; //Device ID byte 2
  }
  WORKING_MODE_REPLY[8] = calculateReplyCheckSum(reply);

  for (int i = 0; i < sizeof(ReplyType); i++)
  {
    if (WORKING_MODE_REPLY[i] != reply[i])
    {
      DebugOut("getWorkingMode - Error on byte " + String(i) + " Recived byte=" + String(reply[i]) +
               " Expected byte=" + String(WORKING_MODE_REPLY[i]));
      return WorkingMode::working_error;
    }
  }

  if (reply[4] == WorkingMode::sleep)
  {
    return WorkingMode::sleep;
  }
  else if (reply[4] == WorkingMode::work)
  {
    return WorkingMode::work;
  }
  else
  {
    return WorkingMode::working_error;
  }
}

// --------------------------------------------------------
// NovaSDS011:setDutyCycle
// --------------------------------------------------------
void NovaSDS011::setDutyCycle(uint8_t duty_cycle)
{

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

  while (_sdsSerial->available() > 0)
  {
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

  while (_sdsSerial->available() > 0)
  {
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

  while (_sdsSerial->available() > 0)
  {
    _sdsSerial->read();
  }
}
