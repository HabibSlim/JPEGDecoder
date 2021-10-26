#!/bin/bash

# Code couleurs ANSII
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

# Génération des sorties
echo "Génération des sorties :"
echo -n "-> Sequential baseline"
for i in {1..12}; do 
    ../bin/jpeg2ppm input/sequential/test${i}.jpg temp/test${i}.ppm &>/dev/null
	if [ $? -eq 139 ]; then
	    echo "Crash on test : $i"
	    exit 1
    fi
    echo -n "."
done
echo "."
echo -n "-> Progressive"
for i in {13..19}; do 
    ../bin/jpeg2ppm input/progressive/test${i}.jpg temp/test${i}.ppm &>/dev/null
    if [ $? -eq 139 ]; then
	    echo "Crash on test : $i"
	    exit 1
    fi
    echo -n "."
done
echo "."
echo "Done !"

# Comparaison des sorties ppm produites
echo "Comparaison avec les sorties de référence"
for i in {1..19}; do 
    diff temp/test${i}.ppm expected_output/test${i}.ppm &>/dev/null
    es=$?
    if [ $es -ne 0 ]; then 
        echo -e "test_"$i".jpg : ${RED}FAILED${NC}"
    else 
        echo -e "test_"$i".jpg : ${GREEN}PASSED${NC}"
    fi
    rm temp/test${i}.ppm
done
