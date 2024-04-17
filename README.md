## Elosys Chain

This is the official Elosys Chain sourcecode repository based on https://github.com/jl777/komodo.

## Komodo Platform Technologies Integrated In Elosys Chain

- Delayed Proof of Work (dPoW) - Additional security layer and Komodos own consensus algorithm  
- zk-SNARKs - Komodo Platform's privacy technology for shielded transactions  


## Tech Specification
- Max Supply: 200 million ELO
- Block Time: 60s
- Block Reward: 256 ELO
- Mining Algorithm: Equihash 200,9

## About this Project
Elosys Chain (ELO) is a 100% private send cryptocurrency. It uses a privacy protocol that cannot be compromised by other users activity on the network. Most privacy coins are riddled with holes created by optional privacy. Elosys Chain uses zk-SNARKs to shield 100% of the peer to peer transactions on the blockchain making for highly anonymous and private transactions.

## Getting started
Build the code as described below. To see instructions on how to construct and send an offline transaction look
at README_offline_transaction_signing.md

A list of outstanding improvements is included in README_todo.md

### Dependencies Ubuntu

```shell
#The following packages are needed:
sudo apt-get update && sudo apt-get upgrade -y
sudo apt-get install build-essential pkg-config libc6-dev m4 g++-multilib autoconf libtool libncurses-dev unzip git python zlib1g-dev wget bsdmainutils automake libboost-all-dev libssl-dev libprotobuf-dev protobuf-compiler libqrencode-dev libdb++-dev ntp ntpdate nano software-properties-common curl libevent-dev libcurl4-gnutls-dev cmake clang libsodium-dev -y


### Dependencies (Ubuntu 20.04)
```shell
#The following packages are needed:
sudo apt-get install build-essential pkg-config libc6-dev m4 g++-multilib autoconf libtool ncurses-dev unzip git python3 python3-zmq zlib1g-dev wget libcurl4-gnutls-dev bsdmainutils automake curl libsodium-dev bison

```shell
#On newer Linux distributions, like Ubuntu 22.04, the following additional packages are required:
sudo apt-get install liblz4-dev libbrotli-dev
```

### Dependencies Manjaro
```shell
#The following packages are needed:
pacman -Syu base-devel pkg-config glibc m4 gcc autoconf libtool ncurses unzip git python python-pyzmq zlib wget libcurl-gnutls automake curl cmake mingw-w64
```
### Build Elosys

This software is based on zcash and considered experimental and is continuously undergoing heavy development.

The dev branch is considered the bleeding edge codebase while the master-branch is considered tested (unit tests, runtime tests, functionality). At no point of time do the Elosys developers take any responsibility for any damage out of the usage of this software.
Elosys builds for all operating systems out of the same codebase. Follow the OS specific instructions from below.

#### Linux
```shell
git clone https://github.com/Elosys/Elosys_Testnet --branch master
cd elosys
# This step is not required for when using the Qt GUI
./zcutil/fetch-params.sh

# -j8 = using 8 threads for the compilation - replace 8 with number of threads you want to use; -j$(nproc) for all threads available

#For CLI binaries
./zcutil/build.sh -j8

#For qt GUI binaries
./zcutil/build-qt-linux.sh -j8.

#If you get this compile error:
qt/moc_addressbookpage.cpp:142:1: error: ‘QT_INIT_METAOBJECT’ does not name a type
  142 | QT_INIT_METAOBJECT const QMetaObject AddressBookPage::staticMetaObject = { {
      | ^~~~~~~~~~~~~~~~~~
  146 | QT_INIT_METAOBJECT const QMetaObject ZAddressBookPage::staticMetaObject = { {
      | ^~~~~~~~~~~~~~~~~~

Qt is incompatible with the libgl library.
Remove library: sudo apt-get remove libgl-dev
Install libraries: sudo apt-get install mesa-common-dev libglu1-mesa-dev
```

#### OSX
Ensure you have [brew](https://brew.sh) and the command line tools installed (comes automatically with XCode) and run:
```shell
brew update
brew upgrade
brew tap discoteq/discoteq; brew install flock
brew install autoconf autogen automake gcc@9 binutilsprotobuf coreutils wget python3
git clone https://github.com/Elosys/Elosys_Testnet --branch master
cd elosys
# This step is not required for when using the Qt GUI
./zcutil/fetch-params.sh

# -j8 = using 8 threads for the compilation - replace 8 with number of threads you want to use; -j$(nproc) for all threads available

#For CLI binaries
./zcutil/build-mac.sh -j8

#For qt GUI binaries
./zcutil/build-qt-mac.sh -j8
```

#### Windows
Use a debian cross-compilation setup with mingw for windows and run:
```shell
sudo apt-get install build-essential pkg-config libc6-dev m4 g++-multilib autoconf libtool ncurses-dev unzip git python python-zmq zlib1g-dev wget libcurl4-gnutls-dev bsdmainutils automake curl cmake mingw-w64
curl https://sh.rustup.rs -sSf | sh
source $HOME/.cargo/env
rustup target add x86_64-pc-windows-gnu

sudo update-alternatives --config x86_64-w64-mingw32-gcc
# (configure to use POSIX variant)
sudo update-alternatives --config x86_64-w64-mingw32-g++
# (configure to use POSIX variant)

git clone https://github.com/Elosys/Elosys_Testnet --branch master
cd elosys
# This step is not required for when using the Qt GUI
./zcutil/fetch-params.sh

# -j8 = using 8 threads for the compilation - replace 8 with number of threads you want to use; -j$(nproc) for all threads available

#For CLI binaries
./zcutil/build-win.sh -j8

#For qt GUI binaries
./zcutil/build-qt-win.sh -j8
```
**Elosys is experimental and a work-in-progress.** Use at your own risk.

To run the Elosys GUI wallet:

**Linux**
`elosys-qt-linux`

**OSX**
`elosys-qt-mac`

**Windows**
`elosys-qt-win.exe`


To run the daemon for Elosys Chain:  
`elosysd`
both elosysd and elosys-cli are located in the src directory after successfully building  

To reset the Elosys Chain blockchain change into the *~/.komodo/ELOSYS* data directory and delete the corresponding files by running `rm -rf blocks chainstate debug.log komodostate db.log` and restart daemon

To initiate a bootstrap download on the GUI wallet add bootstrap=1 to the ELOSYS.conf file.


**Elosys is based on Komodo which is unfinished and highly experimental.** Use at your own risk.

License
-------
For license information see the file [COPYING](COPYING).


Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
