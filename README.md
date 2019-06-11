# xAPI converter

Application that converts Moodle logs into xAPI statements and sends them into LRS.

Anssi Gröhn (c) 2018-2019

(firstname lastname at karelia dot fi)

# Ubuntu 18.04 prerequirements

```bash
$ sudo apt install libboost-program-options-dev libcurl4-gnutls-dev libcurlpp0 libcurlpp-dev gcc-8 cmake pkg-config
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

Commonly there are two usage scenarios.


1. Parse data into memory, batch it into xAPI statements and send it directly to LRS.
2. Parse data, store batches to disk. Load data from disk, and send it to learning locker.

In any case, you need to download grade history and event log from Moodle in JSON format.

```bash
$ xapi_converter --log events.json --grades grade_history.json --courseurl "https://.." --coursename " <more options>
```
Please see xapi_converter --help for details.

# Examples

Some examples for special cases.

## Defining participant data via external file

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
Where `id` is the moodle user id number.

Passing user data to xapi_converter:

```bash
$ xapi_converter --users users.json <other options>

```

## Anonymizing data

Anonymization option attempts to obfuscate course particpant names and id numbers from data before sending it to learing locker. Anonymization works on course level, so if you send data from different courses, particpant data most likely will **not** match between them.

Passing anonymization option to xapi_converter:

```bash
$ xapi_converter --anonymize <other options>

```

## Authorizing assignments

This is an artificial construct, and does not represent actual learning event. This option instructs 
`xapi_converter` to generate special events  for all encountered assignments. 
This allows faster queries on mongodb when you wish to just list all  assignments from a course. Especially useful when 
xAPI data is used directly to provide visualization from all assignments on a course or courses.

Authorization will generate 'authorize' (`http://activitystrea.ms/schema/1.0/authorize`) event for each assignment, 
and set their timestamp to date specified via `--course-start`:

```bash
$ xapi_converter --authorize --course-start 2019-01-01 <other options>
```

