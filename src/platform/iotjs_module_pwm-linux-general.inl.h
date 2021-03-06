/* Copyright 2016-present Samsung Electronics Co., Ltd. and other contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef IOTJS_MODULE_PWM_LINUX_GENERAL_INL_H
#define IOTJS_MODULE_PWM_LINUX_GENERAL_INL_H


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iotjs_systemio-linux.h"
#include "module/iotjs_module_pwm.h"


// Generic PWM implementation for linux.


#define PWM_INTERFACE "/sys/class/pwm/pwmchip%d/"
#define PWM_PIN_INTERFACE "pwm%d/"
#define PWM_PIN_FORMAT PWM_INTERFACE PWM_PIN_INTERFACE
#define PWM_EXPORT PWM_INTERFACE "export"
#define PWM_UNEXPORT PWM_INTERFACE "unexport"
#define PWM_PIN_DUTYCYCLE "duty_cycle"
#define PWM_PIN_PERIOD "period"
#define PWM_PIN_ENABlE "enable"

#define PWM_DEFAULT_CHIP_NUMBER 0
#define PWM_PATH_BUFFER_SIZE 64
#define PWM_VALUE_BUFFER_SIZE 32


void iotjs_pwm_initialize() {
  DDLOG("PWM initialize");
}

// Generate device path for specified PWM device.
// The path may include node suffix if passed ('enable', 'period', 'duty_cycle')
// Pointer to a allocated string is returned, or null in case of error.
// If PWM_PIN_FORMAT format results in an empty string generated by sprintf,
// NULL is returned (and fileName is ignored).
static char* generateDeviceSubpath(iotjs_pwm_reqdata_t* req_data,
                                   const char* fileName) {
  char* devicePath = NULL;
  // Do not print anything, only calculate resulting string length.
  int prefixSize =
      snprintf(NULL, 0, PWM_PIN_FORMAT, PWM_DEFAULT_CHIP_NUMBER, req_data->pin);
  if (prefixSize > 0) {
    int suffixSize = fileName ? strlen(fileName) : 0;
    devicePath = malloc(prefixSize + suffixSize + 1);
    if (devicePath) {
      // Do not need to check bounds, the buffer is of exact required size.
      sprintf(devicePath, PWM_PIN_FORMAT, PWM_DEFAULT_CHIP_NUMBER,
              req_data->pin);
      memcpy(devicePath + prefixSize, fileName, suffixSize);
      devicePath[prefixSize + suffixSize] = 0;
    }
  }
  return devicePath;
}

// Limit period to [0..1]s
static double adjustPeriod(double period) {
  if (period < 0) {
    return 0.0;
  } else if (period > 1) {
    return 1.0;
  } else {
    return period;
  }
}

static bool doSetPeriod(iotjs_pwm_reqdata_t* req_data) {
  bool result = false;
  if (isfinite(req_data->period) && req_data->period >= 0.0) {
    char* devicePath = generateDeviceSubpath(req_data, PWM_PIN_PERIOD);
    if (devicePath) {
      // Linux API uses nanoseconds, thus 1E9
      unsigned int value = (unsigned)(adjustPeriod(req_data->period) * 1.E9);
      DDLOG("PWM SetPeriod - path: %s, value: %fs", devicePath, 1.E-9 * value);
      char buf[PWM_VALUE_BUFFER_SIZE];
      if (snprintf(buf, sizeof(buf), "%d", value) > 0) {
        result = iotjs_systemio_open_write_close(devicePath, buf);
      }
      free(devicePath);
    }
  }
  return result;
}

// Set PWM period.
bool SetPwmPeriod(iotjs_pwm_reqdata_t* req_data) {
  return doSetPeriod(req_data);
}

// Set PWM Duty-Cycle.
bool SetPwmDutyCycle(iotjs_pwm_reqdata_t* req_data) {
  bool result = false;
  double dutyCycle = req_data->duty_cycle;
  if (isfinite(req_data->period) && req_data->period >= 0.0 &&
      isfinite(dutyCycle) && 0.0 <= dutyCycle && dutyCycle <= 1.0) {
    char* devicePath = generateDeviceSubpath(req_data, PWM_PIN_DUTYCYCLE);
    if (devicePath) {
      double period = adjustPeriod(req_data->period);
      // Linux API uses nanoseconds, thus 1E9
      unsigned dutyCycleValue = (unsigned)(period * req_data->duty_cycle * 1E9);

      DDLOG("PWM SetdutyCycle - path: %s, value: %d\n", devicePath,
            dutyCycleValue);
      char buf[PWM_VALUE_BUFFER_SIZE];
      snprintf(buf, sizeof(buf), "%d", dutyCycleValue);

      result = iotjs_systemio_open_write_close(devicePath, buf);
      free(devicePath);
    }
  }
  return result;
}


#define PWM_WORKER_INIT_TEMPLATE                                            \
  iotjs_pwm_reqwrap_t* req_wrap = iotjs_pwm_reqwrap_from_request(work_req); \
  iotjs_pwm_reqdata_t* req_data = iotjs_pwm_reqwrap_data(req_wrap);


void ExportWorker(uv_work_t* work_req) {
  PWM_WORKER_INIT_TEMPLATE;

  char path[PWM_PATH_BUFFER_SIZE] = { 0 };
  if (snprintf(path, PWM_PATH_BUFFER_SIZE - 1, PWM_PIN_FORMAT,
               PWM_DEFAULT_CHIP_NUMBER, req_data->pin) < 0) {
    req_data->result = kPwmErrSys;
    return;
  }


  // See if the PWM is already opened.
  if (!iotjs_systemio_check_path(path)) {
    // Write exporting PWM path
    char export_path[PWM_PATH_BUFFER_SIZE] = { 0 };
    snprintf(export_path, PWM_PATH_BUFFER_SIZE - 1, PWM_EXPORT,
             PWM_DEFAULT_CHIP_NUMBER);

    const char* created_files[] = { PWM_PIN_DUTYCYCLE, PWM_PIN_PERIOD,
                                    PWM_PIN_ENABlE };
    int created_files_length = sizeof(created_files) / sizeof(created_files[0]);
    if (!iotjs_systemio_device_open(export_path, req_data->pin, path,
                                    created_files, created_files_length)) {
      req_data->result = kPwmErrExport;
      return;
    }
  }

  // Set options.
  if (req_data->period >= 0) {
    if (!doSetPeriod(req_data)) {
      req_data->result = kPwmErrWrite;
      return;
    }
    if (req_data->duty_cycle >= 0) {
      if (!SetPwmDutyCycle(req_data)) {
        req_data->result = kPwmErrWrite;
        return;
      }
    }
  }

  DDDLOG("PWM ExportWorker - path: %s", path);

  req_data->result = kPwmErrOk;
}


void SetPeriodWorker(uv_work_t* work_req) {
  PWM_WORKER_INIT_TEMPLATE;

  if (!doSetPeriod(req_data)) {
    req_data->result = kPwmErrWrite;
    return;
  }

  DDDLOG("PWM SetPeriodWorker");

  req_data->result = kPwmErrOk;
}


void SetFrequencyWorker(uv_work_t* work_req) {
  PWM_WORKER_INIT_TEMPLATE;

  if (!doSetPeriod(req_data)) {
    req_data->result = kPwmErrWrite;
    return;
  }

  DDDLOG("PWM SetPeriodWorker");

  req_data->result = kPwmErrOk;
}


void SetDutyCycleWorker(uv_work_t* work_req) {
  PWM_WORKER_INIT_TEMPLATE;

  if (!SetPwmDutyCycle(req_data)) {
    req_data->result = kPwmErrWrite;
    return;
  }

  DDDLOG("PWM SetDutyCycleWorker");

  req_data->result = kPwmErrOk;
}


void SetEnableWorker(uv_work_t* work_req) {
  PWM_WORKER_INIT_TEMPLATE;

  char path[PWM_PATH_BUFFER_SIZE] = { 0 };
  if (snprintf(path, PWM_PATH_BUFFER_SIZE - 1, PWM_PIN_FORMAT,
               PWM_DEFAULT_CHIP_NUMBER, req_data->pin) < 0) {
    return;
  }

  strcat(path, PWM_PIN_ENABlE);

  char value[4];
  snprintf(value, sizeof(value), "%d", req_data->enable);
  if (!iotjs_systemio_open_write_close(path, value)) {
    req_data->result = kPwmErrWrite;
    return;
  }

  DDDLOG("PWM SetEnableWorker - path: %s", path);

  req_data->result = kPwmErrOk;
}


void UnexportWorker(uv_work_t* work_req) {
  PWM_WORKER_INIT_TEMPLATE;

  char path[PWM_PATH_BUFFER_SIZE] = { 0 };
  if (snprintf(path, PWM_PATH_BUFFER_SIZE - 1, PWM_PIN_FORMAT,
               PWM_DEFAULT_CHIP_NUMBER, req_data->pin) < 0) {
    return;
  }

  int32_t chip_number = PWM_DEFAULT_CHIP_NUMBER;

  if (iotjs_systemio_check_path(path)) {
    // Write exporting pin path
    char unexport_path[PWM_PATH_BUFFER_SIZE] = { 0 };
    snprintf(unexport_path, PWM_PATH_BUFFER_SIZE - 1, PWM_UNEXPORT,
             chip_number);

    iotjs_systemio_device_close(unexport_path, req_data->pin);
  }

  DDDLOG("Pwm Unexport - path: %s", path);

  req_data->result = kPwmErrOk;
}


#endif /* IOTJS_MODULE_PWM_LINUX_GENERAL_INL_H */
