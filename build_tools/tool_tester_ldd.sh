#!/bin/bash


find /usr/libexec/cloonix/ | xargs ldd > /tmp/cloonix_ldd.first 2>/dev/null

echo " " > /tmp/cloonix_ldd.second

while IFS= read -r line; do

  if [[ $line == /* ]]; then
    saved_first=$line
  fi

  if [[ ! "$line" == *"/libexec/cloonix/"* ]]; then
    if [[ ! "$line" == *"linux-vdso.so"* ]]; then
      if [[ ! "$line" == *"ld-linux-"* ]]; then
        if [[ "$line" == *"=>"* ]]; then
          echo $saved_first >> /tmp/cloonix_ldd.second
          continue
        fi
      fi
    fi
  fi

done < /tmp/cloonix_ldd.first

sort /tmp/cloonix_ldd.second | uniq | xargs dirname


