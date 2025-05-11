#!/bin/bash
#----------------------------------------------------------------------------#
NET="__IDENT__"
FRRZIP="zipfrr.zip"
#----------------------------------------------------------------------------#
LIST1="1 2 3 4 5"
LIST2="1 2 3"
LIST3="30 31 32 33 34 35 36 37 38 39 40 41 42 43 44"
for i in ${LIST1}; do
  cloonix_cli $NET add zip tod${i} eth=sv   $FRRZIP --startup_env="NODE_ID=tod${i}"
  cloonix_cli $NET add zip nod${i} eth=sv   $FRRZIP --startup_env="NODE_ID=nod${i}"
  cloonix_cli $NET add zip cod${i} eth=vvvv $FRRZIP --startup_env="NODE_ID=cod${i}"
  for j in ${LIST2}; do
    cloonix_cli $NET add zip lod${i}${j} eth=sv $FRRZIP --startup_env="NODE_ID=lod${i}${j}"
    cloonix_cli $NET add zip nod${i}${j} eth=vv $FRRZIP --startup_env="NODE_ID=nod${i}${j}"
  done
done
for i in ${LIST3}; do
  cloonix_cli $NET add zip sod${i} eth=s $FRRZIP --startup_env="NODE_ID=sod${i}"
done
#----------------------------------------------------------------------------#

cloonix_cli $NET add lan tod1 0 lan10
cloonix_cli $NET add lan tod5 1 lan10
cloonix_cli $NET add lan cod1 0 lan10

cloonix_cli $NET add lan tod1 1 lan11
cloonix_cli $NET add lan tod2 0 lan11
cloonix_cli $NET add lan cod2 0 lan11

cloonix_cli $NET add lan tod2 1 lan12
cloonix_cli $NET add lan tod3 0 lan12
cloonix_cli $NET add lan cod3 0 lan12

cloonix_cli $NET add lan tod3 1 lan13
cloonix_cli $NET add lan tod4 0 lan13
cloonix_cli $NET add lan cod4 0 lan13

cloonix_cli $NET add lan tod4 1 lan14
cloonix_cli $NET add lan tod5 0 lan14
cloonix_cli $NET add lan cod5 0 lan14

cloonix_cli $NET add lan nod5  1 lan15
cloonix_cli $NET add lan nod11 0 lan15
cloonix_cli $NET add lan cod1  1 lan15

cloonix_cli $NET add lan nod12 0 lan16
cloonix_cli $NET add lan cod1  2 lan16

cloonix_cli $NET add lan nod1  0 lan17
cloonix_cli $NET add lan nod13 0 lan17
cloonix_cli $NET add lan cod1  3 lan17

cloonix_cli $NET add lan nod1  1 lan18
cloonix_cli $NET add lan nod21 0 lan18
cloonix_cli $NET add lan cod2  1 lan18

cloonix_cli $NET add lan nod22 0 lan19
cloonix_cli $NET add lan cod2  2 lan19

cloonix_cli $NET add lan nod32 0 lan20
cloonix_cli $NET add lan cod3  2 lan20

cloonix_cli $NET add lan nod42 0 lan21
cloonix_cli $NET add lan cod4  2 lan21

cloonix_cli $NET add lan nod52 0 lan22
cloonix_cli $NET add lan cod5  2 lan22

cloonix_cli $NET add lan nod2  0 lan23
cloonix_cli $NET add lan nod23 0 lan23
cloonix_cli $NET add lan cod2  3 lan23

cloonix_cli $NET add lan nod2  1 lan24
cloonix_cli $NET add lan nod31 0 lan24
cloonix_cli $NET add lan cod3  1 lan24

cloonix_cli $NET add lan nod3  0 lan25
cloonix_cli $NET add lan nod33 0 lan25
cloonix_cli $NET add lan cod3  3 lan25

cloonix_cli $NET add lan nod3  1 lan26
cloonix_cli $NET add lan nod41 0 lan26
cloonix_cli $NET add lan cod4  1 lan26

cloonix_cli $NET add lan nod4  0 lan27
cloonix_cli $NET add lan nod43 0 lan27
cloonix_cli $NET add lan cod4  3 lan27

cloonix_cli $NET add lan nod4  1 lan28
cloonix_cli $NET add lan nod51 0 lan28
cloonix_cli $NET add lan cod5  1 lan28

cloonix_cli $NET add lan nod5  0 lan29
cloonix_cli $NET add lan nod53 0 lan29
cloonix_cli $NET add lan cod5  3 lan29

cloonix_cli $NET add lan nod11 1 lan30
cloonix_cli $NET add lan lod11 0 lan30
cloonix_cli $NET add lan lod53 1 lan30
cloonix_cli $NET add lan sod30 0 lan30

cloonix_cli $NET add lan nod12 1 lan31
cloonix_cli $NET add lan lod11 1 lan31
cloonix_cli $NET add lan lod12 0 lan31
cloonix_cli $NET add lan sod31 0 lan31

cloonix_cli $NET add lan nod13 1 lan32
cloonix_cli $NET add lan lod12 1 lan32
cloonix_cli $NET add lan lod13 0 lan32
cloonix_cli $NET add lan sod32 0 lan32

cloonix_cli $NET add lan nod21 1 lan33
cloonix_cli $NET add lan lod13 1 lan33
cloonix_cli $NET add lan lod21 0 lan33
cloonix_cli $NET add lan sod33 0 lan33

cloonix_cli $NET add lan nod22 1 lan34
cloonix_cli $NET add lan lod21 1 lan34
cloonix_cli $NET add lan lod22 0 lan34
cloonix_cli $NET add lan sod34 0 lan34

cloonix_cli $NET add lan nod23 1 lan35
cloonix_cli $NET add lan lod22 1 lan35
cloonix_cli $NET add lan lod23 0 lan35
cloonix_cli $NET add lan sod35 0 lan35

cloonix_cli $NET add lan nod31 1 lan36
cloonix_cli $NET add lan lod23 1 lan36
cloonix_cli $NET add lan lod31 0 lan36
cloonix_cli $NET add lan sod36 0 lan36

cloonix_cli $NET add lan nod32 1 lan37
cloonix_cli $NET add lan lod31 1 lan37
cloonix_cli $NET add lan lod32 0 lan37
cloonix_cli $NET add lan sod37 0 lan37

cloonix_cli $NET add lan nod33 1 lan38
cloonix_cli $NET add lan lod32 1 lan38
cloonix_cli $NET add lan lod33 0 lan38
cloonix_cli $NET add lan sod38 0 lan38

cloonix_cli $NET add lan nod41 1 lan39
cloonix_cli $NET add lan lod33 1 lan39
cloonix_cli $NET add lan lod41 0 lan39
cloonix_cli $NET add lan sod39 0 lan39

cloonix_cli $NET add lan nod42 1 lan40
cloonix_cli $NET add lan lod41 1 lan40
cloonix_cli $NET add lan lod42 0 lan40
cloonix_cli $NET add lan sod40 0 lan40

cloonix_cli $NET add lan nod43 1 lan41
cloonix_cli $NET add lan lod42 1 lan41
cloonix_cli $NET add lan lod43 0 lan41
cloonix_cli $NET add lan sod41 0 lan41

cloonix_cli $NET add lan nod51 1 lan42
cloonix_cli $NET add lan lod43 1 lan42
cloonix_cli $NET add lan lod51 0 lan42
cloonix_cli $NET add lan sod42 0 lan42

cloonix_cli $NET add lan nod52 1 lan43
cloonix_cli $NET add lan lod51 1 lan43
cloonix_cli $NET add lan lod52 0 lan43
cloonix_cli $NET add lan sod43 0 lan43

cloonix_cli $NET add lan nod53 1 lan44
cloonix_cli $NET add lan lod52 1 lan44
cloonix_cli $NET add lan lod53 0 lan44
cloonix_cli $NET add lan sod44 0 lan44
#----------------------------------------------------------------------------#

