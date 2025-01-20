#!/bin/env bash

echo -n "Final populations: "
../build/ecosystem ./profile_config.json | tail -1 | sed 's/.*{/{/'

hyperfine "../build/ecosystem ./profile_config.json"
