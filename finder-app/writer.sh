#!/bin/bash
# Will use this multiple times
# help_statement="Usage: writer.sh <writefile> <writestr>"

# Check if there's any arguments at all
if [ $# == 0 ]; then
    echo -e "error: no arguments.\n$help_statement"
    exit 1
# Check if there's no second argument
elif [ $2 == "" ]; then
    echo -e "error: bad arguments.\n$help_statement"
    exit 1
fi

# Variables
writefile="$1"
writestr="$2"

# Make all parent dirs
mkdir -p $(dirname "$writefile")
# Make or edit the new file
echo "$writestr" > "$writefile"

# Check if we were able to make/edit the file
if [ $? -eq 0 ]; then
    echo "File $writefile filled with $writestr."
    exit 0
else
    echo "Error: Could not make/edit the file $writefile"
    exit 1
fi