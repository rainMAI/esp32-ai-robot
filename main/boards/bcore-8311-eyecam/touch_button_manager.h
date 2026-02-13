#ifndef TOUCH_BUTTON_MANAGER_H_
#define TOUCH_BUTTON_MANAGER_H_

#include <functional>
#include "iot_button.h"

class TouchButtonManager {
public:
    TouchButtonManager();
    ~TouchButtonManager();

    void Initialize();
    void SetTouchSensorCallback(std::function<void()> callback);
    void SetTouchSensor2Callback(std::function<void()> callback);

private:
    std::function<void()> touch_sensor_callback_;
    std::function<void()> touch_sensor2_callback_;

    button_handle_t btn_touch1_;
    button_handle_t btn_touch2_;
};

#endif
