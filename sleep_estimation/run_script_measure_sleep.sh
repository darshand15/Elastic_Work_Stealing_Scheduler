#!/bin/bash

g++ -std=c++20 measure_sleep.cpp -o measure_sleep_out

sum=0
i=1
n=30
while [ $i -le $n ]
do
  output=$(./measure_sleep_out)
  sum=$((sum + output))
  ((i++))
done

avg=$((sum / n))

echo "The average of the sleep estimate is $avg nanoseconds"