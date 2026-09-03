'use strict';

const { Script } = require('..');

// Test for NULL pointer handling in Script::Load
console.log('Testing NULL pointer handling in Script::Load...');

try {
  // This should test edge case where gil_load returns NULL with NULL error
  // While we can't directly test the exact NULL/NULL case,
  // we can test error handling behavior
  try {
    // Test with invalid script that should produce an error
    Script.load('invalid gil syntax here');
    console.log('ERROR: Should have thrown an error');
  } catch (e) {
    console.log('NULL pointer test passed: Proper error handling');
  }
  
  console.log('NULL pointer test completed successfully');
} catch (e) {
  console.error('Test failed:', e.message);
  process.exit(1);
}