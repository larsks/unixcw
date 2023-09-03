#!/bin/bash

root=$(pwd)

cppcheck --enable=all \
		 --platform=unix64 \
		 --force \
		 --project=compile_commands.json \
		 --std=c99 \
		 --error-exitcode=-2 \
		 --max-ctu-depth=20 \
		 -i$root/src//xcwcp/moc_application.cc \
		 ./src

