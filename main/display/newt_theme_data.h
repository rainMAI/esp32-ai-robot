#ifndef NEWT_THEME_DATA_H
#define NEWT_THEME_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

namespace NewtTheme {
constexpr int IRIS_MIN = 180;
constexpr int IRIS_MAX = 280;
constexpr int SCLERA_WIDTH = 375;
constexpr int SCLERA_HEIGHT = 375;
extern const uint16_t* const newt_sclera;
extern const uint16_t* const newt_iris;
}

#ifdef __cplusplus
}
#endif

#endif
