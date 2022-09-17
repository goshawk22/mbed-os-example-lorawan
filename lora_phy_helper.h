#ifndef APP_LORA_PHY_HELPER_H_
#define APP_LORA_PHY_HELPER_H_

#include <stdlib.h>


#include "LoRaPHY.h"
#include "LoRaPHYEU868.h"
LoRaPHYEU868 phy;

/*
#if number == 1
#include "LoRaPHYAS923.h"
LoRaPHYAS923 phy;
#elif number == 2
#include "LoRaPHYAU915.h"
LoRaPHYAU915 phy;
#elif number == 3
#include "LoRaPHYCN470.h"
LoRaPHYCN470 phy;
#elif number == 4
#include "LoRaPHYCN779.h"
LoRaPHYCN779 phy;
#elif number == 5
#include "LoRaPHYEU433.h"
LoRaPHYEU433 phy;
#elif number == 6
#include "LoRaPHYEU868.h"
LoRaPHYEU868 phy;
#elif number == 7
#include "LoRaPHYIN865.h"
LoRaPHYIN865 phy;
#elif number == 8
#include "LoRaPHYKR920.h"
LoRaPHYKR920 phy;
#else
#include "LoRaPHYUS915.h"
LoRaPHYUS915 phy;
#endif /* number */

#endif /* APP_LORA_PHY_HELPER_H_ */