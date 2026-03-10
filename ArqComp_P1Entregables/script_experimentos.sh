#!/bin/bash

# EJECUCION PROGRAMA BASE
gcc acp1.c -o acp1 -O0

for i in {1..10}
do
	for D in {1,2,4,8,16}
	do
		for L in {384,1152,10240,15360,40960,81920,163840}
		do
			./acp1 $D $L >> resultadosbase.txt
		done
	done
done

# VALORES DE L:
# 0.5s1, 1.5s1, 0.75s2, 2s2, 4s2, 8s2
# s1 = 768
# s2 = 20480

# EJECUCION EXPERIMENTO CON ENTEROS
gcc acp1_ints.c -o acp1 -O0

for i in {1..10}
do
	for D in {1,2,4,8,16}
	do
		for L in {384,1152,10240,15360,40960,81920,163840}
		do
			./acp1 $D $L >> resultadosints.txt
		done
	done
done

# EJECUCION EXPERIMENTO CON ACCESO DIRECTO
gcc acp1_directo.c -o acp1 -O0

for i in {1..10}
do
	for D in {1,2,4,8,16}
	do
		for L in {384,1152,10240,15360,40960,81920,163840}
		do
			./acp1 $D $L >> resultadosdirecto.txt
		done
	done
done