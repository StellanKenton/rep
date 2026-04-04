#ifndef PCA9535_PORT_H
#define PCA9535_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "pca9535.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PCA9535_CONSOLE_SUPPORT
#define PCA9535_CONSOLE_SUPPORT              1
#endif

#ifndef PCA9535_PORT_RESET_ASSERT_MS
#define PCA9535_PORT_RESET_ASSERT_MS         1U
#endif

#ifndef PCA9535_PORT_RESET_RELEASE_MS
#define PCA9535_PORT_RESET_RELEASE_MS        1U
#endif

#define PCA9535_PORT_LED_MAX                 8U

#define PCA9535_PORT_LED_PRESS_RED           0x0001U
#define PCA9535_PORT_LED_NUM_2               0x0002U
#define PCA9535_PORT_LED_NUM_3               0x0004U
#define PCA9535_PORT_LED_NUM_4               0x0008U
#define PCA9535_PORT_LED_NUM_5               0x0010U
#define PCA9535_PORT_LED_NUM_6               0x0020U
#define PCA9535_PORT_LED_NUM_7               0x0040U
#define PCA9535_PORT_LED_NUM_8               0x0080U
#define PCA9535_PORT_LED_NUM_1               0x0100U
#define PCA9535_PORT_LED_POWER_BLUE          0x0800U
#define PCA9535_PORT_LED_POWER_GREEN         0x1000U
#define PCA9535_PORT_LED_POWER_RED           0x2000U
#define PCA9535_PORT_LED_PRESS_BLUE          0x4000U
#define PCA9535_PORT_LED_PRESS_GREEN         0x8000U

#define LED_MAX                              PCA9535_PORT_LED_MAX
#define LED_POWER_RED                        PCA9535_PORT_LED_POWER_RED
#define LED_POWER_GREEN                      PCA9535_PORT_LED_POWER_GREEN
#define LED_POWER_BLUE                       PCA9535_PORT_LED_POWER_BLUE
#define LED_PRESS_RED                        PCA9535_PORT_LED_PRESS_RED
#define LED_PRESS_GREEN                      PCA9535_PORT_LED_PRESS_GREEN
#define LED_PRESS_BLUE                       PCA9535_PORT_LED_PRESS_BLUE

typedef stPca9535IicInterface stPca9535PortIicInterface;

void pca9535PortGetDefCfg(ePca9535MapType device, stPca9535Cfg *cfg);
eDrvStatus pca9535PortAssembleSoftIic(stPca9535Cfg *cfg, uint8_t iic);
bool pca9535PortIsValidCfg(const stPca9535Cfg *cfg);
bool pca9535PortHasValidIicIf(const stPca9535Cfg *cfg);
const stPca9535PortIicInterface *pca9535PortGetIicIf(const stPca9535Cfg *cfg);
eDrvStatus pca9535PortInit(void);
bool pca9535PortIsReady(void);
eDrvStatus pca9535PortLedOff(void);
eDrvStatus pca9535PortLedLightNum(uint8_t num);
eDrvStatus pca9535PortLedPowerShow(bool isRedOn, bool isGreenOn, bool isBlueOn);
eDrvStatus pca9535PortLedPressShow(bool isRedOn, bool isGreenOn, bool isBlueOn);
eDrvStatus pca9535PortSetShowMask(uint16_t mask);
eDrvStatus pca9535PortGetShowMask(uint16_t *mask);
eDrvStatus pca9535PortReadInputPort(uint16_t *value);
void pca9535PortResetInit(void);
void pca9535PortResetWrite(bool assertReset);
void pca9535PortDelayMs(uint32_t delayMs);

#define led_Init()                           pca9535PortInit()
#define led_off()                            pca9535PortLedOff()
#define led_light_num(num)                   pca9535PortLedLightNum(num)
#define led_power_show(r, g, b)              pca9535PortLedPowerShow(((r) != 0), ((g) != 0), ((b) != 0))
#define led_press_show(r, g, b)              pca9535PortLedPressShow(((r) != 0), ((g) != 0), ((b) != 0))

#ifdef __cplusplus
}
#endif

#endif  // PCA9535_PORT_H
