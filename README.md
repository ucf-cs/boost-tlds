# Transactional Operations for Linked Data Structures

Fall 2018 Senior Design project for Group 42

Transactional Linked Data Structures refactored for Boost library submission

## Guide to the Documentation

### Preface

This documentation is presented in doxygen format to view the important components of LFDTT.
This is by no means an instruction guide to using LFDTT as an external library that is able 
to be included from the current C++ boost library. This guide allows parts of the code to be 
explained separately, simply through objects and functions.

There are necessary steps before using the LFDTT code along with testing tools that are also
provided. This guide will provide the instructions to utilize the LFDTT codebase and tools
that it provides. The documentation will also cover the basic implementation of the code so that 
a user will be able to properly use and implement their code along with the codebase.
 
Simply begin viewing the documentation through the navigation bar. By clicking on one of the
classes, it will present the name of the class/struct along with the file with which it belongs 
to. They will list the functions and attributes/variables that belong to that reference. By scrolling
down, it will explain what the parameters and return values are for each function. Some classes
will contain member variables which will also contain a brief description of what they represent.

### Installation Guide

The instructions for installing dependices were used in the Ubuntu distribution 18.04.1 for Linux
at the time of this documentation (April 2019). It may be different for some operating systems. 

##### Install Dependencies

sudo apt-get install libboost-all-dev libgoogle-perftools-dev libtool m4 automake cmake libtbb-dev libgsl0-dev <br>
OR <br>
sudo apt-get install libboost-all-dev <br>
sudo apt-get install libgoogle-perftools-dev<br>
sudo apt-get install libtool m4 automake<br>
sudo apt-get install cmake <br>
sudo apt-get install libtbb-dev <br>
sudo apt-get install libgs10-dev<br>

 
##### Install GCC

sudo apt-get update && \ <br>
sudo apt-get install build-essential software-properties-common -y && \ <br>
sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y && \ <br>
sudo apt-get update && \ <br>
sudo apt-get install gcc-snapshot -y && \ <br>
sudo apt-get update && \ <br>
sudo apt-get install gcc-7 g++-7 -y && \ <br>
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 60 --slave /usr/bin/g++ g++ /usr/bin/g++-7 && \ <br>
sudo apt-get install gcc-4.8 g++-4.8 -y && \ <br>
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.8 60 --slave /usr/bin/g++ g++ /usr/bin/g++-4.8; <br>
sudo update-alternatives --config gcc <br>
gcc -v <br>

##### Install PERF (Possibly needed)

sudo apt-get install libgoogle-perftools-dev <br>
sudo apt install linux-tools-common gawk <br>
sudo apt-get install linux-tools-generic <br>
sudo apt-get install linux-tools-4.13.0-45-generic <br>
sudo apt-get install linux-cloud-tools-4.13.0-45-generic <br>

##### Get from Github

mkdir temp <br>
cd temp <br>
git clone https://github.com/ucf-cs/boost-tlds.git <br>
(change "temp" folder name to "trans-dev") <br>
cd trans-dev <br>
bash bootstrap.sh <br>
cd ../trans-compile <br>
../trans-dev/configure <br>

##### MAKE and TEST

cd trans-compile <br>
make -j8 <br>
cd trans-dev/script <br>
python pqtest.py '../../trans-compile/src/trans' <br>
 
##### DEBUG
 
cd trans-compile <br>
make -j8 CXXFLAGS='-O0 -g' <br>
cd src <br>

gdb --args ./trans <data-structure> <nthreads> <iterations> <txn-size> <key-range> <percent-insert> <percent-delete> <br>
OR <br>
libtool --mode=execute gdb trans <br>
OR <br>
gdb ./trans <br>


### This concludes the guide.
