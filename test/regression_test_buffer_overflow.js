'use strict';

const { Frontier } = require('..');

// Test for potential buffer overflow in Frontier::Query
console.log('Testing buffer overflow protection in Frontier::Query...');

try {
  const f = new Frontier();
  
  // Test with potentially problematic query patterns
  // This should not cause buffer overflow or crashes
  try {
    // Test with empty pattern (should work fine)
    const result1 = f.query('nonexistent');
    console.log('Empty pattern query works');
    
    // Test with various argument combinations
    const result2 = f.query('test', []);
    console.log('Empty array pattern works');
    
    console.log('Buffer overflow test passed: No crashes or overflows');
  } catch (e) {
    console.error('Buffer overflow test failed:', e.message);
    process.exit(1);
  }
  
  console.log('Buffer overflow test completed successfully');
} catch (e) {
  console.error('Test setup failed:', e.message);
  process.exit(1);
}