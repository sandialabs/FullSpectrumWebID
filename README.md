# Full Spectrum Web ID

Version 1.0.0 code, 20211003.

* * *

Full Spectrum Web ID is a web or command-line interface to the GADRAS Full Spectrum Isotope ID analysis algorithm.

* * *

Developed using [Wt](https://www.webtoolkit.eu/wt) 4.5.0, [boost](https://www.boost.org) 1.74, and [cmake](https://cmake.org) 3.18.3 on macOS, but should work with earlier
versions of these libraries on macOS, Linux, and Windows.


Use of this code requires pre-compiled libraries for the GADRAS Full Spectrum Isotope ID analysis algorithm, which are included for some Linux and Windows platforms, in the **GADRAS-DRF** package available from [RSICC](https://rsicc.ornl.gov).
After installing **GADRAS-DRF**, see `C:\GADRAS-DRF\Program\Documentation\LinuxAPIExamples` for the libraries and relevant header files.
Please note that the interfaces used in this code to call into the GADRAS libraries may not correspond to the **GADRAS-DRF** version available from [RSICC](https://rsicc.ornl.gov), so some adjustment may be necessary.

* * *
To configure and build:

```bash
cd /path/to/FullSpectrumId

cp -r /path/to/gadras ./3rd_party/Gadras/v18.8.11

mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=/path/to/boost/and/wt/prefix ..
make -j8
```

Please note that support for building the code may not be available.

## Authors
The primary authors of the user interface are Lee Harding and William Johnson.
The GADRAS Full Spectrum Isotope ID analysis algorithm, which is not included in this code, is maintained and written by the GADRAS team; please see the [GADRAS-DRF manual](https://www.osti.gov/servlets/purl/1431293) for more information, and [RSICC](https://rsicc.ornl.gov) to obtain the necessary libraries.
Parts of [InterSpec](http://github.com/sandialabs/InterSpec) code were re-used for spectrum file parsing, spectrum plotting, and time-chart plotting.
  

## License
This project is licensed under the LGPL v2.1 License - see the [LICENSE.txt](LICENSE.txt) file for details


## Copyright
Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC (NTESS).
Under the terms of Contract DE-NA0003525 with NTESS, the U.S. Government retains certain rights in this software.

  

## Disclaimer

```
DISCLAIMER OF LIABILITY NOTICE:
The United States Government shall not be liable or responsible for any maintenance,
updating or for correction of any errors in the SOFTWARE or subsequent approved version
releases.
  
THE Full Spectrum Web ID (SOFTWARE) AND ANY OF ITS SUBSEQUENT VERSION
RELEASES, SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY OF
ANY KIND, EITHER EXPRESSED, IMPLIED, OR STATUTORY, INCLUDING, BUT
NOT LIMITED TO, ANY WARRANTY THAT THE SOFTWARE WILL CONFORM TO
SPECIFICATIONS, ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE, OR FREEDOM FROM INFRINGEMENT, ANY
WARRANTY THAT THE SOFTWARE WILL BE ERROR FREE, OR ANY WARRANTY
THAT THE DOCUMENTATION, IF PROVIDED, WILL CONFORM TO THE
SOFTWARE. IN NO EVENT SHALL THE UNITED STATES GOVERNMENT OR ITS
CONTRACTORS OR SUBCONTRACTORS BE LIABLE FOR ANY DAMAGES,
INCLUDING, BUT NOT LIMITED TO, DIRECT, INDIRECT, SPECIAL OR
CONSEQUENTIAL DAMAGES, ARISING OUT OF, RESULTING FROM, OR IN ANY
WAY CONNECTED WITH THE SOFTWARE OR ANY OTHER PROVIDED
DOCUMENTATION, WHETHER OR NOT BASED UPON WARRANTY, CONTRACT,
TORT, OR OTHERWISE, WHETHER OR NOT INJURY WAS SUSTAINED BY
PERSONS OR PROPERTY OR OTHERWISE, AND WHETHER OR NOT LOSS WAS
SUSTAINED FROM, OR AROSE OUT OF THE RESULT OF, OR USE OF, THE
SOFTWARE OR ANY PROVIDED DOCUMENTATION. THE UNITED STATES
GOVERNMENT DISCLAIMS ALL WARRANTIES AND LIABILITIES REGARDING
THIRD PARTY SOFTWARE, IF PRESENT IN THE SOFTWARE, AND DISTRIBUTES
IT "AS IS."

```



SCR# 2663
