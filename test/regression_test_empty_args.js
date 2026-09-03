'use strict';

const { Frontier, GIL } = require('..');

// Test for inconsistent handling of empty argument arrays
console.log('Testing empty argument array handling...');

try {
  const f = new Frontier();
  
  // Test Set with empty args array
  try {
    f.set('test_predicate', [], GIL.TRUE);
    console.log('Set with empty args works');
  } catch (e) {
    console.error('Set with empty args failed:', e.message);
  }
  
  // Test Del with empty args array
  try {
    f.del('test_predicate', []);
    console.log('Del with empty args works');
  } catch (e) {
    console.error('Del with empty args failed:', e.message);
  }
  
  // Test Get with empty args array
  try {
    const result = f.get('test_predicate', []);
    console.log('Get with empty args works, result:', result);
  } catch (e) {
    console.error('Get with empty args failed:', e.message);
  }
  
  console.log('Empty argument handling test completed successfully');
} catch (e) {
  console.error('Test setup failed:', e.message);
  process.exit(1);
}