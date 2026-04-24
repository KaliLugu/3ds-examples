#!/bin/bash
mkdir -p romfs
curl -o romfs/cacert.pem https://curl.se/ca/cacert.pem
echo "cacert.pem downloaded to romfs/"