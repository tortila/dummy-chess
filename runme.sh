#!/bin/bash
 
gcc serwer.c -o serwer -Wall
gcc klient.c -o klient -Wall -std=c99 -D_SVID_SOURCE=1