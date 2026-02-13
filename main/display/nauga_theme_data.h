#ifndef NAUGA_THEME_DATA_H
#define NAUGA_THEME_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

namespace NaugaTheme {
constexpr int IRIS_MIN = 180;
constexpr int IRIS_MAX = 280;
constexpr int SCLERA_WIDTH = 375;
constexpr int SCLERA_HEIGHT = 375;
extern const uint16_t* const nauga_sclera;
}

#ifdef __cplusplus
}
#endif

#endif
