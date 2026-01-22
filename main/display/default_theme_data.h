#ifndef DEFAULT_THEME_DATA_H
#define DEFAULT_THEME_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

namespace DefaultTheme {
constexpr int IRIS_MIN = 180;
constexpr int IRIS_MAX = 280;
constexpr int SCLERA_WIDTH = 375;
constexpr int SCLERA_HEIGHT = 375;
extern const uint16_t* const default_sclera;
extern const uint16_t* const default_iris;
}

#ifdef __cplusplus
}
#endif

#endif
