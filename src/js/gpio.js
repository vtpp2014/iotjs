/* Copyright 2015-present Samsung Electronics Co., Ltd. and other contributors
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

var gpio = process.binding(process.binding.gpio);
var util = require('util');


var defaultConfiguration = {
  direction: gpio.DIRECTION.OUT,
  mode: gpio.MODE.NONE
};


function Gpio() {
  if (!(this instanceof Gpio)) {
    return new Gpio();
  }
}

Gpio.prototype.open = function(configuration, callback) {
  return new GpioPin(configuration, callback);
};

Gpio.prototype.DIRECTION = gpio.DIRECTION;

Gpio.prototype.MODE = gpio.MODE;


// new GpioPin(configuration[, callback])
function GpioPin(configuration, callback) {
  var self = this;

  // validate pin
  if (util.isObject(configuration)) {
    if (!util.isNumber(configuration.pin)) {
      throw new TypeError('Bad configuration - pin is mandatory and number');
    }
  } else {
    throw new TypeError('Bad arguments - configuration should be Object')
  }

  // validate direction
  if (!util.isUndefined(configuration.direction)) {
    if (configuration.direction !== gpio.DIRECTION.IN &&
        configuration.direction !== gpio.DIRECTION.OUT) {
      throw new TypeError(
        'Bad configuration - direction should be DIRECTION.IN or OUT');
    }
  } else {
    configuration.direction = defaultConfiguration.direction;
  }

  // validate mode
  if (process.platform === 'linux') {
    configuration.mode = defaultConfiguration.mode;
  } else if (process.platform === 'nuttx') {
    var mode = configuration.mode;
    if (!util.isUndefined(mode)) {
      if (configuration.direction === gpio.DIRECTION.IN) {
        if (mode !== gpio.MODE.NONE && mode !== gpio.MODE.PULLUP &&
            mode !== gpio.MODE.PULLDOWN) {
          throw new TypeError(
            'Bad configuration - mode should be MODE.NONE, PULLUP or PULLDOWN');
        }
      } else if (configuration.direction === gpio.DIRECTION.OUT) {
        if (mode !== gpio.MODE.NONE && mode !== gpio.MODE.FLOAT &&
            mode !== gpio.MODE.PUSHPULL && mode !== gpio.MODE.OPENDRAIN) {
          throw new TypeError(
            'Bad configuration - ' +
            'mode should be MODE.NONE, FLOAT, PUSHPULL or OPENDRAIN');
        }
      }
    } else {
      configuration.mode = defaultConfiguration.mode;
    }
  }

  this._binding = new gpio.Gpio(configuration, function(err) {
    util.isFunction(callback) && callback.call(self, err);
  });

  process.on('exit', (function(self) {
    return function() {
      if (!util.isNull(self._binding)) {
        self.closeSync();
      }
    };
  })(this));
}

// gpio.write(value[, callback])
GpioPin.prototype.write = function(value, callback) {
  var self = this;

  if (util.isNull(this._binding)) {
    throw new Error('GPIO pin is not opened');
  }

  if (!util.isNumber(value) && !util.isBoolean(value)) {
    throw new TypeError('Bad arguments - value should be Boolean');
  }

  this._binding.write(!!value, function(err) {
    util.isFunction(callback) && callback.call(self, err);
  });
};

// gpio.writeSync(value)
GpioPin.prototype.writeSync = function(value) {
  if (util.isNull(this._binding)) {
    throw new Error('GPIO pin is not opened');
  }

  if (!util.isNumber(value) && !util.isBoolean(value)) {
    throw new TypeError('Bad arguments - value should be Boolean');
  }

  this._binding.writeSync(!!value);
};

// gpio.read([callback])
GpioPin.prototype.read = function(callback) {
  var self = this;

  if (util.isNull(this._binding)) {
    throw new Error('GPIO pin is not opened');
  }

  this._binding.read(function(err, value) {
    util.isFunction(callback) && callback.call(self, err, value);
  });
};

// gpio.readSync()
GpioPin.prototype.readSync = function() {
  if (util.isNull(this._binding)) {
    throw new Error('GPIO pin is not opened');
  }

  return this._binding.readSync();
};

// gpio.close([callback])
GpioPin.prototype.close = function(callback) {
  var self = this;

  if (util.isNull(this._binding)) {
    throw new Error('GPIO pin is not opened');
  }

  this._binding.close(function(err) {
    util.isFunction(callback) && callback.call(self, err, value);
  });

  this._binding = null;
};

// gpio.closeSync()
GpioPin.prototype.closeSync = function() {
  if (util.isNull(this._binding)) {
    throw new Error('GPIO pin is not opened');
  }

  this._binding.closeSync();

  this._binding = null;
};


module.exports = Gpio;
