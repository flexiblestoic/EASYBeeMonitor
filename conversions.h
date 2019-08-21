#define UINT16_t_MAX  65536
#define UINT8_t_MAX  256


uint16_t convertFloatToUInt16(float value, float min, float max) {
  //manage out of bound values
  float convertedFloat = (value - min)*((float)(UINT16_t_MAX)) / (max - min);
  return (uint16_t)(convertedFloat);
}


uint8_t convertFloatToUInt8(float value, float min, float max) {
  float convertedFloat = (value - min)*((float)(UINT8_t_MAX)) / (max - min);
  return (uint8_t)(convertedFloat);
}
