# xAPI converter

Application that converts Moodle logs into xAPI statements and sends them into LRS.

Anssi Gröhn (c) 2018-2019

(firstname lastname at karelia dot fi)

# Ubuntu 18.04 prerequirements

```bash
$ sudo apt install build-essentials libboost-program-options-dev libcurl4-gnutls libcurlpp0 libcurlpp-dev gcc-8 cmake
$ sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 700 --slave /usr/bin/g++ g++ /usr/bin/g++-7
$ sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 800 --slave /usr/bin/g++ g++ /usr/bin/g++-8
```
# Installing

Please make sure you compile xapi_converter with openmp support enabled.

```bash
$ git clone https://github.com/agrohn/xapi_converter
$ cd xapi_converter
$ mkdir build
$ cmake -DCMAKE_CXX_FLAGS="-O3 -fopenmp -Wall" ..
$ make 
```

# Usage

Please see xapi_converter --help for details.

# Examples

When using log entries from a single day, user data might not match ones visible in grading history. Therefore, it might
be required to pass participant information as a separate json file. You may construct it in various ways, as long as
format is as follows:

users.json:
```javascript
[
        { "name": "User name 1", "id": "0000", "email": "user.name1@domain" },
        { "name": "User name 2", "id": "1111", "email": "user.name2@domain" }
]
```

Pass users.json via --users option to xapi_converter:

```bash
$ xapi_converter --users users.json <other options>

```