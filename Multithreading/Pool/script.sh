#!/bin/bash
for ((i = 1; i<= 100; i++)) 
do
	g++ main.cpp -o main -DTHREADNUM=$i
	./main
done
