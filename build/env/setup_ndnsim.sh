#!/usr/bin/env bash
# Builds ndnSIM inside the ubuntu:20.04 "ndnsim" container (outside /work, so OneDrive never sees build output).
# Usage (host): docker exec -d ndnsim bash -lc 'bash /work/build/env/setup_ndnsim.sh > /root/build.log 2>&1; echo "EXIT $?" >> /root/build.log'
set -euo pipefail

JOBS="${JOBS:-4}"
export DEBIAN_FRONTEND=noninteractive

apt-get update
apt-get install -y build-essential libsqlite3-dev libboost-all-dev libssl-dev git pkg-config \
    python3 python3-setuptools python-is-python3 castxml

mkdir -p /root/ndnSIM
cd /root/ndnSIM
[ -d ns-3 ] || git clone https://github.com/named-data-ndnSIM/ns-3-dev.git ns-3
[ -d pybindgen ] || git clone https://github.com/named-data-ndnSIM/pybindgen.git pybindgen
[ -d ns-3/src/ndnSIM ] || git clone --recursive https://github.com/named-data-ndnSIM/ndnSIM.git ns-3/src/ndnSIM

cd ns-3
# debug profile: asserts + logging, for development (scenario: ./waf configure --debug)
./waf configure --disable-python --enable-examples
./waf -j"$JOBS"
./waf --run=ndn-simple
./waf install

# optimized profile, installed alongside: several times faster, for experiment sweeps
./waf configure -d optimized --out=build-opt --disable-python
./waf -j"$JOBS"
./waf install
ldconfig

echo "ndnSIM commit: $(git -C src/ndnSIM log -1 --format=%H)"
echo "NDNSIM_SETUP_OK"
