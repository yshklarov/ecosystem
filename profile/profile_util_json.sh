#!/bin/env bash

if [ "$#" -ne 1 ]
then
  echo "Usage: profile_util_json.sh <large_test_file.json>"
  exit 1
fi

hyperfine "../build/util_json_test $1"
