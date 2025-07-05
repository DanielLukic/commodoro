#!/bin/bash

echo "Testing input monitoring improvements..."
echo "1. Starting Commodoro with short intervals for testing"
echo "2. Let it go to IDLE state"
echo "3. Then move mouse or type to trigger auto-start"
echo ""

# Run with test intervals: 10s work, 5s break
./commodoro 10s 5s 2 10s