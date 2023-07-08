#!/bin/bash

sed -i s"%/usr/lib/x86_64-linux-gnu/%/usr/libexec/cloonix/common/lib/%" immodules.cache
sed -i s"%/usr/share/locale%/usr/libexec/cloonix/common/share/locale%" immodules.cache

sed -i s"%/usr/lib/x86_64-linux-gnu/%/usr/libexec/cloonix/common/lib/%" loaders.cache
