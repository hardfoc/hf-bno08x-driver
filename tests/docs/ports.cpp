/**
 * @file ports.cpp
 * @brief Instantiates the documented I2C and UART-RVC ports with the driver, so both compile in
 * full.
 */
#include "my_i2c.hpp"
#include "my_rvc.hpp"

template class BNO085<MyI2c>;
template class BNO085<MyRvcUart>;
