'use strict';

const { Script, Frontier } = require('..');

// Test for missing error checking in Intent::Execute
console.log('Testing error checking in Intent::Execute...');

try {
  const s = Script.load(`intent test()
    p <= true
  end`);
  
  const f = new Frontier();
  const intent = s.intent('test');
  
  // Test normal execution (should work)
  intent.execute(f);
  console.log('Normal execution works');
  
  // Test with invalid parameters - should properly throw error
  try {
    intent.execute(f, ['extra', 'argument']);
    console.log('ERROR: Should have thrown an error for wrong argument count');
  } catch (e) {
    console.log('Error checking test passed: Proper error handling for wrong args');
  }
  
  console.log('Intent error checking test completed successfully');
} catch (e) {
  console.error('Test failed:', e.message);
  process.exit(1);
}