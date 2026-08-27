'use strict';

const addon = require('../build/Release/gil_napi');

/** GIL truth-value constants */
const GIL = Object.freeze({
  FALSE: 0,
  TRUE:  1,
  BOTH:  2
});

module.exports = {
  Script:   addon.Script,
  Frontier: addon.Frontier,
  Intent:   addon.Intent,
  GIL
};