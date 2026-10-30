## Welcome to Chan_SCCP

[![CI](https://github.com/mpaland/chan-sccp/actions/workflows/ccpp.yml/badge.svg?branch=develop)](https://github.com/mpaland/chan-sccp/actions/workflows/ccpp.yml)
[![CodeQL](https://github.com/mpaland/chan-sccp/actions/workflows/codeql-analysis.yml/badge.svg?branch=develop)](https://github.com/mpaland/chan-sccp/actions/workflows/codeql-analysis.yml)
[![Asterisk](https://img.shields.io/badge/Asterisk-20%20%7C%2021%20%7C%2022%20%7C%2024-orange?logo=asterisk)](https://www.asterisk.org/)
[![Last commit](https://img.shields.io/github/last-commit/mpaland/chan-sccp/develop?logo=git)](https://github.com/mpaland/chan-sccp/commits/develop)
[![License](https://img.shields.io/badge/license-GPL-blue)](LICENSE)
[![Documentation](https://img.shields.io/badge/docs-wiki-blue?logo=github)](https://github.com/chan-sccp/chan-sccp/wiki)
[![Chat](https://img.shields.io/badge/chat-Matrix-brightgreen?logo=matrix)](https://app.gitter.im/#/room/#chan-sccp_chan-sccp:gitter.im)
[![Upstream](https://img.shields.io/badge/upstream-chan--sccp%2Fchan--sccp-lightgrey?logo=github)](https://github.com/chan-sccp/chan-sccp)

Chan_SCCP is free software. Please see the file [COPYING](COPYING) for details.
For documentation, please see the files in the [doc](doc/) subdirectory; the bug and compatibility analysis
behind this fork is in [doc/ANALYSIS.md](doc/ANALYSIS.md), the change summary in [NEWS](NEWS).
For building and installation instructions please see the [INSTALL](INSTALL) file.


This is a fork of the [chan-sccp](https://github.com/chan-sccp/chan-sccp), lately maintained by Diederik de Groot. Unfortunately there hasn't been any progress the last years but Asterisk keeps moving on.  
I took all available PRs and used AI to analyze and fix further bugs. Result is this repo here.
Why the effort? Because Cisco built with the 7900 series the best phones ever. A rocksolid hardware, excellent buttons, a touch display on office models and a timeless design. Unfortunately, all following series got worse and worse.  
The SIP version of these phones neve got the love of Cisco and even the final version had several bugs, so the SCCP protocol is the way to go.  
I wanted to keep up all the work of Diederik and want to use my beloved 7975 phones the next decade.  
The original project is at [chan-sccp/chan-sccp](https://github.com/chan-sccp/chan-sccp) and accepts donations via [Liberapay](https://liberapay.com/chan-sccp/donate).




### Table of Contents

* [Wiki](#wiki)
* [Quick Start](#quick-start)
  * [Prerequisites](#prerequisites)
  * [Building from source](#building-from-source)
  * [Configuring](#configuring)
  * [Build and Install](#build-and-install)
  * [Required Asterisk Modules](#required-asterisk-modules)
* [License](#license)


### Wiki
You can find more information and documentation on the [Wiki](https://github.com/chan-sccp/chan-sccp/wiki/).  
The wiki contains detailed guides on how to setup and configure chan-sccp, using it directly with plain asterisk or integration into FreePBX.
You can also find dialplan snippets and usefull hints and tips on our wiki.


## Quick Start
### Prerequisites
Make sure you have the following installed on your system:
- c-compiler:
  - gcc >= 4.6  (note: older not supported, higher advised)
  - clang >= 3.6  (note: older not supported, higher advised)
- gnu make
- libraries:
  - libxml2-dev / libxml2-devel
  - libxslt1-dev / libxslt1-devel
  - gettext
  - libssl-dev / openssl-devel
- pbx:
  - asterisk >= 20 (older versions are not supported anymore!)
  - including source headers and debug symbols (asterisk-dev and asterisk-dbg / asterisk-devel and asterisk-debug-info)
  - chan_skinny module is prevented from loading in /etc/asterisk/modules.conf
- standard posix compatible applications like sed, awk, tr

### Building from source
#### Using git (recommended)
##### Clone github repository (once)
    git clone https://github.com/mpaland/chan-sccp.git chan-sccp
    cd chan-sccp

##### Update to latest state
    cd chan-sccp
    git fetch
    git pull

#### Using a tar.gz
retrieve the [develop branch as tar.gz](https://github.com/mpaland/chan-sccp/archive/refs/heads/develop.tar.gz) and save it to /tmp/chan-sccp_develop.tar.gz

    tar xvfz /tmp/chan-sccp_develop.tar.gz
    cd chan-sccp-develop

_Note: the [upstream releases](https://github.com/chan-sccp/chan-sccp/releases) (latest v4.3.5, 2021) do not build against Asterisk >= 20._

### Configuring
    ./configure [....configure flags you prefer...]

_Note: For more information about the possible configure flags, check:_
    ./configure --help 

_Note: When you are making changes to configure.ac, autoconf / or Makefile.am files you should run:_
    ./tools/bootstrap.sh

### Build and Install
    make -j2 && make install && make reload

### Required Asterisk Modules

Make sure you have the following asterisk modules loaded before loading the chan_sccp
module:
 - app_voicemail
 - bridge_simple
 - bridge_native_rtp
 - bridge_softmix
 - bridge_holding
 - res_stasis
 - res_stasis_device_state

### License
[![License](https://img.shields.io/badge/license-GPL-blue)](LICENSE)

Chan_SCCP is licensed under the GNU General Public License, see [LICENSE](LICENSE) and [COPYING](COPYING).
