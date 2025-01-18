#!/bin/env bash

echo -n "Final populations: "
../ecosystem ./profile_config.json | tail -1 | sed 's/.*{/{/'

hyperfine "../ecosystem ./profile_config.json"
